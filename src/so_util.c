/* so_util.c -- utils to load and hook .so modules
 *
 * Copyright (C) 2021 fgsfds, Andy Nguyen (original code for Switch)
 * Copyright (C) 2025 Jaakko Lukkari
 *
 * This software may be modified and distributed under the terms
 * of the MIT license.  See the LICENSE file for details.
 */

#include <assert.h>
#include <elf.h>
#include <errno.h>
#include <malloc.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>

#include "config.h"
#include "error.h"
#include "so_util.h"
#include "util.h"

// ELF constants in case they're not defined
#ifndef EM_AARCH64
#define EM_AARCH64 183
#endif
#include "elf.h"

void *text_base, *text_virtbase;
size_t text_size;

void *data_base, *data_virtbase;
size_t data_size;

static void *load_base, *load_virtbase;
static size_t load_size;

static void *so_base;

static Elf64_Ehdr *elf_hdr;
static Elf64_Phdr *prog_hdr;
static Elf64_Shdr *sec_hdr;
static Elf64_Sym *syms;
static int num_syms;

static char *shstrtab;
static char *dynstrtab;

void hook_thumb(uintptr_t addr, uintptr_t dst) {
  if (addr == 0)
    return;
  addr &= ~1;
  if (addr & 2) {
    uint16_t nop = 0xbf00;
    memcpy((void *)addr, &nop, sizeof(nop));
    addr += 2;
  }
  uint32_t hook[2];
  hook[0] = 0xf000f8df; // LDR PC, [PC]
  hook[1] = dst;
  memcpy((void *)addr, hook, sizeof(hook));
}

void hook_arm(uintptr_t addr, uintptr_t dst) {
  if (addr == 0)
    return;
  uint32_t hook[2];
  hook[0] = 0xe51ff004; // LDR PC, [PC, #-0x4]
  hook[1] = dst;
  memcpy((void *)addr, hook, sizeof(hook));
}

void hook_arm64(uintptr_t addr, uintptr_t dst) {
  if (addr == 0)
    return;
  // debugPrintf("hook_arm64: Hooking address 0x%lx with 0x%lx\n", addr, dst);
  uint32_t *hook = (uint32_t *)addr;
  hook[0] = 0x58000051u; // LDR X17, #0x8
  hook[1] = 0xd61f0220u; // BR X17
  *(uint64_t *)(hook + 2) = dst;
}

void hook_x86_64(uintptr_t addr, uintptr_t dst) {
  if (addr == 0)
    return;
  debugPrintf("hook_x86_64: Hooking address 0x%lx with 0x%lx\n", addr, dst);
  uint8_t *hook = (uint8_t *)addr;
  // JMP to absolute address (14 bytes)
  hook[0] = 0xFF;                // JMP
  hook[1] = 0x25;                // ModR/M for [RIP+0]
  *(uint32_t *)(hook + 2) = 0;   // RIP offset
  *(uint64_t *)(hook + 6) = dst; // Target address
}

// Make text segment writable for hooking
void so_make_text_writable(void) {
  const size_t text_asize = ALIGN_MEM(text_size, 0x1000);
  if (mprotect(text_virtbase, text_asize, PROT_READ | PROT_WRITE | PROT_EXEC) !=
      0) {
    debugPrintf("Warning: Could not make text segment writable for hooking\n");
  } else {
    debugPrintf("Text segment made writable for hooking\n");
  }
}

// Restore text segment to read-execute only
void so_make_text_executable(void) {
  const size_t text_asize = ALIGN_MEM(text_size, 0x1000);
  if (mprotect(text_virtbase, text_asize, PROT_READ | PROT_EXEC) != 0) {
    debugPrintf("Warning: Could not restore text segment permissions\n");
  } else {
    debugPrintf("Text segment restored to read-execute\n");
  }
}

void so_flush_caches(void) {
  // For ARM64 Linux, we need to flush caches manually
  // Use GCC builtin or inline assembly for cache operations
  __builtin___clear_cache((char *)load_virtbase,
                          (char *)load_virtbase + load_size);
}

void so_free_temp(void) {
  free(so_base);
  so_base = NULL;
}

static inline size_t round_up(size_t x, size_t a) {
  return (x + a - 1) & ~(a - 1);
}

static int protect_range(void *start, size_t len, int prot) {
  long ps = sysconf(_SC_PAGESIZE);
  if (ps <= 0)
    ps = 4096; // fallback
  uintptr_t addr = (uintptr_t)start;
  uintptr_t page_base = addr & ~((uintptr_t)ps - 1);
  size_t head = addr - page_base;
  size_t plen = round_up(len + head, (size_t)ps);
  if (mprotect((void *)page_base, plen, prot) != 0) {
    debugPrintf("mprotect(%p, %zu, 0x%x) failed: %s\n", (void *)page_base, plen,
                prot, strerror(errno));
    return -1;
  }
  return 0;
}

void so_finalize(void) {
  // For ARM64 Linux, use mprotect instead of Nintendo Switch syscalls

  // Map the text PT_LOAD as RX
  if (protect_range(text_virtbase, text_size, PROT_READ | PROT_EXEC) != 0) {
    fatal_error("Error: could not set RX on text at %p (size %zu)",
                text_virtbase, text_size);
  }

  // Map the data PT_LOAD as RW
  if (protect_range(data_virtbase, data_size, PROT_READ | PROT_WRITE) != 0) {
    fatal_error("Error: could not set RW on data at %p (size %zu)",
                data_virtbase, data_size);
  }
}

int so_load(const char *filename, void *base, size_t max_size) {
  int res = 0;
  size_t so_size = 0;
  int text_segno = -1;
  int data_segno = -1;

  debugPrintf("so_load: Opening %s\n", filename);
  FILE *fd = fopen(filename, "rb");
  if (fd == NULL) {
    debugPrintf("so_load: Failed to open file\n");
    return -1;
  }

  fseek(fd, 0, SEEK_END);
  so_size = ftell(fd);
  fseek(fd, 0, SEEK_SET);
  debugPrintf("so_load: File size: %zu bytes\n", so_size);

  so_base = malloc(so_size);
  if (!so_base) {
    debugPrintf("so_load: Failed to allocate %zu bytes for so_base\n", so_size);
    fclose(fd);
    return -2;
  }

  if (fread(so_base, so_size, 1, fd) != 1) {
    debugPrintf("so_load: Failed to read file\n");
    fclose(fd);
    free(so_base);
    return -3;
  }
  fclose(fd);
  debugPrintf("so_load: File read successfully\n");

  if (memcmp(so_base, ELFMAG, SELFMAG) != 0) {
    debugPrintf("so_load: Not a valid ELF file\n");
    res = -1;
    goto err_free_so;
  }
  debugPrintf("so_load: Valid ELF file detected\n");

  elf_hdr = (Elf64_Ehdr *)so_base;
  debugPrintf("so_load: ELF header at %p\n", elf_hdr);

  // Check ELF class and architecture
  debugPrintf("so_load: ELF class: %d (expected 2 for 64-bit)\n",
              elf_hdr->e_ident[EI_CLASS]);
  debugPrintf("so_load: ELF data: %d (expected 1 for little-endian)\n",
              elf_hdr->e_ident[EI_DATA]);
  debugPrintf("so_load: ELF machine: %d (expected 183 for AArch64)\n",
              elf_hdr->e_machine);
  debugPrintf("so_load: ELF type: %d (expected 3 for shared object)\n",
              elf_hdr->e_type);

  // Verify this is a valid ARM64 ELF
  if (elf_hdr->e_ident[EI_CLASS] != ELFCLASS64) {
    debugPrintf("so_load: Not a 64-bit ELF file\n");
    res = -1;
    goto err_free_so;
  }

  if (elf_hdr->e_machine != EM_AARCH64) {
    debugPrintf("so_load: Not an AArch64 ELF file (machine=%d)\n",
                elf_hdr->e_machine);
    res = -1;
    goto err_free_so;
  }

  debugPrintf("so_load: Program header offset: %lu, count: %d\n",
              elf_hdr->e_phoff, elf_hdr->e_phnum);
  debugPrintf("so_load: Section header offset: %lu, count: %d\n",
              elf_hdr->e_shoff, elf_hdr->e_shnum);

  // Bounds check for program headers
  if (elf_hdr->e_phoff + (elf_hdr->e_phnum * sizeof(Elf64_Phdr)) > so_size) {
    debugPrintf("so_load: Program headers extend beyond file\n");
    res = -1;
    goto err_free_so;
  }

  prog_hdr = (Elf64_Phdr *)((uintptr_t)so_base + elf_hdr->e_phoff);
  debugPrintf("so_load: Program header at %p\n", prog_hdr);

  // Bounds check for section headers
  if (elf_hdr->e_shoff + (elf_hdr->e_shnum * sizeof(Elf64_Shdr)) > so_size) {
    debugPrintf("so_load: Section headers extend beyond file\n");
    res = -1;
    goto err_free_so;
  }

  sec_hdr = (Elf64_Shdr *)((uintptr_t)so_base + elf_hdr->e_shoff);
  debugPrintf("so_load: Section header at %p\n", sec_hdr);

  // Bounds check for string table
  if (elf_hdr->e_shstrndx >= elf_hdr->e_shnum) {
    debugPrintf("so_load: Invalid string table index\n");
    res = -1;
    goto err_free_so;
  }

  if (sec_hdr[elf_hdr->e_shstrndx].sh_offset > so_size) {
    debugPrintf("so_load: String table extends beyond file\n");
    res = -1;
    goto err_free_so;
  }

  shstrtab =
      (char *)((uintptr_t)so_base + sec_hdr[elf_hdr->e_shstrndx].sh_offset);
  debugPrintf("so_load: String table at %p\n", shstrtab);
  debugPrintf("so_load: ELF header parsed, %d program headers\n",
              elf_hdr->e_phnum);

  // calculate total size of the LOAD segments
  for (int i = 0; i < elf_hdr->e_phnum; i++) {
    if (prog_hdr[i].p_type == PT_LOAD) {
      debugPrintf("so_load: Found LOAD segment %d, flags=0x%x\n", i,
                  prog_hdr[i].p_flags);
      const size_t prog_size =
          ALIGN_MEM(prog_hdr[i].p_memsz, prog_hdr[i].p_align);
      // get the segment numbers of text and data segments
      if ((prog_hdr[i].p_flags & PF_X) == PF_X) {
        text_segno = i;
        debugPrintf("so_load: Text segment found at %d\n", i);
      } else {
        // assume data has to be after text
        if (text_segno < 0) {
          debugPrintf("so_load: Data segment found before text segment\n");
          goto err_free_so;
        }
        data_segno = i;
        debugPrintf("so_load: Data segment found at %d\n", i);
        // since data is after text, total program size = last_data_offset +
        // last_data_aligned_size
        load_size = prog_hdr[i].p_vaddr + prog_size;
      }
    }
  }

  // align total size to page size
  load_size = ALIGN_MEM(load_size, 0x1000);
  debugPrintf("so_load: Total load size: %zu bytes (max: %zu)\n", load_size,
              max_size);
  if (load_size > max_size) {
    debugPrintf("so_load: Load size exceeds maximum\n");
    res = -3;
    goto err_free_so;
  }

  // allocate space for all load segments (align to page size)
  // TODO: find out a way to allocate memory that doesn't fuck with the heap
  load_base = base;
  if (!load_base) {
    debugPrintf("so_load: Load base is null\n");
    goto err_free_so;
  }
  debugPrintf("so_load: Clearing memory at %p, size %zu\n", load_base,
              load_size);
  memset(load_base, 0, load_size);

  // For ARM64 Linux, set load_virtbase to the same as load_base
  load_virtbase = load_base;

  debugPrintf("load base = %p\n", load_virtbase);

  // copy segments to where they belong

  // text
  text_size = prog_hdr[text_segno].p_memsz;
  text_virtbase =
      (void *)(prog_hdr[text_segno].p_vaddr + (Elf64_Addr)load_virtbase);
  text_base = (void *)(prog_hdr[text_segno].p_vaddr + (Elf64_Addr)load_base);
  prog_hdr[text_segno].p_vaddr = (Elf64_Addr)text_virtbase;
  memcpy(text_base,
         (void *)((uintptr_t)so_base + prog_hdr[text_segno].p_offset),
         prog_hdr[text_segno].p_filesz);

  // data
  data_size = prog_hdr[data_segno].p_memsz;
  data_virtbase =
      (void *)(prog_hdr[data_segno].p_vaddr + (Elf64_Addr)load_virtbase);
  data_base = (void *)(prog_hdr[data_segno].p_vaddr + (Elf64_Addr)load_base);
  prog_hdr[data_segno].p_vaddr = (Elf64_Addr)data_virtbase;
  memcpy(data_base,
         (void *)((uintptr_t)so_base + prog_hdr[data_segno].p_offset),
         prog_hdr[data_segno].p_filesz);

  syms = NULL;
  dynstrtab = NULL;

  for (int i = 0; i < elf_hdr->e_shnum; i++) {
    char *sh_name = shstrtab + sec_hdr[i].sh_name;
    if (strcmp(sh_name, ".dynsym") == 0) {
      syms = (Elf64_Sym *)((uintptr_t)text_base + sec_hdr[i].sh_addr);
      num_syms = sec_hdr[i].sh_size / sizeof(Elf64_Sym);
    } else if (strcmp(sh_name, ".dynstr") == 0) {
      dynstrtab = (char *)((uintptr_t)text_base + sec_hdr[i].sh_addr);
    }
  }

  if (syms == NULL || dynstrtab == NULL) {
    res = -2;
    goto err_free_load;
  }

  return 0;

err_free_load:
  free(load_base);
err_free_so:
  free(so_base);

  return res;
}

int so_relocate(void) {
  for (int i = 0; i < elf_hdr->e_shnum; i++) {
    char *sh_name = shstrtab + sec_hdr[i].sh_name;
    if (strcmp(sh_name, ".rela.dyn") == 0 ||
        strcmp(sh_name, ".rela.plt") == 0) {
      Elf64_Rela *rels =
          (Elf64_Rela *)((uintptr_t)text_base + sec_hdr[i].sh_addr);
      for (int j = 0; j < sec_hdr[i].sh_size / sizeof(Elf64_Rela); j++) {
        uintptr_t *ptr = (uintptr_t *)((uintptr_t)text_base + rels[j].r_offset);
        Elf64_Sym *sym = &syms[ELF64_R_SYM(rels[j].r_info)];

        int type = ELF64_R_TYPE(rels[j].r_info);
        switch (type) {
        case R_AARCH64_ABS64:
          // FIXME: = or += ?
          *ptr = (uintptr_t)text_virtbase + sym->st_value + rels[j].r_addend;
          break;

        case R_AARCH64_RELATIVE:
          // sometimes the value of r_addend is also at *ptr
          *ptr = (uintptr_t)text_virtbase + rels[j].r_addend;
          break;

        case R_AARCH64_GLOB_DAT:
        case R_AARCH64_JUMP_SLOT: {
          if (sym->st_shndx != SHN_UNDEF)
            *ptr = (uintptr_t)text_virtbase + sym->st_value + rels[j].r_addend;
          break;
        }

        default:
          fatal_error("Error: unknown relocation type:\n%x\n", type);
          break;
        }
      }
    }
  }

  return 0;
}

int so_resolve(DynLibFunction *funcs, int num_funcs,
               int taint_missing_imports) {
  for (int i = 0; i < elf_hdr->e_shnum; i++) {
    char *sh_name = shstrtab + sec_hdr[i].sh_name;
    if (strcmp(sh_name, ".rela.dyn") == 0 ||
        strcmp(sh_name, ".rela.plt") == 0) {
      Elf64_Rela *rels =
          (Elf64_Rela *)((uintptr_t)text_base + sec_hdr[i].sh_addr);
      for (int j = 0; j < sec_hdr[i].sh_size / sizeof(Elf64_Rela); j++) {
        uintptr_t *ptr = (uintptr_t *)((uintptr_t)text_base + rels[j].r_offset);
        Elf64_Sym *sym = &syms[ELF64_R_SYM(rels[j].r_info)];

        int type = ELF64_R_TYPE(rels[j].r_info);
        switch (type) {
        case R_AARCH64_GLOB_DAT:
        case R_AARCH64_JUMP_SLOT: {
          if (sym->st_shndx == SHN_UNDEF) {
            // make it crash for debugging
            if (taint_missing_imports)
              *ptr = rels[j].r_offset;

            char *name = dynstrtab + sym->st_name;
            for (int k = 0; k < num_funcs; k++) {
              if (strcmp(name, funcs[k].symbol) == 0) {
                *ptr = funcs[k].func;
                break;
              }
            }
          }

          break;
        }

        default:
          break;
        }
      }
    }
  }

  return 0;
}

void so_execute_init_array(void) {
  for (int i = 0; i < elf_hdr->e_shnum; i++) {
    char *sh_name = shstrtab + sec_hdr[i].sh_name;
    if (strcmp(sh_name, ".init_array") == 0) {
      int (**init_array)() =
          (void *)((uintptr_t)text_virtbase + sec_hdr[i].sh_addr);
      for (int j = 0; j < sec_hdr[i].sh_size / 8; j++) {
        if (init_array[j] != 0)
          init_array[j]();
      }
    }
  }
}

uintptr_t so_find_addr(const char *symbol) {
  for (int i = 0; i < num_syms; i++) {
    char *name = dynstrtab + syms[i].st_name;
    if (strcmp(name, symbol) == 0)
      return (uintptr_t)text_base + syms[i].st_value;
  }

  fatal_error("Error: could not find symbol:\n%s\n", symbol);
  return 0;
}

uintptr_t so_find_rel_addr(const char *symbol) {
  for (int i = 0; i < elf_hdr->e_shnum; i++) {
    char *sh_name = shstrtab + sec_hdr[i].sh_name;
    if (strcmp(sh_name, ".rela.dyn") == 0 ||
        strcmp(sh_name, ".rela.plt") == 0) {
      Elf64_Rela *rels =
          (Elf64_Rela *)((uintptr_t)text_base + sec_hdr[i].sh_addr);
      for (int j = 0; j < sec_hdr[i].sh_size / sizeof(Elf64_Rela); j++) {
        Elf64_Sym *sym = &syms[ELF64_R_SYM(rels[j].r_info)];

        int type = ELF64_R_TYPE(rels[j].r_info);
        if (type == R_AARCH64_GLOB_DAT || type == R_AARCH64_JUMP_SLOT) {
          char *name = dynstrtab + sym->st_name;
          if (strcmp(name, symbol) == 0)
            return (uintptr_t)text_base + rels[j].r_offset;
        }
      }
    }
  }

  fatal_error("Error: could not find symbol:\n%s\n", symbol);
  return 0;
}

uintptr_t so_find_addr_rx(const char *symbol) {
  for (int i = 0; i < num_syms; i++) {
    char *name = dynstrtab + syms[i].st_name;
    if (strcmp(name, symbol) == 0)
      return (uintptr_t)text_virtbase + syms[i].st_value;
  }

  fatal_error("Error: could not find symbol:\n%s\n", symbol);
  return 0;
}

DynLibFunction *so_find_import(DynLibFunction *funcs, int num_funcs,
                               const char *name) {
  for (int i = 0; i < num_funcs; ++i)
    if (!strcmp(funcs[i].symbol, name))
      return &funcs[i];
  return NULL;
}

int so_unload(void) {
  if (load_base == NULL)
    return -1;

  if (so_base) {
    // someone forgot to free the temp data
    so_free_temp();
  }

  // For ARM64 Linux, simply unmap the memory
  if (munmap(load_base, load_size) != 0) {
    fatal_error("Error: could not unmap library memory");
  }

  return 0;
}

/* main.c
 *
 * Copyright (C) 2021 fgsfds, Andy Nguyen (Switch port)
 * Copyright (C) 2025 Jaakko Lukkari
 *
 * This software may be modified and distributed under the terms
 * of the MIT license.  See the LICENSE file for details.
 */

#include <ctype.h>
#include <fcntl.h>
#include <linux/fb.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

#include "config.h"
#include "error.h"
#include "gamedata_mapping.h"
#include "hooks.h"
#include "imports.h"
#include "so_util.h"
#include "util.h"
#include "videoplayer.h"

static void *heap_so_base = NULL;
static size_t heap_so_limit = 0;

// Initialize heap for ARM64 Linux
static void init_heap(void) {
  size_t heap_size = MEMORY_MB * 1024 * 1024;

  // Allocate memory for the shared library
  void *addr = mmap(NULL, heap_size, PROT_READ | PROT_WRITE,
                    MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);

  if (addr == MAP_FAILED) {
    fatal_error("Failed to allocate heap memory");
    return;
  }

  heap_so_base = addr;
  heap_so_limit = heap_size;
}

static void check_data(void) {
  const char *files[] = {
      "gamedata/MaxPayneSoundsv2.msf",
      "gamedata/x_data.ras",
      "gamedata/x_english.ras",
      "gamedata/x_level1.ras",
      "gamedata/x_level2.ras",
      "gamedata/x_level3.ras",
      "gamedata/data",
      "gamedata/es2",
      // if this is missing, assets folder hasn't been merged in
      "gamedata/es2/DefaultPixel.txt",
      // mod file goes here
      "",
  };
  struct stat st;
  unsigned int numfiles = (sizeof(files) / sizeof(*files)) - 1;
  // if mod is enabled, also check for mod file
  if (config.mod_file[0])
    files[numfiles++] = config.mod_file;
  // check if all the required files are present
  for (unsigned int i = 0; i < numfiles; ++i) {
    const char *file = gamedata_mapping_get(files[i]);
    if (!file) {
      file = files[i];
    }
    if (stat(file, &st) < 0) {
      fatal_error("Could not find\n%s.\nCheck your data files.", file);
      break;
    }
  }
}

static void check_syscalls(void) {
  // No specific syscalls needed for generic ARM64 Linux
}

static void check_for_4x3(void) {
  // get screen width and height from framebuffer
  int fb_fd = open("/dev/fb0", O_RDONLY);
  if (fb_fd >= 0) {
    struct fb_var_screeninfo vinfo;
    if (ioctl(fb_fd, FBIOGET_VSCREENINFO, &vinfo) == 0) {
      int screen_width = vinfo.xres;
      int screen_height = vinfo.yres;
      debugPrintf("Framebuffer resolution: %dx%d\n", screen_width,
                  screen_height);

      float aspect_ratio = (float)screen_width / (float)screen_height;
      // check if height is approximately 3/4 of width (4:3 aspect ratio)
      if (aspect_ratio < 1.4f) {
        *(uint8_t *)so_find_addr("Use4x3") = 1; // Enable 4:3 aspect ratio
      } else {
        debugPrintf(
            "Aspect ratio is not 4:3 (or close), keeping widescreen mode\n");
      }
    } else {
      debugPrintf("Failed to get variable screen info from framebuffer\n");
    }
    close(fb_fd);
  }
}

int static check_filesystem_case_sensitive(void) {
  // check with a file we know that exists

  char *path = strdup(CONFIG_NAME);
  for (char *p = path; *p; ++p) {
    *p = tolower((unsigned char)*p);
  }

  struct stat st;
  if (stat(path, &st) < 0) {
    free(path);
    return 1;
  }

  free(path);
  return 0;
}

int main(void) {
  debugPrintf("Max Payne for ARM64 Linux\n");

  // Initialize heap for ARM64 Linux
  init_heap();

  // try to read the config file and create one with default values if it's
  // missing
  if (read_config(CONFIG_NAME) < 0)
    write_config(CONFIG_NAME);
  // debugPrintf("Config loaded.\n");

  // debugPrintf("Checking system calls...\n");
  check_syscalls();

  debugPrintf("Checking data files...\n");
  check_data();

  if (check_filesystem_case_sensitive()) {
    debugPrintf("Case sensitive filesystem detected! Creating a lookup map "
                "for filepaths.\n");

    if (gamedata_mapping_init() < 0) {
      fatal_error("Failed to initialize gamedata mapping");
    }
  } else {
    debugPrintf("Case insensitive filesystem detected, no need for gamedata "
                "mapping.\n");
  }

  // debugPrintf("heap size = %u KB\n", MEMORY_MB * 1024);
  // debugPrintf(" lib base = %p\n", heap_so_base);
  // debugPrintf("  lib max = %u KB\n", heap_so_limit / 1024);

  // debugPrintf("Loading %s...\n", SO_NAME);

  // Check if file exists and is readable
  struct stat st;
  if (stat(SO_NAME, &st) != 0) {
    fatal_error("Cannot find %s. Make sure it's in the current directory.",
                SO_NAME);
  }
  // debugPrintf("Found %s (size: %ld bytes)\n", SO_NAME, st.st_size);

  if (so_load(SO_NAME, heap_so_base, heap_so_limit) < 0)
    fatal_error("Could not load\n%s.", SO_NAME);

  // won't save without it
  // debugPrintf("Creating savegames directory...\n");
  mkdir("gamedata/savegames", 0755);

  videoplayer_set_overlay("Loading...");

  // debugPrintf("Updating imports...\n");
  update_imports();

  // debugPrintf("Relocating and resolving...\n");
  // debugPrintf("Relocating and resolving...\n");
  so_relocate();
  so_resolve(dynlib_functions, dynlib_numfunctions, 1);

  // Make text segment writable for patching
  // debugPrintf("Making text segment writable for patching...\n");
  so_make_text_writable();

  // debugPrintf("Patching...\n");
  // debugPrintf("Patching OpenAL...\n");
  patch_openal();

  // debugPrintf("Patching OpenGL...\n");
  patch_opengl();

  // debugPrintf("Patching game...\n");
  patch_game();

  // Restore text segment permissions
  // debugPrintf("Restoring text segment permissions...\n");
  so_make_text_executable();

  // can't set it in the initializer because it's not constant
  stderr_fake = stderr;

  debugPrintf("Setting up game variables...\n");
  strcpy((char *)so_find_addr("StorageRootBuffer"), "gamedata");
  *(uint8_t *)so_find_addr("IsAndroidPaused") = 0;
  *(uint8_t *)so_find_addr("UseRGBA8") = 1; // RGB565 FBOs suck

  if (!config.force_widescreen) {
    check_for_4x3();
  }

  debugPrintf("Finding game functions...\n");
  uint32_t (*initGraphics)(void) = (void *)so_find_addr_rx("_Z12initGraphicsv");
  uint32_t (*ShowJoystick)(int show) =
      (void *)so_find_addr_rx("_Z12ShowJoystickb");
  int (*NVEventAppMain)(int argc, char *argv[]) =
      (void *)so_find_addr_rx("_Z14NVEventAppMainiPPc");

  debugPrintf("initGraphics function at: %p\n", initGraphics);
  debugPrintf("ShowJoystick function at: %p\n", ShowJoystick);
  debugPrintf("NVEventAppMain function at: %p\n", NVEventAppMain);

  debugPrintf("Finalizing ELF...\n");
  so_finalize();
  so_flush_caches();
  so_execute_init_array();

  debugPrintf("Freeing temporary memory...\n");
  so_free_temp();

  debugPrintf("Calling initGraphics()...\n");
  initGraphics();
  debugPrintf("initGraphics() completed\n");

  debugPrintf("Calling ShowJoystick(0)...\n");
  ShowJoystick(0);
  debugPrintf("ShowJoystick() completed\n");

  debugPrintf("Calling NVEventAppMain(0, NULL)...\n");
  NVEventAppMain(0, NULL);
  debugPrintf("NVEventAppMain() completed\n");

  debugPrintf("Cleaning up gamedata mapping...\n");
  gamedata_mapping_cleanup();

  exit_game(0);

  return 0;
}

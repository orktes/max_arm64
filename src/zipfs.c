/* zipfs.c -- ZIP filesystem abstraction providing standard file operations
 *
 * Copyright (C) 2025 Jaakko Lukkari
 *
 * This software may be modified and distributed under the terms
 * of the MIT license.  See the LICENSE file for details.
 */

#define _GNU_SOURCE // For fopencookie on Linux
#include "zipfs.h"
#include "util.h"
#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include "miniz.h"

// Internal structure for a file entry in the ZIP
typedef struct {
  char *normalized_path; // Normalized path (lowercase, forward slashes)
  mz_uint file_index;    // Index in the ZIP archive
  mz_uint64 size;        // Uncompressed size
} zipfs_entry_t;

// ZIP filesystem instance
struct zipfs_t {
  char *zip_path;         // Path to the ZIP file
  mz_zip_archive archive; // miniz archive structure
  zipfs_entry_t *entries; // Array of file entries
  size_t num_entries;     // Number of entries
};

// Cookie data for FILE* created with fopencookie
typedef struct {
  zipfs_t *fs;           // Parent filesystem
  unsigned char *data;   // Cached file data (uncompressed)
  mz_uint64 size;        // File size
  mz_uint64 position;    // Current read position
} zipfs_cookie_t;

// Normalize a path: convert to lowercase and use forward slashes
static char *normalize_path(const char *path) {
  if (!path)
    return NULL;

  // Skip leading slashes/backslashes
  while (*path == '/' || *path == '\\')
    path++;

  char *normalized = strdup(path);
  if (!normalized)
    return NULL;

  // Convert to lowercase and replace backslashes with forward slashes
  for (char *p = normalized; *p; p++) {
    if (*p == '\\')
      *p = '/';
    else
      *p = tolower(*p);
  }

  return normalized;
}

// Find an entry by normalized path
static zipfs_entry_t *find_entry(zipfs_t *fs, const char *path) {
  char *normalized = normalize_path(path);
  if (!normalized)
    return NULL;

  zipfs_entry_t *found = NULL;
  for (size_t i = 0; i < fs->num_entries; i++) {
    if (strcmp(fs->entries[i].normalized_path, normalized) == 0) {
      found = &fs->entries[i];
      break;
    }
  }

  free(normalized);
  return found;
}

zipfs_t *zipfs_open(const char *zip_path) {
  if (!zip_path) {
    debugPrintf("zipfs_open: NULL zip_path\n");
    return NULL;
  }

  zipfs_t *fs = calloc(1, sizeof(zipfs_t));
  if (!fs) {
    return NULL;
  }

  fs->zip_path = strdup(zip_path);
  
  // Initialize miniz archive structure
  memset(&fs->archive, 0, sizeof(mz_zip_archive));
  
  // Open the ZIP file
  if (!mz_zip_reader_init_file(&fs->archive, zip_path, 0)) {
    debugPrintf("zipfs_open: Failed to open ZIP '%s': %s\n", zip_path,
                mz_zip_get_error_string(mz_zip_get_last_error(&fs->archive)));
    free(fs->zip_path);
    free(fs);
    return NULL;
  }

  // Get number of files in archive
  mz_uint num_files = mz_zip_reader_get_num_files(&fs->archive);
  
  // Allocate entries array
  fs->entries = calloc(num_files, sizeof(zipfs_entry_t));
  if (!fs->entries) {
    mz_zip_reader_end(&fs->archive);
    free(fs->zip_path);
    free(fs);
    return NULL;
  }

  // Build index of all files
  size_t valid_entries = 0;
  for (mz_uint i = 0; i < num_files; i++) {
    mz_zip_archive_file_stat file_stat;
    if (!mz_zip_reader_file_stat(&fs->archive, i, &file_stat))
      continue;

    // Skip directories (miniz includes them as separate entries)
    if (mz_zip_reader_is_file_a_directory(&fs->archive, i))
      continue;

    fs->entries[valid_entries].normalized_path = normalize_path(file_stat.m_filename);
    fs->entries[valid_entries].file_index = i;
    fs->entries[valid_entries].size = file_stat.m_uncomp_size;
    valid_entries++;
  }

  fs->num_entries = valid_entries;

  debugPrintf("zipfs_open: Opened ZIP '%s' with %zu files\n", zip_path,
              fs->num_entries);
  return fs;
}

void zipfs_close(zipfs_t *fs) {
  if (!fs)
    return;

  debugPrintf("zipfs_close: Closing ZIP '%s'\n", fs->zip_path);

  // Free entries
  if (fs->entries) {
    for (size_t i = 0; i < fs->num_entries; i++) {
      free(fs->entries[i].normalized_path);
    }
    free(fs->entries);
  }

  // Close miniz archive
  mz_zip_reader_end(&fs->archive);

  free(fs->zip_path);
  free(fs);
}

// Cookie I/O functions for fopencookie
static ssize_t zipfs_cookie_read(void *cookie, char *buf, size_t size) {
  zipfs_cookie_t *c = (zipfs_cookie_t *)cookie;
  
  size_t available = c->size - c->position;
  size_t to_read = size < available ? size : available;

  if (to_read > 0) {
    memcpy(buf, c->data + c->position, to_read);
    c->position += to_read;
  }

  return to_read;
}

static int zipfs_cookie_seek(void *cookie, off64_t *offset, int whence) {
  zipfs_cookie_t *c = (zipfs_cookie_t *)cookie;
  off64_t new_position;

  switch (whence) {
  case SEEK_SET:
    new_position = *offset;
    break;
  case SEEK_CUR:
    new_position = c->position + *offset;
    break;
  case SEEK_END:
    new_position = c->size + *offset;
    break;
  default:
    return -1;
  }

  // Clamp to valid range
  if (new_position < 0)
    new_position = 0;
  if (new_position > (off64_t)c->size)
    new_position = c->size;

  c->position = new_position;
  *offset = new_position;
  return 0;
}

static int zipfs_cookie_close(void *cookie) {
  zipfs_cookie_t *c = (zipfs_cookie_t *)cookie;
  
  if (c->data)
    mz_free(c->data); // Use mz_free for miniz-allocated memory
  free(c);
  
  return 0;
}

FILE *zipfs_fopen(zipfs_t *fs, const char *path, const char *mode) {
  if (!fs || !path || !mode) {
    debugPrintf("zipfs_fopen: Invalid arguments\n");
    return NULL;
  }

  // Only support read modes
  if (mode[0] != 'r') {
    debugPrintf("zipfs_fopen: Unsupported mode '%s' (only 'r' and 'rb' "
                "supported)\n",
                mode);
    return NULL;
  }

  // Find the entry
  zipfs_entry_t *entry = find_entry(fs, path);
  if (!entry) {
    // Not necessarily an error - file might not exist in this archive
    return NULL;
  }

  // Extract file to heap
  size_t uncomp_size;
  void *data = mz_zip_reader_extract_to_heap(&fs->archive, entry->file_index, 
                                             &uncomp_size, 0);
  if (!data) {
    debugPrintf("zipfs_fopen: Failed to extract file at index %u from ZIP: %s\n",
                entry->file_index,
                mz_zip_get_error_string(mz_zip_get_last_error(&fs->archive)));
    return NULL;
  }

  if (uncomp_size != entry->size) {
    debugPrintf("zipfs_fopen: Size mismatch (expected %llu, got %zu)\n",
                (unsigned long long)entry->size, uncomp_size);
    mz_free(data);
    return NULL;
  }

  // Create cookie
  zipfs_cookie_t *cookie = calloc(1, sizeof(zipfs_cookie_t));
  if (!cookie) {
    mz_free(data);
    return NULL;
  }

  cookie->fs = fs;
  cookie->data = data;
  cookie->size = uncomp_size;
  cookie->position = 0;

  // Create FILE* using fopencookie (Linux)
  cookie_io_functions_t io_funcs = {
    .read = zipfs_cookie_read,
    .write = NULL, // Read-only
    .seek = zipfs_cookie_seek,
    .close = zipfs_cookie_close,
  };

  FILE *fp = fopencookie(cookie, "rb", io_funcs);
  if (!fp) {
    mz_free(data);
    free(cookie);
    debugPrintf("zipfs_fopen: fopencookie failed\n");
    return NULL;
  }

  return fp;
}

int zipfs_fclose(FILE *file) {
  if (!file)
    return -1;

  // Just use standard fclose - it will call our cookie close function
  return fclose(file);
}

size_t zipfs_fread(void *ptr, size_t size, size_t nmemb, FILE *file) {
  // Just use standard fread - it will call our cookie read function
  return fread(ptr, size, nmemb, file);
}

int zipfs_fseek(FILE *file, long offset, int whence) {
  // Just use standard fseek - it will call our cookie seek function
  return fseek(file, offset, whence);
}

long zipfs_ftell(FILE *file) {
  // Just use standard ftell
  return ftell(file);
}

int zipfs_exists(zipfs_t *fs, const char *path) {
  if (!fs || !path)
    return 0;

  return find_entry(fs, path) != NULL ? 1 : 0;
}

long zipfs_get_size(zipfs_t *fs, const char *path) {
  if (!fs || !path)
    return -1;

  zipfs_entry_t *entry = find_entry(fs, path);
  if (!entry)
    return -1;

  return (long)entry->size;
}

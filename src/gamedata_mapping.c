/* gamedata_mapping.c
 *
 * Copyright (C) 2025 Jaakko Lukkari
 *
 * This software may be modified and distributed under the terms
 * of the MIT license.  See the LICENSE file for details.
 */

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "error.h"
#include "gamedata_mapping.h"

static struct hashmap_s gamedata_mapping;
static int mapping_initialized = 0;

static void add_path_to_mapping(const char *path) {
  if (!path) {
    return;
  }

  char *key = strdup(path);
  if (!key) {
    fatal_error("Failed to allocate memory for gamedata mapping key");
    return;
  }

  for (char *p = key; *p; ++p) {
    *p = tolower((unsigned char)*p);
  }

  char *value = strdup(path);
  if (!value) {
    free(key);
    fatal_error("Failed to allocate memory for gamedata mapping value");
    return;
  }

  hashmap_put(&gamedata_mapping, key, strlen(key), value);
}

static inline int should_skip_entry(const char *name) {
  return strcmp(name, ".") == 0 || strcmp(name, "..") == 0;
}

static void scan_directory(const char *dir_path) {
  DIR *dir = opendir(dir_path);
  if (!dir) {
    return;
  }

  struct dirent *entry;
  char full_path[PATH_MAX];

  while ((entry = readdir(dir)) != NULL) {
    if (should_skip_entry(entry->d_name)) {
      continue;
    }

    int ret = snprintf(full_path, sizeof(full_path), "%s/%s", dir_path,
                       entry->d_name);
    if (ret >= (int)sizeof(full_path)) {
      continue;
    }

    struct stat st;
    if (stat(full_path, &st) == 0) {
      if (S_ISDIR(st.st_mode)) {
        scan_directory(full_path);
      } else {
        add_path_to_mapping(full_path);
      }
    }
  }

  closedir(dir);
}

int gamedata_mapping_init(void) {
  if (mapping_initialized) {
    return 0;
  }

  if (hashmap_create(GAMEDATA_MAPPING_INITIAL_SIZE, &gamedata_mapping) != 0) {
    return -1;
  }

  DIR *dir = opendir("gamedata");
  if (!dir) {
    hashmap_destroy(&gamedata_mapping);
    return -1;
  }
  closedir(dir);

  scan_directory("gamedata");

  mapping_initialized = 1;
  return 0;
}

static int cleanup_iterator(void *context, struct hashmap_element_s *element) {
  (void)context;

  if (element->key) {
    free((char *)element->key);
  }
  if (element->data) {
    free((char *)element->data);
  }

  return 0;
}

void gamedata_mapping_cleanup(void) {
  if (!mapping_initialized) {
    return;
  }

  hashmap_iterate_pairs(&gamedata_mapping, cleanup_iterator, NULL);
  hashmap_destroy(&gamedata_mapping);
  mapping_initialized = 0;
}

const char *gamedata_mapping_get(const char *path) {
  if (!mapping_initialized || !path) {
    return NULL;
  }

  char *lowercase_path = strdup(path);
  if (!lowercase_path) {
    return NULL;
  }

  for (char *p = lowercase_path; *p; ++p) {
    *p = tolower((unsigned char)*p);
  }

  const char *result = (const char *)hashmap_get(
      &gamedata_mapping, lowercase_path, strlen(lowercase_path));

  free(lowercase_path);
  return result;
}

struct hashmap_s *gamedata_mapping_get_hashmap(void) {
  if (!mapping_initialized) {
    return NULL;
  }
  return &gamedata_mapping;
}
/* config.c -- simple configuration parser
 *
 * Copyright (C) 2021 fgsfds, Andy Nguyen (original code for Switch)
 * Copyright (C) 2025 Jaakko Lukkari
 *
 * This software may be modified and distributed under the terms
 * of the MIT license.  See the LICENSE file for details.
 */

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "config.h"

#define CONFIG_VARS                                                            \
  CONFIG_VAR_INT(use_bloom);                                                   \
  CONFIG_VAR_INT(trilinear_filter);                                            \
  CONFIG_VAR_INT(disable_mipmaps);                                             \
  CONFIG_VAR_INT(language);                                                    \
  CONFIG_VAR_INT(crouch_toggle);                                               \
  CONFIG_VAR_INT(character_shadows);                                           \
  CONFIG_VAR_INT(drop_highest_lod);                                            \
  CONFIG_VAR_INT(show_weapon_menu);                                            \
  CONFIG_VAR_INT(vsync_enabled);                                               \
  CONFIG_VAR_FLOAT(decal_limit);                                               \
  CONFIG_VAR_FLOAT(debris_limit);                                              \
  CONFIG_VAR_STR(mod_file);                                                    \
  CONFIG_VAR_INT(force_widescreen);                                            \
  CONFIG_VAR_FLOAT(stick_deadzone);                                            \
  CONFIG_VAR_FLOAT(aspect_ratio_x_mult);                                       \
  CONFIG_VAR_FLOAT(aspect_ratio_y_mult);

Config config;

// actual screen size that is in use right now
int screen_width = 0;
int screen_height = 0;

static inline void parse_var(const char *name, const char *value) {
#define CONFIG_VAR_INT(var)                                                    \
  if (!strcmp(name, #var)) {                                                   \
    config.var = atoi(value);                                                  \
    return;                                                                    \
  }
#define CONFIG_VAR_FLOAT(var)                                                  \
  if (!strcmp(name, #var)) {                                                   \
    config.var = atof(value);                                                  \
    return;                                                                    \
  }
#define CONFIG_VAR_STR(var)                                                    \
  if (!strcmp(name, #var)) {                                                   \
    strncpy(config.var, value, sizeof(config.var) - 1);                        \
    config.var[sizeof(config.var) - 1] = '\0';                                 \
    return;                                                                    \
  }
  CONFIG_VARS
#undef CONFIG_VAR_INT
#undef CONFIG_VAR_FLOAT
#undef CONFIG_VAR_STR
}

int read_config(const char *file) {
  char line[1024] = {0};

  memset(&config, 0, sizeof(Config));
  config.use_bloom = 0;
  config.trilinear_filter = 1;
  config.disable_mipmaps = 0;
  config.language = 0; // english
  config.crouch_toggle = 1;
  config.character_shadows = 1; // 1 - one blob; 2 - foot shadows
  config.drop_highest_lod = 0;  // does this even do anything?
  config.show_weapon_menu = 0;
  config.vsync_enabled = 1; // Enable VSync by default to prevent screen tearing
  config.decal_limit = 0.5f;
  config.debris_limit = 1.0f;
  config.force_widescreen = 0; // disabled by default
  config.stick_deadzone = 0.1f; // default deadzone for analog sticks
  config.aspect_ratio_x_mult = 1.18f;
  config.aspect_ratio_y_mult = 0.84f;

  FILE *f = fopen(file, "r");
  if (f == NULL)
    return -1;

  // parse lines of the forms
  // <spaces> # <whatever> \n
  // <spaces> NAME <spaces> VALUE <spaces> \n
  do {
    char *name = NULL, *value = NULL, *tmp = NULL;
    if (fgets(line, sizeof(line), f) != NULL) {
      name = line;
      // trim name
      while (*name && isspace((int)*name))
        ++name;
      if (name[0] == '#')
        continue; // skip comments
      for (tmp = name; *tmp && !isspace((int)*tmp); ++tmp)
        ;
      // if tmp points to the end of the string, there's no value to parse
      if (*tmp != 0) {
        *tmp = 0;
        // value is next; trim value
        for (value = tmp + 1; *value && isspace((int)*value); ++value)
          ;
        for (tmp = value + strlen(value) - 1; isspace((int)*tmp); --tmp)
          *tmp = 0;
        // got key value pair
        parse_var(name, value);
      }
    }
  } while (!feof(f));

  fclose(f);

  return 0;
}

int write_config(const char *file) {
  FILE *f = fopen(file, "w");
  if (f == NULL)
    return -1;

#define CONFIG_VAR_INT(var) fprintf(f, "%s %d\n", #var, config.var)
#define CONFIG_VAR_FLOAT(var) fprintf(f, "%s %g\n", #var, config.var)
#define CONFIG_VAR_STR(var)                                                    \
  if (config.var[0])                                                           \
  fprintf(f, "%s %s\n", #var, config.var)
  CONFIG_VARS
#undef CONFIG_VAR_INT
#undef CONFIG_VAR_FLOAT
#undef CONFIG_VAR_STR

  fclose(f);

  return 0;
}

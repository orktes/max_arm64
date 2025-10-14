/* config.h -- global configuration and config file handling
 *
 * Copyright (C) 2021 fgsfds, Andy Nguyen (original code for Switch)
 * Copyright (C) 2025 Jaakko Lukkari
 *
 * This software may be modified and distributed under the terms
 * of the MIT license.  See the LICENSE file for details.
 */

#ifndef __CONFIG_H__
#define __CONFIG_H__

// should be enough for pretend purposes
#define MEMORY_MB 256

#define SO_NAME "libMaxPayne.so"
#define CONFIG_NAME "conf/config.txt"
#define LOG_NAME "debug.log"

#define DEBUG_LOG 1

// actual screen size
extern int screen_width;
extern int screen_height;

typedef struct {
  int use_bloom;
  int trilinear_filter;
  int disable_mipmaps;
  int language;
  int crouch_toggle;
  int character_shadows;
  int drop_highest_lod;
  int show_weapon_menu;
  int vsync_enabled; // Enable VSync to prevent screen tearing (1=on, 0=off)
  float decal_limit;
  float debris_limit;
  char mod_file[0x100];
  int force_widescreen; // 0=disabled, 1=enabled
  float stick_deadzone; // deadzone for analog sticks (0.0 - 1.0)
} Config;

extern Config config;

int read_config(const char *file);
int write_config(const char *file);

#endif

/* gamedata_mapping.h
 *
 * Copyright (C) 2025 Jaakko Lukkari
 *
 * This software may be modified and distributed under the terms
 * of the MIT license.  See the LICENSE file for details.
 */

#ifndef GAMEDATA_MAPPING_H
#define GAMEDATA_MAPPING_H

#include <dirent.h>
#include <limits.h>
#include <sys/stat.h>

#include "hashmap.h"

#define GAMEDATA_MAPPING_INITIAL_SIZE 244

int gamedata_mapping_init(void);
void gamedata_mapping_cleanup(void);
const char *gamedata_mapping_get(const char *path);
struct hashmap_s *gamedata_mapping_get_hashmap(void);

#endif // GAMEDATA_MAPPING_H
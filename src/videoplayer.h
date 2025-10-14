/* videoplayer.h
 *
 * Copyright (C) 2025 Jaakko Lukkari
 *
 * This software may be modified and distributed under the terms
 * of the MIT license.  See the LICENSE file for details.
 */

#ifndef VIDEOPLAYER_H
#define VIDEOPLAYER_H

#include <stdint.h>

int videoplayer_init(void);
void videoplayer_cleanup(void);
int videoplayer_is_available(void);
int videoplayer_play(const char *filename);
void videoplayer_stop(void);
int videoplayer_is_playing(void);
void videoplayer_set_overlay(const char *text);

#endif // VIDEOPLAYER_H
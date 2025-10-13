#ifndef __HOOKS_H__
#define __HOOKS_H__

#include <stdint.h>

void patch_opengl(void);
void patch_openal(void);
void patch_game(void);
void exit_game(int code);
void patch_io(void);

void deinit_opengl(void);
void deinit_openal(void);

// Window size management (from game.c)
void update_game_window_size_to_screen(void);

#endif

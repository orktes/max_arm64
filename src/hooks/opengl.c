/* opengl.c -- OpenGL and shader generator hooks and patches
 *
 * Copyright (C) 2021 fgsfds, Andy Nguyen (original code for Switch)
 * Copyright (C) 2025 Jaakko Lukkari
 *
 * This software may be modified and distributed under the terms
 * of the MIT license.  See the LICENSE file for details.
 */

#include <GLES2/gl2.h>
#include <SDL2/SDL.h>
#include <SDL2/SDL_video.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../config.h"
#include "../so_util.h"
#include "../util.h"

// SDL OpenGL context
static SDL_Window *sdl_window = NULL;
static SDL_GLContext sdl_gl_context = NULL;
static int display_width = 0;
static int display_height = 0;

// Initialize SDL OpenGL ES context
static int init_sdl_opengl(void) {
  debugPrintf("=== Initializing SDL OpenGL ES Context ===\n");

  // Initialize SDL video subsystem if not already initialized
  if (SDL_WasInit(SDL_INIT_VIDEO) == 0) {
    if (SDL_InitSubSystem(SDL_INIT_VIDEO) < 0) {
      debugPrintf("✗ SDL video initialization failed: %s\n", SDL_GetError());
      return -1;
    }
    debugPrintf("✓ SDL video initialized\n");
  }

  // Set OpenGL ES attributes
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
  SDL_GL_SetAttribute(SDL_GL_RED_SIZE, 8);
  SDL_GL_SetAttribute(SDL_GL_GREEN_SIZE, 8);
  SDL_GL_SetAttribute(SDL_GL_BLUE_SIZE, 8);
  SDL_GL_SetAttribute(SDL_GL_ALPHA_SIZE, 0);
  SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
  SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);
  SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);

  debugPrintf("✓ Set OpenGL ES attributes\n");

  // Get display mode for default window size
  SDL_DisplayMode mode;
  if (SDL_GetCurrentDisplayMode(0, &mode) == 0) {
    display_width = mode.w;
    display_height = mode.h;
    debugPrintf("✓ Display resolution: %dx%d\n", display_width, display_height);
  } else {
    // Default resolution if we can't get display info
    display_width = 1280;
    display_height = 720;
    debugPrintf("⚠ Using default resolution: %dx%d\n", display_width,
                display_height);
  }

  // Create SDL window with OpenGL context
  sdl_window = SDL_CreateWindow(
      "Max Payne ARM64", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
      display_width, display_height, SDL_WINDOW_OPENGL | SDL_WINDOW_FULLSCREEN);

  if (!sdl_window) {
    debugPrintf("✗ SDL window creation failed: %s\n", SDL_GetError());
    return -1;
  }
  debugPrintf("✓ SDL window created (%dx%d)\n", display_width, display_height);

  // Create OpenGL context
  sdl_gl_context = SDL_GL_CreateContext(sdl_window);
  if (!sdl_gl_context) {
    debugPrintf("✗ SDL OpenGL context creation failed: %s\n", SDL_GetError());
    SDL_DestroyWindow(sdl_window);
    sdl_window = NULL;
    return -1;
  }
  debugPrintf("✓ SDL OpenGL ES context created\n");

  // Make context current
  if (SDL_GL_MakeCurrent(sdl_window, sdl_gl_context) < 0) {
    debugPrintf("✗ SDL make context current failed: %s\n", SDL_GetError());
    SDL_GL_DeleteContext(sdl_gl_context);
    SDL_DestroyWindow(sdl_window);
    sdl_gl_context = NULL;
    sdl_window = NULL;
    return -1;
  }
  debugPrintf("✓ SDL OpenGL ES context made current\n");

  // Configure VSync
  int vsync_interval = config.vsync_enabled ? 1 : 0;
  if (SDL_GL_SetSwapInterval(vsync_interval) < 0) {
    debugPrintf("⚠ Warning: Could not %s VSync: %s\n",
                config.vsync_enabled ? "enable" : "disable", SDL_GetError());
  } else {
    debugPrintf("✓ VSync %s\n", config.vsync_enabled ? "enabled" : "disabled");
  }

  // Update global screen size variables
  extern int screen_width, screen_height;
  screen_width = display_width;
  screen_height = display_height;
  debugPrintf("✓ Set game screen size to %dx%d\n", screen_width, screen_height);

  // Test rendering
  glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
  glClear(GL_COLOR_BUFFER_BIT);
  glViewport(0, 0, screen_width, screen_height);

  SDL_GL_SwapWindow(sdl_window);
  debugPrintf("✓ Test render completed\n");

  debugPrintf("=== SDL OpenGL ES initialization complete ===\n");
  return 0;
}

void NVEventEGLMakeCurrent(void) {
  debugPrintf("NVEventEGLMakeCurrent called\n");
  if (sdl_window && sdl_gl_context) {
    if (SDL_GL_MakeCurrent(sdl_window, sdl_gl_context) < 0) {
      debugPrintf("NVEventEGLMakeCurrent: SDL make current failed: %s\n",
                  SDL_GetError());
    }
  } else {
    debugPrintf("NVEventEGLMakeCurrent: SDL context not available\n");
  }
}

void NVEventEGLUnmakeCurrent(void) {
  debugPrintf("NVEventEGLUnmakeCurrent called\n");
  if (sdl_window) {
    if (SDL_GL_MakeCurrent(sdl_window, NULL) < 0) {
      debugPrintf("NVEventEGLUnmakeCurrent: SDL unmake current failed: %s\n",
                  SDL_GetError());
    }
  } else {
    debugPrintf("NVEventEGLUnmakeCurrent: SDL window not available\n");
  }
}

int NVEventEGLInit(void) {
  debugPrintf("NVEventEGLInit called\n");

  // Initialize SDL OpenGL ES context
  if (init_sdl_opengl() == 0) {
    debugPrintf("✓ SDL OpenGL ES initialization successful\n");
    return 1;
  } else {
    debugPrintf("✗ SDL OpenGL ES initialization failed\n");
    return 0;
  }
}

void NVEventEGLSwapBuffers(void) {
  static int swap_debug_logged = 0;
  if (swap_debug_logged == 0) {
    debugPrintf("NVEventEGLSwapBuffers called for the first time\n");
    swap_debug_logged = 1;
  }

  if (sdl_window) {

    // hack to fix 1:1 screens rendering in 4:3
    if (screen_height == screen_width && !config.force_widescreen) {
      int wanted_height = screen_height*(3.0f/4.0f);
      int y_offset = (screen_height - wanted_height) / 2;
      
      // render black bars on top and bottom of the screen

      glEnable(GL_SCISSOR_TEST);
      glScissor(0, 0, screen_width, y_offset);
      glClearColor(0.0f, 0.0f, 0.0, 1.0f);
      glClear(GL_COLOR_BUFFER_BIT);
      glScissor(0, screen_height - y_offset, screen_width, y_offset);
      glClear(GL_COLOR_BUFFER_BIT);
      glDisable(GL_SCISSOR_TEST);
    }
   

    SDL_GL_SwapWindow(sdl_window);
  } else {
    debugPrintf("NVEventEGLSwapBuffers: SDL window not available\n");
  }
}

void patch_opengl(void) {
  debugPrintf("patch_opengl: Starting OpenGL patching\n");

  debugPrintf("patch_opengl: Hooking EGL functions\n");
  hook_arm64(so_find_addr("_Z14NVEventEGLInitv"), (uintptr_t)NVEventEGLInit);
  hook_arm64(so_find_addr("_Z21NVEventEGLMakeCurrentv"),
             (uintptr_t)NVEventEGLMakeCurrent);
  hook_arm64(so_find_addr("_Z23NVEventEGLUnmakeCurrentv"),
             (uintptr_t)NVEventEGLUnmakeCurrent);
  hook_arm64(so_find_addr("_Z21NVEventEGLSwapBuffersv"),
             (uintptr_t)NVEventEGLSwapBuffers);

  debugPrintf("patch_opengl: OpenGL patching completed\n");
}

void deinit_opengl(void) {
  debugPrintf("=== Starting SDL OpenGL cleanup ===\n");

  if (sdl_gl_context) {
    debugPrintf("Deleting SDL OpenGL context...\n");
    SDL_GL_DeleteContext(sdl_gl_context);
    sdl_gl_context = NULL;
    debugPrintf("✓ SDL OpenGL context deleted\n");
  }

  if (sdl_window) {
    debugPrintf("Destroying SDL window...\n");
    SDL_DestroyWindow(sdl_window);
    sdl_window = NULL;
    debugPrintf("✓ SDL window destroyed\n");
  }

  debugPrintf("=== SDL OpenGL cleanup completed ===\n");
}
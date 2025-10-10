/* opengl.c -- OpenGL and shader generator hooks and patches
 *
 * Copyright (C) 2021 fgsfds, Andy Nguyen (original code for Switch)
 * Copyright (C) 2025 Jaakko Lukkari
 *
 * This software may be modified and distributed under the terms
 * of the MIT license.  See the LICENSE file for details.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <GLES2/gl2.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <linux/fb.h>
#include <linux/kd.h>
#include <sys/mman.h>
#include <errno.h>
#include <sys/stat.h>

#ifdef __X11_DESKTOP__
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#endif

// Only include DRM/GBM for R36S device builds
#ifndef __X11_DESKTOP__
#include <gbm.h>
#include <xf86drm.h>
#include <xf86drmMode.h>
#endif

#include "../config.h"
#include "../util.h"
#include "../so_util.h"

static EGLDisplay display = NULL;
static EGLSurface surface = NULL;
static EGLContext context = NULL;

#ifdef __X11_DESKTOP__
static Display *x11_display = NULL;
static Window x11_window = 0;
#endif

#ifndef __X11_DESKTOP__
static int fb_fd = -1; // Framebuffer file descriptor (fallback)
static struct fb_var_screeninfo fb_var;
static struct fb_fix_screeninfo fb_fix;
static void *fb_mem = NULL; // Framebuffer memory mapping (fallback)
static size_t fb_size = 0;

// GBM/DRM variables for modern graphics
static int drm_fd = -1;
static struct gbm_device *gbm_device = NULL;
static struct gbm_surface *gbm_surface = NULL;
static struct gbm_bo *previous_bo = NULL;
static uint32_t previous_fb_id = 0;
static drmModeRes *drm_resources = NULL;
static drmModeCrtc *crtc = NULL;
static drmModeConnector *connector = NULL;

// R36S device detection and display configuration
static int display_width = 0;
static int display_height = 0;
#endif

#ifndef __X11_DESKTOP__
// Initialize GBM+DRM for modern graphics
static int init_drm_gbm(void)
{
  debugPrintf("=== Initializing GBM+DRM Graphics ===\n");

  // debugPrintf("=== DISPLAY PROCESS DIAGNOSTICS ===\n");
  // system("ps aux | grep -E '(X|wayland|weston|kms|drm|fb)' | grep -v grep | head -10");
  // system("lsof /dev/dri/card0 2>/dev/null | head -5");
  // system("lsof /dev/fb0 2>/dev/null | head -5");
  // system("cat /sys/class/graphics/fb0/state 2>/dev/null || echo 'No fb0 state'");
  // debugPrintf("=== END DIAGNOSTICS ===\n");

  // First, try to disable console to take control of display
  // debugPrintf("Attempting to take control from console...\n");
  int console_fd = open("/dev/tty0", O_RDWR);
  if (console_fd >= 0)
  {
    // Try to switch to graphics mode
    if (ioctl(console_fd, KDSETMODE, KD_GRAPHICS) == 0)
    {
      // debugPrintf("✓ Switched console to graphics mode\n");
    }
    else
    {
      // debugPrintf("⚠ Could not switch console to graphics mode: %s\n", strerror(errno));
      // debugPrintf("  (This might be why page flip fails)\n");
    }
    close(console_fd);
  }
  else
  {
    // debugPrintf("⚠ Could not open /dev/tty0: %s\n", strerror(errno));
  }

  // Open DRM device
  drm_fd = open("/dev/dri/card0", O_RDWR | O_CLOEXEC);
  if (drm_fd < 0)
  {
    debugPrintf("✗ Cannot open /dev/dri/card0: %s\n", strerror(errno));
    return -1;
  }
  debugPrintf("✓ Opened DRM device\n");

  // Become DRM master to have exclusive control
  if (drmSetMaster(drm_fd) != 0)
  {
    debugPrintf("⚠ Could not become DRM master: %s\n", strerror(errno));
    debugPrintf("  Another process might be controlling the display\n");
    debugPrintf("  Try: sudo pkill X; sudo pkill Xorg; sudo pkill weston\n");
  }
  else
  {
    debugPrintf("✓ Became DRM master\n");
  }

  // Get DRM resources
  drm_resources = drmModeGetResources(drm_fd);
  if (!drm_resources)
  {
    debugPrintf("✗ Cannot get DRM resources\n");
    return -1;
  }
  debugPrintf("✓ Got DRM resources: %d connectors, %d crtcs\n",
              drm_resources->count_connectors, drm_resources->count_crtcs);

  // Find connected display
  for (int i = 0; i < drm_resources->count_connectors; i++)
  {
    connector = drmModeGetConnector(drm_fd, drm_resources->connectors[i]);
    if (connector && connector->connection == DRM_MODE_CONNECTED && connector->count_modes > 0)
    {
      // Detect device type based on native resolution
      int native_width = connector->modes[0].hdisplay;
      int native_height = connector->modes[0].vdisplay;

      debugPrintf("✓ Found connected display: %s (%dx%d)\n",
                  connector->modes[0].name, native_width, native_height);

      display_width = native_width;
      display_height = native_height;

      break;
    }
    if (connector)
    {
      drmModeFreeConnector(connector);
      connector = NULL;
    }
  }

  if (!connector)
  {
    debugPrintf("✗ No connected display found\n");
    return -1;
  }

  // Get CRTC
  if (drm_resources->count_crtcs > 0)
  {
    crtc = drmModeGetCrtc(drm_fd, drm_resources->crtcs[0]);
    if (crtc)
    {
      // debugPrintf("✓ Got CRTC ID %d: %dx%d, mode_valid=%d\n",
      //        crtc->crtc_id, crtc->mode.hdisplay, crtc->mode.vdisplay, crtc->mode_valid);
      // debugPrintf("  Current buffer_id: %d, x=%d, y=%d\n", crtc->buffer_id, crtc->x, crtc->y);
      // if (crtc->mode_valid) {
      // debugPrintf("  Mode: %s %dx%d @%dHz\n", crtc->mode.name,
      //          crtc->mode.hdisplay, crtc->mode.vdisplay, crtc->mode.vrefresh);
      // }
    }
  }

  if (!crtc)
  {
    debugPrintf("✗ No CRTC available\n");
    return -1;
  }

  // Check if CRTC is already being used
  // if (crtc->buffer_id != 0) {
  // debugPrintf("⚠ CRTC %d already has buffer %d attached - another process is using it\n",
  //          crtc->crtc_id, crtc->buffer_id);
  // }

  // Set display mode if not already set properly
  if (crtc->mode_valid == 0 || crtc->mode.hdisplay == 0)
  {
    // Find the best display mode - prefer higher refresh rates to reduce tearing
    drmModeModeInfo *best_mode = &connector->modes[0];
    uint32_t best_refresh = connector->modes[0].vrefresh;

    for (int i = 1; i < connector->count_modes; i++)
    {
      drmModeModeInfo *mode = &connector->modes[i];
      // Prefer same resolution with higher refresh rate
      if (mode->hdisplay == best_mode->hdisplay &&
          mode->vdisplay == best_mode->vdisplay &&
          mode->vrefresh > best_refresh)
      {
        best_mode = mode;
        best_refresh = mode->vrefresh;
      }
    }

    debugPrintf("Setting display mode: %s %dx%d @%dHz (optimized for tear reduction)\n",
                best_mode->name, best_mode->hdisplay, best_mode->vdisplay, best_mode->vrefresh);

    // Set CRTC with the connector and mode
    if (drmModeSetCrtc(drm_fd, crtc->crtc_id, 0, 0, 0,
                       &connector->connector_id, 1, best_mode) != 0)
    {
      debugPrintf("✗ Failed to set CRTC mode: %s\n", strerror(errno));
      debugPrintf("  This might mean another process has control\n");
      // Don't return -1, continue anyway and try page flip without mode set
    }
    else
    {
      debugPrintf("✓ Set CRTC mode to %dx%d @%dHz\n", best_mode->hdisplay, best_mode->vdisplay, best_mode->vrefresh);
    }
  }
  else
  {
    debugPrintf("✓ CRTC already has valid mode set\n");
  }

  // Create GBM device
  gbm_device = gbm_create_device(drm_fd);
  if (!gbm_device)
  {
    debugPrintf("✗ Cannot create GBM device\n");
    return -1;
  }
  debugPrintf("✓ Created GBM device\n");

  // Create GBM surface for the detected display
  gbm_surface = gbm_surface_create(gbm_device, display_width, display_height, GBM_FORMAT_XRGB8888,
                                   GBM_BO_USE_SCANOUT | GBM_BO_USE_RENDERING);
  if (!gbm_surface)
  {
    debugPrintf("✗ Cannot create GBM surface\n");
    return -1;
  }

  // Update global screen size variables for the game
  extern int screen_width, screen_height;
  screen_width = display_width;
  screen_height = display_height;
  debugPrintf("✓ Set game screen size to %dx%d\n", screen_width, screen_height);

  return 0;
}
#endif // __X11_DESKTOP__

void NVEventEGLMakeCurrent(void)
{
  debugPrintf("NVEventEGLMakeCurrent called\n");
  if (display != (EGLDisplay)0x1 && context != (EGLContext)0x1 && surface != (EGLSurface)0x1)
  {
    eglMakeCurrent(display, surface, surface, context);
  }
  else
  {
    debugPrintf("NVEventEGLMakeCurrent: Using stub mode\n");
  }
}

void NVEventEGLUnmakeCurrent(void)
{
  debugPrintf("NVEventEGLUnmakeCurrent called\n");
  if (display != (EGLDisplay)0x1)
  {
    eglMakeCurrent(display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
  }
  else
  {
    debugPrintf("NVEventEGLUnmakeCurrent: Using stub mode\n");
  }
}

int NVEventEGLInit(void)
{
#ifdef __X11_DESKTOP__
  // --- X11 window ---
  x11_display = XOpenDisplay(NULL);
  if (!x11_display)
  {
    debugPrintf("✗ XOpenDisplay failed\n");
    return 0;
  }
  debugPrintf("✓ X11 display opened\n");

  int screen = DefaultScreen(x11_display);
  int win_w = 1280, win_h = 720;

  XSetWindowAttributes swa;
  swa.event_mask = ExposureMask | StructureNotifyMask | KeyPressMask;
  x11_window = XCreateWindow(
      x11_display, RootWindow(x11_display, screen),
      0, 0, win_w, win_h, 0,
      CopyFromParent, InputOutput, CopyFromParent,
      CWEventMask, &swa);
  XStoreName(x11_display, x11_window, "Max Payne - Desktop Debug");
  XMapWindow(x11_display, x11_window);
  debugPrintf("✓ X11 window created (1280x720)\n");

  // --- EGL init ---
  display = eglGetDisplay((EGLNativeDisplayType)x11_display);
  if (display == EGL_NO_DISPLAY)
  {
    debugPrintf("✗ eglGetDisplay failed\n");
    XCloseDisplay(x11_display);
    return 0;
  }
  debugPrintf("✓ EGL display obtained\n");

  EGLint major, minor;
  if (!eglInitialize(display, &major, &minor))
  {
    debugPrintf("✗ EGL initialization failed: 0x%x\n", eglGetError());
    XCloseDisplay(x11_display);
    return 0;
  }
  debugPrintf("✓ EGL initialized: %d.%d\n", major, minor);

  if (!eglBindAPI(EGL_OPENGL_ES_API))
  {
    debugPrintf("✗ Cannot bind OpenGL ES API: 0x%x\n", eglGetError());
  }

  // Configure EGL
  EGLint num_configs = 0;
  EGLConfig egl_config;
  const EGLint config_attribs[] = {
      EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
      EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
      EGL_RED_SIZE, 8,
      EGL_GREEN_SIZE, 8,
      EGL_BLUE_SIZE, 8,
      EGL_ALPHA_SIZE, 0,
      EGL_DEPTH_SIZE, 24,
      EGL_STENCIL_SIZE, 8,
      EGL_NONE};

  if (!eglChooseConfig(display, config_attribs, &egl_config, 1, &num_configs) || num_configs == 0)
  {
    debugPrintf("✗ No suitable EGL config found\n");
    eglTerminate(display);
    XCloseDisplay(x11_display);
    return 0;
  }
  debugPrintf("✓ Found EGL config\n");

  // Create EGL surface
  surface = eglCreateWindowSurface(display, egl_config, (EGLNativeWindowType)x11_window, NULL);
  if (surface == EGL_NO_SURFACE)
  {
    debugPrintf("✗ Cannot create EGL window surface: 0x%x\n", eglGetError());
    eglTerminate(display);
    XCloseDisplay(x11_display);
    return 0;
  }
  debugPrintf("✓ Created EGL window surface\n");

  // Create EGL context
  const EGLint context_attribs[] = {
      EGL_CONTEXT_CLIENT_VERSION, 2,
      EGL_NONE};

  context = eglCreateContext(display, egl_config, EGL_NO_CONTEXT, context_attribs);
  if (context == EGL_NO_CONTEXT)
  {
    debugPrintf("✗ Cannot create EGL context: 0x%x\n", eglGetError());
    eglDestroySurface(display, surface);
    eglTerminate(display);
    XCloseDisplay(x11_display);
    return 0;
  }
  debugPrintf("✓ Created EGL context\n");

  // Make context current
  if (!eglMakeCurrent(display, surface, surface, context))
  {
    debugPrintf("✗ Cannot make context current: 0x%x\n", eglGetError());
    eglDestroyContext(display, context);
    eglDestroySurface(display, surface);
    eglTerminate(display);
    XCloseDisplay(x11_display);
    return 0;
  }
  debugPrintf("✓ EGL context made current\n");

  // Enable VSync for smooth rendering if configured
  int vsync_interval = config.vsync_enabled ? 1 : 0;
  if (!eglSwapInterval(display, vsync_interval))
  {
    debugPrintf("⚠ Warning: Could not %s VSync: 0x%x\n",
                config.vsync_enabled ? "enable" : "disable", eglGetError());
  }
  else
  {
    debugPrintf("✓ VSync %s\n", config.vsync_enabled ? "enabled" : "disabled");
  }

  debugPrintf("✓ X11 EGL initialization complete\n");

#else
  // R36S device initialization with EGL/GBM
  // Try GBM+DRM initialization first
  if (init_drm_gbm() == 0)
  {
    debugPrintf("✓ GBM+DRM initialization successful, setting up EGL...\n");

    // Get EGL display using GBM platform
    PFNEGLGETPLATFORMDISPLAYEXTPROC get_platform_display = NULL;
    get_platform_display = (void *)eglGetProcAddress("eglGetPlatformDisplayEXT");
    if (get_platform_display)
    {
      display = get_platform_display(EGL_PLATFORM_GBM_KHR, gbm_device, NULL);
      debugPrintf("✓ Got GBM platform EGL display\n");
    }
    else
    {
      debugPrintf("✗ eglGetPlatformDisplayEXT not available, trying default\n");
      display = eglGetDisplay((EGLNativeDisplayType)gbm_device);
    }

    if (display == EGL_NO_DISPLAY)
    {
      debugPrintf("✗ Failed to get EGL display from GBM device\n");
      display = NULL;
    }
  }
  else
  {
    debugPrintf("✗ GBM+DRM initialization failed, falling back to legacy methods\n");
  }

  // Fallback to default display if GBM failed
  if (!display)
  {
    // debugPrintf("Attempting fallback to default EGL display...\n");
    display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    if (!display || display == EGL_NO_DISPLAY)
    {
      debugPrintf("✗ Default EGL display also failed: 0x%x\n", eglGetError());
      debugPrintf("Using stub mode - game will run with audio and input only\n");
      display = (EGLDisplay)0x1;
      surface = (EGLSurface)0x1;
      context = (EGLContext)0x1;
      return 1;
    }
  }

  EGLint major, minor;
  if (!eglInitialize(display, &major, &minor))
  {
    debugPrintf("✗ EGL initialization failed: 0x%x\n", eglGetError());
    display = (EGLDisplay)0x1;
    surface = (EGLSurface)0x1;
    context = (EGLContext)0x1;
    return 1;
  }
  debugPrintf("✓ EGL initialized: %d.%d\n", major, minor);

  if (!eglBindAPI(EGL_OPENGL_ES_API))
  {
    debugPrintf("✗ Cannot bind OpenGL ES API: 0x%x\n", eglGetError());
  }

  // Configure EGL
  EGLint num_configs = 0;
  EGLConfig egl_config;
  const EGLint config_attribs[] = {
      EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
      EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
      EGL_RED_SIZE, 8,
      EGL_GREEN_SIZE, 8,
      EGL_BLUE_SIZE, 8,
      EGL_ALPHA_SIZE, 0,
      EGL_DEPTH_SIZE, 24,
      EGL_STENCIL_SIZE, 8,
      EGL_NONE};

  if (!eglChooseConfig(display, config_attribs, &egl_config, 1, &num_configs) || num_configs == 0)
  {
    debugPrintf("✗ No suitable EGL config found\n");
    surface = (EGLSurface)0x1;
    context = (EGLContext)0x1;
    return 1;
  }
  debugPrintf("✓ Found EGL config\n");

  // Create EGL surface
#ifndef __X11_DESKTOP__
  if (gbm_surface)
  {
    surface = eglCreateWindowSurface(display, egl_config, (EGLNativeWindowType)gbm_surface, NULL);
    if (surface == EGL_NO_SURFACE)
    {
      debugPrintf("✗ Cannot create GBM window surface: 0x%x\n", eglGetError());
      surface = (EGLSurface)0x1;
    }
    else
    {
      debugPrintf("✓ Created GBM window surface\n");
    }
  }
  else
  {
    debugPrintf("No GBM surface available, creating pbuffer...\n");
    const EGLint pbuffer_attribs[] = {
        EGL_WIDTH, 720,
        EGL_HEIGHT, 720,
        EGL_NONE};
    surface = eglCreatePbufferSurface(display, egl_config, pbuffer_attribs);
    if (surface == EGL_NO_SURFACE)
    {
      debugPrintf("✗ Cannot create pbuffer surface: 0x%x\n", eglGetError());
      surface = (EGLSurface)0x1;
    }
    else
    {
      debugPrintf("✓ Created pbuffer surface (720x720)\n");
    }
  }
#else
  // Create a pbuffer surface for desktop
  const EGLint pbuffer_attribs[] = {
      EGL_WIDTH, 1280,
      EGL_HEIGHT, 720,
      EGL_NONE};
  surface = eglCreatePbufferSurface(display, egl_config, pbuffer_attribs);
  if (surface == EGL_NO_SURFACE)
  {
    debugPrintf("✗ Cannot create pbuffer surface: 0x%x\n", eglGetError());
    surface = (EGLSurface)0x1;
  }
  else
  {
    debugPrintf("✓ Created pbuffer surface for desktop\n");
  }
#endif

  // Create EGL context
  const EGLint context_attribs[] = {
      EGL_CONTEXT_CLIENT_VERSION, 2,
      EGL_NONE};

  context = eglCreateContext(display, egl_config, EGL_NO_CONTEXT, context_attribs);
  if (context == EGL_NO_CONTEXT)
  {
    debugPrintf("✗ Cannot create EGL context: 0x%x\n", eglGetError());
    context = (EGLContext)0x1;
  }
  else
  {
    // debugPrintf("✓ Created EGL context\n");
  }

  // Make context current
  if (context != (EGLContext)0x1 && surface != (EGLSurface)0x1)
  {
    if (!eglMakeCurrent(display, surface, surface, context))
    {
      debugPrintf("✗ Cannot make context current: 0x%x\n", eglGetError());
    }
    else
    {
      // debugPrintf("✓ EGL context made current\n");

      // Enable VSync for smooth rendering and prevent screen tearing if configured
      int vsync_interval = config.vsync_enabled ? 1 : 0;
      if (!eglSwapInterval(display, vsync_interval))
      {
        debugPrintf("⚠ Warning: Could not %s VSync: 0x%x\n",
                    config.vsync_enabled ? "enable" : "disable", eglGetError());
      }
      else
      {
        debugPrintf("✓ VSync %s to prevent screen tearing\n",
                    config.vsync_enabled ? "enabled" : "disabled");
      }

      // Test rendering
      glClearColor(1.0f, 0.0f, 0.0f, 1.0f); // Red screen
      glClear(GL_COLOR_BUFFER_BIT);

      // Set viewport to match detected screen size
      extern int screen_width, screen_height;
      glViewport(0, 0, screen_width, screen_height);

      eglSwapBuffers(display, surface);
      // debugPrintf("✓ Test render completed\n");
    }
  }

  // debugPrintf("=== EGL initialization complete ===\n");
  
  // Now that graphics are initialized and screen dimensions are set,
  // update the game's internal windowSize to match our actual screen
  //update_game_window_size_to_screen();
  
  return 1;
#endif
}

void NVEventEGLSwapBuffers(void)
{
  // R36S device swap buffers
  if (display != (EGLDisplay)0x1 && surface != (EGLSurface)0x1)
  {
    // Always do EGL swap buffers first
    eglSwapBuffers(display, surface);

#ifndef __X11_DESKTOP__
    // Handle GBM surface and proper double buffering
    if (gbm_surface && drm_fd >= 0 && crtc && connector)
    {
      struct gbm_bo *bo = gbm_surface_lock_front_buffer(gbm_surface);
      if (bo)
      {
        uint32_t handle = gbm_bo_get_handle(bo).u32;
        uint32_t pitch = gbm_bo_get_stride(bo);
        uint32_t fb_id_local;

        // Create framebuffer for this buffer object
        int add_fb_result = drmModeAddFB(drm_fd, display_width, display_height, 32, 32, pitch, handle, &fb_id_local);
        if (add_fb_result == 0)
        {
          // For the first frame, use drmModeSetCrtc to establish the mode
          static int first_frame = 1;
          if (first_frame)
          {
            int set_result = drmModeSetCrtc(drm_fd, crtc->crtc_id, fb_id_local, 0, 0,
                                            &connector->connector_id, 1, &connector->modes[0]);
            if (set_result == 0)
            {
              debugPrintf("✓ First frame displayed successfully!\n");
              first_frame = 0;
            }
            else
            {
              debugPrintf("✗ Failed to set initial CRTC: %s\n", strerror(errno));
            }
          }
          else
          {
            // For subsequent frames, use page flip with VBlank synchronization for smooth, tear-free transitions
            int flip_result = drmModePageFlip(drm_fd, crtc->crtc_id, fb_id_local, DRM_MODE_PAGE_FLIP_EVENT, NULL);
            if (flip_result != 0)
            {
              // Track page flip failures to detect if we need to use different approach
              static int page_flip_failures = 0;
              page_flip_failures++;

              if (page_flip_failures < 10)
              {
                // Try page flip without event flag as fallback
                flip_result = drmModePageFlip(drm_fd, crtc->crtc_id, fb_id_local, 0, NULL);
              }

              if (flip_result != 0)
              {
                // Final fallback to CRTC set if page flip consistently fails
                // This may cause tearing but ensures display still works
                drmModeSetCrtc(drm_fd, crtc->crtc_id, fb_id_local, 0, 0,
                               &connector->connector_id, 1, &connector->modes[0]);
                if (page_flip_failures == 10)
                {
                  debugPrintf("⚠ Warning: Page flip consistently failing, using immediate mode (may cause tearing)\n");
                }
              }
            }
          }

          // Clean up previous framebuffer
          if (previous_fb_id != 0)
          {
            drmModeRmFB(drm_fd, previous_fb_id);
          }
          if (previous_bo)
          {
            gbm_surface_release_buffer(gbm_surface, previous_bo);
          }

          // Store current buffer for next frame
          previous_fb_id = fb_id_local;
          previous_bo = bo;

          // static int frame_count = 0;
          // frame_count++;
          // if (frame_count % 300 == 0) {
          // debugPrintf("Graphics: Frame %d displayed\n", frame_count);
          // }
        }
        else
        {
          // debugPrintf("✗ Failed to add framebuffer: %s\n", strerror(errno));
          gbm_surface_release_buffer(gbm_surface, bo);
        }
      }
    }
#endif // __X11_DESKTOP__
  }
}

void patch_opengl(void)
{
  debugPrintf("patch_opengl: Starting OpenGL patching\n");

  // patch egl stuff
  debugPrintf("patch_opengl: Hooking EGL functions\n");
  hook_arm64(so_find_addr("_Z14NVEventEGLInitv"), (uintptr_t)NVEventEGLInit);
  hook_arm64(so_find_addr("_Z21NVEventEGLMakeCurrentv"), (uintptr_t)NVEventEGLMakeCurrent);
  hook_arm64(so_find_addr("_Z23NVEventEGLUnmakeCurrentv"), (uintptr_t)NVEventEGLUnmakeCurrent);
  hook_arm64(so_find_addr("_Z21NVEventEGLSwapBuffersv"), (uintptr_t)NVEventEGLSwapBuffers);


  debugPrintf("patch_opengl: OpenGL patching completed\n");
}

void deinit_opengl(void)
{
#ifdef __X11_DESKTOP__
  // Clean up X11/EGL
  if (display && display != (EGLDisplay)0x1)
  {
    eglMakeCurrent(display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
    if (context && context != (EGLContext)0x1)
      eglDestroyContext(display, context);
    if (surface && surface != (EGLSurface)0x1)
      eglDestroySurface(display, surface);
    eglTerminate(display);
  }
  if (x11_window)
  {
    XDestroyWindow(x11_display, x11_window);
  }
  if (x11_display)
  {
    XCloseDisplay(x11_display);
  }
  debugPrintf("✓ X11/EGL cleaned up\n");
#else
#ifndef __X11_DESKTOP__
  // Restore console mode before cleanup
  int console_fd = open("/dev/tty0", O_RDWR);
  if (console_fd >= 0)
  {
    ioctl(console_fd, KDSETMODE, KD_TEXT);
    // debugPrintf("Restored console to text mode\n");
    close(console_fd);
  }

  // Clean up framebuffer
  if (previous_fb_id != 0 && drm_fd >= 0)
  {
    drmModeRmFB(drm_fd, previous_fb_id);
    previous_fb_id = 0;
  }

  // Clean up previous buffer
  if (previous_bo && gbm_surface)
  {
    gbm_surface_release_buffer(gbm_surface, previous_bo);
    previous_bo = NULL;
  }

  // Drop DRM master
  if (drm_fd >= 0)
  {
    drmDropMaster(drm_fd);
  }
#endif

  if (display && display != (EGLDisplay)0x1)
  {
    eglMakeCurrent(display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
    if (context && context != (EGLContext)0x1)
      eglDestroyContext(display, context);
    if (surface && surface != (EGLSurface)0x1)
      eglDestroySurface(display, surface);
    eglTerminate(display);
  }

#ifndef __X11_DESKTOP__
  // Clean up GBM/DRM resources
  if (gbm_surface)
  {
    gbm_surface_destroy(gbm_surface);
    gbm_surface = NULL;
  }

  if (gbm_device)
  {
    gbm_device_destroy(gbm_device);
    gbm_device = NULL;
  }

  if (connector)
  {
    drmModeFreeConnector(connector);
    connector = NULL;
  }

  if (crtc)
  {
    drmModeFreeCrtc(crtc);
    crtc = NULL;
  }

  if (drm_resources)
  {
    drmModeFreeResources(drm_resources);
    drm_resources = NULL;
  }

  if (drm_fd >= 0)
  {
    close(drm_fd);
    drm_fd = -1;
  }

  if (fb_mem && fb_mem != MAP_FAILED)
  {
    munmap(fb_mem, fb_size);
    fb_mem = NULL;
  }

  if (fb_fd >= 0)
  {
    close(fb_fd);
    fb_fd = -1;
  }
#endif
#endif
}

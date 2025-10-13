/* opengl.c -- OpenGL and shader generator hooks and patches
 *
 * Copyright (C) 2021 fgsfds, Andy Nguyen (original code for Switch)
 * Copyright (C) 2025 Jaakko Lukkari
 *
 * This software may be modified and distributed under the terms
 * of the MIT license.  See the LICENSE file for details.
 */

#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <GLES2/gl2.h>
#include <errno.h>
#include <fcntl.h>
#include <linux/fb.h>
#include <linux/kd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

#ifdef __X11_DESKTOP__
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#endif

// Only include wrappers for R36S device builds
#ifndef __X11_DESKTOP__
#include "../wrappers/drm_wrapper.h"
#include "../wrappers/gbm_wrapper.h"
#endif

#include "../config.h"
#include "../so_util.h"
#include "../util.h"

static EGLDisplay display = NULL;
static EGLSurface surface = NULL;
static EGLContext context = NULL;

#ifdef __X11_DESKTOP__
static Display *x11_display = NULL;
static Window x11_window = 0;
#endif

#ifndef __X11_DESKTOP__
// Display mode tracking
typedef enum {
  DISPLAY_MODE_NONE = 0,
  DISPLAY_MODE_DRM_GBM,
  DISPLAY_MODE_FB0
} DisplayMode;

static DisplayMode display_mode = DISPLAY_MODE_NONE;

// Framebuffer variables (for FB0 fallback)
static int fb_fd = -1;
static struct fb_var_screeninfo fb_var;
static struct fb_fix_screeninfo fb_fix;
static void *fb_mem = NULL;
static size_t fb_size = 0;
static int fb_page_offset = 0; // For double buffering

// GBM/DRM variables for modern graphics
static int drm_fd = -1;
static gbm_device *gbm_dev = NULL;
static gbm_surface *gbm_surf = NULL;
static gbm_bo *previous_bo = NULL;
static uint32_t previous_fb_id = 0;
static drmModeRes *drm_resources = NULL;
static drmModeCrtc *crtc = NULL;
static drmModeConnector *connector = NULL;

// Display configuration
static int display_width = 0;
static int display_height = 0;
#endif

#ifndef __X11_DESKTOP__
// Initialize framebuffer fallback mode
static int init_fb0(void) {
  debugPrintf("=== Initializing FB0 Framebuffer Mode ===\n");

  // Open framebuffer device
  fb_fd = open("/dev/fb0", O_RDWR);
  if (fb_fd < 0) {
    debugPrintf("✗ Cannot open /dev/fb0: %s\n", strerror(errno));
    return -1;
  }
  debugPrintf("✓ Opened /dev/fb0\n");

  // Get variable screen info
  if (ioctl(fb_fd, FBIOGET_VSCREENINFO, &fb_var) < 0) {
    debugPrintf("✗ Cannot get variable screen info: %s\n", strerror(errno));
    close(fb_fd);
    fb_fd = -1;
    return -1;
  }

  // Get fixed screen info
  if (ioctl(fb_fd, FBIOGET_FSCREENINFO, &fb_fix) < 0) {
    debugPrintf("✗ Cannot get fixed screen info: %s\n", strerror(errno));
    close(fb_fd);
    fb_fd = -1;
    return -1;
  }

  display_width = fb_var.xres;
  display_height = fb_var.yres;

  debugPrintf("✓ Framebuffer info:\n");
  debugPrintf("  Resolution: %dx%d\n", display_width, display_height);
  debugPrintf("  Virtual: %dx%d\n", fb_var.xres_virtual, fb_var.yres_virtual);
  debugPrintf("  Bits per pixel: %d\n", fb_var.bits_per_pixel);
  debugPrintf("  Line length: %d\n", fb_fix.line_length);

  // Setup double buffering if possible
  if (fb_var.yres_virtual >= fb_var.yres * 2) {
    debugPrintf("✓ Double buffering available\n");
  } else {
    debugPrintf("⚠ No double buffering (virtual height: %d)\n",
                fb_var.yres_virtual);
  }

  // Calculate framebuffer size
  fb_size = fb_fix.line_length * fb_var.yres_virtual;

  // Map framebuffer memory
  fb_mem = mmap(NULL, fb_size, PROT_READ | PROT_WRITE, MAP_SHARED, fb_fd, 0);
  if (fb_mem == MAP_FAILED) {
    debugPrintf("✗ Cannot map framebuffer memory: %s\n", strerror(errno));
    close(fb_fd);
    fb_fd = -1;
    return -1;
  }
  debugPrintf("✓ Mapped framebuffer memory (%zu bytes)\n", fb_size);

  
  // Update global screen size variables
  extern int screen_width, screen_height;
  screen_width = display_width;
  screen_height = display_height;
  debugPrintf("✓ Set game screen size to %dx%d\n", screen_width, screen_height);

  display_mode = DISPLAY_MODE_FB0;
  return 0;
}

// Initialize GBM+DRM for modern graphics
static int init_drm_gbm(void) {
  debugPrintf("=== Initializing GBM+DRM Graphics ===\n");

  // Initialize wrappers first
  debugPrintf("DRM/GBM: Loading libraries dynamically...\n");

  int drm_available = drm_wrapper_init();
  int gbm_available = gbm_wrapper_init();

  if (!drm_available || !gbm_available) {
    debugPrintf("DRM/GBM: Libraries not available, falling back to FB0\n");
    drm_wrapper_cleanup();
    gbm_wrapper_cleanup();
    return -1;
  }

  debugPrintf("DRM/GBM: Successfully loaded libraries\n");
  // Open DRM device
  drm_fd = open("/dev/dri/card0", O_RDWR | O_CLOEXEC);
  if (drm_fd < 0) {
    debugPrintf("✗ Cannot open /dev/dri/card0: %s\n", strerror(errno));
    drm_wrapper_cleanup();
    gbm_wrapper_cleanup();
    return -1;
  }
  debugPrintf("✓ Opened DRM device\n");

  // Become DRM master to have exclusive control
  if (drm_funcs.drmSetMaster(drm_fd) != 0) {
    debugPrintf("⚠ Could not become DRM master: %s\n", strerror(errno));
    debugPrintf("  Another process might be controlling the display\n");
  } else {
    debugPrintf("✓ Became DRM master\n");
  }

  // Get DRM resources
  drm_resources = drm_funcs.drmModeGetResources(drm_fd);
  if (!drm_resources) {
    debugPrintf("✗ Cannot get DRM resources\n");
    close(drm_fd);
    drm_fd = -1;
    drm_wrapper_cleanup();
    gbm_wrapper_cleanup();
    return -1;
  }
  debugPrintf("✓ Got DRM resources: %d connectors, %d crtcs\n",
              drm_resources->count_connectors, drm_resources->count_crtcs);

  // Find connected display
  for (int i = 0; i < drm_resources->count_connectors; i++) {
    connector =
        drm_funcs.drmModeGetConnector(drm_fd, drm_resources->connectors[i]);
    if (connector && connector->connection == DRM_MODE_CONNECTED &&
        connector->count_modes > 0) {
      int native_width = connector->modes[0].hdisplay;
      int native_height = connector->modes[0].vdisplay;

      debugPrintf("✓ Found connected display: %s (%dx%d)\n",
                  connector->modes[0].name, native_width, native_height);

      display_width = native_width;
      display_height = native_height;

      break;
    }
    if (connector) {
      drm_funcs.drmModeFreeConnector(connector);
      connector = NULL;
    }
  }

  if (!connector) {
    debugPrintf("✗ No connected display found\n");
    drm_funcs.drmModeFreeResources(drm_resources);
    drm_resources = NULL;
    close(drm_fd);
    drm_fd = -1;
    drm_wrapper_cleanup();
    gbm_wrapper_cleanup();
    return -1;
  }

  // Get CRTC
  if (drm_resources->count_crtcs > 0) {
    crtc = drm_funcs.drmModeGetCrtc(drm_fd, drm_resources->crtcs[0]);
  }

  if (!crtc) {
    debugPrintf("✗ No CRTC available\n");
    drm_funcs.drmModeFreeConnector(connector);
    connector = NULL;
    drm_funcs.drmModeFreeResources(drm_resources);
    drm_resources = NULL;
    close(drm_fd);
    drm_fd = -1;
    drm_wrapper_cleanup();
    gbm_wrapper_cleanup();
    return -1;
  }

  // Set display mode if needed
  if (crtc->mode_valid == 0 || crtc->mode.hdisplay == 0) {
    drmModeModeInfo *best_mode = &connector->modes[0];
    uint32_t best_refresh = connector->modes[0].vrefresh;

    for (int i = 1; i < connector->count_modes; i++) {
      drmModeModeInfo *mode = &connector->modes[i];
      if (mode->hdisplay == best_mode->hdisplay &&
          mode->vdisplay == best_mode->vdisplay &&
          mode->vrefresh > best_refresh) {
        best_mode = mode;
        best_refresh = mode->vrefresh;
      }
    }

    debugPrintf("Setting display mode: %s %dx%d @%dHz\n", best_mode->name,
                best_mode->hdisplay, best_mode->vdisplay, best_mode->vrefresh);

    if (drm_funcs.drmModeSetCrtc(drm_fd, crtc->crtc_id, 0, 0, 0,
                                 &connector->connector_id, 1, best_mode) != 0) {
      debugPrintf("✗ Failed to set CRTC mode: %s\n", strerror(errno));
    } else {
      debugPrintf("✓ Set CRTC mode\n");
    }
  }

  // Create GBM device
  gbm_dev = gbm_funcs.gbm_create_device(drm_fd);
  if (!gbm_dev) {
    debugPrintf("✗ Cannot create GBM device\n");
    drm_funcs.drmModeFreeCrtc(crtc);
    crtc = NULL;
    drm_funcs.drmModeFreeConnector(connector);
    connector = NULL;
    drm_funcs.drmModeFreeResources(drm_resources);
    drm_resources = NULL;
    close(drm_fd);
    drm_fd = -1;
    drm_wrapper_cleanup();
    gbm_wrapper_cleanup();
    return -1;
  }
  debugPrintf("✓ Created GBM device\n");

  // Create GBM surface
  gbm_surf = gbm_funcs.gbm_surface_create(
      gbm_dev, display_width, display_height, GBM_FORMAT_XRGB8888,
      GBM_BO_USE_SCANOUT | GBM_BO_USE_RENDERING);
  if (!gbm_surf) {
    debugPrintf("✗ Cannot create GBM surface\n");
    gbm_funcs.gbm_device_destroy(gbm_dev);
    gbm_dev = NULL;
    drm_funcs.drmModeFreeCrtc(crtc);
    crtc = NULL;
    drm_funcs.drmModeFreeConnector(connector);
    connector = NULL;
    drm_funcs.drmModeFreeResources(drm_resources);
    drm_resources = NULL;
    close(drm_fd);
    drm_fd = -1;
    drm_wrapper_cleanup();
    gbm_wrapper_cleanup();
    return -1;
  }
  debugPrintf("✓ Created GBM surface\n");

  // Update global screen size variables
  extern int screen_width, screen_height;
  screen_width = display_width;
  screen_height = display_height;
  debugPrintf("✓ Set game screen size to %dx%d\n", screen_width, screen_height);

  display_mode = DISPLAY_MODE_DRM_GBM;
  return 0;
}
#endif // __X11_DESKTOP__

void NVEventEGLMakeCurrent(void) {
  debugPrintf("NVEventEGLMakeCurrent called\n");
  if (display != (EGLDisplay)0x1 && context != (EGLContext)0x1 &&
      surface != (EGLSurface)0x1) {
    eglMakeCurrent(display, surface, surface, context);
  } else {
    debugPrintf("NVEventEGLMakeCurrent: Using stub mode\n");
  }
}

void NVEventEGLUnmakeCurrent(void) {
  debugPrintf("NVEventEGLUnmakeCurrent called\n");
  if (display != (EGLDisplay)0x1) {
    eglMakeCurrent(display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
  } else {
    debugPrintf("NVEventEGLUnmakeCurrent: Using stub mode\n");
  }
}

int NVEventEGLInit(void) {
#ifdef __X11_DESKTOP__
  // --- X11 window ---
  x11_display = XOpenDisplay(NULL);
  if (!x11_display) {
    debugPrintf("✗ XOpenDisplay failed\n");
    return 0;
  }
  debugPrintf("✓ X11 display opened\n");

  int screen = DefaultScreen(x11_display);
  int win_w = 1280, win_h = 720;

  XSetWindowAttributes swa;
  swa.event_mask = ExposureMask | StructureNotifyMask | KeyPressMask;
  x11_window = XCreateWindow(x11_display, RootWindow(x11_display, screen), 0, 0,
                             win_w, win_h, 0, CopyFromParent, InputOutput,
                             CopyFromParent, CWEventMask, &swa);
  XStoreName(x11_display, x11_window, "Max Payne - Desktop Debug");
  XMapWindow(x11_display, x11_window);
  debugPrintf("✓ X11 window created (1280x720)\n");

  // --- EGL init ---
  display = eglGetDisplay((EGLNativeDisplayType)x11_display);
  if (display == EGL_NO_DISPLAY) {
    debugPrintf("✗ eglGetDisplay failed\n");
    XCloseDisplay(x11_display);
    return 0;
  }
  debugPrintf("✓ EGL display obtained\n");

  EGLint major, minor;
  if (!eglInitialize(display, &major, &minor)) {
    debugPrintf("✗ EGL initialization failed: 0x%x\n", eglGetError());
    XCloseDisplay(x11_display);
    return 0;
  }
  debugPrintf("✓ EGL initialized: %d.%d\n", major, minor);

  if (!eglBindAPI(EGL_OPENGL_ES_API)) {
    debugPrintf("✗ Cannot bind OpenGL ES API: 0x%x\n", eglGetError());
  }

  // Configure EGL
  EGLint num_configs = 0;
  EGLConfig egl_config;
  const EGLint config_attribs[] = {EGL_SURFACE_TYPE,
                                   EGL_WINDOW_BIT,
                                   EGL_RENDERABLE_TYPE,
                                   EGL_OPENGL_ES2_BIT,
                                   EGL_RED_SIZE,
                                   8,
                                   EGL_GREEN_SIZE,
                                   8,
                                   EGL_BLUE_SIZE,
                                   8,
                                   EGL_ALPHA_SIZE,
                                   0,
                                   EGL_DEPTH_SIZE,
                                   24,
                                   EGL_STENCIL_SIZE,
                                   8,
                                   EGL_NONE};

  if (!eglChooseConfig(display, config_attribs, &egl_config, 1, &num_configs) ||
      num_configs == 0) {
    debugPrintf("✗ No suitable EGL config found\n");
    eglTerminate(display);
    XCloseDisplay(x11_display);
    return 0;
  }
  debugPrintf("✓ Found EGL config\n");

  // Create EGL surface
  surface = eglCreateWindowSurface(display, egl_config,
                                   (EGLNativeWindowType)x11_window, NULL);
  if (surface == EGL_NO_SURFACE) {
    debugPrintf("✗ Cannot create EGL window surface: 0x%x\n", eglGetError());
    eglTerminate(display);
    XCloseDisplay(x11_display);
    return 0;
  }
  debugPrintf("✓ Created EGL window surface\n");

  // Create EGL context
  const EGLint context_attribs[] = {EGL_CONTEXT_CLIENT_VERSION, 2, EGL_NONE};

  context =
      eglCreateContext(display, egl_config, EGL_NO_CONTEXT, context_attribs);
  if (context == EGL_NO_CONTEXT) {
    debugPrintf("✗ Cannot create EGL context: 0x%x\n", eglGetError());
    eglDestroySurface(display, surface);
    eglTerminate(display);
    XCloseDisplay(x11_display);
    return 0;
  }
  debugPrintf("✓ Created EGL context\n");

  // Make context current
  if (!eglMakeCurrent(display, surface, surface, context)) {
    debugPrintf("✗ Cannot make context current: 0x%x\n", eglGetError());
    eglDestroyContext(display, context);
    eglDestroySurface(display, surface);
    eglTerminate(display);
    XCloseDisplay(x11_display);
    return 0;
  }
  debugPrintf("✓ EGL context made current\n");

  // Enable VSync
  int vsync_interval = config.vsync_enabled ? 1 : 0;
  if (!eglSwapInterval(display, vsync_interval)) {
    debugPrintf("⚠ Warning: Could not %s VSync: 0x%x\n",
                config.vsync_enabled ? "enable" : "disable", eglGetError());
  } else {
    debugPrintf("✓ VSync %s\n", config.vsync_enabled ? "enabled" : "disabled");
  }

  debugPrintf("✓ X11 EGL initialization complete\n");

#else
  // R36S device initialization
  // Try DRM/GBM first, then fall back to FB0
  if (init_drm_gbm() != 0) {
    debugPrintf("GBM+DRM failed, trying FB0 fallback...\n");
    if (init_fb0() != 0) {
      debugPrintf("✗ Both DRM/GBM and FB0 initialization failed\n");
      debugPrintf(
          "Using stub mode - game will run with audio and input only\n");
      display = (EGLDisplay)0x1;
      surface = (EGLSurface)0x1;
      context = (EGLContext)0x1;
      return 1;
    }
  }

  // Initialize EGL based on display mode
  if (display_mode == DISPLAY_MODE_DRM_GBM) {
    // Get EGL display using GBM platform
    PFNEGLGETPLATFORMDISPLAYEXTPROC get_platform_display = NULL;
    get_platform_display =
        (void *)eglGetProcAddress("eglGetPlatformDisplayEXT");
    if (get_platform_display) {
      display = get_platform_display(EGL_PLATFORM_GBM_KHR, gbm_dev, NULL);
      debugPrintf("✓ Got GBM platform EGL display\n");
    } else {
      display = eglGetDisplay((EGLNativeDisplayType)gbm_dev);
    }
  } else if (display_mode == DISPLAY_MODE_FB0) {
    // Use default display for FB0 mode
    display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    debugPrintf("✓ Got default EGL display for FB0 mode\n");
  }

  if (!display || display == EGL_NO_DISPLAY) {
    debugPrintf("✗ Failed to get EGL display: 0x%x\n", eglGetError());
    display = (EGLDisplay)0x1;
    surface = (EGLSurface)0x1;
    context = (EGLContext)0x1;
    return 1;
  }

  EGLint major, minor;
  if (!eglInitialize(display, &major, &minor)) {
    debugPrintf("✗ EGL initialization failed: 0x%x\n", eglGetError());
    display = (EGLDisplay)0x1;
    surface = (EGLSurface)0x1;
    context = (EGLContext)0x1;
    return 1;
  }
  debugPrintf("✓ EGL initialized: %d.%d\n", major, minor);

  if (!eglBindAPI(EGL_OPENGL_ES_API)) {
    debugPrintf("✗ Cannot bind OpenGL ES API: 0x%x\n", eglGetError());
  }

  // Configure EGL
  EGLint num_configs = 0;
  EGLConfig egl_config;
  const EGLint config_attribs[] = {EGL_SURFACE_TYPE,
                                   EGL_WINDOW_BIT,
                                   EGL_RENDERABLE_TYPE,
                                   EGL_OPENGL_ES2_BIT,
                                   EGL_RED_SIZE,
                                   8,
                                   EGL_GREEN_SIZE,
                                   8,
                                   EGL_BLUE_SIZE,
                                   8,
                                   EGL_ALPHA_SIZE,
                                   0,
                                   EGL_DEPTH_SIZE,
                                   24,
                                   EGL_STENCIL_SIZE,
                                   8,
                                   EGL_NONE};

  if (!eglChooseConfig(display, config_attribs, &egl_config, 1, &num_configs) ||
      num_configs == 0) {
    debugPrintf("✗ No suitable EGL config found\n");
    surface = (EGLSurface)0x1;
    context = (EGLContext)0x1;
    return 1;
  }
  debugPrintf("✓ Found EGL config\n");

  // Create EGL surface based on display mode
  if (display_mode == DISPLAY_MODE_DRM_GBM && gbm_surf) {
    surface = eglCreateWindowSurface(display, egl_config,
                                     (EGLNativeWindowType)gbm_surf, NULL);
    if (surface == EGL_NO_SURFACE) {
      debugPrintf("✗ Cannot create GBM window surface: 0x%x\n", eglGetError());
      surface = (EGLSurface)0x1;
    } else {
      debugPrintf("✓ Created GBM window surface\n");
    }
  } else {
    // For FB0 mode, create a pbuffer surface
    debugPrintf("Creating pbuffer surface for FB0 mode...\n");
    const EGLint pbuffer_attribs[] = {EGL_WIDTH, display_width, EGL_HEIGHT,
                                      display_height, EGL_NONE};
    surface = eglCreatePbufferSurface(display, egl_config, pbuffer_attribs);
    if (surface == EGL_NO_SURFACE) {
      debugPrintf("✗ Cannot create pbuffer surface: 0x%x\n", eglGetError());
      surface = (EGLSurface)0x1;
    } else {
      debugPrintf("✓ Created pbuffer surface (%dx%d)\n", display_width,
                  display_height);
    }
  }

  // Create EGL context
  const EGLint context_attribs[] = {EGL_CONTEXT_CLIENT_VERSION, 2, EGL_NONE};

  context =
      eglCreateContext(display, egl_config, EGL_NO_CONTEXT, context_attribs);
  if (context == EGL_NO_CONTEXT) {
    debugPrintf("✗ Cannot create EGL context: 0x%x\n", eglGetError());
    context = (EGLContext)0x1;
  } else {
    debugPrintf("✓ Created EGL context\n");
  }

  // Make context current
  if (context != (EGLContext)0x1 && surface != (EGLSurface)0x1) {
    if (!eglMakeCurrent(display, surface, surface, context)) {
      debugPrintf("✗ Cannot make context current: 0x%x\n", eglGetError());
    } else {
      debugPrintf("✓ EGL context made current\n");

      // Enable VSync
      int vsync_interval = config.vsync_enabled ? 1 : 0;
      if (!eglSwapInterval(display, vsync_interval)) {
        debugPrintf("⚠ Warning: Could not %s VSync: 0x%x\n",
                    config.vsync_enabled ? "enable" : "disable", eglGetError());
      } else {
        debugPrintf("✓ VSync %s\n",
                    config.vsync_enabled ? "enabled" : "disabled");
      }

      // Test rendering
      glClearColor(1.0f, 0.0f, 0.0f, 1.0f);
      glClear(GL_COLOR_BUFFER_BIT);

      extern int screen_width, screen_height;
      glViewport(0, 0, screen_width, screen_height);

      eglSwapBuffers(display, surface);
      debugPrintf("✓ Test render completed\n");
    }
  }

  debugPrintf("=== EGL initialization complete (mode: %s) ===\n",
              display_mode == DISPLAY_MODE_DRM_GBM ? "DRM/GBM" : "FB0");

  return 1;
#endif
}

void NVEventEGLSwapBuffers(void) {
  static int swap_debug_logged = 0;
  if (swap_debug_logged == 0) {
    debugPrintf("NVEventEGLSwapBuffers called for the first time\n");
  }

  if (display != (EGLDisplay)0x1 && surface != (EGLSurface)0x1) {
    // Always do EGL swap buffers first
    eglSwapBuffers(display, surface);

#ifndef __X11_DESKTOP__
    if (display_mode == DISPLAY_MODE_DRM_GBM) {
      // Handle GBM surface with DRM page flipping
      if (gbm_surf && drm_fd >= 0 && crtc && connector) {
        gbm_bo *bo = gbm_funcs.gbm_surface_lock_front_buffer(gbm_surf);
        if (bo) {
          uint32_t handle = gbm_funcs.gbm_bo_get_handle(bo).u32;
          uint32_t pitch = gbm_funcs.gbm_bo_get_stride(bo);
          uint32_t fb_id_local;

          int add_fb_result =
              drm_funcs.drmModeAddFB(drm_fd, display_width, display_height, 32,
                                     32, pitch, handle, &fb_id_local);
          if (add_fb_result == 0) {
            static int first_frame = 1;
            if (first_frame) {
              int set_result = drm_funcs.drmModeSetCrtc(
                  drm_fd, crtc->crtc_id, fb_id_local, 0, 0,
                  &connector->connector_id, 1, &connector->modes[0]);
              if (set_result == 0) {
                debugPrintf("✓ First frame displayed (DRM/GBM)\n");
                first_frame = 0;
              }
            } else {
              int flip_result =
                  drm_funcs.drmModePageFlip(drm_fd, crtc->crtc_id, fb_id_local,
                                            DRM_MODE_PAGE_FLIP_EVENT, NULL);
              if (flip_result != 0) {
                static int page_flip_failures = 0;
                page_flip_failures++;

                if (page_flip_failures < 10) {
                  flip_result = drm_funcs.drmModePageFlip(drm_fd, crtc->crtc_id,
                                                          fb_id_local, 0, NULL);
                }

                if (flip_result != 0) {
                  drm_funcs.drmModeSetCrtc(drm_fd, crtc->crtc_id, fb_id_local,
                                           0, 0, &connector->connector_id, 1,
                                           &connector->modes[0]);
                  if (page_flip_failures == 10) {
                    debugPrintf("⚠ Page flip failing, using immediate mode\n");
                  }
                }
              }
            }

            if (previous_fb_id != 0) {
              drm_funcs.drmModeRmFB(drm_fd, previous_fb_id);
            }
            if (previous_bo) {
              gbm_funcs.gbm_surface_release_buffer(gbm_surf, previous_bo);
            }

            previous_fb_id = fb_id_local;
            previous_bo = bo;
          } else {
            gbm_funcs.gbm_surface_release_buffer(gbm_surf, bo);
          }
        }
      }
    } else if (display_mode == DISPLAY_MODE_FB0) {
      // Handle FB0 framebuffer display
      if (fb_fd >= 0 && fb_mem && fb_mem != MAP_FAILED) {
        // Read the rendered frame from EGL
        static unsigned char *pixel_buffer = NULL;
        if (!pixel_buffer) {
          debugPrintf("Allocating pixel buffer for FB0 mode (%dx%d)\n",
                      display_width, display_height);

          pixel_buffer =
              (unsigned char *)malloc(display_width * display_height * 4);
        }

        if (pixel_buffer) {
          // Read pixels from OpenGL
          if (swap_debug_logged == 0)
            debugPrintf("1st glReadPixels\n");

          glReadPixels(0, 0, display_width, display_height, GL_RGBA,
                       GL_UNSIGNED_BYTE, pixel_buffer);

          // Copy to framebuffer with format conversion if needed
          int bytes_per_pixel = fb_var.bits_per_pixel / 8;
          int fb_line_length = fb_fix.line_length;

          // Calculate offset for double buffering
          char *fb_ptr = (char *)fb_mem +
                         (fb_page_offset * fb_line_length * display_height);

          for (int y = 0; y < display_height; y++) {
            // OpenGL has origin at bottom-left, framebuffer at top-left
            int gl_y = display_height - 1 - y;
            unsigned char *src = pixel_buffer + (gl_y * display_width * 4);
            unsigned char *dst = (unsigned char *)fb_ptr + (y * fb_line_length);

            for (int x = 0; x < display_width; x++) {
              unsigned char r = src[0];
              unsigned char g = src[1];
              unsigned char b = src[2];

              if (bytes_per_pixel == 4) {
                // RGBA8888 or XRGB8888
                dst[0] = b;
                dst[1] = g;
                dst[2] = r;
                dst[3] = 0xFF;
              } else if (bytes_per_pixel == 2) {
                // RGB565
                unsigned short color =
                    ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3);
                *((unsigned short *)dst) = color;
              } else if (bytes_per_pixel == 3) {
                // RGB888
                dst[0] = b;
                dst[1] = g;
                dst[2] = r;
              }

              src += 4;
              dst += bytes_per_pixel;
            }
          }

          // Flip to the page we just wrote if double buffering is available
          if (fb_var.yres_virtual >= fb_var.yres * 2) {
            fb_var.yoffset = fb_page_offset * display_height;
            if (swap_debug_logged == 0)
              debugPrintf("1st FBIOPAN_DISPLAY\n");
            if (ioctl(fb_fd, FBIOPAN_DISPLAY, &fb_var) == 0) {
              // Switch to other buffer for next frame
              fb_page_offset = 1 - fb_page_offset;
            }
          }
        }
      }
    }
#endif // __X11_DESKTOP__
  }

  if (swap_debug_logged == 0) {
    debugPrintf("NVEventEGLSwapBuffers: First swap completed\n");
    swap_debug_logged = 1;
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
  debugPrintf("=== Starting OpenGL cleanup ===\n");

#ifdef __X11_DESKTOP__
  debugPrintf("Cleaning up X11/EGL...\n");
  // Clean up X11/EGL
  if (display && display != (EGLDisplay)0x1) {
    debugPrintf("Making EGL context current...\n");
    eglMakeCurrent(display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);

    if (context && context != (EGLContext)0x1) {
      debugPrintf("Destroying EGL context...\n");
      eglDestroyContext(display, context);
    }

    if (surface && surface != (EGLSurface)0x1) {
      debugPrintf("Destroying EGL surface...\n");
      eglDestroySurface(display, surface);
    }

    debugPrintf("Terminating EGL display...\n");
    eglTerminate(display);
  }

  if (x11_window) {
    debugPrintf("Destroying X11 window...\n");
    XDestroyWindow(x11_display, x11_window);
  }

  if (x11_display) {
    debugPrintf("Closing X11 display...\n");
    XCloseDisplay(x11_display);
  }
  debugPrintf("✓ X11/EGL cleaned up\n");
#else
  debugPrintf("Display mode: %d\n", display_mode);

  // Clean up based on display mode
  if (display_mode == DISPLAY_MODE_DRM_GBM) {
    debugPrintf("Cleaning up DRM/GBM resources...\n");

    // Clean up DRM/GBM resources - check if wrapper functions are available
    if (previous_fb_id != 0 && drm_fd >= 0 && drm_funcs.drmModeRmFB) {
      debugPrintf("Removing framebuffer (ID: %u)...\n", previous_fb_id);
      drm_funcs.drmModeRmFB(drm_fd, previous_fb_id);
      previous_fb_id = 0;
      debugPrintf("✓ Framebuffer removed\n");
    }

    if (previous_bo && gbm_surf && gbm_funcs.gbm_surface_release_buffer) {
      debugPrintf("Releasing previous buffer object...\n");
      gbm_funcs.gbm_surface_release_buffer(gbm_surf, previous_bo);
      previous_bo = NULL;
      debugPrintf("✓ Buffer object released\n");
    }

    if (drm_fd >= 0 && drm_funcs.drmDropMaster) {
      debugPrintf("Dropping DRM master...\n");
      drm_funcs.drmDropMaster(drm_fd);
      debugPrintf("✓ DRM master dropped\n");
    }
  }

  // Clean up EGL
  debugPrintf("Cleaning up EGL...\n");
  if (display && display != (EGLDisplay)0x1) {
    debugPrintf("Making EGL context current (none)...\n");
    eglMakeCurrent(display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);

    if (context && context != (EGLContext)0x1) {
      debugPrintf("Destroying EGL context...\n");
      eglDestroyContext(display, context);
      debugPrintf("✓ EGL context destroyed\n");
    }

    if (surface && surface != (EGLSurface)0x1) {
      debugPrintf("Destroying EGL surface...\n");
      eglDestroySurface(display, surface);
      debugPrintf("✓ EGL surface destroyed\n");
    }

    debugPrintf("Terminating EGL display...\n");
    eglTerminate(display);
    debugPrintf("✓ EGL display terminated\n");
  }

  // Clean up GBM resources
  debugPrintf("Cleaning up GBM resources...\n");
  if (gbm_surf && gbm_funcs.gbm_surface_destroy) {
    debugPrintf("Destroying GBM surface...\n");
    gbm_funcs.gbm_surface_destroy(gbm_surf);
    gbm_surf = NULL;
    debugPrintf("✓ GBM surface destroyed\n");
  }

  if (gbm_dev && gbm_funcs.gbm_device_destroy) {
    debugPrintf("Destroying GBM device...\n");
    gbm_funcs.gbm_device_destroy(gbm_dev);
    gbm_dev = NULL;
    debugPrintf("✓ GBM device destroyed\n");
  }

  // Clean up DRM resources
  debugPrintf("Cleaning up DRM resources...\n");
  if (connector && drm_funcs.drmModeFreeConnector) {
    debugPrintf("Freeing DRM connector...\n");
    drm_funcs.drmModeFreeConnector(connector);
    connector = NULL;
    debugPrintf("✓ DRM connector freed\n");
  }

  if (crtc && drm_funcs.drmModeFreeCrtc) {
    debugPrintf("Freeing DRM CRTC...\n");
    drm_funcs.drmModeFreeCrtc(crtc);
    crtc = NULL;
    debugPrintf("✓ DRM CRTC freed\n");
  }

  if (drm_resources && drm_funcs.drmModeFreeResources) {
    debugPrintf("Freeing DRM resources...\n");
    drm_funcs.drmModeFreeResources(drm_resources);
    drm_resources = NULL;
    debugPrintf("✓ DRM resources freed\n");
  }

  if (drm_fd >= 0) {
    debugPrintf("Closing DRM file descriptor...\n");
    close(drm_fd);
    drm_fd = -1;
    debugPrintf("✓ DRM file descriptor closed\n");
  }

  // Clean up wrapper libraries
  debugPrintf("Cleaning up wrapper libraries...\n");
  if (drm_wrapper_is_available()) {
    drm_wrapper_cleanup();
  } else {
    debugPrintf("DRM wrapper not available, skipping cleanup\n");
  }

  if (gbm_wrapper_is_available()) {
    gbm_wrapper_cleanup();
  } else {
    debugPrintf("GBM wrapper not available, skipping cleanup\n");
  }
  debugPrintf("✓ Wrapper libraries cleaned up\n");

  // Clean up FB0 resources
  debugPrintf("Cleaning up FB0 resources...\n");
  if (fb_mem && fb_mem != MAP_FAILED) {
    debugPrintf("Unmapping framebuffer memory...\n");
    munmap(fb_mem, fb_size);
    fb_mem = NULL;
    debugPrintf("✓ Framebuffer memory unmapped\n");
  }

  if (fb_fd >= 0) {
    debugPrintf("Closing framebuffer file descriptor...\n");
    close(fb_fd);
    fb_fd = -1;
    debugPrintf("✓ Framebuffer file descriptor closed\n");
  }

  debugPrintf("✓ Graphics resources cleaned up\n");
#endif
  debugPrintf("=== OpenGL cleanup completed ===\n");
}
/* drm_wrapper.c -- Dynamic DRM library wrapper implementation
 *
 * Copyright (C) 2025 Jaakko Lukkari
 *
 * This software may be modified and distributed under the terms
 * of the MIT license.  See the LICENSE file for details.
 */

#include "drm_wrapper.h"
#include "../util.h"
#include <dlfcn.h>
#include <stdio.h>
#include <sys/stat.h>
#include <unistd.h>

// Global DRM function table
drm_functions_t drm_funcs = {0};

// Library handle
static void *libdrm_handle = NULL;

static int drm_device_exists(void) {
  struct stat st;
  return (stat("/dev/dri/card0", &st) == 0);
}

int drm_wrapper_init(void) {
  // Check if DRM device exists
  if (!drm_device_exists()) {
    debugPrintf("DRM: /dev/dri/card0 not found, skipping DRM initialization\n");
    return 0; // Not an error, just not available
  }

  // Try to load libdrm
  libdrm_handle = dlopen("libdrm.so.2", RTLD_LAZY);
  if (!libdrm_handle) {
    libdrm_handle = dlopen("libdrm.so", RTLD_LAZY);
  }

  if (!libdrm_handle) {
    debugPrintf("DRM: Failed to load libdrm: %s\n", dlerror());
    return 0; // Not an error, just not available
  }

  // Load DRM functions
  drm_funcs.drmOpen = dlsym(libdrm_handle, "drmOpen");
  drm_funcs.drmSetMaster = dlsym(libdrm_handle, "drmSetMaster");
  drm_funcs.drmDropMaster = dlsym(libdrm_handle, "drmDropMaster");
  drm_funcs.drmClose = dlsym(libdrm_handle, "drmClose");
  drm_funcs.drmModeGetResources = dlsym(libdrm_handle, "drmModeGetResources");
  drm_funcs.drmModeFreeResources = dlsym(libdrm_handle, "drmModeFreeResources");
  drm_funcs.drmModeGetConnector = dlsym(libdrm_handle, "drmModeGetConnector");
  drm_funcs.drmModeFreeConnector = dlsym(libdrm_handle, "drmModeFreeConnector");
  drm_funcs.drmModeGetCrtc = dlsym(libdrm_handle, "drmModeGetCrtc");
  drm_funcs.drmModeFreeCrtc = dlsym(libdrm_handle, "drmModeFreeCrtc");
  drm_funcs.drmModeSetCrtc = dlsym(libdrm_handle, "drmModeSetCrtc");
  drm_funcs.drmModePageFlip = dlsym(libdrm_handle, "drmModePageFlip");
  drm_funcs.drmModeAddFB = dlsym(libdrm_handle, "drmModeAddFB");
  drm_funcs.drmModeRmFB = dlsym(libdrm_handle, "drmModeRmFB");
  drm_funcs.drmHandleEvent = dlsym(libdrm_handle, "drmHandleEvent");

  // Check if all required functions were loaded
  if (!drm_funcs.drmOpen || !drm_funcs.drmSetMaster || !drm_funcs.drmClose ||
      !drm_funcs.drmModeGetResources || !drm_funcs.drmModeGetConnector ||
      !drm_funcs.drmModeGetCrtc || !drm_funcs.drmModeSetCrtc) {
    debugPrintf("DRM: Failed to load required functions\n");
    drm_wrapper_cleanup();
    return 0;
  }

  debugPrintf("DRM: Successfully loaded libdrm functions\n");
  return 1;
}

void drm_wrapper_cleanup(void) {
  if (libdrm_handle) {
    dlclose(libdrm_handle);
    libdrm_handle = NULL;
  }

  // Clear function pointers
  drm_funcs = (drm_functions_t){0};
}

int drm_wrapper_is_available(void) {
  return (libdrm_handle != NULL && drm_funcs.drmOpen != NULL);
}
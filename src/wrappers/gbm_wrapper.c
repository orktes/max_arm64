/* gbm_wrapper.c -- Dynamic GBM library wrapper implementation
 *
 * Copyright (C) 2025 Jaakko Lukkari
 *
 * This software may be modified and distributed under the terms
 * of the MIT license.  See the LICENSE file for details.
 */

#include "gbm_wrapper.h"
#include "../util.h"
#include <dlfcn.h>
#include <stdio.h>
#include <sys/stat.h>
#include <unistd.h>

// Global GBM function table
gbm_functions_t gbm_funcs = {0};

// Library handle
static void *libgbm_handle = NULL;

static int gbm_device_exists(void) {
    struct stat st;
    return (stat("/dev/dri/card0", &st) == 0);
}

int gbm_wrapper_init(void) {
    // Check if DRM device exists (required for GBM)
    if (!gbm_device_exists()) {
        debugPrintf("GBM: /dev/dri/card0 not found, skipping GBM initialization\n");
        return 0; // Not an error, just not available
    }

    // Try to load libgbm
    libgbm_handle = dlopen("libgbm.so.1", RTLD_LAZY);
    if (!libgbm_handle) {
        libgbm_handle = dlopen("libgbm.so", RTLD_LAZY);
    }
    
    if (!libgbm_handle) {
        debugPrintf("GBM: Failed to load libgbm: %s\n", dlerror());
        return 0; // Not an error, just not available
    }

    // Load GBM functions
    gbm_funcs.gbm_create_device = dlsym(libgbm_handle, "gbm_create_device");
    gbm_funcs.gbm_device_destroy = dlsym(libgbm_handle, "gbm_device_destroy");
    gbm_funcs.gbm_surface_create = dlsym(libgbm_handle, "gbm_surface_create");
    gbm_funcs.gbm_surface_destroy = dlsym(libgbm_handle, "gbm_surface_destroy");
    gbm_funcs.gbm_surface_lock_front_buffer = dlsym(libgbm_handle, "gbm_surface_lock_front_buffer");
    gbm_funcs.gbm_surface_release_buffer = dlsym(libgbm_handle, "gbm_surface_release_buffer");
    gbm_funcs.gbm_bo_get_handle = dlsym(libgbm_handle, "gbm_bo_get_handle");
    gbm_funcs.gbm_bo_get_stride = dlsym(libgbm_handle, "gbm_bo_get_stride");
    gbm_funcs.gbm_bo_destroy = dlsym(libgbm_handle, "gbm_bo_destroy");
    gbm_funcs.gbm_bo_get_user_data = dlsym(libgbm_handle, "gbm_bo_get_user_data");
    gbm_funcs.gbm_bo_set_user_data = dlsym(libgbm_handle, "gbm_bo_set_user_data");

    // Check if all required functions were loaded
    if (!gbm_funcs.gbm_create_device || !gbm_funcs.gbm_device_destroy ||
        !gbm_funcs.gbm_surface_create || !gbm_funcs.gbm_surface_destroy ||
        !gbm_funcs.gbm_surface_lock_front_buffer || !gbm_funcs.gbm_surface_release_buffer ||
        !gbm_funcs.gbm_bo_get_handle || !gbm_funcs.gbm_bo_get_stride) {
        debugPrintf("GBM: Failed to load required functions\n");
        gbm_wrapper_cleanup();
        return 0;
    }

    debugPrintf("GBM: Successfully loaded libgbm functions\n");
    return 1;
}

void gbm_wrapper_cleanup(void) {
    if (libgbm_handle) {
        dlclose(libgbm_handle);
        libgbm_handle = NULL;
    }
    
    // Clear function pointers
    gbm_funcs = (gbm_functions_t){0};
}

int gbm_wrapper_is_available(void) {
    return (libgbm_handle != NULL && gbm_funcs.gbm_create_device != NULL);
}
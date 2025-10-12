/* gbm_wrapper.h -- Dynamic GBM library wrapper
 *
 * Copyright (C) 2025 Jaakko Lukkari
 *
 * This software may be modified and distributed under the terms
 * of the MIT license.  See the LICENSE file for details.
 */

#ifndef GBM_WRAPPER_H
#define GBM_WRAPPER_H

#include <stdint.h>

// Forward declarations to avoid including gbm headers
typedef struct gbm_device gbm_device;
typedef struct gbm_surface gbm_surface;
typedef struct gbm_bo gbm_bo;

// GBM Constants
#define GBM_FORMAT_XRGB8888 0x34325258
#define GBM_BO_USE_SCANOUT (1 << 0)
#define GBM_BO_USE_RENDERING (1 << 2)

// Union for GBM buffer object handle
union gbm_bo_handle {
    void *ptr;
    int32_t s32;
    uint32_t u32;
    int64_t s64;
    uint64_t u64;
};

// GBM function pointers
typedef struct {
    gbm_device* (*gbm_create_device)(int fd);
    void (*gbm_device_destroy)(gbm_device *gbm);
    gbm_surface* (*gbm_surface_create)(gbm_device *gbm,
                                      uint32_t width, uint32_t height,
                                      uint32_t format, uint32_t flags);
    void (*gbm_surface_destroy)(gbm_surface *surface);
    gbm_bo* (*gbm_surface_lock_front_buffer)(gbm_surface *surface);
    void (*gbm_surface_release_buffer)(gbm_surface *surface, gbm_bo *bo);
    union gbm_bo_handle (*gbm_bo_get_handle)(gbm_bo *bo);
    uint32_t (*gbm_bo_get_stride)(gbm_bo *bo);
    void (*gbm_bo_destroy)(gbm_bo *bo);
    void* (*gbm_bo_get_user_data)(gbm_bo *bo);
    void (*gbm_bo_set_user_data)(gbm_bo *bo, void *data,
                                void (*destroy_user_data)(gbm_bo*, void*));
} gbm_functions_t;

// Global GBM function table
extern gbm_functions_t gbm_funcs;

// Wrapper functions
int gbm_wrapper_init(void);
void gbm_wrapper_cleanup(void);
int gbm_wrapper_is_available(void);

#endif // GBM_WRAPPER_H
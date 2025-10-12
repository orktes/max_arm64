/* drm_wrapper.h -- Dynamic DRM library wrapper
 *
 * Copyright (C) 2025 Jaakko Lukkari
 *
 * This software may be modified and distributed under the terms
 * of the MIT license.  See the LICENSE file for details.
 */

#ifndef DRM_WRAPPER_H
#define DRM_WRAPPER_H

#include <stdint.h>

// Forward declarations to avoid including drm headers
typedef struct _drmModeRes drmModeRes;
typedef struct _drmModeConnector drmModeConnector;
typedef struct _drmModeCrtc drmModeCrtc;
typedef struct _drmModeModeInfo drmModeModeInfo;

// DRM Constants
#define DRM_MODE_CONNECTED 1
#define DRM_MODE_TYPE_DRIVER (1<<6)
#define DRM_IOCTL_SET_MASTER 0x641e
#define DRM_IOCTL_DROP_MASTER 0x641f
#define DRM_MODE_PAGE_FLIP_EVENT 0x01

// DRM Mode Info structure
struct _drmModeModeInfo {
    uint32_t clock;
    uint16_t hdisplay, hsync_start, hsync_end, htotal, hskew;
    uint16_t vdisplay, vsync_start, vsync_end, vtotal, vscan;
    uint32_t vrefresh;
    uint32_t flags;
    uint32_t type;
    char name[32];
};

// DRM Resources structure
struct _drmModeRes {
    int count_fbs;
    uint32_t *fbs;
    int count_crtcs;
    uint32_t *crtcs;
    int count_connectors;
    uint32_t *connectors;
    int count_encoders;
    uint32_t *encoders;
    uint32_t min_width, max_width;
    uint32_t min_height, max_height;
};

// DRM Connector structure
struct _drmModeConnector {
    uint32_t connector_id;
    uint32_t encoder_id;
    uint32_t connector_type;
    uint32_t connector_type_id;
    uint32_t connection;
    uint32_t mmWidth, mmHeight;
    uint32_t subpixel;
    int count_modes;
    drmModeModeInfo *modes;
    int count_props;
    uint32_t *props;
    uint64_t *prop_values;
    int count_encoders;
    uint32_t *encoders;
};

// DRM CRTC structure
struct _drmModeCrtc {
    uint32_t crtc_id;
    uint32_t buffer_id;
    uint32_t x, y;
    uint32_t width, height;
    int mode_valid;
    drmModeModeInfo mode;
    int gamma_size;
};

// DRM function pointers
typedef struct {
    int (*drmOpen)(const char *name, const char *busid);
    int (*drmSetMaster)(int fd);
    int (*drmDropMaster)(int fd);
    void (*drmClose)(int fd);
    drmModeRes* (*drmModeGetResources)(int fd);
    void (*drmModeFreeResources)(drmModeRes *ptr);
    drmModeConnector* (*drmModeGetConnector)(int fd, uint32_t connectorId);
    void (*drmModeFreeConnector)(drmModeConnector *ptr);
    drmModeCrtc* (*drmModeGetCrtc)(int fd, uint32_t crtcId);
    void (*drmModeFreeCrtc)(drmModeCrtc *ptr);
    int (*drmModeSetCrtc)(int fd, uint32_t crtcId, uint32_t bufferId,
                         uint32_t x, uint32_t y, uint32_t *connectors,
                         int count, drmModeModeInfo *mode);
    int (*drmModePageFlip)(int fd, uint32_t crtc_id, uint32_t fb_id,
                          uint32_t flags, void *user_data);
    int (*drmModeAddFB)(int fd, uint32_t width, uint32_t height, uint8_t depth,
                       uint8_t bpp, uint32_t pitch, uint32_t bo_handle,
                       uint32_t *buf_id);
    int (*drmModeRmFB)(int fd, uint32_t bufferId);
    int (*drmHandleEvent)(int fd, void *evctx);
} drm_functions_t;

// Global DRM function table
extern drm_functions_t drm_funcs;

// Wrapper functions
int drm_wrapper_init(void);
void drm_wrapper_cleanup(void);
int drm_wrapper_is_available(void);

#endif // DRM_WRAPPER_H
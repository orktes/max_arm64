/* videoplayer.c
 *
 * Copyright (C) 2025 Jaakko Lukkari
 *
 * This software may be modified and distributed under the terms
 * of the MIT license.  See the LICENSE file for details.
 */

#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

#include "videoplayer.h"
#include "config.h"
#include "util.h"

#define OVERLAY_FILENAME "loading_video_overlay.txt"

static int ffmpeg_available = -1;  // -1 = not checked, 0 = not available, 1 = available
static pid_t ffmpeg_pid = -1;     // PID of running ffmpeg process
static int videoplayer_initialized = 0;

static int check_ffmpeg_availability(void) {
    if (ffmpeg_available != -1) {
        return ffmpeg_available;
    }
    
    // Test if ffmpeg is available by running ffmpeg -version
    FILE *fp = popen("ffmpeg -version 2>/dev/null", "r");
    if (!fp) {
        debugPrintf("videoplayer: popen failed, ffmpeg not available\n");
        ffmpeg_available = 0;
        return 0;
    }
    
    char buffer[256];
    int found_version = 0;
    while (fgets(buffer, sizeof(buffer), fp)) {
        if (strstr(buffer, "ffmpeg version")) {
            found_version = 1;
            debugPrintf("videoplayer: Found %s", buffer);
            break;
        }
    }
    
    int exit_code = pclose(fp);
    if (exit_code == 0 && found_version) {
        debugPrintf("videoplayer: ffmpeg is available\n");
        ffmpeg_available = 1;
    } else {
        debugPrintf("videoplayer: ffmpeg not available (exit code: %d)\n", exit_code);
        ffmpeg_available = 0;
    }
    
    return ffmpeg_available;
}

int videoplayer_init(void) {
    if (videoplayer_initialized) {
        return 0;
    }
    
    debugPrintf("videoplayer: Initializing video player\n");
    
    // Check if ffmpeg is available
    if (!check_ffmpeg_availability()) {
        debugPrintf("videoplayer: ffmpeg not available, video playback disabled\n");
        videoplayer_initialized = 1;  // Still mark as initialized
        return 0;
    }
    
    // Reset state
    ffmpeg_pid = -1;
    
    videoplayer_initialized = 1;
    debugPrintf("videoplayer: Video player initialized successfully\n");
    return 0;
}

void videoplayer_cleanup(void) {
    if (!videoplayer_initialized) {
        return;
    }
    
    debugPrintf("videoplayer: Cleaning up video player\n");
    
    // Stop any running video
    videoplayer_stop();
    
    videoplayer_initialized = 0;
    debugPrintf("videoplayer: Video player cleanup complete\n");
}

int videoplayer_is_available(void) {
    return check_ffmpeg_availability();
}

int videoplayer_play(const char *filename, uint8_t arg1, uint8_t arg2, float arg3) {
    (void)arg1; (void)arg2; (void)arg3; // Unused parameters
    
    if (!videoplayer_initialized) {
        debugPrintf("videoplayer: Not initialized\n");
        return -1;
    }
    
    if (!videoplayer_is_available()) {
        debugPrintf("videoplayer: ffmpeg not available, skipping video %s\n", 
                   filename ? filename : "NULL");
        return -1;
    }
    
    if (!filename) {
        debugPrintf("videoplayer: No filename provided\n");
        return -1;
    }
    
    // Stop any currently playing video
    videoplayer_stop();
    
    debugPrintf("videoplayer: Playing video %s\n", filename);
    
    // Fork a new process to run ffmpeg
    ffmpeg_pid = fork();
    
    if (ffmpeg_pid == -1) {
        debugPrintf("videoplayer: fork() failed: %s\n", strerror(errno));
        return -1;
    } else if (ffmpeg_pid == 0) {
        // Child process - run ffmpeg
        char input_path[512];
        char scale_filter[128];
        char overlay_filter[256];
        
        snprintf(input_path, sizeof(input_path), "gamedata/%s", filename);
        
        // Create a composite filter that scales and adds text overlay
        // The drawtext filter reads from the overlay file and positions text at bottom left
        snprintf(scale_filter, sizeof(scale_filter), "scale=%d:%d", screen_width, screen_height);
        snprintf(overlay_filter, sizeof(overlay_filter), 
                "%s,drawtext=textfile=%s:fontcolor=white:fontsize=24:x=10:y=h-th-10:reload=1",
                scale_filter, OVERLAY_FILENAME);
        
        // Redirect stdout and stderr to /dev/null to avoid cluttering output
        if (!freopen("/dev/null", "w", stdout)) {
            // If freopen fails, we can still continue but output won't be redirected
        }
        if (!freopen("/dev/null", "w", stderr)) {
            // If freopen fails, we can still continue but output won't be redirected
        }
        
        // Execute ffmpeg with text overlay
        execl("/usr/bin/ffmpeg", "ffmpeg",
              "-i", input_path,
              "-pix_fmt", "bgra",
              "-vf", overlay_filter,
              "-f", "fbdev",
              "/dev/fb0",
              NULL);
              
        // If execl fails, try from PATH
        execlp("ffmpeg", "ffmpeg",
               "-i", input_path,
               "-pix_fmt", "bgra", 
               "-vf", overlay_filter,
               "-f", "fbdev",
               "/dev/fb0",
               NULL);
        
        // If we get here, exec failed
        _exit(1);
    } else {
        // Parent process
        debugPrintf("videoplayer: Started ffmpeg process with PID %d\n", ffmpeg_pid);
        
        // Give ffmpeg a moment to start up
        usleep(100000); // 100ms
        
        // Check if process is still running
        int status;
        pid_t result = waitpid(ffmpeg_pid, &status, WNOHANG);
        
        if (result == ffmpeg_pid) {
            // Process already exited
            debugPrintf("videoplayer: ffmpeg process exited immediately with status %d\n", 
                       WEXITSTATUS(status));
            ffmpeg_pid = -1;
            return -1;
        } else if (result == -1) {
            debugPrintf("videoplayer: waitpid failed: %s\n", strerror(errno));
            ffmpeg_pid = -1;
            return -1;
        }
        
        // Process is running
        return 0;
    }
}

void videoplayer_stop(void) {
    if (ffmpeg_pid <= 0) {
        return;
    }
    
    debugPrintf("videoplayer: Stopping video playback (PID %d)\n", ffmpeg_pid);
    
    // Send SIGTERM to ffmpeg process
    if (kill(ffmpeg_pid, SIGTERM) == 0) {
        // Wait for process to exit (with timeout)
        int status;
        for (int i = 0; i < 50; i++) { // Wait up to 500ms
            pid_t result = waitpid(ffmpeg_pid, &status, WNOHANG);
            if (result == ffmpeg_pid) {
                debugPrintf("videoplayer: ffmpeg process terminated gracefully\n");
                ffmpeg_pid = -1;
                return;
            } else if (result == -1) {
                debugPrintf("videoplayer: waitpid failed: %s\n", strerror(errno));
                ffmpeg_pid = -1;
                return;
            }
            usleep(10000); // 10ms
        }
        
        // If still running, force kill
        debugPrintf("videoplayer: ffmpeg didn't exit gracefully, force killing\n");
        kill(ffmpeg_pid, SIGKILL);
        waitpid(ffmpeg_pid, &status, 0);
    } else {
        debugPrintf("videoplayer: Failed to kill ffmpeg process: %s\n", strerror(errno));
    }
    
    ffmpeg_pid = -1;
    
    // Clean up overlay file
    unlink(OVERLAY_FILENAME);
}

int videoplayer_is_playing(void) {
    if (ffmpeg_pid <= 0) {
        return 0;
    }
    
    // Check if process is still running
    int status;
    pid_t result = waitpid(ffmpeg_pid, &status, WNOHANG);
    
    if (result == ffmpeg_pid) {
        // Process exited
        debugPrintf("videoplayer: ffmpeg process finished with status %d\n", 
                   WEXITSTATUS(status));
        ffmpeg_pid = -1;
        
        // Clean up overlay file when video finishes
        unlink(OVERLAY_FILENAME);
        
        return 0;
    } else if (result == -1) {
        // Error checking process
        debugPrintf("videoplayer: Error checking ffmpeg process: %s\n", strerror(errno));
        ffmpeg_pid = -1;
        
        // Clean up overlay file on error
        unlink(OVERLAY_FILENAME);
        
        return 0;
    }
    
    // Process is still running
    return 1;
}

void videoplayer_set_overlay(const char *text) {
    FILE *f = fopen(OVERLAY_FILENAME, "w");
    if (!f) {
        debugPrintf("videoplayer: Failed to create %s: %s\n", OVERLAY_FILENAME, strerror(errno));
        return;
    }
    fprintf(f, "%s\n", text ? text : "Loading...");
    fclose(f);
}
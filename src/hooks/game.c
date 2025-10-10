/* game.c -- hooks and patches for everything other than AL and GL
 *
 * Copyright (C) 2021 fgsfds, Andy Nguyen (original code for Switch)
 * Copyright (C) 2025 Jaakko Lukkari
 *
 * This software may be modified and distributed under the terms
 * of the MIT license.  See the LICENSE file for details.
 */

#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <stdarg.h>
#include <string.h>
#include <math.h>
#include <threads.h>
#include <unistd.h>
#include <SDL2/SDL.h>

#include "../config.h"
#include "../util.h"
#include "../so_util.h"
#include "../hooks.h"

#define APK_PATH "main.obb"

#define GAMEDATA_DIR "gamedata/"
#define PATH_MAX 512

#define BTN_A          0x1
#define BTN_B          0x2
#define BTN_X          0x4
#define BTN_Y          0x8
#define BTN_START      0x10
#define BTN_BACK       0x20
#define BTN_L1         0x40
#define BTN_R1         0x80
#define BTN_DPAD_UP    0x100
#define BTN_DPAD_DOWN  0x200
#define BTN_DPAD_LEFT  0x400
#define BTN_DPAD_RIGHT 0x800


extern uintptr_t __cxa_guard_acquire;
extern uintptr_t __cxa_guard_release;
extern uintptr_t __cxa_throw;

static int *deviceChip;
static int *deviceForm;
static int *definedDevice;

// For ARM64 Linux, we'll use a simple stub for gamepad input
// Real implementation would require SDL2 or similar input library
static uint8_t fake_tls[0x100];


// Joystick and input state
static int joystick_initialized = 0;
static SDL_Joystick *joystick = NULL;

static float l2_axis = 0.0f;
static float r2_axis = 0.0f;
static int r1_pressed = 0;

// Initialize SDL2 for gamepad support
static void init_joystick(void) {
  if (joystick_initialized) return;
  
  if (SDL_Init(SDL_INIT_JOYSTICK) < 0) {
    debugPrintf("SDL joystick init failed: %s\n", SDL_GetError());
    return;
  }
  
  // Check if gamepad is present
  int num_joysticks = SDL_NumJoysticks();
  if (num_joysticks > 0) {
    joystick = SDL_JoystickOpen(0);
    if (joystick) {
      const char* name = SDL_JoystickName(joystick);
      debugPrintf("INPUT DEBUG: Found joystick: %s\n", name ? name : "Unknown");
    } else {
      debugPrintf("INPUT DEBUG: Failed to open joystick: %s\n", SDL_GetError());
    }
  } else {
    debugPrintf("INPUT DEBUG: No joystick found\n");
  }
  
  joystick_initialized = 1;
}


// Safe hook that checks if symbol exists before hooking
static void safe_hook_arm64(const char *symbol, uintptr_t replacement) {
  uintptr_t addr = so_find_addr(symbol);
  if (addr) {
    debugPrintf("patch_game: Hooking %s at 0x%lx\n", symbol, addr);
    hook_arm64(addr, replacement);
  } else {
    debugPrintf("patch_game: Symbol %s not found, skipping\n", symbol);
  }
}

// control binding array
typedef struct {
  int unk[14];
} MaxPayne_InputControl;
static MaxPayne_InputControl *sm_control = NULL; // [32]

static int (* MaxPayne_InputControl_getButton)(MaxPayne_InputControl *, int);

int NvAPKOpen(const char *path) {
  //debugPrintf("NvAPKOpen: %s\n", path);
  return 0;
}

int ProcessEvents(void) {
  return 0; // 1 is exit!
}

int AND_DeviceType(void) {
  debugPrintf("AND_DeviceType: returning device info\n");
  // 0x1: phone
  // 0x2: tegra
  // low memory is < 256
  return (MEMORY_MB << 6) | (3 << 2) | 0x2;
}

int AND_DeviceLocale(void) {
  debugPrintf("AND_DeviceLocale: returning 0 (english)\n");
  return 0; // english
}

int AND_SystemInitialize(void) {
  debugPrintf("AND_SystemInitialize: Setting device information\n");
  debugPrintf("AND_SystemInitialize: deviceForm ptr = %p, deviceChip ptr = %p, definedDevice ptr = %p\n", 
              (void*)deviceForm, (void*)deviceChip, (void*)definedDevice);
  
  if (deviceForm && deviceChip && definedDevice) {
    // set device information in such a way that bloom isn't enabled
    *deviceForm = 1; // phone
    *deviceChip = 14; // some tegra? tegras are 12, 13, 14
    *definedDevice = 27; // some tegra?
    debugPrintf("AND_SystemInitialize: Device info set successfully\n");
  } else {
    debugPrintf("AND_SystemInitialize: WARNING - device pointers not initialized!\n");
  }
  return 0;
}

int OS_ScreenGetHeight(void) {
  //debugPrintf("OS_ScreenGetHeight: returning %d\n", screen_height);
  return screen_height;
}

int OS_ScreenGetWidth(void) {
  //debugPrintf("OS_ScreenGetWidth: returning %d\n", screen_width);
  return screen_width;
}

char *OS_FileGetArchiveName(int mode) {
  char *out = malloc(strlen(APK_PATH) + 1);
  out[0] = '\0';
  if (mode == 1) // main
    strcpy(out, APK_PATH);
  return out;
}

void ExitAndroidGame(int code) {
  debugPrintf("ExitAndroidGame: code=%d\n", code);
  
  // Cleanup SDL2
  if (joystick) {
    SDL_JoystickClose(joystick);
    joystick = NULL;
  }
  if (joystick_initialized) {
    SDL_Quit();
    joystick_initialized = 0;
  }
  
  // deinit openal
  deinit_openal();
  // deinit EGL
  deinit_opengl();
  // unmap lib
  so_unload();
  // die
  exit(0);
}

// this is supposed to allocate and return a thread handle struct, but the game never uses it
// and never frees it, so we just return a pointer to some static garbage
void *OS_ThreadLaunch(int (* func)(void *), void *arg, int r2, char *name, int r4, int priority) {
  debugPrintf("OS_ThreadLaunch: Creating thread '%s' with priority %d\n", name ? name : "unnamed", priority);
  static char buf[0x80];
  thrd_t thrd;
  int result = thrd_create(&thrd, func, arg);
  if (result != thrd_success) {
    debugPrintf("OS_ThreadLaunch: Thread creation failed with result %d\n", result);
  } else {
    debugPrintf("OS_ThreadLaunch: Thread created successfully\n");
  }
  return buf;
}

int ReadDataFromPrivateStorage(const char *file, void **data, int *size) {
  debugPrintf("ReadDataFromPrivateStorage %s\n", file);

  char fullpath[PATH_MAX];
  snprintf(fullpath, sizeof(fullpath), "%s%s", GAMEDATA_DIR, file);

  FILE *f = fopen(fullpath, "rb");
  if (!f) return 0;

  fseek(f, 0, SEEK_END);
  const int sz = ftell(f);
  fseek(f, 0, SEEK_SET);

  int ret = 0;

  if (sz > 0) {
    void *buf = malloc(sz);
    if (buf && fread(buf, sz, 1, f)) {
      ret = 1;
      *size = sz;
      *data = buf;
    } else {
      free(buf);
    }
  }

  fclose(f);

  return ret;
}

int WriteDataToPrivateStorage(const char *file, const void *data, int size) {
  debugPrintf("WriteDataToPrivateStorage %s\n", file);

  char fullpath[PATH_MAX];
  snprintf(fullpath, sizeof(fullpath), "%s%s", GAMEDATA_DIR, file);

  FILE *f = fopen(fullpath, "wb");
  if (!f) return 0;

  const int ret = fwrite(data, size, 1, f);
  fclose(f);

  return ret;
}

void inputControllerDebug(int indx) {
  int val = MaxPayne_InputControl_getButton(&sm_control[indx], 0);
  if (val > 0) {
    debugPrintf("InputController %d: MaxPayne_InputControl_getButton returned %d\n", indx, val);
  }
}

// 0, 5, 6: XBOX 360
// 4: MogaPocket
// 7: MogaPro
// 8: PS3
// 9: IOSExtended
// 10: IOSSimple
int WarGamepad_GetGamepadType(int padnum) {
  if (padnum != 0) {
    return 0; // only support one gamepad
  }

  return 8; 
}

uint32_t WarGamepad_GetGamepadButtons(int padnum) {
	if (!joystick_initialized) {
		init_joystick();
	}

	if (!joystick) {
		return 0;
	}

	SDL_JoystickUpdate();
	
	uint32_t mask = 0;
	
	// R36S button mapping to SDL joystick buttons
  if (SDL_JoystickGetButton(joystick, 0))  // A button
    mask |= BTN_A;
  if (SDL_JoystickGetButton(joystick, 1))  // B button
    mask |= BTN_B;
  if (SDL_JoystickGetButton(joystick, 2))  // X button
    mask |= BTN_X;
  if (SDL_JoystickGetButton(joystick, 3))  // Y button
    mask |= BTN_Y;
  if (SDL_JoystickGetButton(joystick, 13)) // Start button
    mask |= BTN_START;
  if (SDL_JoystickGetButton(joystick, 12)) // Select/Guide button
    mask |= BTN_BACK;
  if (SDL_JoystickGetButton(joystick, 4))  // L1
    mask |= BTN_L1;
  if (SDL_JoystickGetButton(joystick, 5)) { // R1
    mask |= BTN_R1;
    r1_pressed = 1;
  } else {
    // R1 not pressed; reset r1_pressed state
    r1_pressed = 0;
  }
  if (SDL_JoystickGetButton(joystick, 6)) // L2
    l2_axis = 1.0f; // treat as fully pressed
  else
    l2_axis = 0.0f;
  if (SDL_JoystickGetButton(joystick, 7)) // R2
    r2_axis = 1.0f; // treat as fully pressed
  else
    r2_axis = 0.0f;
  if (SDL_JoystickGetButton(joystick, 8))  // D-pad up
    mask |= BTN_DPAD_UP;
  if (SDL_JoystickGetButton(joystick, 9))  // D-pad down
    mask |= BTN_DPAD_DOWN;
  if (SDL_JoystickGetButton(joystick, 10)) // D-pad left
    mask |= BTN_DPAD_LEFT;
  if (SDL_JoystickGetButton(joystick, 11)) // D-pad right
    mask |= BTN_DPAD_RIGHT;
    
	return mask;
}


float WarGamepad_GetGamepadAxis(int padnum, int axis) {
	if (!joystick_initialized) {
		init_joystick();
	}

	if (!joystick) {
		return 0.0f;
	}

	SDL_JoystickUpdate();
	
	if (SDL_JoystickNumAxes(joystick) <= axis) {
    if (axis == 4 || axis == 5) {
      // L2/R2 analog triggers not present; return last known value
      return (axis == 4) ? l2_axis : r2_axis;
    }

		return 0.0f;
	}
	
	float value = 0.0f;
	Sint16 raw_value = SDL_JoystickGetAxis(joystick, axis);
	
	// Convert from Sint16 (-32768 to 32767) to float (-1.0 to 1.0)
	value = raw_value / 32767.0f;
	
	// Apply deadzone
	if (fabs(value) > 0.15f)
		return value;
		
	return 0.0f;
}

int MaxPayne_ConfiguredInput_readCrouch(void *this) {
  static int prev = 0;
  static int latch = 0;
  // crouch is control #5
  const int new = MaxPayne_InputControl_getButton(&sm_control[5], 0);
  if (prev != new) {
    prev = new;
    if (new) latch = !latch;
  }
  return latch;
}

int MaxPayne_ConfiguredInput_readShoot(void *this) {
  //debugPrintf("MaxPayne_ConfiguredInput_readShoot called\n");
  // MaxPayne_InputControl_getButton(&sm_control[2], 0); // only makes it shoot once
  return r1_pressed; // shoot while R1 is held

}

int GetAndroidCurrentLanguage(void) {
  debugPrintf("GetAndroidCurrentLanguage: returning %d\n", config.language);
  // this will be loaded from config.txt; cap it
  if (config.language < 0 || config.language > 6)
    config.language = 0; // english
  return config.language;
}

void SetAndroidCurrentLanguage(int lang) {
  debugPrintf("SetAndroidCurrentLanguage: lang=%d\n", lang);
  if (config.language != lang) {
    // changed; save config
    config.language = lang;
    write_config(CONFIG_NAME);
  }
}

static int (* R_File_loadArchives)(void *this);
static void (* R_File_unloadArchives)(void *this);
static void (* R_File_enablePriorityArchive)(void *this, const char *arc);

int R_File_setFileSystemRoot(void *this, const char *root) {
  debugPrintf("R_File_setFileSystemRoot: %s\n", root ? root : "NULL");
  // root appears to be unused?
  R_File_unloadArchives(this);
  const int res = R_File_loadArchives(this);
  R_File_enablePriorityArchive(this, config.mod_file);
  return res;
}

int X_DetailLevel_getCharacterShadows(void) {
  return config.character_shadows;
}

int X_DetailLevel_getDropHighestLOD(void) {
  return config.drop_highest_lod;
}

float X_DetailLevel_getDecalLimitMultiplier(void) {
  return config.decal_limit;
}

float X_DetailLevel_getDebrisProjectileLimitMultiplier(void) {
  return config.debris_limit;
}

int64_t UseBloom(void) {
  return config.use_bloom;
}

// JNI environment stub for ARM64 Linux
// This function should return a JNI environment pointer
// For our wrapper, we'll return a fake pointer that won't be used
void *NVThreadGetCurrentJNIEnv(void) {
  // Return a pointer to fake TLS storage
  // This should be safe since we don't actually use JNI
  static int call_count = 0;
  call_count++;
  
  // Log occasionally to show it's being called
  if (call_count <= 5 || call_count % 1000 == 0) {
    debugPrintf("NVThreadGetCurrentJNIEnv called (%d times), returning fake JNI env\n", call_count);
  }
  
  return &fake_tls[0];
}

void patch_game(void) {
  // No platform-specific input initialization needed for ARM64 Linux
  debugPrintf("patch_game: Starting game patching\n");

  // make it crash in an obvious location when it calls JNI methods
  debugPrintf("patch_game: Hooking JNI method\n");
  safe_hook_arm64("_Z24NVThreadGetCurrentJNIEnvv", (uintptr_t)NVThreadGetCurrentJNIEnv);

  debugPrintf("patch_game: Hooking C++ runtime\n");
  safe_hook_arm64("__cxa_throw", (uintptr_t)&__cxa_throw);
  safe_hook_arm64("__cxa_guard_acquire", (uintptr_t)&__cxa_guard_acquire);
  safe_hook_arm64("__cxa_guard_release", (uintptr_t)&__cxa_guard_release);

  debugPrintf("patch_game: Hooking thread launch\n");
  safe_hook_arm64("_Z15OS_ThreadLaunchPFjPvES_jPKci16OSThreadPriority", (uintptr_t)OS_ThreadLaunch);

  // used to check some flags
  hook_arm64(so_find_addr("_Z20OS_ServiceAppCommandPKcS0_"), (uintptr_t)ret0);
  hook_arm64(so_find_addr("_Z23OS_ServiceAppCommandIntPKci"), (uintptr_t)ret0);
  // this is checked on startup
  hook_arm64(so_find_addr("_Z25OS_ServiceIsWifiAvailablev"), (uintptr_t)ret0);
  hook_arm64(so_find_addr("_Z28OS_ServiceIsNetworkAvailablev"), (uintptr_t)ret0);
  // don't bother opening links
  hook_arm64(so_find_addr("_Z18OS_ServiceOpenLinkPKc"), (uintptr_t)ret0);

  // don't have movie playback yet
  hook_arm64(so_find_addr("_Z12OS_MoviePlayPKcbbf"), (uintptr_t)ret0);
  hook_arm64(so_find_addr("_Z12OS_MovieStopv"), (uintptr_t)ret0);
  hook_arm64(so_find_addr("_Z20OS_MovieSetSkippableb"), (uintptr_t)ret0);
  hook_arm64(so_find_addr("_Z17OS_MovieTextScalei"), (uintptr_t)ret0);
  hook_arm64(so_find_addr("_Z17OS_MovieIsPlayingPi"), (uintptr_t)ret0);
  hook_arm64(so_find_addr("_Z20OS_MoviePlayinWindowPKciiiibbf"), (uintptr_t)ret0);

  hook_arm64(so_find_addr("_Z17OS_ScreenGetWidthv"), (uintptr_t)OS_ScreenGetWidth);
  hook_arm64(so_find_addr("_Z18OS_ScreenGetHeightv"), (uintptr_t)OS_ScreenGetHeight);

  hook_arm64(so_find_addr("_Z9NvAPKOpenPKc"), (uintptr_t)NvAPKOpen);

  hook_arm64(so_find_addr("_Z13ProcessEventsb"), (uintptr_t)ProcessEvents);

  // both set and get are called, remember the language that it sets
  hook_arm64(so_find_addr("_Z25GetAndroidCurrentLanguagev"), (uintptr_t)GetAndroidCurrentLanguage);
  hook_arm64(so_find_addr("_Z25SetAndroidCurrentLanguagei"), (uintptr_t)SetAndroidCurrentLanguage);

  hook_arm64(so_find_addr("_Z14AND_DeviceTypev"), (uintptr_t)AND_DeviceType);
  hook_arm64(so_find_addr("_Z16AND_DeviceLocalev"), (uintptr_t)AND_DeviceLocale);
  hook_arm64(so_find_addr("_Z20AND_SystemInitializev"), (uintptr_t)AND_SystemInitialize);
  hook_arm64(so_find_addr("_Z21AND_ScreenSetWakeLockb"), (uintptr_t)ret0);
  hook_arm64(so_find_addr("_Z22AND_FileGetArchiveName13OSFileArchive"), (uintptr_t)OS_FileGetArchiveName);

  hook_arm64(so_find_addr("_Z26ReadDataFromPrivateStoragePKcRPcRi"), (uintptr_t)ReadDataFromPrivateStorage);
  hook_arm64(so_find_addr("_Z25WriteDataToPrivateStoragePKcS0_i"), (uintptr_t)WriteDataToPrivateStorage);

  hook_arm64(so_find_addr("_Z25WarGamepad_GetGamepadTypei"), (uintptr_t)WarGamepad_GetGamepadType);
  hook_arm64(so_find_addr("_Z28WarGamepad_GetGamepadButtonsi"), (uintptr_t)WarGamepad_GetGamepadButtons);
  hook_arm64(so_find_addr("_Z25WarGamepad_GetGamepadAxisii"), (uintptr_t)WarGamepad_GetGamepadAxis);

  // TODO implement these once we figure out how to do it with R36S (it supports vibrate if hardware hooked up)
  hook_arm64(so_find_addr("_Z12VibratePhonei"), (uintptr_t)ret0);
  hook_arm64(so_find_addr("_Z14Mobile_Vibratei"), (uintptr_t)ret0);

  hook_arm64(so_find_addr("_Z15ExitAndroidGamev"), (uintptr_t)ExitAndroidGame);

  // hook detail level getters to our own settings
  hook_arm64(so_find_addr("_ZN13X_DetailLevel19getCharacterShadowsEv"), (uintptr_t)X_DetailLevel_getCharacterShadows);
  hook_arm64(so_find_addr("_ZN13X_DetailLevel34getDebrisProjectileLimitMultiplierEv"), (uintptr_t)X_DetailLevel_getDebrisProjectileLimitMultiplier);
  hook_arm64(so_find_addr("_ZN13X_DetailLevel23getDecalLimitMultiplierEv"), (uintptr_t)X_DetailLevel_getDecalLimitMultiplier);
  hook_arm64(so_find_addr("_ZN13X_DetailLevel13dropHighesLODEv"), (uintptr_t)X_DetailLevel_getDropHighestLOD);

  // force bloom to our config value
  hook_arm64(so_find_addr("_Z8UseBloomv"), (uintptr_t)UseBloom);

  // dummy out the weapon menu arrow drawer if it's disabled
  if (!config.show_weapon_menu)
    hook_arm64(so_find_addr("_ZN12WeaponSwiper4DrawEv"), (uintptr_t)ret0);

  // crouch toggle
  if (config.crouch_toggle) {
    sm_control = (void *)so_find_addr_rx("_ZN24MaxPayne_ConfiguredInput10sm_controlE");
    MaxPayne_InputControl_getButton = (void *)so_find_addr_rx("_ZNK21MaxPayne_InputControl9getButtonEi");
    hook_arm64(so_find_addr("_ZNK24MaxPayne_ConfiguredInput10readCrouchEv"), (uintptr_t)MaxPayne_ConfiguredInput_readCrouch);
  }

  // for some reason shooting wont work unless we patch it
  hook_arm64(so_find_addr("_ZNK24MaxPayne_ConfiguredInput9readShootEv"), (uintptr_t)MaxPayne_ConfiguredInput_readShoot); 

  // if mod file is enabled, hook into R_File::setFileSystemRoot to set the mod as the priority archive
  // before R_File::loadArchives is called
  if (config.mod_file[0]) {
    R_File_unloadArchives = (void *)so_find_addr_rx("_ZN6R_File14unloadArchivesEv");
    R_File_loadArchives = (void *)so_find_addr_rx("_ZN6R_File12loadArchivesEv");
    R_File_enablePriorityArchive = (void *)so_find_addr_rx("_ZN6R_File21enablePriorityArchiveEPKc");
    hook_arm64(so_find_addr("_ZN6R_File17setFileSystemRootEPKc"), (uintptr_t)R_File_setFileSystemRoot);
  }

  // vars used in AND_SystemInitialize
  deviceChip = (int *)so_find_addr_rx("deviceChip");
  deviceForm = (int *)so_find_addr_rx("deviceForm");
  definedDevice = (int *)so_find_addr_rx("definedDevice");
}

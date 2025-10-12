#!/bin/bash

XDG_DATA_HOME=${XDG_DATA_HOME:-$HOME/.local/share}

if [ -d "/opt/system/Tools/PortMaster/" ]; then
  controlfolder="/opt/system/Tools/PortMaster"
elif [ -d "/opt/tools/PortMaster/" ]; then
  controlfolder="/opt/tools/PortMaster"
elif [ -d "$XDG_DATA_HOME/PortMaster/" ]; then
  controlfolder="$XDG_DATA_HOME/PortMaster"
else
  controlfolder="/roms/ports/PortMaster"
fi

source $controlfolder/control.txt

#export PORT_32BIT="Y" # If using a 32 bit port
[ -f "${controlfolder}/mod_${CFW_NAME}.txt" ] && source "${controlfolder}/mod_${CFW_NAME}.txt"

get_controls

GAMEDIR=/$directory/ports/maxpayne
CONFDIR="$GAMEDIR/conf/"

mkdir -p "$GAMEDIR/conf"

> "$GAMEDIR/log.txt" && exec > >(tee "$GAMEDIR/log.txt") 2>&1

unpack_apk() {
    pm_message "Unpacking APK file..." 
    # Unpack APK

    mkdir -p "$GAMEDIR/.apktemp"
    unzip -o "$GAMEDIR/*.apk" -d "$GAMEDIR/.apktemp"

    # Move lib/arm64-v8a contents to GAMEDIR
    rsync -a "$GAMEDIR/.apktemp/lib/arm64-v8a/" "$GAMEDIR/"

    # Move assets
    rsync -a "$GAMEDIR/.apktemp/assets/" "$GAMEDIR/gamedata/" && rm "$GAMEDIR/"*.apk

    rm -rf "$GAMEDIR/.apktemp"
    
    
    pm_message "Unpacking APK file... done"
}

unpack_obb() {
    pm_message "Unpacking OBB file..." 
    mkdir -p "$GAMEDIR/gamedata"
    unzip -o "$GAMEDIR/*.obb" -d "$GAMEDIR/.obbtemp" && rm "$GAMEDIR/"*.obb
    rsync -a "$GAMEDIR/.obbtemp/" "$GAMEDIR/gamedata/"
    rm -rf "$GAMEDIR/.obbtemp"
    
    pm_message "Unpacking OBB file... done"
}

handle_patch_files() {
    pm_message "Handling patch files..." 
    if [ -d "$GAMEDIR/patch" ]; then
        rsync -a "$GAMEDIR/patch/" "$GAMEDIR/gamedata/"
    fi
    pm_message "Handling patch files... done"
}

apk_count=$(ls -1 "$GAMEDIR/"*.apk 2>/dev/null | wc -l)
if [ "$apk_count" -gt 0 ]; then
    unpack_apk
fi

obb_count=$(ls -1 "$GAMEDIR/"*.obb 2>/dev/null | wc -l)
if [ "$obb_count" -gt 0 ]; then
    unpack_obb
    handle_patch_files
fi

if [ ! -f "$GAMEDIR/libMaxPayne.so" ]; then
    pm_message "No libMaxPayne.so found! Please place the .apk file in the game folder ($GAMEDIR) and restart the game."
    sleep 10s
    exit 1
fi

if [ ! -d "$GAMEDIR/gamedata" ]; then
    pm_message "No gamedata found! Please place the .obb file in the game folder ($GAMEDIR) and restart the game."
    sleep 10s
    exit 1
fi

cd $GAMEDIR

export SDL_GAMECONTROLLERCONFIG="$sdl_controllerconfig"

pm_message "Starting Max Payne... This can take a few seconds."

# Remove debug log
rm -f "debug.log"

touch "debug.log"
tail -f "debug.log" | while read LOGLINE; do
   pm_message "$LOGLINE"
done &

./maxpayne_arm64

pm_finish

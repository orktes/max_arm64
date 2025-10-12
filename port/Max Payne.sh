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

cd $GAMEDIR

export PATCHER_FILE="$GAMEDIR/tools/patchscript"
export PATCHER_TIME="10 to 20 minutes"

apk_count=$(ls -1 "$GAMEDIR/"*.apk 2>/dev/null | wc -l)
obb_count=$(ls -1 "$GAMEDIR/"*.obb 2>/dev/null | wc -l)

if ! [ "$apk_count" -eq 0 ] || ! [ "$obb_count" -eq 0 ]; then
  pm_message "APK or OBB file found. Running patchscript..."
   if [ -f "$controlfolder/utils/patcher.txt" ]; then
    $ESUDO chmod a+x "$GAMEDIR/tools/patchscript"
    source "$controlfolder/utils/patcher.txt"
    $ESUDO kill -9 $(pidof gptokeyb)
  else
    pm_message "This port requires the latest version of PortMaster."
    exit 0
  fi 
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


export SDL_GAMECONTROLLERCONFIG="$sdl_controllerconfig"

$GPTOKEYB "maxpayne_arm64" & pm_platform_helper "$GAMEDIR/maxpayne_arm64"

pm_message "Starting Max Payne... This can take a few seconds."

# Remove debug log
rm -f "debug.log"

touch "debug.log"
tail -f "debug.log" | while read LOGLINE; do
   pm_message "$LOGLINE"
done &

./maxpayne_arm64 

pm_finish

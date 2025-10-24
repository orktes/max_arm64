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
export PATCHER_TIME="5 to 10 minutes"

last_patch_version=$(cat "$GAMEDIR/LAST_PATCH_VERSION.txt" 2>/dev/null || echo "")
current_version=$(cat "$GAMEDIR/VERSION.txt" 2>/dev/null || echo "")

if [ "$last_patch_version" != "$current_version" ]; then
  pm_message "Handling patch files..." 
  rsync -a "$GAMEDIR/patch/" "$GAMEDIR/gamedata/"
  pm_message "Handling patch files... done"
  echo "$current_version" > "$GAMEDIR/LAST_PATCH_VERSION.txt"
fi

export SDL_GAMECONTROLLERCONFIG="$sdl_controllerconfig"

source $controlfolder/runtimes/"love_11.5"/love.txt

pm_message "Starting Launcher"

# Remove debug log
rm -f "debug.log"

$GPTOKEYB "$LOVE_GPTK" &
launcherGPTOKEYBPid=$!

pm_platform_helper "$LOVE_BINARY"


function start_maxpayne {
  $ESUDO kill -9 $launcherGPTOKEYBPid
  $GPTOKEYB "maxpayne_arm64" & 
  pm_platform_helper "$GAMEDIR/maxpayne_arm64"

  pm_message "Starting Max Payne"
  ./maxpayne_arm64 
}


$LOVE_RUN "$GAMEDIR/launcher.love" && start_maxpayne

pm_finish

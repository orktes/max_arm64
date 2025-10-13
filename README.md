# Max Payne for R36S

Port of Max Payne for the R36S game console.

![Devices](https://github.com/user-attachments/assets/541a65db-31b9-483f-a5dd-63b76a830da8)


## Installing

1. Install Max Payne for R36S by either copying the [zip](https://github.com/orktes/max_r36s/releases/latest/download/maxpayne_arm64.zip) into PortMasters autoinstall directory (/roms/tools/PortMaster/autoinstall/ on ArkOS) and starting PortMaster or by extracting and copying (Max Payne.sh and maxpayne directory) into `/roms/ports/` of your SD card.
2. Install the game (Max Payne Mobile 1.7 or newer so that it includes Arm64 libs) on your Android device from Google Play or other legit source.
3. Retrieve the APK and OBB files from your Android device. You can use a apk extractor app or connect your device to a computer and copy the files directly. Many ways to achieve this so just google it.
4. Copy the APK and OBB files over to the r36s console (SD card or over SSH). Place them in the `/roms/ports/maxpayne/` directory.
5. Launch Max Payne from the Ports menu. The first time you launch the game, it will unpack the APK and OBB files. This may take a few minutes.
6. After unpacking, the game will start loading. Enjoy playing Max Payne on your R36S!

## Controls

| Button     | Description      |
|------------|------------------|
| A          | Jump             |
| B          | Crouch           |
| X          | Reload           |
| Y          | Use/Zoom         |
| L1         | Bullet time      |
| R1         | Shoot            |
| L2         | Previous Weapon  |
| R2         | Next Weapon      |
| D-Up       | Pain Killer      |
| D-Down     | Reload           |
| D-Left     | Previous Weapon  |
| D-Right    | Next Weapon      |
| Select+R1  | Quicksave        |
| Start      | Menu             |
| Left Stick | Move             |
| Right Stick| Look/Aim         |

![Menu_bg_Controls](https://github.com/user-attachments/assets/a818d8d9-3787-4108-b043-a3a3db2c7eb5)

## Known Issues
- On non 4:3 or widescreen devices (devices like R36S Max) the game screen is offset to the bottom of the screen. This is a temp hack to get 4:3 mode out of the door. Enabling `force_widescreen` in config.txt will center the viewport but will make the view 16:9. 
- Changing shooting from R1 to anything else will cause shooting to not work. This is due to a small hack in the input system to make R1 work as shoot button. Changing other buttons is however possible. 

## Building
1. $ cmake .
2. make archive
3. See archive in `archive/` folder.

## Credits

This port is largely based on the work of [Andy Nguyen](https://github.com/fgsfdsfgs) who ported the game to [PSVita](https://github.com/fgsfdsfgs/max_vita) and [Nintendo Switch](https://github.com/fgsfdsfgs/max_nx) so lots of credit needs to go there.

Special thanks to [Dia2809](https://github.com/Dia2809) for contributing a fix to the framebuffer fallback so that the port works on CFWs without DRM+GDM and otherwise helping me out on the [PortMaster](https://portmaster.games/) packing of the port.

## License
This project is licensed under the MIT License. See the [LICENSE](LICENSE) file for details

## Legal

This project has no direct affiliation with Take-Two Interactive Software, Inc., Rockstar Games, Inc. or Remedy Entertainment Oyj and/or the "Max Payne" brand. "Max Payne" is a Take-Two Interactive Software, Inc. brand. All Rights Reserved.

No assets or program code from the original game or its Android port are included in this project. We do not condone piracy in any way, shape or form and encourage users to legally own the original game.

The video game "Max Payne" is copyright © 2001 Remedy Entertainment Oyj and/or Take-Two Interactive Software, Inc. The Android version, "Max Payne Mobile", is copyright © 2012 Rockstar Games, Inc. and/or Take-Two Interactive Software, Inc. "Max Payne" and "Max Payne Mobile" are trademarks of their respective owners. All Rights Reserved.

Unless specified otherwise, the source code provided in this repository is licenced under the MIT License. Please see the accompanying LICENSE file.

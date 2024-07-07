# esp32-sid-register-player-rlc

This repo contains a register based sid player for the Aithinker ESP32 Audio board.

https://docs.ai-thinker.com/en/esp32-audio-kit

The features the ESP32-A1S module, which has a built-in I2S based audio DAC.

This project uses the board with the Arduino environment provided for by the PlatformIO plugin with Visual Studio Code. 

Run length encoded register dump files of SID tunes can be stored on an SD card in a folder called "DMP3". Use the KEY5 button to play next song.

Original ReSID emulation version 0.16 provided by Dag Lem.
https://github.com/daglem/reSID/

Audio tools/driver for the ESP32-A1S module by Phil Schatzmann.
https://github.com/pschatzmann/arduino-audio-driver
https://github.com/pschatzmann/arduino-audio-tools

Ported to work on the ESP32 by beachviking.

This solution works reasonably well with many SID tunes.
Only PSIDs are supported by the code.

This project depends on the following libraries to work:
https://github.com/pschatzmann/arduino-audio-tools
https://github.com/pschatzmann/arduino-audio-driver
https://github.com/greiman/SdFat
https://github.com/beachviking/arduino-sid-tools
 
This solution was developped using PlatformIO, my platformio.ini file is included to show my setup. You probably need to tweak this one to work for you.
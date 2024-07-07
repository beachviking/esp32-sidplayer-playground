# esp32-sidplayer-aithinker

This repo contains a sid player for the Aithinker ESP32 Audio board.

https://docs.ai-thinker.com/en/esp32-audio-kit

The features the ESP32-A1S module, which has a built-in I2S based audio DAC.

This project uses the board with the Arduino environment provided for by the PlatformIO plugin with Visual Studio Code. 

This project features a 6502 emulator along with a SID chip emulator. 

SID tunes can be stored on an SD card in a folder called "SID". Use the buttons to navigate tunes and songs.

Original ReSID emulation version 0.16 provided by Dag Lem.
https://github.com/daglem/reSID/

6502 emulation core from the "siddump tool" in the public domain.
https://github.com/cadaver/siddump

Audio tools/driver for the ESP32-A1S module by Phil Schatzmann.
https://github.com/pschatzmann/arduino-audio-driver
https://github.com/pschatzmann/arduino-audio-tools

Ported to work on the ESP32 by beachviking.

This solution works reasonably well with many SID tunes.
Only PSIDs are supported by the code.

The reason for this is that the 6502 code is only executed at the interrupt timeout period.

After the 6502 code has executed, all the SID samples are generated for the same time window, i.e sequentially. 

No effort into making the 6502 and the SID work more in parallel has been done, which
is required for RSIDs to work. 

Some tunes seem to play back incorrectly, have not looked further into this yet.

This project depends on the following libraries to work:
https://github.com/pschatzmann/arduino-audio-tools
https://github.com/pschatzmann/arduino-audio-driver
https://github.com/greiman/SdFat
https://github.com/beachviking/arduino-sid-tools
 
This solution was developped using PlatformIO, my platformio.ini file is included to show my setup. You probably need to tweak this one to work for you.
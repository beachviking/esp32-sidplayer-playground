# esp32-sidplayer-makerfab

This repo contains a sid player for the Makerfab ESP32 Audio Player.

https://www.makerfabs.com/eps32-audio-player.html

The audio board features the UDA1334ATS Audio I2S DAC. This solution "defaults" to using MicroPython, however it can be used with the Arduino environment as well.

This project uses the board with the Arduino environment provided for by the PlatformIO solution. 

To program the board, you need to disconnect the Audio shield. In addition, the auto programming feature is not supported, so you need to
hold down the boot button and then press the reset button shortly to force the board into reflash mode. Do this while PlatformIO is "searching" for the board during the 
reflash process.

This project features a 6502 emulator along with a SID chip emulator. 
¨
SID tunes can be stored on an SD card in a folder called "SID". Use the buttons to navigate tunes and songs.

Original ReSID emulation version 0.16 provided by Dag Lem.
https://github.com/daglem/reSID/

6502 emulation core from the "siddump tool" in the public domain.
https://github.com/cadaver/siddump

Ported to work on the ESP32 by beachviking.

This solution works reasonably well with many SID tunes.
Only PSIDs are supported by the code.

The reason for this is that the 6502 code is only executed at the interrupt timeout period.

After the 6502 code has executed, all the SID samples are generated for the same time window, i.e sequentially. 

No effort into making the 6502 and the SID work more in parallel has been done, which
is required for RSIDs to work. 

Some tunes seem to play back incorrectly, have not looked further into this yet.

This project depends on the following libraries to work:
https://github.com/adafruit/Adafruit-GFX-Library 
https://github.com/adafruit/Adafruit_SSD1306
https://github.com/greiman/SdFat
https://github.com/beachviking/arduino-sid-tools

This solution was developped using PlatformIO, my platformio.ini file is included to show my setup. You probably need to tweak this one to work for you.
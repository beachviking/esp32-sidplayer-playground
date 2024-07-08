/*
  main.cpp - application to show a simple register based 
  SID player for the ESP32 using an AI Thinker audio board.

  This code supports 2 file types:
  1. Regular sid register dumps, assumes default VBI timing is used. Code assumes SD card usage and that the files are stored in "/DMP".
  
  2. Enhanced sid register dumps, will also work with CIA timing usage. This is accomplished by that the frame period is also
  captured along with the sid registers and applied while the song is being played.  The timing values are captured at the end of the register
  dumps at the end of each frame. Remove comment around CIA_SUPPORT to enable.  Code assumes SD card usage and that the files are stored in "/DMP2".

  This scheme produces larger files, but you avoid having to emulate the 6502
  in your code.

  Created by beachviking, November 2, 2023.
  Original ReSID emulation provided by Dag Lem
  Ported to work on the ESP32 by beachviking
*/

#include <Arduino.h>
#include "AudioKitHAL.h"
#include "AudioTools.h"
#include "SdFat.h"
#include "SPI.h"
#include "SidTools.h"

// #define CIA_SUPPORT

#define PIN_AUDIO_KIT_SD_CARD_CS 13
#define PIN_AUDIO_KIT_SD_CARD_MISO 2
#define PIN_AUDIO_KIT_SD_CARD_MOSI 15
#define PIN_AUDIO_KIT_SD_CARD_CLK  14
#define PIN_AUDIO_KIT_NEXT_SONG_BUTTON 18

// Store error strings in flash to save RAM.
#define error(s) sd.errorHalt(&Serial, F(s))

// Try max SPI clock for an SD. Reduce SPI_CLOCK if errors occur.
#define SPI_CLOCK SD_SCK_MHZ(10)
#define SD_CONFIG SdSpiConfig(PIN_AUDIO_KIT_SD_CARD_CS, DEDICATED_SPI, SPI_CLOCK)


#ifdef CIA_SUPPORT
const char *startFilePath="/DMP2/";
#else
const char *startFilePath="/DMP/";
#endif

SdFat32 sd;
File32 myFile;
File32 myDir;

AudioKit kit;
AudioActions actions;

SID sid;
SidRegPlayer player(&sid);
SidRegPlayerConfig sid_cfg;

char buffer[27];
char oldbuffer[27];

const int BUFFER_SIZE = 4 * 882;  // needs to be at least (2 CH * 2 BYTES * SAMPLERATE/PAL_CLOCK). Ex. 4 * (44100/50)
uint8_t audiobuffer[BUFFER_SIZE];

void getNext() 
{
  myFile.close();
  Serial.println("Getting next song...");
  if (!myFile.openNext(&myDir, O_RDONLY))
  {
    if (myDir.getError())
      return;
      
    myDir.rewind();
  }

  myFile.printName(&Serial);
  Serial.println();
}

void nextSong(bool, int, void*) {
   getNext();
}

void setup() {
  LOGLEVEL_AUDIOKIT = AudioKitInfo;   
  Serial.begin(115200);

  SPI.begin(PIN_AUDIO_KIT_SD_CARD_CLK, PIN_AUDIO_KIT_SD_CARD_MISO, PIN_AUDIO_KIT_SD_CARD_MOSI, PIN_AUDIO_KIT_SD_CARD_CS);

  // open in write mode
  auto cfg = kit.defaultConfig(audiokit::AudioOutput);
  //cfg.sample_rate = audio_hal_iface_samples_t::AUDIO_HAL_22K_SAMPLES;
  kit.begin(cfg);

  player.setDefaultConfig(&sid_cfg);
  sid_cfg.samplerate = cfg.sampleRate();
  player.begin(&sid_cfg);
  memset(buffer,0,sizeof(buffer));
  memset(oldbuffer,0,sizeof(oldbuffer)); 

  // Initialize the SD.
  if (!sd.begin(SD_CONFIG)) {
    sd.initErrorHalt(&Serial);
    return;
  }

  // Open root directory
  if (!myDir.open(startFilePath)) {
    error("failed to open directory");
    while (1 == 1);
  }

 // setup button actions
  actions.add(PIN_AUDIO_KIT_NEXT_SONG_BUTTON, nextSong); 
}

void loop() {
  static long m = micros();
  if (micros()-m < player.getFramePeriod()) return;
  m = micros();

  if (!myFile.available()) 
    getNext();

  // Update the 25 sid registers, done every frameperiod us
  for(int i=0;i<25;i++) {
    if(buffer[i] == oldbuffer[i])
      continue;

    player.setreg(i, buffer[i]);
    oldbuffer[i] = buffer[i];                  

  }

  // CIA_DC05 << 8) + CIA_DC04
#ifdef CIA_SUPPORT
  player.setFramePeriod((buffer[25] << 8) + (buffer[26]));
#endif

  // Render samples for this frame, to fill framePeriod worth of data 
  // to match current sample rate
  size_t l = player.read(audiobuffer, player.getSamplesPerFrame());
  kit.write(audiobuffer, l);

  // Get next values for sid registers
#ifdef CIA_SUPPORT
  myFile.read(buffer, 27);
#else
  myFile.read(buffer, 25);
#endif

  // check for buttons
  actions.processActions();
}

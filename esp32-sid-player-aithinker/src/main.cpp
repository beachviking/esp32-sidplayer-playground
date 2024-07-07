/*
  main.cpp - application to show a SID player with
  6502 emulation for the ESP32 using an AI Thinker audio board. 
  
  Audio driver provided for by the arduino-audio-driver and arduino-audio-tools
  libraries by Phil Schatzmann at https://github.com/pschatzmann

  Stitched together for ESP32 by beachviking, February 2, 2024.

  Original ReSID emulation provided by Dag Lem
  Ported to work on the ESP32 by beachviking

  6502 emulation core from the siddump tool in the public domain.
  Ported to work on the ESP32 by beachviking 

*/

#include "main.h"

const char *startFilePath="/SID/";

// Store error strings in flash to save RAM.
#define error(s) sd.errorHalt(&Serial, F(s))

// Try max SPI clock for an SD. Reduce SPI_CLOCK if errors occur.
#define SPI_CLOCK SD_SCK_MHZ(10)
#define SD_CONFIG SdSpiConfig(PIN_AUDIO_KIT_SD_CARD_CS, DEDICATED_SPI, SPI_CLOCK)

SdFat32 sd;
File32 myFile;
File32 myDir;

AudioBoardStream kit(AudioKitEs8388V1);

SID sid;
SidPlayer player(&sid);

const int BUFFER_SIZE = 4 * 2048;  // needs to be at least (2 CH * 2 BYTES * SAMPLERATE/PAL_CLOCK). Ex. 4 * (44100/50)
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
    myFile.openNext(&myDir, O_RDONLY);
  }

  myFile.printName(&Serial);
  Serial.println();

  player.load(&myFile);
  myFile.close();
  player.play();
}

void nextSong(bool, int, void*) {
   getNext();
}

void nextTune(bool, int, void*) {
  player.playNext();
}

void setup() {
  Serial.begin(115200);
  AudioLogger::instance().begin(Serial, AudioLogger::Info);

  SPI.begin(PIN_AUDIO_KIT_SD_CARD_CLK,  \
            PIN_AUDIO_KIT_SD_CARD_MISO, \
            PIN_AUDIO_KIT_SD_CARD_MOSI, \
            PIN_AUDIO_KIT_SD_CARD_CS);


  // open in write mode
  auto cfg = kit.defaultConfig(TX_MODE);
  kit.begin(cfg);

  player.setSampleRate(cfg.sample_rate);

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

  getNext();
  
  // setup button actions
  kit.addDefaultActions();
  kit.addAction(PIN_AUDIO_KIT_NEXT_SONG_BUTTON, nextSong);
  kit.addAction(PIN_AUDIO_KIT_NEXT_TUNE_BUTTON, nextTune);
}

void loop() {

  // check for buttons
  kit.processActions();

  if (!player.isPlaying())
    return;
  
  if (player.tick())
    return;

  // CPU simulation step ok
  if (player.getSamplesPerFrame() > BUFFER_SIZE >> 2) {
    printf("Buffer overruns with current samplerate, increase audio buffer >= %ld bytes to fix!\n", player.getSamplesPerFrame() * 4);
    player.playNext();
    return;
  }

  // Render samples for this frame
  size_t l = player.read(audiobuffer);
  kit.write(audiobuffer, l);
}
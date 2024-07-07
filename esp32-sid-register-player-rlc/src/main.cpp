/*
  main.cpp - application to show a simple register based 
  SID player for the ESP32 using an AI Thinker audio board. 
  Sid files have been encoded using a simple run-length encoding scheme.
  Altered sid registers have been dumped per frame period using a modified
  version of the sidddump tool which is publicly available here:

  https://github.com/cadaver/siddump

  Frame period is set by either the VBI or CIA settings.
  This scheme produces larger files, but you avoid having to emulate the 6502
  in your code.

  Created by beachviking, November 2, 2023.
  Original ReSID emulation provided by Dag Lem
  ESP32 audio driver by Phil Schatzmann
  Ported to work on the ESP32 by beachviking
*/

#include "main.h"

// Store error strings in flash to save RAM.
#define error(s) sd.errorHalt(&Serial, F(s))

// Try max SPI clock for an SD. Reduce SPI_CLOCK if errors occur.
#define SPI_CLOCK SD_SCK_MHZ(10)
#define SD_CONFIG SdSpiConfig(PIN_AUDIO_KIT_SD_CARD_CS, DEDICATED_SPI, SPI_CLOCK)

const char *startFilePath="/DMP3/";

SdFat32 sd;
File32 myFile;
File32 myDir;

AudioKit kit;
AudioActions actions;

SID sid;
SidRegPlayer player(&sid);
SidRegPlayerConfig sid_cfg;

const int bufferSize = 512;
char buffer[bufferSize];
int buf_idx = 0;

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

inline void readNextData() 
{
  if (!myFile.available()) 
    getNext();

  // first read number of registers that is to be updated, 1 byte value  
  myFile.read(buffer, 1);
  // then read number of value pairs(reg, val) that is to be updated
  myFile.read(&buffer[1], buffer[0]*2);
  buf_idx = 0;
}

inline char getNextValue(){
  static int currValue = 0;

  currValue = buffer[buf_idx];
  ++buf_idx;
  return(currValue);
}

void updateCurrentFrame() {
  static int reg = 0;
  static int reg_cnt = 0;
  static int val = 0;
  static char frame_period_lobyte = 0;
  static char frame_period_hibyte = 0;

  // update sid registers for current frame
  // get next data from sd card...
  readNextData();

  // get number of sid registers to update for this frame
  reg_cnt = getNextValue();

  // update changed sid registers, done for every frame
  for(int i=0;i<reg_cnt;i++) {
    reg = getNextValue(); // register num
    val = getNextValue(); // new val for reg

    if (reg < 25)
    {
      player.setreg(reg, val);
      continue;
    }

    if (reg == 25)
      frame_period_hibyte = val;

    if (reg == 26)
      frame_period_lobyte = val;
  }

  player.setFramePeriod((frame_period_hibyte << 8) + (frame_period_lobyte));
}


void setup() {
  LOGLEVEL_AUDIOKIT = AudioKitInfo;   
  Serial.begin(115200);

  SPI.begin(PIN_AUDIO_KIT_SD_CARD_CLK, PIN_AUDIO_KIT_SD_CARD_MISO, PIN_AUDIO_KIT_SD_CARD_MOSI, PIN_AUDIO_KIT_SD_CARD_CS);

  // open in write mode
  //auto cfg = kit.defaultConfig(audiokit::AudioOutput);
  auto cfg = kit.defaultConfig(audiokit::KitOutput);
  //cfg.sample_rate = audio_hal_iface_samples_t::AUDIO_HAL_22K_SAMPLES;
  kit.begin(cfg);

  player.setDefaultConfig(&sid_cfg);
  sid_cfg.samplerate = cfg.sampleRate();
  player.begin(&sid_cfg);
  memset(buffer,0,sizeof(buffer));

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
  // get and apply current updates to this audio frame
  updateCurrentFrame();

  // Render samples for this frame, to fill framePeriod worth of data 
  // to match current sample rate
  size_t l = player.read(audiobuffer, player.getSamplesPerFrame());
  kit.write(audiobuffer, l);

  // check for buttons
  actions.processActions();
}

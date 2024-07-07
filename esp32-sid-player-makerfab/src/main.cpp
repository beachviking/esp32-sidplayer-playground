/*
  main.cpp - application to show a SID player with
  6502 emulation for the ESP32 using the Makerfab ESP32/audio board combo.
  The audio board features the UDA1334ATS Audio I2S DAC.

  Created by beachviking, February 2, 2024.
  Original ReSID emulation provided by Dag Lem
  Ported to work on the ESP32 by beachviking

  6502 emulation core from the siddump tool in the public domain.
  Ported to work on the ESP32 by beachviking 
  
  This solution works reasonably well with many SID tunes.
  Only PSIDs are supported by the code.

  The reason for this is that the 6502 code is only executed at the interrupt timeout mark.
  After the 6502 code has executed, all the SID samples are generated for
  the same time window, i.e sequentially. 
  
  No effort into making the 6502 and the SID work more in parallel has been done, which
  is required for RSIDs to work. 

  Some tunes seem to play back incorrectly, have not looked further into this yet.

  Problem tunes:
  - Frankie goes to Hollywood
  - Some non-digi Arkanoid tunes, seems to play very slowly?
*/

#include "main.h"

#include "SdFat.h"
#include "SPI.h"
#include "SidTools.h"

const char *startFilePath="/SID/";

// Store error strings in flash to save RAM.
#define error(s) sd.errorHalt(&Serial, F(s))

// Try max SPI clock for an SD. Reduce SPI_CLOCK if errors occur.
#define SPI_CLOCK SD_SCK_MHZ(10)
#define SD_CONFIG SdSpiConfig(PIN_AUDIO_KIT_SD_CARD_CS, DEDICATED_SPI, SPI_CLOCK)

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
SdFat32 sd;
File32 myFile;
File32 myDir;
ButtonActions actions;
SID sid;
SidPlayer player(&sid);

//const int BUFFER_SIZE = 4 * 882;  // needs to be at least (2 CH * 2 BYTES * SAMPLERATE/PAL_CLOCK). Ex. 4 * (44100/50)
const int BUFFER_SIZE = 4 * 2048;  // needs to be at least (2 CH * 2 BYTES * SAMPLERATE/PAL_CLOCK). Ex. 4 * (44100/50)
uint8_t audiobuffer[BUFFER_SIZE];

// Function prototypes
void logoshow();
void initScreen();

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

  initScreen();

  SPI.begin(PIN_AUDIO_KIT_SD_CARD_CLK,  \
            PIN_AUDIO_KIT_SD_CARD_MISO, \
            PIN_AUDIO_KIT_SD_CARD_MOSI, \
            PIN_AUDIO_KIT_SD_CARD_CS);


  // start I2S & codec
  Serial.println("starting I2S...");
  i2s_driver_install(i2s_num, &i2s_config, 0, NULL);   //install and start i2s driver
  i2s_set_pin(i2s_num, &pin_config);

  player.setSampleRate(i2s_config.sample_rate);

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
  actions.add(PIN_AUDIO_KIT_NEXT_SONG_BUTTON, nextSong);
  actions.add(PIN_AUDIO_KIT_NEXT_TUNE_BUTTON, nextTune);
}

void loop() {
  // check for buttons
  actions.processActions();

  if (!player.isPlaying())
    return;

  if (player.tick())
    return;

  // CPU simulation step ok.
  // Render samples for this frame, to fill framePeriod worth of data 
  // to match current sample rate
  if (player.getSamplesPerFrame() > BUFFER_SIZE >> 2) {
    printf("Buffer overruns with current samplerate, increase audio buffer >= %ld bytes to fix!\n", player.getSamplesPerFrame() * 4);
    player.playNext();
    return;
  }

  size_t l = player.read(audiobuffer);
  size_t i2s_bytes_written;
  i2s_write(i2s_num, audiobuffer, l, &i2s_bytes_written, 100);

}


void initScreen() {
    //LCD
    Wire.begin(MAKEPYTHON_ESP32_SDA, MAKEPYTHON_ESP32_SCL);
    // SSD1306_SWITCHCAPVCC = generate display voltage from 3.3V internally
    if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C))
    { // Address 0x3C for 128x32
        Serial.println(F("SSD1306 allocation failed"));
        for (;;)
            ; // Don't proceed, loop forever
    }
    display.clearDisplay();
    logoshow();

}

void logoshow(void)
{
    display.clearDisplay();

    display.setTextSize(2);              // Normal 1:1 pixel scale
    display.setTextColor(SSD1306_WHITE); // Draw white text
    display.setCursor(0, 0);             // Start at top-left corner
    display.println(F("ESP32"));
    display.setCursor(0, 20); // Start at top-left corner
    display.println(F("SID"));
    display.setCursor(0, 40); // Start at top-left corner
    display.println(F("PLAYER V1"));
    display.display();
    delay(2000);
}

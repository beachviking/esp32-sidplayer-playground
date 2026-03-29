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
#include <math.h>

const char *startFilePath="/SID/";

// Oscilloscope display — runs on Core 0 so I2C never blocks audio on Core 1
static SemaphoreHandle_t xDisplaySemaphore;

// Shared frame data: per-voice samples (envelope-modulated) + register snapshot.
// Two frames of 128 samples are kept so the trigger can always find a rising edge
// in the first half and still have a full 128-sample window to display.
struct ScopeFrame {
    int16_t voice[3][256]; // rolling 2-frame buffer; newest 128 in [128..255]
    uint8_t regs[25];      // SID registers 0xD400-0xD418 (for silence detection)
};
static ScopeFrame scopeFrame;

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
void renderOscilloscope(const ScopeFrame& frame);
void displayTask(void* pvParam);

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

  // Launch oscilloscope display task on Core 0 (audio runs on Core 1)
  xDisplaySemaphore = xSemaphoreCreateBinary();
  xTaskCreatePinnedToCore(displayTask, "display", 4096, NULL, 1, NULL, 0);
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

  // Inline audio loop — replaces player.read() so we can capture per-voice samples.
  // voice_output(i) returns waveform * envelope (pre-filter), giving ADSR-accurate levels.
  {
    SID* sid_ptr = player.getSID();
    cycle_count dt = player.getDeltaT();
    long spf = player.getSamplesPerFrame();
    int16_t* ptr = (int16_t*)audiobuffer;
    int stride = (int)(spf / 128);
    if (stride < 1) stride = 1;

    // Shift previous frame into the first half; new samples fill the second half.
    // This gives the trigger search a full 128-sample lookahead window.
    for (int v = 0; v < 3; v++)
      memmove(scopeFrame.voice[v], scopeFrame.voice[v] + 128, 128 * sizeof(int16_t));

    for (long j = 0; j < spf; j++) {
      sid_ptr->clock(dt);
      int16_t sample = (int16_t)sid_ptr->output();
      *ptr++ = sample; // L
      *ptr++ = sample; // R

      // Stride-sample per-voice output into the second half of the rolling buffer
      int idx = (int)(j / stride);
      if (idx < 128) {
        for (int v = 0; v < 3; v++) {
          // voice_output() range is roughly ±(2047*255) ≈ ±521985 (21-bit signed).
          // Shift right by 4 to fit in int16_t with headroom.
          scopeFrame.voice[v][128 + idx] = (int16_t)(sid_ptr->voice_output(v) >> 4);
        }
      }
    }

    size_t l = (size_t)(spf * 4);
    size_t i2s_bytes_written;
    i2s_write(i2s_num, audiobuffer, l, &i2s_bytes_written, 100);
  }

  // Snapshot SID registers (still used for silence detection) and signal display task
  memcpy(scopeFrame.regs, mem + 0xD400, 25);
  xSemaphoreGive(xDisplaySemaphore);
}


// ---------------------------------------------------------------------------
// Oscilloscope renderer — called from displayTask() on Core 0.
// Draws three voice lanes using actual per-voice audio samples (waveform *
// envelope from reSID), so ADSR curves are visible in the waveform amplitude.
//
// Trigger: the first active voice is used as the sync reference. A rising
// midpoint-crossing is found in its first 128 samples; all three voices are
// then displayed starting at that same offset. This keeps waveforms stationary
// and gives a common time reference across all lanes.
//
// Display layout (64 px tall):
//   y=0..20   Voice 1  (21 px, center y=10)
//   y=21..41  Voice 2  (21 px, center y=31)
//   y=42..62  Voice 3  (21 px, center y=52)
//   y=63      (unused)
// ---------------------------------------------------------------------------
void renderOscilloscope(const ScopeFrame& frame) {
    display.clearDisplay();

    uint8_t vol = frame.regs[24] & 0x0F;

    // Find the first active voice to use as trigger reference
    int ref_v = -1;
    for (int v = 0; v < 3; v++) {
        uint8_t ctrl  = frame.regs[v * 7 + 4];
        bool    gate  = (ctrl & 0x01) != 0;
        uint8_t wform = (ctrl >> 4) & 0x0F;
        if (gate && wform != 0 && vol != 0) { ref_v = v; break; }
    }

    // Trigger: rising midpoint-crossing of the reference voice in the first half
    // of the rolling buffer. The second half always has a full 128-sample window
    // available after any trigger point found in [1..127].
    int trigger = 0;
    if (ref_v >= 0) {
        int16_t mn = 32767, mx = -32767;
        for (int i = 0; i < 128; i++) {
            if (frame.voice[ref_v][i] < mn) mn = frame.voice[ref_v][i];
            if (frame.voice[ref_v][i] > mx) mx = frame.voice[ref_v][i];
        }
        int16_t threshold = mn + (mx - mn) / 2;
        for (int i = 1; i < 128; i++) {
            if (frame.voice[ref_v][i - 1] < threshold && frame.voice[ref_v][i] >= threshold) {
                trigger = i;
                break;
            }
        }
    }

    for (int v = 0; v < 3; v++) {
        int r = v * 7;
        uint8_t ctrl  = frame.regs[r + 4];
        bool    gate  = (ctrl & 0x01) != 0;
        uint8_t wform = (ctrl >> 4) & 0x0F;

        int lane_center = v * 21 + 10;
        int lane_top    = v * 21 + 1;
        int lane_bottom = v * 21 + 19;
        int lane_half   = 9;

        if (!gate || wform == 0 || vol == 0) {
            display.drawFastHLine(0, lane_center, 128, SSD1306_WHITE);
            continue;
        }

        int prev_y = lane_center;
        for (int x = 0; x < 128; x++) {
            // Read from the rolling buffer starting at the trigger offset.
            // trigger is in [0..127] and x in [0..127], so index is in [0..254].
            float norm = (float)frame.voice[v][trigger + x] / 32767.0f;
            if (norm > 1.0f) norm = 1.0f;
            if (norm < -1.0f) norm = -1.0f;
            int y = lane_center - (int)(norm * (float)lane_half);
            y = constrain(y, lane_top, lane_bottom);
            if (x > 0) {
                display.drawLine(x - 1, prev_y, x, y, SSD1306_WHITE);
            }
            prev_y = y;
        }
    }

    display.display();
}

// FreeRTOS task — runs on Core 0; audio loop runs on Core 1
void displayTask(void* pvParam) {
    for (;;) {
        if (xSemaphoreTake(xDisplaySemaphore, portMAX_DELAY) == pdTRUE) {
            renderOscilloscope(scopeFrame);
        }
    }
}

void initScreen() {
    //LCD
    Wire.begin(MAKEPYTHON_ESP32_SDA, MAKEPYTHON_ESP32_SCL);
    Wire.setClock(400000);  // 400 kHz — cuts I2C frame time from ~100ms to ~25ms
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

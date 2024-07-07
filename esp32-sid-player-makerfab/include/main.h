#pragma once
#include <Arduino.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#include "esp_err.h"
#include "esp_log.h"
#include "driver/i2s.h"

// // Makerfab Audio shield pins
#define PIN_AUDIO_KIT_SD_CARD_CS 22
#define PIN_AUDIO_KIT_SD_CARD_MISO 19
#define PIN_AUDIO_KIT_SD_CARD_MOSI 23
#define PIN_AUDIO_KIT_SD_CARD_CLK  18

//Digital I/O used  //Makerfabs Audio V2.0
#define I2S_DOUT 27
#define I2S_BCLK 26
#define I2S_LRC 25

// Possible buttons pins
// volume 39, 36, 35 (up, down, mute)
// navigate 15, 33, 2 (prev, pause, next)

#define PIN_AUDIO_KIT_NEXT_SONG_BUTTON 2
#define PIN_AUDIO_KIT_NEXT_TUNE_BUTTON 15

//SSD1306
#define MAKEPYTHON_ESP32_SDA 4
#define MAKEPYTHON_ESP32_SCL 5
#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 64 // OLED display height, in pixels
#define OLED_RESET -1    // Reset pin # (or -1 if sharing Arduino reset pin)

static const i2s_port_t i2s_num = I2S_NUM_0; // i2s port number

static const i2s_config_t i2s_config = {
    .mode = static_cast<i2s_mode_t>(I2S_MODE_MASTER | I2S_MODE_TX),
    .sample_rate = 44100,
    .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT,
    .channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT,
    .communication_format = I2S_COMM_FORMAT_STAND_I2S,
    .intr_alloc_flags = 0, // default interrupt priority
    .dma_buf_count = 8,
    .dma_buf_len = 64,
    .use_apll = false
};

static const i2s_pin_config_t pin_config = {
    .bck_io_num = I2S_BCLK,
    .ws_io_num = I2S_LRC,
    .data_out_num = I2S_DOUT,
    .data_in_num = I2S_PIN_NO_CHANGE
};
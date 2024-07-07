#include <Arduino.h>
#include "AudioKitHAL.h"
#include "AudioTools.h"
#include "SdFat.h"
#include "SPI.h"
#include "SidTools.h"

#define PIN_AUDIO_KIT_SD_CARD_CS 13
#define PIN_AUDIO_KIT_SD_CARD_MISO 2
#define PIN_AUDIO_KIT_SD_CARD_MOSI 15
#define PIN_AUDIO_KIT_SD_CARD_CLK  14

#define PIN_AUDIO_KIT_NEXT_SONG_BUTTON 18

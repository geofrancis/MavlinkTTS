// SerialSpeak - Earle F. Philhower, III <earlephilhower@yahoo.com>
// Released to the public domain January 2025
// Modified for ArduPilot Rover Status & Proximity Alerts (2026)
#include "FastLED.h"
#include <BackgroundAudioSpeech.h>
#include <libespeak-ng/voice/en_gb_scotland.h>
#include <MAVLink.h>
#include <mavlink/common/mavlink_msg_rc_channels.h>
#include <mavlink/common/mavlink_msg_mission_current.h>
#include <mavlink/common/mavlink_msg_command_long.h>
#include <mavlink/common/mavlink_msg_distance_sensor.h>  // Required for rangefinder parsing

BackgroundAudioVoice v[] = {
  voice_en_gb_scotland,
};

#include <ESP32I2SAudio.h>
ESP32I2SAudio audio(15, 16, 14);  // BCLK, LRCLK, DOUT (,MCLK)
BackgroundAudioSpeech BMP(audio);


String mavmessage;
String mavmessageold;
uint8_t gps_Fix = 0;
uint8_t lastGpsFix = 0;
int32_t gps_Lat = 0;
int32_t gps_Long = 0;
uint16_t gps_Vel = 0;
uint16_t gps_Head = 0;
uint16_t sys_Bat = 0;
uint16_t sys_Bcur = 0;
int8_t sys_Brem = 0;
int wp_number;
int wp_numberold = -1;
int wp_total;
int mission_state;
int mission_mode;
float HDOP;
float VOLTS = 0;
float AMPS = 0;
int flightmode = 0;
long GPSLAT = 0.1234567;
long GPSLON = 0.1234567;
int satelites = 0;
int satelitesold = -1;
int comdroprate = 60;
int BASEMODE = 0;

// MARKED VOLATILE: Multi-core safety (Updated on Core 0, read on Core 1)
volatile bool lastArmedState = false;

int lastFlightMode = -1;
float roll = 0;
float pitch = 0;
float yaw = 0;
float gspeed = 0;
float heading = 0;
float fcmodein = -1;
float navbearing = 0;
float wpdist = 0;
float xtrackerror = 0;
int TIME_TO_SLEEP = 0;

int gain = 100;                     // Global variable to track current volume
bool lastRangefinderAlert = false;  // Prevents continuous warning spam


#define NUM_LEDS 144
#define BRIGHTNESS 255
#define COLOR_ORDER RGB
CRGB leds[NUM_LEDS];
uint8_t pos = 0;
bool toggle = false;
CRGBPalette16 currentPalette;
TBlendType currentBlending;


// Global variables needed for the new patterns
uint8_t gHue = 0;  // rotating base color used by rainbow and juggle
int cylonPos = 0;  // position tracking for the cylon effect
int cylonDir = 1;  // direction tracking for cylon (1 or -1)


int bright = 5;
int rc10 = 1500;
int rc11 = 1500;
int rc12;
int rc13;
int rc14;
int rc15;
int rc16;


// RTOS Task Handle for the Background Thread
TaskHandle_t AudioMavlinkTaskHandle = NULL;


// --- 1. MATRIX CONFIGURATION & FUNCTIONS FIRST ---
const bool kMatrixSerpentineLayout = true;

uint16_t XY(uint8_t x, uint8_t y) {
  // Invert Y-axis so 0 becomes 7, 1 becomes 6, etc.
  y = 7 - y;

  uint16_t i;
  if (kMatrixSerpentineLayout == false) {
    i = (y * 8) + x;
  } else {
    if (y & 0x01) {
      uint8_t reverseX = (8 - 1) - x;
      i = (y * 8) + reverseX;
    } else {
      i = (y * 8) + x;
    }
  }
  return i;
}

void drawSprite(const uint32_t sprite[8][8]) {
  for (uint8_t y = 0; y < 8; y++) {
    for (uint8_t x = 0; x < 8; x++) {
      uint32_t colorRGB = sprite[y][x];

      uint8_t r = (colorRGB >> 16) & 0xFF;
      uint8_t g = (colorRGB >> 8) & 0xFF;
      uint8_t b = colorRGB & 0xFF;

      // XY(x,y) safely outputs 0 to 63 to target your matrix perfectly
      leds[XY(x, y)] = CRGB(r, g, b);
    }
  }
}



// --- ORIGINAL SPRITES ---
const uint32_t heartSprite[8][8] = {
  { 0x000000, 0xFF0000, 0xFF0000, 0x000000, 0x000000, 0xFF0000, 0xFF0000, 0x000000 },
  { 0xFF0000, 0xFF0000, 0xFF0000, 0xFF0000, 0xFF0000, 0xFF0000, 0xFF0000, 0xFF0000 },
  { 0xFF0000, 0xFF0000, 0xFF0000, 0xFF0000, 0xFF0000, 0xFF0000, 0xFF0000, 0xFF0000 },
  { 0xFF0000, 0xFF0000, 0xFF0000, 0xFF0000, 0xFF0000, 0xFF0000, 0xFF0000, 0xFF0000 },
  { 0x000000, 0xFF0000, 0xFF0000, 0xFF0000, 0xFF0000, 0xFF0000, 0xFF0000, 0x000000 },
  { 0x000000, 0x000000, 0xFF0000, 0xFF0000, 0xFF0000, 0xFF0000, 0x000000, 0x000000 },
  { 0x000000, 0x000000, 0x000000, 0xFF0000, 0xFF0000, 0x000000, 0x000000, 0x000000 },
  { 0x000000, 0x000000, 0x000000, 0x000000, 0x000000, 0x000000, 0x000000, 0x000000 }
};

// --- FORWARD ARROW (Solid Green - Moving / Proceeding) ---
const uint32_t arrowSprite[8][8] = {
  { 0x000000, 0x000000, 0x000000, 0x00FF00, 0x00FF00, 0x000000, 0x000000, 0x000000 },
  { 0x000000, 0x000000, 0x00FF00, 0x00FF00, 0x00FF00, 0x00FF00, 0x000000, 0x000000 },
  { 0x000000, 0x00FF00, 0x00FF00, 0x00FF00, 0x00FF00, 0x00FF00, 0x00FF00, 0x000000 },
  { 0x00FF00, 0x00FF00, 0x00FF00, 0x00FF00, 0x00FF00, 0x00FF00, 0x00FF00, 0x00FF00 },
  { 0x000000, 0x000000, 0x000000, 0x00FF00, 0x00FF00, 0x000000, 0x000000, 0x000000 },
  { 0x000000, 0x000000, 0x000000, 0x00FF00, 0x00FF00, 0x000000, 0x000000, 0x000000 },
  { 0x000000, 0x000000, 0x000000, 0x00FF00, 0x00FF00, 0x000000, 0x000000, 0x000000 },
  { 0x000000, 0x000000, 0x000000, 0x00FF00, 0x00FF00, 0x000000, 0x000000, 0x000000 }
};



const uint32_t warningSprite[8][8] = {
  { 0x000000, 0x000000, 0x000000, 0xFFFF00, 0xFFFF00, 0x000000, 0x000000, 0x000000 },
  { 0x000000, 0x000000, 0x000000, 0xFFFF00, 0xFFFF00, 0x000000, 0x000000, 0x000000 },
  { 0x000000, 0x000000, 0x000000, 0xFFFF00, 0xFFFF00, 0x000000, 0x000000, 0x000000 },
  { 0x000000, 0x000000, 0x000000, 0xFFFF00, 0xFFFF00, 0x000000, 0x000000, 0x000000 },
  { 0x000000, 0x000000, 0x000000, 0xFFFF00, 0xFFFF00, 0x000000, 0x000000, 0x000000 },
  { 0x000000, 0x000000, 0x000000, 0x000000, 0x000000, 0x000000, 0x000000, 0x000000 },
  { 0x000000, 0x000000, 0x000000, 0xFFFF00, 0xFFFF00, 0x000000, 0x000000, 0x000000 },
  { 0x000000, 0x000000, 0x000000, 0x000000, 0x000000, 0x000000, 0x000000, 0x000000 }
};

const uint32_t checkmarkSprite[8][8] = {
  { 0x000000, 0x000000, 0x000000, 0x000000, 0x000000, 0x000000, 0x00FF00, 0x00FF00 },
  { 0x000000, 0x000000, 0x000000, 0x000000, 0x000000, 0x00FF00, 0x00FF00, 0x000000 },
  { 0x000000, 0x000000, 0x000000, 0x000000, 0x00FF00, 0x00FF00, 0x000000, 0x000000 },
  { 0x000000, 0x000000, 0x000000, 0x00FF00, 0x00FF00, 0x000000, 0x000000, 0x000000 },
  { 0x000000, 0x000000, 0x00FF00, 0x00FF00, 0x000000, 0x000000, 0x000000, 0x000000 },
  { 0x000000, 0x00FF00, 0x00FF00, 0x00FF00, 0x00FF00, 0x000000, 0x000000, 0x000000 },
  { 0x000000, 0x000000, 0x000000, 0x000000, 0x000000, 0x000000, 0x000000, 0x000000 },
  { 0x000000, 0x000000, 0x000000, 0x000000, 0x000000, 0x000000, 0x000000, 0x000000 }
};


const uint32_t stopSprite[8][8] = {
  { 0xFF0000, 0x000000, 0x000000, 0x000000, 0x000000, 0x000000, 0x000000, 0xFF0000 },
  { 0x000000, 0xFF0000, 0x000000, 0x000000, 0x000000, 0x000000, 0xFF0000, 0x000000 },
  { 0x000000, 0x000000, 0xFF0000, 0x000000, 0x000000, 0xFF0000, 0x000000, 0x000000 },
  { 0x000000, 0x000000, 0x000000, 0xFF0000, 0xFF0000, 0x000000, 0x000000, 0x000000 },
  { 0x000000, 0x000000, 0x000000, 0xFF0000, 0xFF0000, 0x000000, 0x000000, 0x000000 },
  { 0x000000, 0x000000, 0xFF0000, 0x000000, 0x000000, 0xFF0000, 0x000000, 0x000000 },
  { 0x000000, 0xFF0000, 0x000000, 0x000000, 0x000000, 0x000000, 0xFF0000, 0x000000 },
  { 0xFF0000, 0x000000, 0x000000, 0x000000, 0x000000, 0x000000, 0x000000, 0xFF0000 }
};

// --- UP / FORWARD ARROW ---
const uint32_t arrowUpSprite[8][8] = {
  { 0x000000, 0x000000, 0x000000, 0x00FF00, 0x00FF00, 0x000000, 0x000000, 0x000000 },
  { 0x000000, 0x000000, 0x00FF00, 0x00FF00, 0x00FF00, 0x00FF00, 0x000000, 0x000000 },
  { 0x000000, 0x00FF00, 0x00FF00, 0x00FF00, 0x00FF00, 0x00FF00, 0x00FF00, 0x000000 },
  { 0x00FF00, 0x00FF00, 0x00FF00, 0x00FF00, 0x00FF00, 0x00FF00, 0x00FF00, 0x00FF00 },
  { 0x000000, 0x000000, 0x000000, 0x00FF00, 0x00FF00, 0x000000, 0x000000, 0x000000 },
  { 0x000000, 0x000000, 0x000000, 0x00FF00, 0x00FF00, 0x000000, 0x000000, 0x000000 },
  { 0x000000, 0x000000, 0x000000, 0x00FF00, 0x00FF00, 0x000000, 0x000000, 0x000000 },
  { 0x000000, 0x000000, 0x000000, 0x00FF00, 0x00FF00, 0x000000, 0x000000, 0x000000 }
};

// --- DOWN / BACKWARD ARROW ---
const uint32_t arrowDownSprite[8][8] = {
  { 0x000000, 0x000000, 0x000000, 0x00FF00, 0x00FF00, 0x000000, 0x000000, 0x000000 },
  { 0x000000, 0x000000, 0x000000, 0x00FF00, 0x00FF00, 0x000000, 0x000000, 0x000000 },
  { 0x000000, 0x000000, 0x000000, 0x00FF00, 0x00FF00, 0x000000, 0x000000, 0x000000 },
  { 0x000000, 0x000000, 0x000000, 0x00FF00, 0x00FF00, 0x000000, 0x000000, 0x000000 },
  { 0x00FF00, 0x00FF00, 0x00FF00, 0x00FF00, 0x00FF00, 0x00FF00, 0x00FF00, 0x00FF00 },
  { 0x000000, 0x00FF00, 0x00FF00, 0x00FF00, 0x00FF00, 0x00FF00, 0x00FF00, 0x000000 },
  { 0x000000, 0x000000, 0x00FF00, 0x00FF00, 0x00FF00, 0x00FF00, 0x000000, 0x000000 },
  { 0x000000, 0x000000, 0x000000, 0x00FF00, 0x00FF00, 0x000000, 0x000000, 0x000000 }
};

// --- LEFT ARROW ---
const uint32_t arrowRightSprite[8][8] = {
  { 0x000000, 0x000000, 0x000000, 0x000000, 0x00FF00, 0x000000, 0x000000, 0x000000 },
  { 0x000000, 0x000000, 0x00FF00, 0x00FF00, 0x000000, 0x000000, 0x000000, 0x000000 },
  { 0x000000, 0x000000, 0x000000, 0x000000, 0x00FF00, 0x00FF00, 0x00FF00, 0x000000 },
  { 0x00FF00, 0x00FF00, 0x00FF00, 0x00FF00, 0x00FF00, 0x00FF00, 0x00FF00, 0x00FF00 },
  { 0x00FF00, 0x00FF00, 0x00FF00, 0x00FF00, 0x00FF00, 0x00FF00, 0x00FF00, 0x00FF00 },
  { 0x000000, 0x00FF00, 0x00FF00, 0x00FF00, 0x000000, 0x000000, 0x000000, 0x000000 },
  { 0x000000, 0x000000, 0x000000, 0x000000, 0x00FF00, 0x00FF00, 0x000000, 0x000000 },
  { 0x000000, 0x000000, 0x000000, 0x00FF00, 0x000000, 0x000000, 0x000000, 0x000000 }
};

// --- RIGHT ARROW ---
const uint32_t arrowLeftSprite[8][8] = {
  { 0x000000, 0x000000, 0x000000, 0x00FF00, 0x000000, 0x000000, 0x000000, 0x000000 },
  { 0x000000, 0x000000, 0x000000, 0x000000, 0x00FF00, 0x00FF00, 0x000000, 0x000000 },
  { 0x000000, 0x00FF00, 0x00FF00, 0x00FF00, 0x000000, 0x000000, 0x000000, 0x000000 },
  { 0x00FF00, 0x00FF00, 0x00FF00, 0x00FF00, 0x00FF00, 0x00FF00, 0x00FF00, 0x00FF00 },
  { 0x00FF00, 0x00FF00, 0x00FF00, 0x00FF00, 0x00FF00, 0x00FF00, 0x00FF00, 0x00FF00 },
  { 0x000000, 0x000000, 0x000000, 0x000000, 0x00FF00, 0x00FF00, 0x00FF00, 0x000000 },
  { 0x000000, 0x000000, 0x00FF00, 0x00FF00, 0x000000, 0x000000, 0x000000, 0x000000 },
  { 0x000000, 0x000000, 0x000000, 0x000000, 0x00FF00, 0x000000, 0x000000, 0x000000 }
};
// --- UP-LEFT ARROW ---
const uint32_t arrowUpLeftSprite[8][8] = {
  { 0x00FF00, 0x00FF00, 0x00FF00, 0x00FF00, 0x00FF00, 0x00FF00, 0x000000, 0x000000 },
  { 0x000000, 0x000000, 0x000000, 0x00FF00, 0x00FF00, 0x00FF00, 0x00FF00, 0x00FF00 },
  { 0x00FF00, 0x00FF00, 0x00FF00, 0x00FF00, 0x000000, 0x000000, 0x000000, 0x000000 },
  { 0x000000, 0x000000, 0x000000, 0x000000, 0x000000, 0x00FF00, 0x00FF00, 0x00FF00 },
  { 0x00FF00, 0x00FF00, 0x000000, 0x00FF00, 0x00FF00, 0x000000, 0x000000, 0x000000 },
  { 0x000000, 0x000000, 0x00FF00, 0x00FF00, 0x000000, 0x000000, 0x000000, 0x00FF00 },
  { 0x000000, 0x000000, 0x000000, 0x000000, 0x000000, 0x00FF00, 0x00FF00, 0x000000 },
  { 0x00FF00, 0x00FF00, 0x000000, 0x000000, 0x000000, 0x000000, 0x000000, 0x000000 }

};

// --- UP-RIGHT ARROW ---
const uint32_t arrowUpRightSprite[8][8] = {
  { 0x000000, 0x000000, 0x00FF00, 0x00FF00, 0x00FF00, 0x00FF00, 0x00FF00, 0x00FF00 },
  { 0x00FF00, 0x00FF00, 0x00FF00, 0x00FF00, 0x00FF00, 0x000000, 0x000000, 0x000000 },
  { 0x000000, 0x000000, 0x000000, 0x000000, 0x00FF00, 0x00FF00, 0x00FF00, 0x00FF00 },
  { 0x00FF00, 0x00FF00, 0x00FF00, 0x000000, 0x000000, 0x000000, 0x000000, 0x000000 },
  { 0x000000, 0x000000, 0x000000, 0x00FF00, 0x00FF00, 0x000000, 0x00FF00, 0x00FF00 },
  { 0x000000, 0x000000, 0x000000, 0x000000, 0x00FF00, 0x00FF00, 0x00FF00, 0x000000 },
  { 0x000000, 0x00FF00, 0x00FF00, 0x000000, 0x000000, 0x000000, 0x000000, 0x000000 },
  { 0x000000, 0x000000, 0x000000, 0x000000, 0x000000, 0x000000, 0x00FF00, 0x00FF00 }
};

// --- DOWN-LEFT ARROW ---
const uint32_t arrowDownLeftSprite[8][8] = {

  { 0x000000, 0x000000, 0x000000, 0x000000, 0x000000, 0x000000, 0xFFA500, 0xFFA500 },
  { 0x000000, 0xFFA500, 0xFFA500, 0x000000, 0x000000, 0x000000, 0x000000, 0x000000 },
  { 0xFFA500, 0x000000, 0x000000, 0x000000, 0xFFA500, 0xFFA500, 0x000000, 0x000000 },
  { 0x000000, 0x000000, 0x000000, 0xFFA500, 0xFFA500, 0x000000, 0xFFA500, 0xFFA500 },
  { 0xFFA500, 0xFFA500, 0xFFA500, 0xFFA500, 0x000000, 0x000000, 0x000000, 0x000000 },
  { 0x000000, 0x000000, 0x000000, 0x000000, 0xFFA500, 0xFFA500, 0xFFA500, 0xFFA500 },
  { 0xFFA500, 0xFFA500, 0xFFA500, 0xFFA500, 0xFFA500, 0x000000, 0x000000, 0x000000 },
  { 0x000000, 0x000000, 0xFFA500, 0xFFA500, 0xFFA500, 0xFFA500, 0xFFA500, 0xFFA500 }


};

// --- DOWN-RIGHT ARROW ---
const uint32_t arrowDownRightSprite[8][8] = {

  { 0xFFA500, 0xFFA500, 0x000000, 0x000000, 0x000000, 0x000000, 0x000000, 0x000000 },
  { 0x000000, 0x000000, 0x000000, 0x000000, 0x000000, 0xFFA500, 0xFFA500, 0x000000 },
  { 0x000000, 0x000000, 0xFFA500, 0xFFA500, 0x000000, 0x000000, 0x000000, 0xFFA500 },
  { 0xFFA500, 0xFFA500, 0x000000, 0xFFA500, 0xFFA500, 0x000000, 0x000000, 0x000000 },
  { 0x000000, 0x000000, 0x000000, 0x000000, 0xFFA500, 0xFFA500, 0xFFA500, 0xFFA500 },
  { 0xFFA500, 0xFFA500, 0xFFA500, 0xFFA500, 0x000000, 0x000000, 0x000000, 0x000000 },
  { 0x000000, 0x000000, 0x000000, 0xFFA500, 0xFFA500, 0xFFA500, 0xFFA500, 0xFFA500 },
  { 0xFFA500, 0xFFA500, 0xFFA500, 0xFFA500, 0xFFA500, 0xFFA500, 0x000000, 0x000000 }

};

// --- DOWN-RIGHT ARROW ---
const uint32_t White[8][8] = {
  { 0xFFFFFF, 0xFFFFFF, 0xFFFFFF, 0xFFFFFF, 0xFFFFFF, 0xFFFFFF, 0xFFFFFF, 0xFFFFFF },
  { 0xFFFFFF, 0xFFFFFF, 0xFFFFFF, 0xFFFFFF, 0xFFFFFF, 0xFFFFFF, 0xFFFFFF, 0xFFFFFF },
  { 0xFFFFFF, 0xFFFFFF, 0xFFFFFF, 0xFFFFFF, 0xFFFFFF, 0xFFFFFF, 0xFFFFFF, 0xFFFFFF },
  { 0xFFFFFF, 0xFFFFFF, 0xFFFFFF, 0xFFFFFF, 0xFFFFFF, 0xFFFFFF, 0xFFFFFF, 0xFFFFFF },
  { 0xFFFFFF, 0xFFFFFF, 0xFFFFFF, 0xFFFFFF, 0xFFFFFF, 0xFFFFFF, 0xFFFFFF, 0xFFFFFF },
  { 0xFFFFFF, 0xFFFFFF, 0xFFFFFF, 0xFFFFFF, 0xFFFFFF, 0xFFFFFF, 0xFFFFFF, 0xFFFFFF },
  { 0xFFFFFF, 0xFFFFFF, 0xFFFFFF, 0xFFFFFF, 0xFFFFFF, 0xFFFFFF, 0xFFFFFF, 0xFFFFFF },
  { 0xFFFFFF, 0xFFFFFF, 0xFFFFFF, 0xFFFFFF, 0xFFFFFF, 0xFFFFFF, 0xFFFFFF, 0xFFFFFF }
};

void RGB_SETUP() {
  FastLED.addLeds<WS2812, 3, GRB>(leds, NUM_LEDS).setCorrection(TypicalLEDStrip);
  FastLED.setBrightness(BRIGHTNESS);
  FastLED.show();

  currentPalette = RainbowColors_p;
  currentBlending = LINEARBLEND;

  for (int i = 0; i < NUM_LEDS; ++i) {
    if (i <= 64) leds[i] = CRGB::White;
    else if (i >= 64 && i < 80) leds[i] = CRGB::OrangeRed;
    else if (i >= 80 && i < 96) leds[i] = CRGB::White;
    else if (i >= 96 && i < 112) leds[i] = CRGB::White;
    else if (i >= 112 && i < 128) leds[i] = CRGB::Orange;
    else if (i >= 128 && i < 136) leds[i] = CRGB::Green;
    else leds[i] = CRGB::Green;
  }
  FastLED.show();
}

void FillLEDsFromPaletteColors(uint8_t colorIndex) {
  uint8_t brightness = 255;

  for (int i = 0; i < NUM_LEDS; ++i) {
    leds[i] = ColorFromPalette(currentPalette, colorIndex, brightness, currentBlending);
    colorIndex += 3;
  }
}

// This function fills the palette with totally random colors.
void SetupTotallyRandomPalette() {
  for (int i = 0; i < 16; ++i) {
    currentPalette[i] = CHSV(random8(), 255, random8());
  }
}













void RGB_LOOP() {
  // Global slow color rotation for some of the effects
  EVERY_N_MILLISECONDS(20) {
    gHue++;
  }

  // --- ARMED / DISARMED BRIGHTNESS SCALING ---
  if (lastArmedState) {
    // Armed: Full, visible range scale (e.g. 15 up to 255) based on RC13 Knob
    bright = map(rc13, 1000, 2000, 15, 255);
  } else {
    // Disarmed: Dim bench mode (Limits brightness to a max of 30, saving eyes/battery)
    bright = 10;
  }
  bright = constrain(bright, 2, 255);
  FastLED.setBrightness(bright);

  // ==========================================
  // 1. SPRITE SELECTION (Always Visible & Active on LEDs 0-63)
  // ==========================================
  int spriteIndex = map(rc10, 1000, 2000, 0, 14);
  spriteIndex = constrain(spriteIndex, 0, 14);

  // drawSprite completely overwrites all 64 matrix pixels every frame,
  // so we don't need to clear them beforehand.
  switch (spriteIndex) {
    case 0: drawSprite(stopSprite); break;
    case 1: drawSprite(warningSprite); break;
    case 2: drawSprite(arrowSprite); break;
    case 3: drawSprite(heartSprite); break;
    case 4: drawSprite(arrowUpSprite); break;
    case 5: drawSprite(arrowDownSprite); break;
    case 6: drawSprite(arrowLeftSprite); break;
    case 7: drawSprite(arrowRightSprite); break;
    case 8: drawSprite(arrowUpLeftSprite); break;
    case 9: drawSprite(arrowUpRightSprite); break;
    case 10: drawSprite(arrowDownLeftSprite); break;
    case 11: drawSprite(arrowDownRightSprite); break;
    case 12: drawSprite(White); break;
    case 13: drawSprite(stopSprite); break;
    case 14: drawSprite(stopSprite); break;
  }

  // ==========================================
  // 2. EFFECTS SELECTION (Only affects LEDs 64-143)
  // ==========================================
  bool effectActive = false;

  // --- CHANNEL 2: Fixed Static Color Blocks ---
  if (rc11 >= 1250 && rc11 < 1350) {
    effectActive = true;
    for (int i = 64; i < NUM_LEDS; ++i) {
      if (i >= 64 && i < 80) { leds[i] = CRGB::OrangeRed; }
      if (i >= 80 && i < 96) { leds[i] = CRGB::White; }
      if (i >= 96 && i < 112) { leds[i] = CRGB::White; }
      if (i >= 112 && i < 128) { leds[i] = CRGB::Orange; }
      if (i >= 128 && i < 136) { leds[i] = CRGB::Red; }
      if (i >= 136 && i < 144) { leds[i] = CRGB::Red; }
    }
  }

  // --- CHANNEL 3: Moving Palette Rainbow ---
  else if (rc11 >= 1400 && rc11 < 1480) {
    effectActive = true;
    static uint8_t startIndex = 0;
    EVERY_N_MILLISECONDS(5) {
      startIndex++;  // Non-blocking palette movement
    }

    uint8_t colorIndex = startIndex;
    for (int i = 64; i < NUM_LEDS; ++i) {
      leds[i] = ColorFromPalette(currentPalette, colorIndex, 255, currentBlending);
      colorIndex += 3;
    }
  }

  // --- CHANNEL 4: Cycling Single LED per Group ---
  else if (rc11 >= 1480 && rc11 < 1650) {
    effectActive = true;
    static uint16_t groupStep = 0;

    EVERY_N_MILLISECONDS(100) {
      groupStep++;
    }

    // Clear only the indicator portion (64 to 143)
    fadeToBlackBy(&(leds[64]), NUM_LEDS - 64, 255);

    // Group math shifted forward by +64 to preserve matrix space:
    int idx1 = 64 + (groupStep % 16);   // Group 2 (original Group 1 is now your matrix!)
    int idx2 = 80 + (groupStep % 16);   // Group 3
    int idx3 = 96 + (groupStep % 16);   // Group 4
    int idx4 = 112 + (groupStep % 16);  // Group 5
    int idx5 = 128 + (groupStep % 8);   // Group 6
    int idx6 = 136 + (groupStep % 8);   // Group 7

    leds[idx1] = CRGB::OrangeRed;
    leds[idx2] = CRGB::White;
    leds[idx3] = CRGB::White;
    leds[idx4] = CRGB::Orange;
    leds[idx5] = CRGB::Red;
    leds[idx6] = CRGB::Red;
  }

  // --- CHANNEL 6: Rainbow with Glitter ---
  else if (rc11 >= 1650 && rc11 < 2000) {
    effectActive = true;
    // Fill only the second half of the strip with the rainbow
    fill_rainbow(&(leds[64]), NUM_LEDS - 64, gHue, 7);
    if (random8() < 80) {
      // Pick a random index strictly between 64 and 143
      leds[random16(64, NUM_LEDS)] += CRGB::White;
    }
  }

  // --- CHANNEL 7: Juggle (Multi-dot weaving) ---
  else if (rc11 >= 2000) {
    effectActive = true;
    // Fade only the indicator portion
    fadeToBlackBy(&(leds[64]), NUM_LEDS - 64, 20);
    uint8_t coloredhue = 0;
    for (int i = 0; i < 8; i++) {
      // Constrain the weaving dots strictly to indices 64 to 143
      leds[beatsin16(i + 7, 64, NUM_LEDS - 1)] |= CHSV(coloredhue, 200, 255);
      coloredhue += 32;
    }
  }

  // Clear indicators if rc11 matches none of the ranges
  if (!effectActive) {
    fadeToBlackBy(&(leds[64]), NUM_LEDS - 64, 255);
  }

  // Draw the final state of all LEDs to the physical strip once
  FastLED.show();
}






void usage() {
  Serial.printf("Simple Speech Example\r\n");
  Serial.printf("Enter a line with *# to change voice (i.e. *3 to go to 3rd voice in list)\r\n");
  Serial.printf("Installed voices:\r\n");
  for (int i = 0; i < (int)sizeof(v) / (int)sizeof(v[0]); i++) {
    Serial.printf("*%d: %s\r\n", i, v[i].name);
  }
  Serial.printf("!rate <rate in wpm , 175 default>\r\n");
  Serial.printf("!pitch <pitch adjustment 0-99, 50 default>\r\n");
  Serial.printf("!gap <word gap in 10-ms>\r\n");
  Serial.printf("Otherwise, any typed words will be read aloud.\r\n\r\n");
}

bool lastRangefinderAlerts[16] = { false };

const char* getDirectionName(uint8_t orientation) {
  switch (orientation) {
    case 0: return "Forward";
    case 1: return "Forward Right";
    case 2: return "Right";
    case 3: return "Back Right";
    case 4: return "Backward";
    case 5: return "Back Left";
    case 6: return "Left";
    case 7: return "Forward Left";
    case 24: return "Upward";
    case 25: return "Downward";
    default: return "Unknown";
  }
}

const char* get_rover_mode_name(uint8_t mode) {
  switch (mode) {
    case 0: return "Manual";
    case 1: return "Acro";
    case 3: return "Steering";
    case 4: return "Hold";
    case 5: return "Loiter";
    case 6: return "Follow";
    case 7: return "Simple";
    case 8: return "Dock";
    case 10: return "Auto";
    case 11: return "Return to Launch";
    case 12: return "Smart Return to Launch";
    case 15: return "Guided";
    case 16: return "Initializing";
    default: return "Unknown Mode";
  }
}

void setVolume(int targetVolume) {
  gain = constrain(targetVolume, 10, 200);
  BMP.setGain(gain / 100.0);
  Serial.printf("Volume set to: %d%%\r\n", gain);
}

void FetchMavlinkSerial() {
  mavlink_message_t msg;
  mavlink_status_t status;

  while (Serial2.available()) {
    uint8_t c = Serial2.read();
    if (mavlink_parse_char(MAVLINK_COMM_0, c, &msg, &status)) {
      switch (msg.msgid) {

        case MAVLINK_MSG_ID_HEARTBEAT:
          {
            if (msg.compid == MAV_COMP_ID_AUTOPILOT1) {
              mavlink_heartbeat_t hb;
              mavlink_heartbeat_t hb_decoded;
              mavlink_msg_heartbeat_decode(&msg, &hb_decoded);

              bool isArmed = (hb_decoded.base_mode & MAV_MODE_FLAG_SAFETY_ARMED);
              if (isArmed != lastArmedState) {
                lastArmedState = isArmed;
                BMP.speak(isArmed ? "Vehicle Armed" : "Vehicle Disarmed");
              }

              if ((int)hb_decoded.custom_mode != lastFlightMode) {
                lastFlightMode = hb_decoded.custom_mode;
                char modeStr[64];
                snprintf(modeStr, sizeof(modeStr), "Flight mode %s", get_rover_mode_name(lastFlightMode));
                BMP.speak(modeStr);
              }

              BASEMODE = hb_decoded.base_mode;
              fcmodein = hb_decoded.custom_mode;
            }
            break;
          }

        case MAVLINK_MSG_ID_GPS_RAW_INT:
          {
            mavlink_gps_raw_int_t datagps;
            mavlink_msg_gps_raw_int_decode(&msg, &datagps);

            satelites = datagps.satellites_visible;
            gps_Fix = datagps.fix_type;
            HDOP = datagps.eph;

            if (satelitesold != satelites) {
              char satStr[32];
              snprintf(satStr, sizeof(satStr), "%d satellites", satelites);
              BMP.speak(satStr);
              satelitesold = satelites;
            }

            if (gps_Fix != lastGpsFix) {
              char fixStr[48];
              switch (gps_Fix) {
                case 0:
                case 1:
                  snprintf(fixStr, sizeof(fixStr), "GPS status. No Fix");
                  break;
                case 2:
                  snprintf(fixStr, sizeof(fixStr), "GPS status. Two D Fix");
                  break;
                case 3:
                  snprintf(fixStr, sizeof(fixStr), "GPS status. Three D Fix");
                  break;
                case 4:
                  snprintf(fixStr, sizeof(fixStr), "GPS status. D G P S 3D Fix");
                  break;
                case 5:
                  snprintf(fixStr, sizeof(fixStr), "GPS status. Float RTK");
                  break;
                case 6:
                  snprintf(fixStr, sizeof(fixStr), "GPS status. Fixed RTK");
                  break;
                default:
                  snprintf(fixStr, sizeof(fixStr), "GPS fix level changed to %d", gps_Fix);
                  break;
              }
              BMP.speak(fixStr);
              lastGpsFix = gps_Fix;
            }
            break;
          }

        case MAVLINK_MSG_ID_SERVO_OUTPUT_RAW:
          {
            mavlink_servo_output_raw_t SERVOCHANNEL;
            mavlink_msg_servo_output_raw_decode(&msg, &SERVOCHANNEL);
            rc10 = (SERVOCHANNEL.servo10_raw);
            rc11 = (SERVOCHANNEL.servo11_raw);
            rc12 = (SERVOCHANNEL.servo12_raw);
            rc13 = (SERVOCHANNEL.servo13_raw);
            rc14 = (SERVOCHANNEL.servo14_raw);
            rc15 = (SERVOCHANNEL.servo15_raw);
            rc16 = (SERVOCHANNEL.servo16_raw);
            break;
          }

        case MAVLINK_MSG_ID_SYS_STATUS:
          {
            mavlink_sys_status_t sys_status;
            mavlink_msg_sys_status_decode(&msg, &sys_status);
            VOLTS = sys_status.voltage_battery;
            AMPS = sys_status.current_battery;
            break;
          }

        case MAVLINK_MSG_ID_ATTITUDE:
          {
            mavlink_attitude_t attitude;
            mavlink_msg_attitude_decode(&msg, &attitude);
            break;
          }

        case MAVLINK_MSG_ID_RC_CHANNELS_RAW:
          {
            mavlink_rc_channels_raw_t chs;
            mavlink_msg_rc_channels_raw_decode(&msg, &chs);
            break;
          }

        case MAVLINK_MSG_ID_MISSION_CURRENT:
          {
            mavlink_mission_current_t RPNUM;
            mavlink_msg_mission_current_decode(&msg, &RPNUM);
            wp_number = RPNUM.seq;

            if (wp_numberold != wp_number) {
              char wpStr[64];
              snprintf(wpStr, sizeof(wpStr), "Waypoint %d. Heading %d degrees. Distance %d meters",
                       wp_number, (int)navbearing, (int)wpdist);
              BMP.speak(wpStr);
              wp_numberold = wp_number;
            }
            break;
          }

        case MAVLINK_MSG_ID_VFR_HUD:
          {
            mavlink_vfr_hud_t vfrhud;
            mavlink_msg_vfr_hud_decode(&msg, &vfrhud);
            gps_Vel = vfrhud.groundspeed;
            gps_Head = vfrhud.heading;
            break;
          }

        case MAVLINK_MSG_ID_NAV_CONTROLLER_OUTPUT:
          {
            mavlink_nav_controller_output_t navout;
            mavlink_msg_nav_controller_output_decode(&msg, &navout);
            navbearing = navout.target_bearing;
            wpdist = navout.wp_dist;
            xtrackerror = navout.xtrack_error;

            static uint32_t lastDistAnnounce = 0;
            if (millis() - lastDistAnnounce > 10000 && wpdist > 0) {
              char distStr[32];
              snprintf(distStr, sizeof(distStr), "%d meters to target", (int)wpdist);
              BMP.speak(distStr);
              lastDistAnnounce = millis();
            }
            break;
          }
        case MAVLINK_MSG_ID_DISTANCE_SENSOR:
          {
            mavlink_distance_sensor_t dist;
            mavlink_msg_distance_sensor_decode(&msg, &dist);

            if (dist.id < 16) {
              bool& lastAlert = lastRangefinderAlerts[dist.id];
              const char* dirStr = getDirectionName(dist.orientation);

              if (dist.current_distance < 50) {
                if (dist.current_distance > 0) {
                  if (!lastAlert) {
                    char rfStr[128];
                    snprintf(rfStr, sizeof(rfStr),
                             "Proximity Alert. Object %d centimeters on the %s.",
                             (int)dist.current_distance, dirStr);

                    BMP.speak(rfStr);
                    lastAlert = true;
                  }
                }
              } else if (dist.current_distance >= 40) {
                lastAlert = false;
              }
            }
            break;
          }
        case MAVLINK_MSG_ID_STATUSTEXT:
          {
            mavlink_statustext_t packet;
            mavlink_msg_statustext_decode(&msg, &packet);
            if (packet.severity > 0) {
              mavmessage = packet.text;
              if (mavmessageold != mavmessage) {
                BMP.speak(mavmessage.c_str());
                mavmessageold = mavmessage;
              }
            }
            break;
          }
      }
    }
  }
}

// This FreeRTOS task runs continuously on Core 0
void AudioMavlinkTask(void* pvParameters) {
  // Initialize BackgroundAudioSpeech here on Core 0
  BMP.begin();
  BMP.speak("Audio startup complete.");

  while (1) {
    FetchMavlinkSerial();
    vTaskDelay(pdMS_TO_TICKS(5));  // Clean yield back to FreeRTOS
  }
}

#define Serial1T 9
#define Serial1R 10

#define Serial2T 11
#define Serial2R 13

void setup() {
  Serial.begin(115200);
  Serial1.begin(460800, SERIAL_8N1, Serial1R, Serial1T);
  Serial2.begin(460800, SERIAL_8N1, Serial2R, Serial2T);

  BMP.setVoice(v[0]);
  setVolume(50);
  RGB_SETUP();
  delay(3000);
  Serial.printf("Ready!\r\n");

  // Spin up Core 0 task to completely decouple sound processing and MAVlink from LEDs
  xTaskCreatePinnedToCore(
    AudioMavlinkTask,        /* Task function */
    "AudioMavlinkTask",      /* Task name */
    8192,                    /* Stack size in words (large stack for speech synth) */
    NULL,                    /* Parameters */
    1,                       /* Priority */
    &AudioMavlinkTaskHandle, /* Handle */
    0                        /* Pin task to Core 0 */
  );
}

void loop() {
  // Core 1 handles only light displays and slow delays
  RGB_LOOP();
  delay(10);
}
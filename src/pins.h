#pragma once

#ifdef ARDUINO_DFROBOT_FIREBEETLE_2_ESP32E

#define SD_CARD_CS 12 // D13

#define I2S_WCLK 0 // D5
#define I2S_BCLK 14 // D6
#define I2S_DOUT 13 // D7

#elif defined(ARDUINO_XIAO_ESP32C3)

#define SD_CARD_CS 5 // D3
#define I2S_WCLK 4 // D2 / LRC
#define I2S_BCLK 3 // D1
#define I2S_DOUT 2 // D0

#endif
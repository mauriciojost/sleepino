#ifndef PINOUT_INC
#define PINOUT_INC

#ifdef ESP8266

#include <PinoutESP8266.h>

// LCD
#define LCD_CLK_PIN GPIO2_PIN
#define LCD_DIN_PIN GPIO0_PIN
#define LCD_DC_PIN GPIO4_PIN
#define LCD_CS_PIN GPIO5_PIN
#define LCD_RST_PIN GPIO14_PIN

// DEEP SLEEP

// GPIO16_PIN -> REST if deep sleep enabled on ESP8266
// GND -> capacitor 47uF -> Vcc

#endif // ESP8266


#ifdef ESP32

#include <PinoutESP32.h>

// LCD
#define LCD_CLK_PIN GPIO14_PIN
#define LCD_DIN_PIN GPIO13_PIN
#define LCD_DC_PIN GPIO27_PIN
#define LCD_CS_PIN GPIO15_PIN
#define LCD_RST_PIN GPIO26_PIN

#endif // ESP32

#endif // PINOUT_INC

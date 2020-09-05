#ifndef PINOUT_INC
#define PINOUT_INC

#ifdef ESP8266
#include <PinoutESP8266.h>
#endif // ESP8266


#ifdef ESP32
#include <PinoutESP32.h>
#endif // ESP32

#ifdef X86_64
#define POWER_PIN 0
#define HIGH 1
#define LOW 0
#endif // X8_664


#endif // PINOUT_INC

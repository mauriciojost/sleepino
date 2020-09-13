#ifndef CONSTANTS_INC
#define CONSTANTS_INC

#define PROJECT_ID "waterino"

#ifdef ARDUINO
#ifdef ESP8266 // on ESP8266
#define PLATFORM_ID "esp8266"
#endif // ESP8266

#ifdef ESP32 // on ESP8266
#define PLATFORM_ID "esp32"
#endif // ESP8266

#else // ARDUINO (on PC)
#define PLATFORM_ID "x86_64"
#endif // ARDUINO

#ifndef PROJ_VERSION
#define PROJ_VERSION "snapshot"
#endif // PROJ_VERSION

#endif // CONSTANTS_INC

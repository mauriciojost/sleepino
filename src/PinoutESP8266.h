#ifdef ESP8266 // NODEMCU based on ESP8266

#include <pinouts/PinoutESP8266.h>

// IO
#define SERVO0_PIN PIN_D7
#define POWER_PIN PIN_D3

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

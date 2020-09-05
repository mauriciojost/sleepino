#ifdef ESP32 // NODEMCU based on ESP32

// FROM: https://www.instructables.com/id/ESP32-Internal-Details-and-Pinout/

#include <pinouts/PinoutESP32DevKit.h>

// IO
#define SERVO0_PIN GPIO22_PIN
#define POWER_PIN  GPIO23_PIN

// LCD
#define LCD_CLK_PIN GPIO14_PIN
#define LCD_DIN_PIN GPIO13_PIN
#define LCD_DC_PIN GPIO27_PIN
#define LCD_CS_PIN GPIO15_PIN
#define LCD_RST_PIN GPIO26_PIN

#endif // ESP32

/**
 * This file contains:
 * - entry point for arduino programs (setup and loop functions)
 * - declaration of HW specific functions (the definition is in a dedicated file)
 * - other functions that are not defined by HW specific but that use them, that are required by the module
 *   (so that it can be passed as callback).
 * The rest should be put in Module so that they can be tested regardless of the HW used behind.
 */

#include <Main.h>

ModuleSleepino *m;

#ifdef ARDUINO // on ESP8266
#include <MainESP8266.h>
#else // on PC
#include <MainX86_64.h>
#endif // ARDUINO

bool initWifiSimple() {
  Settings *s = m->getModuleSettings();
  log(CLASS_MAIN, Info, "W.steady");
  bool connected = initWifi(s->getSsid(), s->getPass(), true, 10);
  return connected;
}

void setup() {
  m = new ModuleSleepino();
  m->setup(setupArchitecture,
           messageFunc,
           initWifiSimple,
           httpPost,
           httpGet,
           clearDevice,
           readFile,
           writeFile,
           abort,
           sleepInterruptable,
           configureModeArchitecture,
           runModeArchitecture,
           commandArchitecture,
           infoArchitecture,
           updateFirmware,
           testArchitecture,
           apiDeviceLogin,
           apiDevicePass);
  bool succ = m->startupProperties();
  if (!succ) {
    abort("Could not startup");
  }
}

void loop() {
  m->loop();
}

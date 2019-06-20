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
  bool connected = initWifi(s->getSsid(), s->getPass(), true, 3);
  return connected;
}

void commandFunc(const char* c) {
	m->command(c);
}

void setup() {
  m = new ModuleSleepino();
  m->setup(setupArchitecture,
           messageFunc,
           initWifiSimple,
           stopWifi,
           httpPost,
           httpGet,
           clearDevice,
           readFile,
           writeFile,
           sleepInterruptable,
           deepSleepNotInterruptable,
           configureModeArchitecture,
           runModeArchitecture,
           commandArchitecture,
           infoArchitecture,
           updateFirmware,
           testArchitecture,
           apiDeviceLogin,
           apiDevicePass,
           commandFunc,
           inDeepSleepMode
  );
  ModuleStartupPropertiesCode s = m->startupProperties();
  if (s != ModuleStartupPropertiesCodeSuccess && s != ModuleStartupPropertiesCodeSkipped) {
     abort("Cannot startup properties");
  }
}

void loop() {
  m->loop();
}

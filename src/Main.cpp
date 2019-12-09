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

#ifdef ARDUINO

#ifdef ESP8266 // on ESP8266
#include <MainESP8266.h>
#endif // ESP8266

#ifdef ESP32 // on ESP32
#include <MainESP32.h>
#endif // ESP32

#else // on PC
#include <MainX86_64.h>
#endif // ARDUINO

bool initWifiSimple() {
  Settings *s = m->getModuleSettings();
  bool connected = initializeWifi(s->getSsid(), s->getPass(), s->getSsidBackup(), s->getPassBackup(), true, 3);
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
  log(CLASS_MAIN, Info, "Startup properties...");
  ModuleStartupPropertiesCode s = m->startupProperties();
  if (s != ModuleStartupPropertiesCodeSuccess && s != ModuleStartupPropertiesCodeSkipped) {
    log(CLASS_MAIN, Error, "Failed: %d", (int)s);
    abort("Cannot startup properties");
  }
  log(CLASS_MAIN, Info, "Setup done.");
  log(CLASS_MAIN, Info, "Loop started...");
  if (m->getBot()->getMode() == ConfigureMode) {
    logRaw(CLASS_MAIN, User, "Configure mode...");
  }
}

void loop() {
  m->loop();
}

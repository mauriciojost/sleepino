/**
 * This file contains:
 * - entry point for arduino programs (setup and loop functions)
 * - declaration of HW specific functions (the definition is in a dedicated file)
 * - other functions that are not defined by HW specific but that use them, that are required by the module
 *   (so that it can be passed as callback).
 * The rest should be put in Module so that they can be tested regardless of the HW used behind.
 */

#include <Main.h>

#ifdef ARDUINO

#ifdef ESP8266 // on ESP8266
#include <PlatformESP8266.h>
#endif // ESP8266

#ifdef ESP32 // on ESP32
#include <PlatformESP32.h>
#endif // ESP32

#else // on PC
#include <PlatformX86_64.h>
#endif // ARDUINO

#define WIFI_CONNECT_ATTEMPTS 10

#define PROJECT_ID "sleepino"

bool initWifiSimple() {
  Settings *s = m->getModuleSettings();
  bool connected = initializeWifi(s->getSsid(), s->getPass(), s->getSsidBackup(), s->getPassBackup(), true, WIFI_CONNECT_ATTEMPTS);
  return connected;
}

void commandFunc(const char* c) {
  m->command(c);
}

void updateFirmwareVersion(const char *targetVersion, const char *currentVersion) {
  bool c = initWifiSimple();
  if (c) {
    updateFirmwareFromMain4ino(m->getModule()->getPropSync()->getSession(), apiDeviceLogin(), PROJECT_ID, PLATFORM_ID, targetVersion, currentVersion);
  } else {
    log(CLASS_MAIN, Error, "Could not update");
  }
}

#define MAX_SLEEP_CYCLE_SECS 1800 // 30min
void deepSleepNotInterruptableCustom(time_t cycleBegin, time_t periodSecs) {
  if (periodSecs <= MAX_SLEEP_CYCLE_SECS) {
    log(CLASS_MAIN, Debug, "Regular DS %d", periodSecs);
    writeRemainingSecs(0); // clean RTC for next boot
    deepSleepNotInterruptable(now(), periodSecs);
  } else {
    int remaining = periodSecs - MAX_SLEEP_CYCLE_SECS;
    log(CLASS_MAIN, Debug, "EDS: %d(+%d rem.)", MAX_SLEEP_CYCLE_SECS, remaining);
    writeRemainingSecs(remaining);
    deepSleepNotInterruptable(now(), MAX_SLEEP_CYCLE_SECS);
  }
}

void resumeDeepSleepIfApplicable() {
  int remainingSecs = readRemainingSecs();
  if (remainingSecs > MAX_SLEEP_CYCLE_SECS) {
    log(CLASS_MAIN, Info, "EDS ongoing %d(+%d remaining)", MAX_SLEEP_CYCLE_SECS, remainingSecs);
    writeRemainingSecs(remainingSecs - MAX_SLEEP_CYCLE_SECS);
    deepSleepNotInterruptable(now(), MAX_SLEEP_CYCLE_SECS);
  } else if (remainingSecs > 0) {
    log(CLASS_MAIN, Info, "EDS ongoing %d (+0 remaining)", remainingSecs);
    writeRemainingSecs(0);
    deepSleepNotInterruptable(now(), remainingSecs);
  } else {
    log(CLASS_MAIN, Info, "No EDS ongoing");
  }
}


void setup() {
  m = new ModuleSleepino();
  m->setup(setupArchitecture,
           messageFunc,
           initWifiSimple,
           stopWifi,
           httpMethod,
           clearDevice,
           readFile,
           writeFile,
           sleepInterruptable,
           deepSleepNotInterruptableCustom,
           configureModeArchitecture,
           runModeArchitecture,
           commandArchitecture,
           infoArchitecture,
           updateFirmwareVersion,
           testArchitecture,
           apiDeviceLogin,
           apiDevicePass,
           commandFunc,
           getLogBuffer,
           vcc
  );

  log(CLASS_MAIN, Info, "Resume DS...");
  resumeDeepSleepIfApplicable();
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

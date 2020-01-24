/**
 * This file contains:
 * - entry point for arduino programs (setup and loop functions)
 * - declaration of HW specific functions (the definition is in a dedicated file)
 * - other functions that are not defined by HW specific but that use them, that are required by the module
 *   (so that it can be passed as callback).
 * The rest should be put in Module so that they can be tested regardless of the HW used behind.
 */

#define CLASS_MAIN "MA"

#include <main4ino/Misc.h>
#include <ModuleSleepino.h>
#include <Platform.h>

#ifndef PROJ_VERSION
#define PROJ_VERSION "snapshot"
#endif // PROJ_VERSION

//////////////////////////////////////////////////////////////
// Provided by generic Main
//////////////////////////////////////////////////////////////

// Standard arduino setup


#define PROJECT_ID "sleepino"

void setup() {
  BotMode mode = setupArchitecture();
  m = new ModuleSleepino();
  m->setup(messageFunc,
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
  m->getBot()->setMode(mode);

  log(CLASS_MAIN, Info, "Resume DS...");
  resumeExtendedDeepSleepIfApplicable();
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


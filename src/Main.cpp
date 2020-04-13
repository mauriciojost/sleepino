#include <main4ino/Misc.h>
#include <ModuleSleepino.h>
#include <Platform.h>
#include <Constants.h>

#ifndef PROJ_VERSION
#define PROJ_VERSION "snapshot"
#endif // PROJ_VERSION

#define CLASS_MAIN "MA"

//////////////////////////////////////////////////////////////
// Provided by generic Main
//////////////////////////////////////////////////////////////

// Standard arduino setup


void setup() {
  setupArchitecture();

  log(CLASS_MAIN, Info, "Resume DS...");
  resumeExtendedDeepSleepIfApplicable();

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

  log(CLASS_MAIN, Info, "Startup properties...");
  StartupStatus s = m->startupProperties();
  m->getBot()->setMode(s.botMode);
  if (s.startupCode != ModuleStartupPropertiesCodeSuccess && s.startupCode != ModuleStartupPropertiesCodeSkipped) {
    log(CLASS_MAIN, Error, "Failed: %d", (int)s.startupCode);
    abort("Cannot startup properties");
  }
  log(CLASS_MAIN, Info, "Setup done.");
}

void loop() {
  m->loop();
}


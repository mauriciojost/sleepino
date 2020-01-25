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
  log(CLASS_MAIN, Info, "Resume DS...");
  resumeExtendedDeepSleepIfApplicable();

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


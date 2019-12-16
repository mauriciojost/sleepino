#include <cstdio>
#include <iostream>
#include <stdio.h>
#include <time.h>
#include <unistd.h>
#include <Main.h>
#include <primitives/BoardX86_64.h>

#ifndef SIMULATOR_LOGIN
#error "Must define SIMULATOR_LOGIN"
#endif // SIMULATOR_LOGIN

#ifndef SIMULATOR_PASS
#error "Must define SIMULATOR_PASS"
#endif // SIMULATOR_PASS

#define CL_MAX_LENGTH 65000
#define HTTP_CODE_KEY "HTTP_CODE:"
#define CURL_COMMAND_GET "/usr/bin/curl --silent -w '" HTTP_CODE_KEY "%%{http_code}' -XGET '%s'"
#define CURL_COMMAND_POST "/usr/bin/curl --silent -w '" HTTP_CODE_KEY "%%{http_code}' -XPOST '%s' -d '%s'"

enum AppMode { Interactive = 0, NonInteractive = 1 };
AppMode appMode = Interactive;


////////////////////////////////////////
// Functions requested for architecture
////////////////////////////////////////

// Callbacks
///////////////////

const char *apiDeviceLogin() {
  return SIMULATOR_LOGIN;
}

const char *apiDevicePass() {
  return SIMULATOR_PASS;
}

void logLine(const char *str) {
  printf("LOG: %s", str);
}

void messageFunc(int x, int y, int color, bool wrap, MsgClearMode clear, int size, const char *str) {
  printf("\n\n***** LCD (size %d)\n  %s\n*****\n\n", size, str);
}

void clearDevice() {
  log(CLASS_MAIN, Debug, "Clear device");
}



void infoArchitecture() {}

void testArchitecture() {}

void updateFirmwareVersion(const char *targetVersion, const char *currentVersion) {}

// Execution
///////////////////

void heartbeat() {}

bool haveToInterrupt() {
  return false;
}

bool sleepInterruptable(time_t cycleBegin, time_t periodSecs) {
  return lightSleepInterruptable(cycleBegin, periodSecs, m->getModuleSettings()->miniPeriodMsec(), haveToInterrupt, heartbeat);
}

BotMode setupArchitecture() {
  log(CLASS_MAIN, Debug, "Setup timing");
  setExternalMillis(millis);
  return RunMode;
}

void runModeArchitecture() {
  if (appMode == Interactive) {
    char str[100];
    printf("Waiting for input: \n   ");
    fgets(str, 100, stdin);
    if (strlen(str) != 0) {
      Buffer cmdBuffer(str);
      cmdBuffer.replace('\n', 0);
      cmdBuffer.replace('\r', 0);
      m->command(cmdBuffer.getBuffer());
    }
  }
}

CmdExecStatus commandArchitecture(const char *c) {
  return NotFound;
}

void configureModeArchitecture() {
  // nothing to be done here
}

void abort(const char *msg) {
  log(CLASS_MAIN, Error, "Abort: %s", msg);
}

////////////////////////////////////////
// Architecture specific functions
////////////////////////////////////////

void setup();
void loop();

int main(int argc, const char *argv[]) {
  setup();

  int simulationSteps = 10;

  if (argc == 1) {
    appMode = NonInteractive;
  } else if (argc == 1 + 1) {
    appMode = (AppMode)atoi(argv[1]);
  } else if (argc == 1 + 2) {
    appMode = (AppMode)atoi(argv[1]);
    simulationSteps = atoi(argv[2]);
  } else if (argc != 1 + 2) {
    log(CLASS_MAIN, Error, "2 args max: <starter> [appMode [steps]]");
    return -1;
  }

  for (int i = 0; i < simulationSteps; i++) {
    log(CLASS_MAIN, Debug, "### Step %d/%d", i, simulationSteps);
    loop();
  }
  log(CLASS_MAIN, Debug, "### DONE");
  return 0;
}
bool inDeepSleepMode() {
  return true;
}

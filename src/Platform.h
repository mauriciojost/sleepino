#ifndef PLATFORM_INC
#define PLATFORM_INC

#include <Constants.h>

/**
 * This file contains common-to-any-platform declarations or functions:
 * - implementation of entry points for arduino programs (setup and loop functions)
 * - declaration of HW specific functions (the definition is in a per-platform-dedicated file)
 * - other functions that are not defined by HW specific but that use them, that are required by the module
 *   (so that it can be passed as callback).
 * The rest should be put in Module so that they can be tested regardless of the HW used behind.
 */

#define CLASS_PLATFORM "PL"

#define WIFI_CONNECTION_RETRIES 12

#ifndef HTTP_TIMEOUT_MS
#define HTTP_TIMEOUT_MS 10000
#endif // HTTP_TIMEOUT_MS

#ifndef USER_INTERACTION_LOOPS_MAX
#define USER_INTERACTION_LOOPS_MAX 40
#endif // USER_INTERACTION_LOOPS_MAX

#ifndef INVALID_THRESHOLD_SLEEP_CYCLE_SECS
#define INVALID_THRESHOLD_SLEEP_CYCLE_SECS (3600*6)
#endif // INVALID_THRESHOLD_SLEEP_CYCLE_SECS

#ifndef QUESTION_ANSWER_MAX_LENGTH
#define QUESTION_ANSWER_MAX_LENGTH 128
#endif // QUESTION_ANSWER_MAX_LENGTH


Buffer *logBuffer = NULL;
ModuleSleepino *m = NULL;

//////////////////////////////////////////////////////////////
// To be provided by the specific Platform (ESPXXX, X86, ...)
//////////////////////////////////////////////////////////////

// Callbacks
///////////////////

// Allow to retrieve the main4ino API login
const char *apiDeviceLogin();

// Allow to retrieve the main4ino API password
const char *apiDevicePass();

// The log function (that will print to screen, Serial, telnet, or whatever wished).
// It should not include "\n" ending as the log4ino library handles newline addition.
void logLine(const char *str);

// Stop wifi (and reduce power consumption).
void stopWifi();

// Message function.
// Parameters x/y are text coordinates: 1;0 means second line to the left.
void messageFunc(int x, int y, int color, bool wrap, MsgClearMode clear, int size, const char *str);

// Arms control function.
// Parameters left/right are from 0 to 9 included, 0 being down, 9 being up.
// Parameter steps defines how slow the movement is (1 being the fastest).
void arms(int left, int right, int steps);

// Clear device (for development purposes, to clear logs, stacktraces, etc)
void clearDevice();

// Show an image (either a catalog image or a custom bitmap)
void lcdImg(char img, uint8_t bitmap[]);

// Read a file from the filesystem (returns true if success)
bool readFile(const char *fname, Buffer *content);

// Write a file to the filesystem (returns true if success)
bool writeFile(const char *fname, const char *content);

// Display some useful info related to the HW
void infoArchitecture();

// Perform tests on the architecture
void testArchitecture();

// Execution
///////////////////

// Not interruptable sleep function.
void sleepNotInterruptable(time_t cycleBegin, time_t periodSec);

// Interruptable sleep function (haveToInterrupt called within).
// Returns true if it was interrupted.
bool sleepInterruptable(time_t cycleBegin, time_t periodSec);

// Setup step specific to the architecture, tell bot mode to go to
void setupArchitecture();

// Loop in run mode specific to the architecture
void runModeArchitecture();

// Handle an architecture specific command (if all the regular commands don't match).
// Returns true if the command requires the current wait batch to be interrupted (normally true with change of bot mode)
// Should provide a 'help' command too.
CmdExecStatus commandArchitecture(const char *command);

// Loop in configure mode specific to the architecture
void configureModeArchitecture();

// Abort execution (non-recoverable-error)
void abort(const char *msg);

// Read amount of seconds missing in deep sleep.
int readRemainingSecs();

// Write amount of seconds missing in deep sleep.
void writeRemainingSecs(int s);

// Get VCC measure in volts.
float vcc();

// Generic functions common to all architectures
///////////////////

void askStringQuestion(const char *question, Buffer *answer) {
  log(CLASS_PLATFORM, User, "Question: %s", question);
  Serial.readBytesUntil('\n', answer->getUnsafeBuffer(), QUESTION_ANSWER_MAX_LENGTH);
  answer->replace('\n', '\0');
  answer->replace('\r', '\0');
}

Buffer *initializeTuningVariable(Buffer **var, const char *filename, int maxLength, const char *defaultContent, bool obfuscate) {
	bool first = false;
  if (*var == NULL) {
  	first = true;
    *var = new Buffer(maxLength);
    bool succValue = readFile(filename, *var); // read value from file
    if (succValue) {                           // managed to retrieve the value
      log(CLASS_PLATFORM, Debug, "Read %s: OK", filename);
      (*var)->replace('\n', 0);                // minor formatting
    } else if (defaultContent != NULL) {       // failed to retrieve value, use default content if provided
      log(CLASS_PLATFORM, Debug, "Read %s: KO", filename);
      log(CLASS_PLATFORM, Debug, "Using default: %s", defaultContent);
      (*var)->fill(defaultContent);
    } else {
      Buffer buffer(QUESTION_ANSWER_MAX_LENGTH);
      askStringQuestion(filename, &buffer);
      writeFile(filename, buffer.getBuffer());
      (*var)->fill(buffer.getBuffer());
    }
  }
  if (first) {
    if (obfuscate) {
      log(CLASS_PLATFORM, Debug, "Tuning: %s=***", filename);
    } else {
      log(CLASS_PLATFORM, Debug, "Tuning: %s=%s", filename, (*var)->getBuffer());
    }
  }
  return *var;
}

Buffer *getLogBuffer() {
  return logBuffer;
}

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

bool initWifiSimple() {
  Settings *s = m->getModuleSettings();
  log(CLASS_PLATFORM, Info, "W.steady");
  bool connected = initializeWifi(s->getSsid(), s->getPass(), s->getSsidBackup(), s->getPassBackup(), true, WIFI_CONNECTION_RETRIES);
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
    log(CLASS_PLATFORM, Error, "Could not update");
  }
}

void deepSleepNotInterruptableCustom(time_t cycleBegin, time_t periodSecs) {
  if (periodSecs > INVALID_THRESHOLD_SLEEP_CYCLE_SECS) {
    log(CLASS_PLATFORM, Warn, "Invalid DS: %d", periodSecs);
    writeRemainingSecs(0); // clean RTC for next boot
  } else if (periodSecs <= MAX_SLEEP_CYCLE_SECS) {
    log(CLASS_PLATFORM, Debug, "Regular DS %d", periodSecs);
    writeRemainingSecs(0); // clean RTC for next boot
    deepSleepNotInterruptable(now(), periodSecs);
  } else {
    int remaining = periodSecs - MAX_SLEEP_CYCLE_SECS;
    log(CLASS_PLATFORM, Debug, "EDS: %d(+%d rem.)", MAX_SLEEP_CYCLE_SECS, remaining);
    writeRemainingSecs(remaining);
    deepSleepNotInterruptable(now(), MAX_SLEEP_CYCLE_SECS);
  }
}

void resumeExtendedDeepSleepIfApplicable() {
  int remainingSecs = readRemainingSecs();
  if (remainingSecs > INVALID_THRESHOLD_SLEEP_CYCLE_SECS) {
    log(CLASS_PLATFORM, Warn, "Invalid DS: %d", remainingSecs);
    writeRemainingSecs(0); // clean RTC for next boot
  } else if (remainingSecs > MAX_SLEEP_CYCLE_SECS) {
    log(CLASS_PLATFORM, Info, "EDS ongoing %d(+%d remaining)", MAX_SLEEP_CYCLE_SECS, remainingSecs);
    writeRemainingSecs(remainingSecs - MAX_SLEEP_CYCLE_SECS);
    deepSleepNotInterruptable(now(), MAX_SLEEP_CYCLE_SECS);
  } else if (remainingSecs > 0) {
    log(CLASS_PLATFORM, Info, "EDS ongoing %d (+0 remaining)", remainingSecs);
    writeRemainingSecs(0);
    deepSleepNotInterruptable(now(), remainingSecs);
  } else {
    log(CLASS_PLATFORM, Info, "No EDS ongoing");
  }
}

bool sleepInterruptable(time_t cycleBegin, time_t periodSecs) {
  int msec = (m==NULL?1000:m->getModuleSettings()->miniPeriodMsec());
  return lightSleepInterruptable(cycleBegin, periodSecs, msec, haveToInterrupt, heartbeat);
}

#endif // PLATFORM_INC

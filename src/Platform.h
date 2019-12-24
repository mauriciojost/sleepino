#ifndef PLATFORM_INC
#define PLATFORM_INC

/**
 * This file contains common-to-any-platform declarations or functions:
 * - implementation of entry points for arduino programs (setup and loop functions)
 * - declaration of HW specific functions (the definition is in a per-platform-dedicated file)
 * - other functions that are not defined by HW specific but that use them, that are required by the module
 *   (so that it can be passed as callback).
 * The rest should be put in Module so that they can be tested regardless of the HW used behind.
 */

#define CLASS_PLATFORM "PL"

#ifndef HTTP_TIMEOUT_MS
#define HTTP_TIMEOUT_MS 10000
#endif // HTTP_TIMEOUT_MS

#ifndef USER_INTERACTION_LOOPS_MAX
#define USER_INTERACTION_LOOPS_MAX 40
#endif // USER_INTERACTION_LOOPS_MAX

ModuleSleepino *m;

//////////////////////////////////////////////////////////////
// To be provided by the Main of a specific architecture
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
BotMode setupArchitecture();

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

// Generic functions common to all architectures
///////////////////

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
      abort(filename);
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


#endif // PLATFORM_INC

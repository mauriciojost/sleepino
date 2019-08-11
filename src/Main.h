#ifndef MAIN_INC
#define MAIN_INC

#define CLASS_MAIN "MA"

#include <ModuleSleepino.h>
#include <main4ino/Misc.h>

#ifndef PROJ_VERSION
#define PROJ_VERSION "snapshot"
#endif // PROJ_VERSION

enum WifiNetwork {
  WifiNoNetwork = 0,
  WifiMainNetwork,
  WifiBackupNetwork
};

//////////////////////////////////////////////////////////////
// Provided by generic Main
//////////////////////////////////////////////////////////////

// Standard arduino setup
void setup();

// Standard arduino loop
void loop();

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

// Setup wifi using provided parameters.
bool initWifi(const char *ssid, const char *pass, bool skipIfAlreadyConnected, int retries);

// Stop wifi (and reduce power consumption).
void stopWifi();

// HTTP GET function.
int httpGet(const char *url, ParamStream *response, Table *headers);

// HTTP POST function.
int httpPost(const char *url, const char *body, ParamStream *response, Table *headers);

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

// Update the firmware and restart the device
void updateFirmware(const char *descriptor);

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
      log(CLASS_MAIN, Debug, "Read %s: OK", filename);
      (*var)->replace('\n', 0);                // minor formatting
    } else if (defaultContent != NULL) {       // failed to retrieve value, use default content if provided
      log(CLASS_MAIN, Debug, "Read %s: KO", filename);
      log(CLASS_MAIN, Debug, "Using default: %s", defaultContent);
      (*var)->fill(defaultContent);
    } else {
      abort(filename);
    }
  }
  if (first) {
    if (obfuscate) {
      log(CLASS_MAIN, Debug, "Tuning: %s=***", filename);
    } else {
      log(CLASS_MAIN, Debug, "Tuning: %s=%s", filename, (*var)->getBuffer());
    }
  }
  return *var;
}


#endif // MAIN_INC

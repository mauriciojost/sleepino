#ifndef MODULE_BOTINO_INC
#define MODULE_BOTINO_INC

#include <Pinout.h>
#include <log4ino/Log.h>
#include <main4ino/Actor.h>

#include <actors/SleepinoSettings.h>
#include <mod4ino/Module.h>

#define CLASS_MODULEB "MB"

#define COMMAND_MAX_LENGTH 128

#define PERIOD_CONFIGURE_MSEC 4000

#define HELP_COMMAND_CLI_PROJECT                                                                                                           \
  "\n  BOTINO HELP"                                                                                                                        \
  "\n  move ...        : execute a move (example: 'move A00C55')"                                                                          \
  "\n  lcd ...         : write on display <x> <y> <color> <wrap> <clear> <size> <str>"                                                     \
  "\n  help            : show this help"                                                                                                   \
  "\n"

/**
 * This class represents the integration of all components (LCD, buttons, buzzer, etc).
 */
class ModuleSleepino {

private:
  // Main4ino Module
  Module *module;

  // Actors
  SleepinoSettings *bsettings;

  void (*message)(int x, int y, int color, bool wrap, MsgClearMode clear, int size, const char *str);

public:
  ModuleSleepino() {

    module = new Module();

    bsettings = new SleepinoSettings("sleepino");

    module->getActors()->add(1, (Actor *)bsettings);

    message = NULL;
  }

  void setup(BotMode (*setupArchitectureFunc)(),
             void (*messageFunc)(int x, int y, int color, bool wrap, MsgClearMode clear, int size, const char *str),
             bool (*initWifiFunc)(),
             int (*httpPostFunc)(const char *url, const char *body, ParamStream *response, Table *headers),
             int (*httpGetFunc)(const char *url, ParamStream *response, Table *headers),
             void (*clearDeviceFunc)(),
             bool (*fileReadFunc)(const char *fname, Buffer *content),
             bool (*fileWriteFunc)(const char *fname, const char *content),
             void (*abortFunc)(const char *msg),
             bool (*sleepInterruptableFunc)(time_t cycleBegin, time_t periodSec),
             void (*configureModeArchitectureFunc)(),
             void (*runModeArchitectureFunc)(),
             CmdExecStatus (*commandArchitectureFunc)(const char *cmd),
             void (*infoFunc)(),
             void (*updateFunc)(const char *),
             void (*testFunc)(),
             const char *(*apiDeviceLoginFunc)(),
             const char *(*apiDevicePassFunc)()) {

    module->setup(setupArchitectureFunc,
                  initWifiFunc,
                  httpPostFunc,
                  httpGetFunc,
                  clearDeviceFunc,
                  fileReadFunc,
                  fileWriteFunc,
                  abortFunc,
                  sleepInterruptableFunc,
                  configureModeArchitectureFunc,
                  runModeArchitectureFunc,
                  commandArchitectureFunc,
                  infoFunc,
                  updateFunc,
                  testFunc,
                  apiDeviceLoginFunc,
                  apiDevicePassFunc);

    message = messageFunc;

  }

  bool startupProperties() {
    return module->startupProperties();
  }

  /**
   * Handle a user command.
   */
  CmdExecStatus command(const char *cmd) {

    {
      Buffer b(cmd);
      logUser("\n> %s\n", b.getBuffer());

      if (b.getLength() == 0) {
        return NotFound;
      }

      char *c = strtok(b.getUnsafeBuffer(), " ");

      if (strcmp("lcd", c) == 0) {
        const char *x = strtok(NULL, " ");
        const char *y = strtok(NULL, " ");
        const char *color = strtok(NULL, " ");
        const char *wrap = strtok(NULL, " ");
        const char *clear = strtok(NULL, " ");
        const char *size = strtok(NULL, " ");
        const char *str = strtok(NULL, " ");
        if (x == NULL || y == NULL || color == NULL || wrap == NULL || clear == NULL || size == NULL || str == NULL) {
          logRaw(CLASS_MODULEB, Warn, "Arguments needed:\n  lcd <x> <y> <color> <wrap> <clear> <size> <str>");
          return InvalidArgs;
        }
        logUser("-> Lcd %s", str);
        message(atoi(x), atoi(y), atoi(color), atoi(wrap), (MsgClearMode)atoi(clear), atoi(size), str);
        return Executed;
      } else if (strcmp("help", c) == 0 || strcmp("?", c) == 0) {
        logRawUser(HELP_COMMAND_CLI_PROJECT);
        return module->command("?");
      }
      // deallocate buffer memory
    }
    // if none of the above went through
    return module->command(cmd);
  }

  /**
   * Execute a command given an index
   *
   * Thought to be used via single button devices, so that
   * a button pressed can execute one of many available commands.
   */
  void sequentialCommand(int index, bool dryRun) { }

  Settings *getModuleSettings() {
    return module->getSettings();
  }

  SleepinoSettings *getSleepinoSettings() {
    return bsettings;
  }

  Module *getModule() {
    return module;
  }

  SerBot *getBot() {
    return module->getBot();
  }

  void loop() {
    Buffer timeAux(19);
    Timing::humanize(getBot()->getClock()->currentTime(), &timeAux);
    timeAux.replace(' ', '\n');
    message(0, 0, 1, false, FullClear, 1, timeAux.getBuffer());
    module->loop();
  }
};

#endif // MODULE_BOTINO_INC

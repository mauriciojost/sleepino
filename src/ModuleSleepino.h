#ifndef MODULE_SLEEPINO_INC
#define MODULE_SLEEPINO_INC

#include <Pinout.h>
#include <Constants.h>
#include <log4ino/Log.h>
#include <main4ino/Actor.h>

#include <actors/SleepinoSettings.h>
#include <mod4ino/Module.h>

#define CLASS_MODULEB "MB"

#define COMMAND_MAX_LENGTH 128

#define HELP_COMMAND_CLI_PROJECT                                                                                                           \
  "\n  SLEEPINO HELP"                                                                                                                        \
  "\n  move ...        : execute a move (example: 'move A00C55')"                                                                          \
  "\n  lcd ...         : write on display <x> <y> <color> <wrap> <clear> <size> <str>"                                                     \
  "\n  help            : show this help"                                                                                                   \
  "\n"

bool alwaysTrue() {return true;}
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
  void (*commandFunc)(const char *str);

public:
  ModuleSleepino() {

    module = new Module();

    bsettings = new SleepinoSettings("sleepino");

    module->getActors()->add(1, (Actor *)bsettings);

    message = NULL;
    commandFunc = NULL;
  }

  void setup(BotMode (*setupArchitectureFunc)(),
             void (*messageFunc)(int x, int y, int color, bool wrap, MsgClearMode clear, int size, const char *str),
             bool (*initWifiFunc)(),
             void (*stopWifiFunc)(),
             int (*httpMethodFunc)(HttpMethod m, const char *url, const char *body, ParamStream *response, Table *headers, const char *fingerp),
             void (*clearDeviceFunc)(),
             bool (*fileReadFunc)(const char *fname, Buffer *content),
             bool (*fileWriteFunc)(const char *fname, const char *content),
             bool (*sleepInterruptableFunc)(time_t cycleBegin, time_t periodSec),
             void (*deepSleepNotInterruptableFunc)(time_t cycleBegin, time_t periodSec),
             void (*configureModeArchitectureFunc)(),
             void (*runModeArchitectureFunc)(),
             CmdExecStatus (*commandArchitectureFunc)(const char *cmd),
             void (*infoFunc)(),
             void (*updateFunc)(const char *, const char *),
             void (*testFunc)(),
             const char *(*apiDeviceLoginFunc)(),
             const char *(*apiDevicePassFunc)(),
             void (*cmdFunc)(const char*),
             Buffer *(*getLogBufferFunc)()
  ) {

    module->setup(PROJECT_ID,
                  PLATFORM_ID,
                  setupArchitectureFunc,
                  initWifiFunc,
                  stopWifiFunc,
                  httpMethodFunc,
                  clearDeviceFunc,
                  fileReadFunc,
                  fileWriteFunc,
                  sleepInterruptableFunc,
                  deepSleepNotInterruptableFunc,
                  configureModeArchitectureFunc,
                  runModeArchitectureFunc,
                  commandArchitectureFunc,
                  infoFunc,
                  updateFunc,
                  testFunc,
                  apiDeviceLoginFunc,
                  apiDevicePassFunc,
                  alwaysTrue,
                  getLogBufferFunc);

    message = messageFunc;
    commandFunc = cmdFunc;

    bsettings->setup(commandFunc);
  }

  ModuleStartupPropertiesCode startupProperties() {
    ModuleStartupPropertiesCode c = module->startupProperties();

    // if running once every while, stay with properties synchronization
      // at the beginning and before sleeping, and nothing else
    log(CLASS_MODULEB, Debug, "Force-skip acting synchronization");
    Buffer never("never");
    module->getPropSync()->setPropValue(PropSyncFreqProp, &never);
    module->getPropSync()->setPropValue(PropForceSyncFreqProp, &never);

    return c;

  }

  /**
   * Handle a user command.
   */
  CmdExecStatus command(const char *cmd) {

    {
      Buffer b(cmd);
      log(CLASS_MODULEB, Debug, "\n> %s\n", b.getBuffer());

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
        log(CLASS_MODULEB, User, "-> Lcd %s", str);
        message(atoi(x), atoi(y), atoi(color), atoi(wrap), (MsgClearMode)atoi(clear), atoi(size), str);
        return Executed;
      } else if (strcmp("help", c) == 0 || strcmp("?", c) == 0) {
        logRaw(CLASS_MODULEB, User, HELP_COMMAND_CLI_PROJECT);
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

  Clock *getClock() {
    return module->getBot()->getClock();
  }

  void loop() {
    module->loop();
  }
};

#endif // MODULE_SLEEPINO_INC

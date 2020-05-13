#ifndef MODULE_SLEEPINO_INC
#define MODULE_SLEEPINO_INC

#include <Pinout.h>
#include <Constants.h>
#include <log4ino/Log.h>
#include <main4ino/Actor.h>

#include <actors/SleepinoSettings.h>
#include <actors/Battery.h>
#include <mod4ino/Module.h>

#define CLASS_MODULEB "MB"

#define COMMAND_MAX_LENGTH 128

#define HELP_COMMAND_CLI_PROJECT                                                                                                           \
  "\n  SLEEPINO HELP"                                                                                                                      \
  "\n  lcd ...         : write on display <x> <y> <color> <wrap> <clear> <size> <str>"                                                     \
  "\n  servo ...       : control servo <idx> and put it in position <pos>"                                                                 \
  "\n  io ...          : control pin <pin> and put it in level <out>"                                                                      \
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
  Battery *battery;

  void (*message)(int x, int y, int color, bool wrap, MsgClearMode clear, int size, const char *str);
  void (*commandFunc)(const char *str);
  void (*servo)(int idx, int pos);
  void (*io)(int pin, int level);


public:
  ModuleSleepino() {

    module = new Module();

    bsettings = new SleepinoSettings("sleepino");
    battery = new Battery("battery");

    module->getActors()->add(2, (Actor *)bsettings, (Actor *)battery);

    message = NULL;
    commandFunc = NULL;
    servo = NULL;
    io = NULL;
  }

  void setup(
             void (*messageFunc)(int x, int y, int color, bool wrap, MsgClearMode clear, int size, const char *str),
             bool (*initWifiFunc)(),
             void (*stopWifiFunc)(),
             HttpResponse (*httpMethodFunc)(HttpMethod m, const char *url, Stream *body, Table *headers, const char *fingerprint),
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
             Buffer *(*getLogBufferFunc)(),
             float (*vcc)(),
             void (*servoFunc)(int, int),
             void (*ioFunc)(int, int)
  ) {
    message = messageFunc;
    commandFunc = cmdFunc;
    servo = servoFunc;
    io = ioFunc;
    bsettings->setup(commandFunc);
    battery->setup(vcc);

    module->setup(PROJECT_ID,
                  PLATFORM_ID,
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

  }

  StartupStatus startupProperties() {
    StartupStatus c = module->startupProperties();

    // if running once every while, stay with properties synchronization
    // at the beginning and before sleeping, and nothing else
    log(CLASS_MODULEB, Debug, "Force-skip acting synchronization");
    Buffer never("never");
    module->getPropSync()->setPropValue(PropSyncFreqProp, &never);
    module->getPropSync()->setPropValue(PropSyncForceSyncFreqProp, &never);

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
      } else if (strcmp("servo", c) == 0) {
        const char *sidx = strtok(NULL, " ");
        const char *pos = strtok(NULL, " ");
        if (sidx == NULL || pos == NULL) {
          logRaw(CLASS_MODULEB, Warn, "Arguments needed:\n  servo <idx> <pos>");
          return InvalidArgs;
        }
        log(CLASS_MODULEB, User, "-> Servo %s %s", sidx, pos);
        servo(atoi(sidx), atoi(pos));
        return Executed;
      } else if (strcmp("io", c) == 0) {
        const char *pin = strtok(NULL, " ");
        const char *out = strtok(NULL, " ");
        if (pin == NULL || out == NULL) {
          logRaw(CLASS_MODULEB, Warn, "Arguments needed:\n  io <pin> <out>");
          return InvalidArgs;
        }
        log(CLASS_MODULEB, User, "-> Io %s %s", pin, out);
        io(atoi(pin), atoi(out));
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

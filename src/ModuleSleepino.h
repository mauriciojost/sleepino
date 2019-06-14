#ifndef MODULE_BOTINO_INC
#define MODULE_BOTINO_INC

#include <Pinout.h>
#include <log4ino/Log.h>
#include <main4ino/Actor.h>

#include <mod4ino/Module.h>
#include <actors/Notifier.h>
#include <actors/SleepinoSettings.h>

#define CLASS_MODULEB "MB"

#define COMMAND_MAX_LENGTH 128

#define PERIOD_CONFIGURE_MSEC 4000

#define HELP_COMMAND_CLI_PROJECT                                                                                                           \
  "\n  BOTINO HELP"                                                                                                                        \
  "\n  move ...        : execute a move (example: 'move A00C55')"                                                                          \
  "\n  lcd ...         : write on display <x> <y> <color> <wrap> <clear> <size> <str>"                                                     \
  "\n  ack             : notification read"                                                                                                \
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
  Notifier *notifier;
  SleepinoSettings *bsettings;

  void (*message)(int x, int y, int color, bool wrap, MsgClearMode clear, int size, const char *str);

public:
  ModuleSleepino() {

    module = new Module();

    bsettings = new SleepinoSettings("sleepino");
    notifier = new Notifier("notifier");

    module->getActors()->add(2, (Actor *)bsettings, (Actor *)notifier);

    message = NULL;

  }

  void setup(BotMode (*setupArchitectureFunc)(),
             void (*lcdImgFunc)(char img, uint8_t bitmap[]),
             void (*armsFunc)(int left, int right, int steps),
             void (*messageFunc)(int x, int y, int color, bool wrap, MsgClearMode clear, int size, const char *str),
             void (*iosFunc)(char led, IoMode v),
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
             void (*updateFunc)(const char*),
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

    notifier->setup(lcdImgFunc, messageFunc);
  }


  bool startupProperties() {
    return module->startupProperties();
  }

  void ackCmd() {
    notifier->notificationRead();
    zCmd();
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

      if (strcmp("ack", c) == 0) {
        ackCmd();
        log(CLASS_MODULEB, Info, "Notification read");
        return Executed;
      } else if (strcmp("lcd", c) == 0) {
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
  void sequentialCommand(int index, bool dryRun) {
    switch (index) {
      case 0:
      case 1:
      case 2:
      case 3:
      case 4:
      case 5:
      case 6:
      case 7: {
        int ind = index - 0;
        const char *mvName = getCommands()->getCmdName(ind);
        getNotifier()->message(0, 2, "%s?", mvName);
        if (!dryRun) {
          command(getCommands()->getCmdValue(ind));
        }
      } break;
      case 8: {
        getNotifier()->message(0, 2, "All act?");
        if (!dryRun) {
          module->actall();
          getNotifier()->message(0, 1, "All act one-off");
        }
      } break;
      case 9: {
        getNotifier()->message(0, 2, "Config mode?");
        if (!dryRun) {
          module->confCmd();
          getNotifier()->message(0, 1, "In config mode");
        }
      } break;
      case 10: {
        getNotifier()->message(0, 2, "Run mode?");
        if (!dryRun) {
          module->runCmd();
          getNotifier()->message(0, 1, "In run mode");
        }
      } break;
      case 11: {
        getNotifier()->message(0, 2, "Show info?");
        if (!dryRun) {
          module->infoCmd();
        }
      } break;
      default: {
        getNotifier()->message(0, 2, "Abort?");
        if (!dryRun) {
        	zCmd();
        }
      } break;
    }
  }

  Notifier *getNotifier() {
    return notifier;
  }

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
  	module->loop();
  }
};

#endif // MODULE_BOTINO_INC

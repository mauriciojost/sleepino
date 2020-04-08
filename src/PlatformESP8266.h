#include <Adafruit_GFX.h>      // include adafruit graphics library
#include <Adafruit_PCD8544.h>  // include adafruit PCD8544 (Nokia 5110) library
#include <Arduino.h>
#include <Platform.h>
#ifdef OTA_ENABLED
#include <ArduinoOTA.h>
#endif // OTA_ENABLED
#include <ESP8266HTTPClient.h>
#include <ESP8266WiFi.h>
#include <ESP8266httpUpdate.h>
#include <EspSaveCrash.h>
#include <FS.h>
#include <Pinout.h>
#ifdef TELNET_ENABLED
#include <RemoteDebug.h>
#endif // TELNET_ENABLED
#include <SPI.h>
#include <Wire.h>
#include <primitives/BoardESP8266.h>

#define MAX_SLEEP_CYCLE_SECS 1800 // 30min

#define DELAY_MS_SPI 1
#define HW_STARTUP_DELAY_MSECS 10

#define DEVICE_ALIAS_FILENAME "/alias.tuning"
#define DEVICE_ALIAS_MAX_LENGTH 16

#define DEVICE_PWD_FILENAME "/pass.tuning"
#define DEVICE_PWD_MAX_LENGTH 16

#define DEVICE_CONTRAST_FILENAME "/contrast.tuning"
#define DEVICE_CONTRAST_MAX_LENGTH 3

#define SLEEP_PERIOD_UPON_BOOT_SEC 2
#define SLEEP_PERIOD_UPON_ABORT_SEC 600

#define SLEEP_PERIOD_PRE_ABORT_SEC 5

#define LCD_CHAR_WIDTH 6
#define LCD_CHAR_HEIGHT 8
#define LCD_DEFAULT_BIAS 0x17

#define NEXT_LOG_LINE_ALGORITHM ((currentLogLine + 1) % 6)

#define LOG_BUFFER_MAX_LENGTH 1024

#define VCC_FLOAT ((float)ESP.getVcc() / 1024)

extern "C" {
#include "user_interface.h"
}

#define HELP_COMMAND_ARCH_CLI                                                                                                              \
  "\n  ESP8266 HELP"                                                                                                                       \
  "\n  init              : initialize essential settings (wifi connection, logins, etc.)"                                                  \
  "\n  rm ...            : remove file in FS "                                                                                             \
  "\n  lcdcont ...       : change lcd contrast"                                                                                            \
  "\n  ls                : list files present in FS "                                                                                      \
  "\n  reset             : reset the device"                                                                                               \
  "\n  freq ...          : set clock frequency in MHz (80 or 160 available only, 160 faster but more power consumption)"                   \
  "\n  deepsleep ...     : deep sleep N provided seconds"                                                                                  \
  "\n  lightsleep ...    : light sleep N provided seconds"                                                                                 \
  "\n  clearstack        : clear stack trace "                                                                                             \
  "\n"

#ifdef TELNET_ENABLED
RemoteDebug telnet;
#endif // TELNET_ENABLED
Adafruit_PCD8544* lcd = NULL;
Buffer *apiDeviceId = NULL;
Buffer *apiDevicePwd = NULL;
Buffer *contrast = NULL;
int currentLogLine = 0;
Buffer *cmdBuffer = NULL;
Buffer *cmdLast = NULL;
EspSaveCrash espSaveCrash;

ADC_MODE(ADC_VCC);

float vcc();
void reactCommandCustom();
void heartbeat();
bool lightSleepInterruptable(time_t cycleBegin, time_t periodSecs);
void deepSleepNotInterruptableSecs(time_t cycleBegin, time_t periodSecs);
void debugHandle();
bool haveToInterrupt();
void handleInterrupt();
void dumpLogBuffer();
int lcdContrast();

////////////////////////////////////////
// Functions requested for architecture
////////////////////////////////////////

// Callbacks
///////////////////

float vcc() {
  return VCC_FLOAT;
}

const char *apiDeviceLogin() {
  return initializeTuningVariable(&apiDeviceId, DEVICE_ALIAS_FILENAME, DEVICE_ALIAS_MAX_LENGTH, NULL, false)->getBuffer();
}

const char *apiDevicePass() {
  return initializeTuningVariable(&apiDevicePwd, DEVICE_PWD_FILENAME, DEVICE_PWD_MAX_LENGTH, NULL, true)->getBuffer();
}

void logLine(const char *str, const char *clz, LogLevel l, bool newline) {
  int ts = (int)((millis()/1000) % 10000);
  Buffer aux(8);
  aux.fill("%04d|", ts);
  // serial print
#ifdef HEAP_VCC_LOG
  Serial.print("HEA:");
  Serial.print(ESP.getFreeHeap());
  Serial.print("|");
  Serial.print("VCC:");
  Serial.print(VCC_FLOAT);
  Serial.print("|");
#endif // HEAP_VCC_LOG
  Serial.print(str);
  // telnet print
#ifdef TELNET_ENABLED
  if (telnet.isActive()) {
    for (unsigned int i = 0; i < strlen(str); i++) {
      telnet.write(str[i]);
    }
  }
#endif // TELNET_ENABLED
  bool lcdLogsEnabled = (m==NULL?true:m->getBotinoSettings()->getLcdLogs());
  bool fsLogsEnabled = (m==NULL?true:m->getBotinoSettings()->fsLogsEnabled());
  int fsLogsLength = (m==NULL?DEFAULT_FS_LOGS_LENGTH:m->getBotinoSettings()->getFsLogsLength());

  // lcd print
  if (lcd != NULL && lcdLogsEnabled) { // can be called before LCD initialization
    currentLogLine = NEXT_LOG_LINE_ALGORITHM;
    int line = currentLogLine + 2;
    lcd->setTextWrap(false);
    lcd->fillRect(0, line * LCD_CHAR_HEIGHT, 84, LCD_CHAR_HEIGHT, WHITE);
    lcd->setTextSize(1);
    lcd->setTextColor(BLACK);
    lcd->setCursor(0, line * LCD_CHAR_HEIGHT);
    lcd->print(str);
    lcd->display();
    delay(DELAY_MS_SPI);
  }
  // local logs (to be sent via network)
  if (fsLogsEnabled) {
    if (logBuffer == NULL) {
      logBuffer = new Buffer(LOG_BUFFER_MAX_LENGTH);
    }
    if (newline) {
      logBuffer->append(aux.getBuffer());
    }
    unsigned int s = (unsigned int)(fsLogsLength) + 1;
    char aux2[s];
    strncpy(aux2, str, s);
    aux2[s - 1] = 0;
    aux2[s - 2] = '\n';
    logBuffer->append(aux2);
  }
}

void messageFunc(int x, int y, int color, bool wrap, MsgClearMode clearMode, int size, const char *str) {
  switch (clearMode) {
    case FullClear:
      lcd->clearDisplay();
      break;
    case LineClear:
      lcd->fillRect(x * size * LCD_CHAR_WIDTH, y * size * LCD_CHAR_HEIGHT, 128, size * LCD_CHAR_HEIGHT, !color);
      wrap = false;
      break;
    case NoClear:
      break;
  }
  lcd->setTextWrap(wrap);
  lcd->setTextSize(size);
  lcd->setTextColor(color);
  lcd->setCursor(x * size * LCD_CHAR_WIDTH, y * size * LCD_CHAR_HEIGHT);
  lcd->print(str);
  lcd->display();
  log(CLASS_PLATFORM, Debug, "Msg(%d,%d):%s", x, y, str);
  delay(DELAY_MS_SPI);
}

void clearDevice() {
  log(CLASS_PLATFORM, User, "   rm %s", DEVICE_ALIAS_FILENAME);
  log(CLASS_PLATFORM, User, "   rm %s", DEVICE_PWD_FILENAME);
  log(CLASS_PLATFORM, User, "   ls");
  log(CLASS_PLATFORM, User, "   <remove all .properties>");
  espSaveCrash.clear();
}

void infoArchitecture() {}

void testArchitecture() {}

// Execution
///////////////////

BotMode setupArchitecture() {

  // Let HW startup
  delay(HW_STARTUP_DELAY_MSECS);

  // Intialize the logging framework
  Serial.begin(115200);     // Initialize serial port
  Serial.setTimeout(1000); // Timeout for read
  setupLog(logLine);
  log(CLASS_PLATFORM, Info, "Log initialized");

  log(CLASS_PLATFORM, Debug, "Setup cmds");
  cmdBuffer = new Buffer(COMMAND_MAX_LENGTH);
  cmdLast = new Buffer(COMMAND_MAX_LENGTH);

  log(CLASS_PLATFORM, Debug, "Setup timing");
  setExternalMillis(millis);


  log(CLASS_PLATFORM, Debug, "Setup LCD");
  lcd = new Adafruit_PCD8544(LCD_CLK_PIN, LCD_DIN_PIN, LCD_DC_PIN, LCD_CS_PIN, LCD_RST_PIN);
  lcd->begin(lcdContrast(), LCD_DEFAULT_BIAS);
  delay(DELAY_MS_SPI);

  heartbeat();

  log(CLASS_PLATFORM, Debug, "Setup wdt");
  ESP.wdtEnable(1); // argument not used

  log(CLASS_PLATFORM, Debug, "Setup wifi");
  WiFi.persistent(false);
  WiFi.hostname(apiDeviceLogin());
  heartbeat();

  log(CLASS_PLATFORM, Debug, "Setup http");
  httpClient.setTimeout(HTTP_TIMEOUT_MS);
  heartbeat();

  log(CLASS_PLATFORM, Debug, "Setup commands");
#ifdef TELNET_ENABLED
  telnet.setCallBackProjectCmds(reactCommandCustom);
  String helpCli("Type 'help' for help");
  telnet.setHelpProjectsCmds(helpCli);
#endif // TELNET_ENABLED
  heartbeat();

  if (espSaveCrash.count() > 5) {
    log(CLASS_PLATFORM, Warn, "Crshs:%d", espSaveCrash.count());
    log(CLASS_PLATFORM, Warn, "Too many Stack-trcs / clearing (!!!)");
    espSaveCrash.clear();
  } else if (espSaveCrash.count() > 0) {
    log(CLASS_PLATFORM, Warn, "Crshs:%d", espSaveCrash.count());
    char logBfr[256];
    espSaveCrash.print(logBfr, 256);
    logRaw(CLASS_PLATFORM, Warn, logBfr);
  } else {
    log(CLASS_PLATFORM, Debug, "No crashes");
  }

  log(CLASS_PLATFORM, Debug, "Wait user...");
  bool i = sleepInterruptable(now(), SLEEP_PERIOD_UPON_BOOT_SEC);
  if (i) {
    log(CLASS_PLATFORM, Info, "SetpOK:confmode");
    return ConfigureMode;
  } else {
    log(CLASS_PLATFORM, Info, "SetpOK:runmode");
    return RunMode;
  }

}

void runModeArchitecture() {

  // display lcd metrics (time, vcc, version)
  Buffer timeAux(32);
  Timing::humanize(m->getClock()->currentTime(), &timeAux);
  timeAux.replace(' ', '\n');

  Buffer lcdAux(64);

  lcdAux.fill("%s\nVcc: %0.4f\nV:%s", timeAux.getBuffer(), VCC_FLOAT, STRINGIFY(PROJ_VERSION));

  messageFunc(0, 0, 1, false, FullClear, 1, lcdAux.getBuffer());

  handleInterrupt();
  debugHandle();

}

CmdExecStatus commandArchitecture(const char *c) {
  if (strcmp("init", c) == 0) {
    logRaw(CLASS_PLATFORM, User, "-> Initialize");
    logRaw(CLASS_PLATFORM, User, "Execute:");
    logRaw(CLASS_PLATFORM, User, "   ls");
    log(CLASS_PLATFORM, User, "   save %s <alias>", DEVICE_ALIAS_FILENAME);
    log(CLASS_PLATFORM, User, "   save %s <pwd>", DEVICE_PWD_FILENAME);
    logRaw(CLASS_PLATFORM, User, "   wifissid <ssid>");
    logRaw(CLASS_PLATFORM, User, "   wifipass <password>");
    log(CLASS_PLATFORM, User, "   save %s <contrast-0-100>", DEVICE_CONTRAST_FILENAME);
    logRaw(CLASS_PLATFORM, User, "   (setup of power consumption settings architecture specific if any)");
    logRaw(CLASS_PLATFORM, User, "   store");
    logRaw(CLASS_PLATFORM, User, "   ls");
    return Executed;
  } else if (strcmp("ls", c) == 0) {
    SPIFFS.begin();
    Dir dir = SPIFFS.openDir("/");
    while (dir.next()) {
      log(CLASS_PLATFORM, User, "- %s (%d bytes)", dir.fileName().c_str(), (int)dir.fileSize());
    }
    SPIFFS.end();
    return Executed;
  } else if (strcmp("rm", c) == 0) {
    const char *f = strtok(NULL, " ");
    SPIFFS.begin();
    bool succ = SPIFFS.remove(f);
    log(CLASS_PLATFORM, User, "### File '%s' %s removed", f, (succ ? "" : "NOT"));
    SPIFFS.end();
    return Executed;
  } else if (strcmp("lcdcont", c) == 0) {
    const char *c = strtok(NULL, " ");
    int i = atoi(c);
    log(CLASS_PLATFORM, User, "Set contrast to: %d", i);
    lcd->setContrast(i);
    return Executed;
  } else if (strcmp("reset", c) == 0) {
    ESP.restart(); // it is normal that it fails if invoked the first time after firmware is written
    return Executed;
  } else if (strcmp("freq", c) == 0) {
    uint8 fmhz = (uint8)atoi(strtok(NULL, " "));
    bool succ = system_update_cpu_freq(fmhz);
    log(CLASS_PLATFORM, User, "Freq updated: %dMHz (succ %s)", (int)fmhz, BOOL(succ));
    return Executed;
  } else if (strcmp("deepsleep", c) == 0) {
    int s = atoi(strtok(NULL, " "));
    deepSleepNotInterruptableSecs(now(), s);
    return Executed;
  } else if (strcmp("lightsleep", c) == 0) {
    int s = atoi(strtok(NULL, " "));
    return (sleepInterruptable(now(), s) ? ExecutedInterrupt : Executed);
  } else if (strcmp("clearstack", c) == 0) {
    espSaveCrash.clear();
    return Executed;
  } else if (strcmp("help", c) == 0 || strcmp("?", c) == 0) {
    logRaw(CLASS_PLATFORM, User, HELP_COMMAND_ARCH_CLI);
    return Executed;
  } else {
    return NotFound;
  }
}

void configureModeArchitecture() {
  handleInterrupt();
  debugHandle();
}

void abort(const char *msg) {
  log(CLASS_PLATFORM, Error, "Abort: %s", msg);
  log(CLASS_PLATFORM, Warn, "Will deep sleep upon abort...");
  bool inte = sleepInterruptable(now(), SLEEP_PERIOD_PRE_ABORT_SEC);
  if (!inte) {
    deepSleepNotInterruptableSecs(now(), SLEEP_PERIOD_UPON_ABORT_SEC);
  } else {
    m->getBot()->setMode(ConfigureMode);
    log(CLASS_PLATFORM, Warn, "Abort skipped");
  }
}

#define USER_RTC_OFFSET 64
int readRemainingSecs() {
  //from 256 is user mem
  int s;
  bool res = system_rtc_mem_read(USER_RTC_OFFSET, &s, sizeof(int));
  if (res) {
    return s;
  } else {
    log(CLASS_PLATFORM, Debug, "No ds remaining");
    return -1;
  }
}

void writeRemainingSecs(int s) {
  bool res = system_rtc_mem_write(USER_RTC_OFFSET, &s, sizeof(int));
  if (!res) {
    log(CLASS_PLATFORM, Warn, "Failed to write remaining");
  }
}

////////////////////////////////////////
// Architecture specific functions
////////////////////////////////////////

void debugHandle() {
  if (!m->getModuleSettings()->getDebug()) {
    return;
  }
  static bool firstTime = true;
  Serial.setDebugOutput(getLogLevel() == Debug && m->getModuleSettings()->getDebug()); // deep HW logs
  if (firstTime) {
    log(CLASS_PLATFORM, Debug, "Initialize debuggers...");
#ifdef TELNET_ENABLED
    telnet.begin(apiDeviceLogin()); // Intialize the remote logging framework
#endif // TELNET_ENABLED
#ifdef OTA_ENABLED
    ArduinoOTA.begin();             // Intialize OTA
#endif // OTA_ENABLED
    firstTime = false;
  }

  m->getSleepinoSettings()->getStatus()->fill("freeheap:%d", ESP.getFreeHeap());
  m->getSleepinoSettings()->getMetadata()->changed();

#ifdef TELNET_ENABLED
  telnet.handle();     // Handle telnet log server and commands
#endif // TELNET_ENABLED
#ifdef OTA_ENABLED
  ArduinoOTA.handle(); // Handle on the air firmware load
#endif // OTA_ENABLED
}

void reactCommandCustom() { // for the use via telnet
#ifdef TELNET_ENABLED
  m->command(telnet.getLastCommand().c_str());
#endif // TELNET_ENABLED
}

void heartbeat() {
  digitalWrite(GPIO2_PIN, HIGH); // built-in LCD... warning, overlaps with LCD!
  delay(10);
  digitalWrite(GPIO2_PIN, LOW);
}

void handleInterrupt() {
  if (Serial.available()) {
    // Handle serial commands
    uint8_t c;

    while (true) {
      int inLoop = 0;
      size_t n = Serial.readBytes(&c, 1);

      if (c == 0x08 && n == 1) { // backspace
        log(CLASS_PLATFORM, Debug, "Backspace");
        if (cmdBuffer->getLength() > 0) {
          cmdBuffer->getUnsafeBuffer()[cmdBuffer->getLength() - 1] = 0;
        }
      } else if (c == 0x1b && n == 1) { // up/down
        log(CLASS_PLATFORM, Debug, "Up/down");
        cmdBuffer->load(cmdLast->getBuffer());
      } else if ((c == '\n' || c == '\r') && n == 1) { // if enter is pressed...
        log(CLASS_PLATFORM, Debug, "Enter");
        if (cmdBuffer->getLength() > 0) {
          CmdExecStatus execStatus = m->command(cmdBuffer->getBuffer());
          bool interrupt = (execStatus == ExecutedInterrupt);
          log(CLASS_PLATFORM, Debug, "Interrupt: %d", interrupt);
          log(CLASS_PLATFORM, Debug, "Cmd status: %s", CMD_EXEC_STATUS(execStatus));
          log(CLASS_PLATFORM, User, "('%s' => %s)", cmdBuffer->getBuffer(), CMD_EXEC_STATUS(execStatus));
          cmdLast->load(cmdBuffer->getBuffer());
          cmdBuffer->clear();
        }
        break;
      } else if (n == 1) {
        cmdBuffer->append(c);
      }
      // echo
      log(CLASS_PLATFORM, User, "> %s (%d)", cmdBuffer->getBuffer(), (int)c);
      while (!Serial.available() && inLoop < USER_INTERACTION_LOOPS_MAX) {
        inLoop++;
        delay(100);
      }
      if (inLoop >= USER_INTERACTION_LOOPS_MAX) {
        log(CLASS_PLATFORM, User, "> (timeout)");
        break;
      }
    }
    log(CLASS_PLATFORM, Debug, "Done with interrupt");

  }
}

bool haveToInterrupt() {
  if (Serial.available()) {
    log(CLASS_PLATFORM, Debug, "Serial pinged: int");
    return true;
  } else {
    return false;
  }
}


int lcdContrast() {
  return atoi(initializeTuningVariable(&contrast, DEVICE_CONTRAST_FILENAME, DEVICE_CONTRAST_MAX_LENGTH, "50", false)->getBuffer());
}



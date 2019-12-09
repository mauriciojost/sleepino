
#include <Adafruit_GFX.h>      // include adafruit graphics library
#include <Adafruit_PCD8544.h>  // include adafruit PCD8544 (Nokia 5110) library
#include <Arduino.h>
#ifdef OTA_ENABLED
#include <ArduinoOTA.h>
#endif // OTA_ENABLED
#include <ESP8266HTTPClient.h>
#include <ESP8266WiFi.h>
#include <ESP8266httpUpdate.h>
#include <EspSaveCrash.h>
#include <FS.h>
#include <Main.h>
#include <Pinout.h>
#ifdef TELNET_ENABLED
#include <RemoteDebug.h>
#endif // TELNET_ENABLED
#include <SPI.h>
#include <Wire.h>
#include <primitives/BoardESP8266.h>

#define DELAY_MS_SPI 1
#define ABORT_DELAY_SECS 5
#define HW_STARTUP_DELAY_MSECS 10

#define DEVICE_ALIAS_FILENAME "/alias.tuning"
#define DEVICE_ALIAS_MAX_LENGTH 16

#define DEVICE_PWD_FILENAME "/pass.tuning"
#define DEVICE_PWD_MAX_LENGTH 16

#define DEVICE_DSLEEP_FILENAME "/deepsleep.tuning"
#define DEVICE_DSLEEP_MAX_LENGTH 1

#define DEVICE_CONTRAST_FILENAME "/contrast.tuning"
#define DEVICE_CONTRAST_MAX_LENGTH 3

#define SLEEP_PERIOD_UPON_BOOT_SEC 2
#define SLEEP_PERIOD_UPON_ABORT_SEC 600

#define SLEEP_PERIOD_PRE_ABORT_SEC 5

#define MAX_DEEP_SLEEP_PERIOD_SECS 2100 // 35 minutes

#define LCD_PIXEL_WIDTH 6
#define LCD_PIXEL_HEIGHT 8
#define LCD_DEFAULT_BIAS 0x17

#define MAX_ROUND_ROBIN_LOG_FILES 5

#ifndef FIRMWARE_UPDATE_URL
#define FIRMWARE_UPDATE_URL MAIN4INOSERVER_API_HOST_BASE "/firmwares/sleepino/esp8266.%s.bin"
#endif // FIRMWARE_UPDATE_URL

#define FIRMWARE_UPDATE_URL_MAX_LENGTH 128

#define PRE_DEEP_SLEEP_WINDOW_SECS 5

#define NEXT_LOG_LINE_ALGORITHM ((currentLogLine + 1) % 6)

#define LOG_BUFFER_MAX_LENGTH 1024

#define VCC_FLOAT ((float)ESP.getVcc() / 1024)

#define ONLY_SHOW_MSG true
#define SHOW_MSG_AND_REACT false

#define FPM_SLEEP_MAX_TIME 0xFFFFFFF

extern "C" {
#include "user_interface.h"
}

#ifndef HTTP_TIMEOUT_MS
#define HTTP_TIMEOUT_MS 60000
#endif // HTTP_TIMEOUT_MS


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
Buffer *deepSleepMode = NULL;
Buffer *contrast = NULL;
int currentLogLine = 0;
Buffer *logBuffer = NULL;
Buffer *cmdBuffer = NULL;
Buffer *cmdLast = NULL;
EspSaveCrash espSaveCrash;

ADC_MODE(ADC_VCC);

void bitmapToLcd(uint8_t bitmap[]);
void reactCommandCustom();
void heartbeat();
bool lightSleepInterruptable(time_t cycleBegin, time_t periodSecs);
void deepSleepNotInterruptableSecs(time_t cycleBegin, time_t periodSecs);
void debugHandle();
bool haveToInterrupt();
void handleInterrupt();
void dumpLogBuffer();
bool inDeepSleepMode();
int lcdContrast();

////////////////////////////////////////
// Functions requested for architecture
////////////////////////////////////////

// Callbacks
///////////////////

const char *apiDeviceLogin() {
  return initializeTuningVariable(&apiDeviceId, DEVICE_ALIAS_FILENAME, DEVICE_ALIAS_MAX_LENGTH, NULL, false)->getBuffer();
}

const char *apiDevicePass() {
  return initializeTuningVariable(&apiDevicePwd, DEVICE_PWD_FILENAME, DEVICE_PWD_MAX_LENGTH, NULL, true)->getBuffer();
}

void logLine(const char *str, const char *clz, LogLevel l) {
  Serial.setDebugOutput(getLogLevel() == Debug && m->getModuleSettings()->getDebug()); // deep HW logs
  int ts = (int)(now() % 10000);
  Buffer aux(8);
  aux.fill("%d|", ts);

  Serial.print(ts);
  Serial.print('|');
  Serial.print(str);
#ifdef TELNET_ENABLED
  // telnet print
  if (telnet.isActive()) {
    for (unsigned int i = 0; i < strlen(str); i++) {
      telnet.write(str[i]);
    }
  }
#endif // TELNET_ENABLED
  // lcd print
  if (lcd != NULL && m->getSleepinoSettings()->getLcdLogs()) { // can be called before LCD initialization
    currentLogLine = NEXT_LOG_LINE_ALGORITHM;
    int line = currentLogLine + 2;
    lcd->setTextWrap(false);
    lcd->fillRect(0, line * LCD_PIXEL_HEIGHT, 84, LCD_PIXEL_HEIGHT, WHITE);
    lcd->setTextSize(1);
    lcd->setTextColor(BLACK);
    lcd->setCursor(0, line * LCD_PIXEL_HEIGHT);
    lcd->print(str);
    lcd->display();
    delay(DELAY_MS_SPI);
  }
  // local logs (to be sent via network)
  if (m->getSleepinoSettings()->fsLogsEnabled()) {
    if (logBuffer == NULL) {
      logBuffer = new Buffer(LOG_BUFFER_MAX_LENGTH);
    }
    logBuffer->append(aux.getBuffer());
    logBuffer->append(str);
  }
}

void messageFunc(int x, int y, int color, bool wrap, MsgClearMode clearMode, int size, const char *str) {
  switch (clearMode) {
    case FullClear:
      lcd->clearDisplay();
      break;
    case LineClear:
      lcd->fillRect(x * size * LCD_PIXEL_WIDTH, y * size * LCD_PIXEL_HEIGHT, 128, size * LCD_PIXEL_HEIGHT, !color);
      wrap = false;
      break;
    case NoClear:
      break;
  }
  lcd->setTextWrap(wrap);
  lcd->setTextSize(size);
  lcd->setTextColor(color);
  lcd->setCursor(x * size * LCD_PIXEL_WIDTH, y * size * LCD_PIXEL_HEIGHT);
  lcd->print(str);
  lcd->display();
  log(CLASS_MAIN, Debug, "Msg(%d,%d):%s", x, y, str);
  delay(DELAY_MS_SPI);
}

void clearDevice() {
  log(CLASS_MAIN, User, "   rm %s", DEVICE_ALIAS_FILENAME);
  log(CLASS_MAIN, User, "   rm %s", DEVICE_PWD_FILENAME);
  log(CLASS_MAIN, User, "   ls");
  log(CLASS_MAIN, User, "   <remove all .properties>");
  espSaveCrash.clear();
}

void infoArchitecture() {}

void testArchitecture() {}

// Execution
///////////////////

bool sleepInterruptable(time_t cycleBegin, time_t periodSecs) {
  return lightSleepInterruptable(cycleBegin, periodSecs, m->getModuleSettings()->miniPeriodMsec(), haveToInterrupt, heartbeat);
}

BotMode setupArchitecture() {

  // Let HW startup
  delay(HW_STARTUP_DELAY_MSECS);

  // Intialize the logging framework
  Serial.begin(115200);     // Initialize serial port
  Serial.setTimeout(10); // Timeout for read
  setupLog(logLine);
  log(CLASS_MAIN, Info, "Log initialized");

  log(CLASS_MAIN, Debug, "Setup cmds");
  cmdBuffer = new Buffer(COMMAND_MAX_LENGTH);
  cmdLast = new Buffer(COMMAND_MAX_LENGTH);

  log(CLASS_MAIN, Debug, "Setup timing");
  setExternalMillis(millis);

  log(CLASS_MAIN, Debug, "Setup pins & deepsleep (if failure think of activating deep sleep mode?)");

  log(CLASS_MAIN, Debug, "Setup LCD");
  lcd = new Adafruit_PCD8544(LCD_CLK_PIN, LCD_DIN_PIN, LCD_DC_PIN, LCD_CS_PIN, LCD_RST_PIN);
  lcd->begin(lcdContrast(), LCD_DEFAULT_BIAS);
  lcd->clearDisplay();
  delay(DELAY_MS_SPI);

  messageFunc(0, 0, 1, false, FullClear, 1, "booting...");

  heartbeat();

  log(CLASS_MAIN, Debug, "Setup wdt");
  ESP.wdtEnable(1); // argument not used

  log(CLASS_MAIN, Debug, "Setup wifi");
  WiFi.persistent(false);
  WiFi.hostname(apiDeviceLogin());
  stopWifi();
  heartbeat();

  log(CLASS_MAIN, Debug, "Setup http");
  httpClient.setTimeout(HTTP_TIMEOUT_MS);
  heartbeat();

  log(CLASS_MAIN, Debug, "Setup random");
  randomSeed(analogRead(0) * 256 + analogRead(0));
  heartbeat();

  log(CLASS_MAIN, Debug, "Setup commands");
#ifdef TELNET_ENABLED
  telnet.setCallBackProjectCmds(reactCommandCustom);
  String helpCli("Type 'help' for help");
  telnet.setHelpProjectsCmds(helpCli);
#endif // TELNET_ENABLED
  heartbeat();

  log(CLASS_MAIN, Debug, "Clean up crashes");
  if (espSaveCrash.count() > 5) {
    log(CLASS_MAIN, Warn, "Too many Stack-trcs / clearing (!!!)");
    espSaveCrash.clear();
  } else if (espSaveCrash.count() > 0) {
    log(CLASS_MAIN, Warn, "Stack-trcs (!!!)");
    espSaveCrash.print();
  }

  log(CLASS_MAIN, Debug, "Letting user interrupt...");
  bool i = sleepInterruptable(now(), SLEEP_PERIOD_UPON_BOOT_SEC);
  if (i) {
    return ConfigureMode;
  } else {
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

  // other
  handleInterrupt();
  if (m->getModuleSettings()->getDebug()) {
    debugHandle();
  }

}

CmdExecStatus commandArchitecture(const char *c) {
  if (strcmp("init", c) == 0) {
    logRaw(CLASS_MAIN, User, "-> Initialize");
    logRaw(CLASS_MAIN, User, "Execute:");
    logRaw(CLASS_MAIN, User, "   ls");
    log(CLASS_MAIN, User, "   save %s <alias>", DEVICE_ALIAS_FILENAME);
    log(CLASS_MAIN, User, "   save %s <pwd>", DEVICE_PWD_FILENAME);
    logRaw(CLASS_MAIN, User, "   wifissid <ssid>");
    logRaw(CLASS_MAIN, User, "   wifipass <password>");
    log(CLASS_MAIN, User, "   save %s <contrast-0-100>", DEVICE_CONTRAST_FILENAME);
    logRaw(CLASS_MAIN, User, "   (setup of power consumption settings architecture specific if any)");
    logRaw(CLASS_MAIN, User, "   store");
    logRaw(CLASS_MAIN, User, "   ls");
    return Executed;
  } else if (strcmp("ls", c) == 0) {
    SPIFFS.begin();
    Dir dir = SPIFFS.openDir("/");
    while (dir.next()) {
      log(CLASS_MAIN, User, "- %s (%d bytes)", dir.fileName().c_str(), (int)dir.fileSize());
    }
    SPIFFS.end();
    return Executed;
  } else if (strcmp("rm", c) == 0) {
    const char *f = strtok(NULL, " ");
    SPIFFS.begin();
    bool succ = SPIFFS.remove(f);
    log(CLASS_MAIN, User, "### File '%s' %s removed", f, (succ ? "" : "NOT"));
    SPIFFS.end();
    return Executed;
  } else if (strcmp("lcdcont", c) == 0) {
    const char *c = strtok(NULL, " ");
    int i = atoi(c);
    log(CLASS_MAIN, User, "Set contrast to: %d", i);
    lcd->setContrast(i);
    return Executed;
  } else if (strcmp("reset", c) == 0) {
    ESP.restart(); // it is normal that it fails if invoked the first time after firmware is written
    return Executed;
  } else if (strcmp("freq", c) == 0) {
    uint8 fmhz = (uint8)atoi(strtok(NULL, " "));
    bool succ = system_update_cpu_freq(fmhz);
    log(CLASS_MAIN, User, "Freq updated: %dMHz (succ %s)", (int)fmhz, BOOL(succ));
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
    logRaw(CLASS_MAIN, User, HELP_COMMAND_ARCH_CLI);
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
  log(CLASS_MAIN, Error, "Abort: %s", msg);
  if (inDeepSleepMode()) {
    log(CLASS_MAIN, Warn, "Will deep sleep upon abort...");
    bool inte = sleepInterruptable(now(), SLEEP_PERIOD_PRE_ABORT_SEC);
    if (!inte) {
      deepSleepNotInterruptableSecs(now(), SLEEP_PERIOD_UPON_ABORT_SEC);
    } else {
      m->getBot()->setMode(ConfigureMode);
      log(CLASS_MAIN, Warn, "Abort skipped");
    }
  } else {
    log(CLASS_MAIN, Warn, "Will light sleep and restart upon abort...");
    bool inte = sleepInterruptable(now(), SLEEP_PERIOD_UPON_ABORT_SEC);
    if (!inte) {
      ESP.restart();
    } else {
    	m->getBot()->setMode(ConfigureMode);
    }
  }
}

////////////////////////////////////////
// Architecture specific functions
////////////////////////////////////////

void debugHandle() {
  static bool firstTime = true;
  if (firstTime) {
    log(CLASS_MAIN, Debug, "Initialize debuggers...");
#ifdef TELNET_ENABLED
    telnet.begin(apiDeviceLogin()); // Intialize the remote logging framework
#endif // TELNET_ENABLED
#ifdef OTA_ENABLED
    ArduinoOTA.begin();             // Intialize OTA
#endif // OTA_ENABLED
    firstTime = false;
  }

  m->getSleepinoSettings()->getStatus()->fill("vcc:%0.4f,heap:%d", VCC_FLOAT, ESP.getFreeHeap());
  m->getSleepinoSettings()->getMetadata()->changed();

  if (logBuffer != NULL && m->getSleepinoSettings()->fsLogsEnabled()) {
    log(CLASS_MAIN, Debug, "Push logs...");
    PropSync *ps = m->getModule()->getPropSync();
    initWifiSimple();
    PropSyncStatusCode status = ps->pushLogMessages(logBuffer->getBuffer());
    if (ps->isFailure(status)) {
      log(CLASS_MAIN, Warn, "Failed to push logs...");
    }
    logBuffer->clear();
  }

#ifdef TELNET_ENABLED
  telnet.handle();     // Handle telnet log server and commands
#endif // TELNET_ENABLED
#ifdef OTA_ENABLED
  ArduinoOTA.handle(); // Handle on the air firmware load
#endif // OTA_ENABLED
}

void bitmapToLcd(uint8_t bitmap[]) {
  for (char yi = 0; yi < 8; yi++) {
    for (char xi = 0; xi < 2; xi++) {
      uint8_t imgbyte = bitmap[yi * 2 + xi];
      for (char b = 0; b < 8; b++) {
        uint8_t color = (imgbyte << b) & 0b10000000;
        int16_t xl = (int16_t)xi * 64 + (int16_t)b * 8;
        int16_t yl = (int16_t)yi * 8;
        uint16_t cl = color == 0 ? BLACK : WHITE;
        lcd->fillRect(xl, yl, 8, 8, cl);
      }
    }
  }
}

void reactCommandCustom() { // for the use via telnet
#ifdef TELNET_ENABLED
  m->command(telnet.getLastCommand().c_str());
#endif // TELNET_ENABLED
}

void heartbeat() { }

void handleInterrupt() {
  if (Serial.available()) {
    // Handle serial commands
  	uint8_t c;

  	while (true) {
      size_t n = Serial.readBytes(&c, 1);

      if (c == 0x08 && n == 1) { // backspace
        log(CLASS_MAIN, Debug, "Backspace");
        if (cmdBuffer->getLength() > 0) {
          cmdBuffer->getUnsafeBuffer()[cmdBuffer->getLength() - 1] = 0;
        }
      } else if (c == 0x1b && n == 1) { // up/down
        log(CLASS_MAIN, Debug, "Up/down");
        cmdBuffer->load(cmdLast->getBuffer());
      } else if ((c == '\r') && n == 1) { // ignore
        log(CLASS_MAIN, Debug, "\\r pressed (ignored)");
      } else if (c == '\n' && n == 1) { // if enter is pressed...
        log(CLASS_MAIN, Debug, "Enter");
        if (cmdBuffer->getLength() > 0) {
          CmdExecStatus execStatus = m->command(cmdBuffer->getBuffer());
          bool interrupt = (execStatus == ExecutedInterrupt);
          log(CLASS_MAIN, Debug, "Interrupt: %d", interrupt);
          log(CLASS_MAIN, Debug, "Cmd status: %s", CMD_EXEC_STATUS(execStatus));
          log(CLASS_MAIN, User, "('%s' => %s)", cmdBuffer->getBuffer(), CMD_EXEC_STATUS(execStatus));
          cmdLast->load(cmdBuffer->getBuffer());
          cmdBuffer->clear();
        }
        break;
      } else if (n == 1){
        cmdBuffer->append(c);
      }
      // echo
      log(CLASS_MAIN, User, "> %s", cmdBuffer->getBuffer());
      while(!Serial.available()) {delay(100);}
  	}
    log(CLASS_MAIN, Debug, "Done with interrupt");
  }
}

bool haveToInterrupt() {
  if (Serial.available()) {
    log(CLASS_MAIN, Debug, "Serial pinged: int");
    return true;
  } else {
    return false;
  }
}


bool inDeepSleepMode() {
  return (bool)atoi(initializeTuningVariable(&deepSleepMode, DEVICE_DSLEEP_FILENAME, DEVICE_DSLEEP_MAX_LENGTH, "0", false)->getBuffer());
}

int lcdContrast() {
  return atoi(initializeTuningVariable(&contrast, DEVICE_CONTRAST_FILENAME, DEVICE_CONTRAST_MAX_LENGTH, "50", false)->getBuffer());
}



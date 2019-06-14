
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Arduino.h>
#include <ArduinoOTA.h>
#include <ESP8266HTTPClient.h>
#include <ESP8266WiFi.h>
#include <ESP8266httpUpdate.h>
#include <EspSaveCrash.h>
#include <FS.h>
#include <Main.h>
#include <Pinout.h>
#include <RemoteDebug.h>
#include <SPI.h>
#include <Wire.h>

#define DELAY_MS_SPI 3
#define ABORT_DELAY_SECS 5
#define HW_STARTUP_DELAY_MSECS 500

#define DEVICE_ALIAS_FILENAME "/alias.tuning"
#define DEVICE_ALIAS_MAX_LENGTH 16

#define DEVICE_PWD_FILENAME "/pass.tuning"
#define DEVICE_PWD_MAX_LENGTH 16

#define SERVO_0_FILENAME "/servo0.tuning"
#define SERVO_1_FILENAME "/servo1.tuning"

#define DEVICE_DSLEEP_FILENAME "/deepsleep.tuning"
#define DEVICE_DSLEEP_MAX_LENGTH 1

#define MAX_DEEP_SLEEP_PERIOD_SECS 1800

#define LCD_PIXEL_WIDTH 6
#define LCD_PIXEL_HEIGHT 8

#ifndef WIFI_DELAY_MS
#define WIFI_DELAY_MS 4000
#endif // WIFI_DELAY_MS

#define MAX_ROUND_ROBIN_LOG_FILES 5

#ifndef FIRMWARE_UPDATE_URL
#define FIRMWARE_UPDATE_URL MAIN4INOSERVER_API_HOST_BASE "/firmwares/sleepino/%s"
#endif // FIRMWARE_UPDATE_URL

#define PRE_DEEP_SLEEP_WINDOW_SECS 5

#define SERVO_PERIOD_REACTION_MS 15

#define NEXT_LOG_LINE_ALGORITHM ((currentLogLine + 1) % 4)

#define LOG_BUFFER_MAX_LENGTH 1024

#ifndef URL_PRINT_MAX_LENGTH
#define URL_PRINT_MAX_LENGTH 20
#endif // URL_PRINT_MAX_LENGTH

#ifndef USER_DELAY_MS
#define USER_DELAY_MS 4000
#endif // USER_DELAY_MS

#define VCC_FLOAT ((float)ESP.getVcc() / 1024)

#define ONLY_SHOW_MSG true
#define SHOW_MSG_AND_REACT false

#define WAIT_BEFORE_HTTP_MS 100

extern "C" {
#include "user_interface.h"
}

#define HTTP_TIMEOUT_MS 8000

#define HELP_COMMAND_ARCH_CLI                                                                                                              \
  "\n  ESP8266 HELP"                                                                                                                        \
  "\n  init              : initialize essential settings (wifi connection, logins, etc.)"                                                  \
  "\n  rm ...            : remove file in FS "                                                                                             \
  "\n  ls                : list files present in FS "                                                                                      \
  "\n  reset             : reset the device"                                                                                               \
  "\n  freq ...          : set clock frequency in MHz (80 or 160 available only, 160 faster but more power consumption)"                   \
  "\n  deepsleep ...     : deep sleep N provided seconds"                                                                                  \
  "\n  lightsleep ...    : light sleep N provided seconds"                                                                                 \
  "\n  clearstack        : clear stack trace "                                                                                             \
  "\n"

HTTPClient httpClient;
RemoteDebug telnet;
Adafruit_SSD1306 *lcd = NULL;
Buffer *apiDeviceId = NULL;
Buffer *apiDevicePwd = NULL;
Buffer *deepSleepMode = NULL;
int currentLogLine = 0;
Buffer *logBuffer = NULL;

ADC_MODE(ADC_VCC);

void bitmapToLcd(uint8_t bitmap[]);
void reactCommandCustom();
void heartbeat();
bool lightSleepInterruptable(time_t cycleBegin, time_t periodSecs);
void deepSleepNotInterruptable(time_t cycleBegin, time_t periodSecs);
void debugHandle();
bool haveToInterrupt();
void handleInterrupt();
Buffer *initializeTuningVariable(Buffer **var, const char *filename, int maxLength, const char *defaultContent, bool obfuscate);
void dumpLogBuffer();
bool inDeepSleepMode();

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

void logLine(const char *str) {
  Serial.setDebugOutput(getLogLevel() == Debug); // deep HW logs
  Serial.print(str);
  // telnet print
  if (telnet.isActive()) {
    for (unsigned int i = 0; i < strlen(str); i++) {
      telnet.write(str[i]);
    }
  }
  // lcd print
  if (lcd != NULL && m->getSleepinoSettings()->getLcdLogs()) { // can be called before LCD initialization
    currentLogLine = NEXT_LOG_LINE_ALGORITHM;
    lcd->setTextWrap(false);
    lcd->fillRect(0, currentLogLine * LCD_PIXEL_HEIGHT, 128, LCD_PIXEL_HEIGHT, BLACK);
    lcd->setTextSize(1);
    lcd->setTextColor(WHITE);
    lcd->setCursor(0, currentLogLine * LCD_PIXEL_HEIGHT);
    lcd->print(str);
    lcd->display();
    delay(DELAY_MS_SPI);
  }
  // filesystem logs
  if (m->getSleepinoSettings()->fsLogsEnabled()) {
    if (logBuffer == NULL) {
      logBuffer = new Buffer(LOG_BUFFER_MAX_LENGTH);
    }
    logBuffer->append(str);
    logBuffer->append("\n");
  }
}

bool initWifi(const char *ssid, const char *pass, bool skipIfConnected, int retries) {
  wl_status_t status;
  log(CLASS_MAIN, Info, "To '%s'...", ssid);

  if (skipIfConnected) { // check if connected
    log(CLASS_MAIN, Info, "Conn. '%s'?", ssid);
    status = WiFi.status();
    if (status == WL_CONNECTED) {
      log(CLASS_MAIN, Info, "IP: %s", WiFi.localIP().toString().c_str());
      return true; // connected
    }
  } else { // force disconnection
    log(CLASS_MAIN, Info, "W.Off.");
    WiFi.disconnect();
    delay(WIFI_DELAY_MS);
    WiFi.mode(WIFI_OFF); // to be removed after SDK update to 1.5.4
    delay(WIFI_DELAY_MS);
  }

  WiFi.mode(WIFI_STA);
  delay(WIFI_DELAY_MS);
  WiFi.begin(ssid, pass);

  int attemptsLeft = retries;
  while (true) {
    bool interrupt = lightSleepInterruptable(now(), WIFI_DELAY_MS / 1000);
    if (interrupt) {
      log(CLASS_MAIN, Info, "Interrupted");
      return false; // not connected
    }
    status = WiFi.status();
    log(CLASS_MAIN, Info, "..'%s'(%d)", ssid, attemptsLeft);
    attemptsLeft--;
    if (status == WL_CONNECTED) {
      log(CLASS_MAIN, Info, "IP: %s", WiFi.localIP().toString().c_str());
      return true; // connected
    }
    if (attemptsLeft < 0) {
      log(CLASS_MAIN, Warn, "Conn. to '%s' failed %d", ssid, status);
      return false; // not connected
    }
  }
}

// TODO: add https support, which requires fingerprint of server that can be obtained as follows:
//  openssl s_client -connect dweet.io:443 < /dev/null 2>/dev/null | openssl x509 -fingerprint -noout -in /dev/stdin
int httpGet(const char *url, ParamStream *response, Table *headers) {
  httpClient.begin(url);
  int i = 0;
  while ((i = headers->next(i)) != -1) {
    httpClient.addHeader(headers->getKey(i), headers->getValue(i));
    i++;
  }
  log(CLASS_MAIN, Debug, "> GET:..%s", tailStr(url, URL_PRINT_MAX_LENGTH));
  int errorCode = httpClient.GET();
  log(CLASS_MAIN, Debug, "> GET:%d", errorCode);

  if (errorCode == HTTP_OK || errorCode == HTTP_NO_CONTENT) {
    if (response != NULL) {
      httpClient.writeToStream(response);
    }
  } else {
    int e = httpClient.writeToStream(&Serial);
    log(CLASS_MAIN, Error, "> GET(%d):%d %s", e, errorCode, httpClient.errorToString(errorCode).c_str());
  }
  httpClient.end();

  delay(WAIT_BEFORE_HTTP_MS);

  return errorCode;
}

int httpPost(const char *url, const char *body, ParamStream *response, Table *headers) {
  httpClient.begin(url);
  int i = 0;
  while ((i = headers->next(i)) != -1) {
    httpClient.addHeader(headers->getKey(i), headers->getValue(i));
    i++;
  }

  log(CLASS_MAIN, Debug, "> POST:..%s", tailStr(url, URL_PRINT_MAX_LENGTH));
  log(CLASS_MAIN, Debug, "> POST:'%s'", body);
  int errorCode = httpClient.POST(body);
  log(CLASS_MAIN, Debug, "> POST:%d", errorCode);

  if (errorCode == HTTP_OK || errorCode == HTTP_CREATED) {
    if (response != NULL) {
      httpClient.writeToStream(response);
    }
  } else {
    int e = httpClient.writeToStream(&Serial);
    log(CLASS_MAIN, Error, "> POST(%d):%d %s", e, errorCode, httpClient.errorToString(errorCode).c_str());
  }
  httpClient.end();

  delay(WAIT_BEFORE_HTTP_MS);

  return errorCode;
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
  log(CLASS_MAIN, Debug, "Msg: (%d,%d)'%s'", x, y, str);
  delay(DELAY_MS_SPI);
}

void clearDevice() {
  SPIFFS.format();
  SaveCrash.clear();
}

bool readFile(const char *fname, Buffer *content) {
  bool success = false;
  SPIFFS.begin();
  File f = SPIFFS.open(fname, "r");
  if (!f) {
    log(CLASS_MAIN, Warn, "File reading failed: %s", fname);
    content->clear();
    success = false;
  } else {
    String s = f.readString();
    content->load(s.c_str());
    log(CLASS_MAIN, Info, "File read: %s", fname);
    success = true;
  }
  SPIFFS.end();
  return success;
}

bool writeFile(const char *fname, const char *content) {
  bool success = false;
  SPIFFS.begin();
  File f = SPIFFS.open(fname, "w+");
  if (!f) {
    log(CLASS_MAIN, Warn, "File writing failed: %s", fname);
    success = false;
  } else {
    f.write((const uint8_t *)content, strlen(content));
    log(CLASS_MAIN, Info, "File written: %s", fname);
    success = true;
  }
  SPIFFS.end();
  return success;
}

void infoArchitecture() {

  m->getNotifier()->message(0,
                            1,
                            "ID:%s\nV:%s\nCrashes:%d\nIP: %s\nMemory:%lu\nUptime:%luh\nVcc: %0.2f",
                            apiDeviceLogin(),
                            STRINGIFY(PROJ_VERSION),
                            SaveCrash.count(),
                            WiFi.localIP().toString().c_str(),
                            ESP.getFreeHeap(),
                            (millis() / 1000) / 3600,
                            VCC_FLOAT);
}

void testArchitecture() { }

void updateFirmware(const char* descriptor) {
  ESP8266HTTPUpdate updater;
  Buffer url(64);
  url.fill(FIRMWARE_UPDATE_URL, descriptor);

  Settings *s = m->getModuleSettings();
  bool connected = initWifi(s->getSsid(), s->getPass(), false, 10);
  if (!connected) {
    log(CLASS_MAIN, Error, "Cannot connect to wifi");
    m->getNotifier()->message(0, 1, "Cannot connect to wifi: %s", s->getSsid());
    return; // fail fast
  }

  log(CLASS_MAIN, Info, "Updating firmware from '%s'...", url.getBuffer());
  m->getNotifier()->message(0, 1, "Updating: %s", url.getBuffer());

  t_httpUpdate_return ret = updater.update(url.getBuffer());
  switch (ret) {
    case HTTP_UPDATE_FAILED:
      log(CLASS_MAIN,
          Error,
          "HTTP_UPDATE_FAILD Error (%d): %s\n",
          ESPhttpUpdate.getLastError(),
          ESPhttpUpdate.getLastErrorString().c_str());
      break;
    case HTTP_UPDATE_NO_UPDATES:
      log(CLASS_MAIN, Info, "No updates.");
      break;
    case HTTP_UPDATE_OK:
      log(CLASS_MAIN, Info, "Done!");
      break;
  }
}

// Execution
///////////////////

bool sleepInterruptable(time_t cycleBegin, time_t periodSecs) {
  if (inDeepSleepMode() && m->getBot()->getMode() == RunMode) { // in deep sleep mode and running
    bool interrupt = lightSleepInterruptable(now() /* always do it */, PRE_DEEP_SLEEP_WINDOW_SECS);
    if (interrupt) {
      return true;
    }
    deepSleepNotInterruptable(cycleBegin, periodSecs);
    return false; // won't be called ever
  } else {
    return lightSleepInterruptable(cycleBegin, periodSecs);
  }
}

BotMode setupArchitecture() {

  // Let HW startup
  delay(HW_STARTUP_DELAY_MSECS);

  // Intialize the logging framework
  Serial.begin(115200);     // Initialize serial port
  Serial.setTimeout(10000); // Timeout for read
  setupLog(logLine);
  log(CLASS_MAIN, Info, "Log initialized");

  log(CLASS_MAIN, Debug, "Setup timing");
  setExternalMillis(millis);

  log(CLASS_MAIN, Debug, "Setup pins & deepsleep (if failure think of activating deep sleep mode?)");

  log(CLASS_MAIN, Debug, "Setup LCD");
  lcd = new Adafruit_SSD1306(-1);
  lcd->begin(SSD1306_SWITCHCAPVCC, 0x3C); // Initialize LCD
  delay(DELAY_MS_SPI);
  heartbeat();

  log(CLASS_MAIN, Debug, "Setup wdt");
  ESP.wdtEnable(1); // argument not used

  log(CLASS_MAIN, Debug, "Setup wifi");
  WiFi.persistent(false);
  WiFi.hostname(apiDeviceLogin());
  heartbeat();

  log(CLASS_MAIN, Debug, "Setup http");
  httpClient.setTimeout(HTTP_TIMEOUT_MS);
  heartbeat();

  log(CLASS_MAIN, Debug, "Setup random");
  randomSeed(analogRead(0) * 256 + analogRead(0));
  heartbeat();

  log(CLASS_MAIN, Debug, "Setup commands");
  telnet.setCallBackProjectCmds(reactCommandCustom);
  String helpCli("Type 'help' for help");
  telnet.setHelpProjectsCmds(helpCli);
  heartbeat();

  log(CLASS_MAIN, Debug, "Setup IO/lcd");
  heartbeat();

  log(CLASS_MAIN, Debug, "Clean up crashes");
  if (SaveCrash.count() > 5) {
    log(CLASS_MAIN, Warn, "Too many Stack-trcs / clearing (!!!)");
    SaveCrash.clear();
  } else if (SaveCrash.count() > 0) {
    log(CLASS_MAIN, Warn, "Stack-trcs (!!!)");
    SaveCrash.print();
  }

  return RunMode;
}

void runModeArchitecture() {
  handleInterrupt();
  if (m->getModuleSettings()->getDebug()) {
    debugHandle();
  }
}

CmdExecStatus commandArchitecture(const char *c) {
  if (strcmp("init", c) == 0) {
    logRawUser("-> Initialize");
    logRawUser("Execute:");
    logRawUser("   ls");
    logUser("   save %s <alias>", DEVICE_ALIAS_FILENAME);
    logUser("   save %s <pwd>", DEVICE_PWD_FILENAME);
    logRawUser("   wifissid <ssid>");
    logRawUser("   wifissid <ssid>");
    logRawUser("   wifipass <password>");
    logRawUser("   ifttttoken <token>");
    logRawUser("   (setup of power consumption settings architecture specific if any)");
    logRawUser("   store");
    logRawUser("   ls");
    return Executed;
  } else if (strcmp("ls", c) == 0) {
    SPIFFS.begin();
    Dir dir = SPIFFS.openDir("/");
    while (dir.next()) {
      logUser("- %s (%d bytes)", dir.fileName().c_str(), (int)dir.fileSize());
    }
    SPIFFS.end();
    return Executed;
  } else if (strcmp("rm", c) == 0) {
    const char *f = strtok(NULL, " ");
    SPIFFS.begin();
    bool succ = SPIFFS.remove(f);
    logUser("### File '%s' %s removed", f, (succ?"":"NOT"));
    SPIFFS.end();
    return Executed;
  } else if (strcmp("reset", c) == 0) {
    ESP.restart(); // it is normal that it fails if invoked the first time after firmware is written
    return Executed;
  } else if (strcmp("freq", c) == 0) {
    uint8 fmhz = (uint8)atoi(strtok(NULL, " "));
    bool succ = system_update_cpu_freq(fmhz);
    logUser("Freq updated: %dMHz (succ %s)", (int)fmhz, BOOL(succ));
    return Executed;
  } else if (strcmp("deepsleep", c) == 0) {
    int s = atoi(strtok(NULL, " "));
    deepSleepNotInterruptable(now(), s);
    return Executed;
  } else if (strcmp("lightsleep", c) == 0) {
    int s = atoi(strtok(NULL, " "));
    return (lightSleepInterruptable(now(), s)? ExecutedInterrupt: Executed);
  } else if (strcmp("clearstack", c) == 0) {
    SaveCrash.clear();
    return Executed;
  } else if (strcmp("help", c) == 0 || strcmp("?", c) == 0) {
    logRawUser(HELP_COMMAND_ARCH_CLI);
    return Executed;
  } else {
    return NotFound;
  }
}

void configureModeArchitecture() {
  handleInterrupt();
  debugHandle();
  if (m->getBot()->getClock()->currentTime() % 60 == 0) { // every minute
    m->getNotifier()->message(0, 1, "telnet %s", WiFi.localIP().toString().c_str());
  }
}

void abort(const char *msg) {
  log(CLASS_MAIN, Error, "Abort: %s", msg);
  m->getNotifier()->message(0, 1, "Abort: %s", msg);
  bool interrupt = sleepInterruptable(now(), ABORT_DELAY_SECS);
  if (interrupt) {
    log(CLASS_MAIN, Debug, "Abort sleep interrupted");
  } else if (inDeepSleepMode()) {
    log(CLASS_MAIN, Warn, "Will deep sleep upon abort...");
    deepSleepNotInterruptable(now(), m->getModuleSettings()->periodMsec() / 1000);
  } else {
    log(CLASS_MAIN, Warn, "Will light sleep and restart upon abort...");
    bool i = sleepInterruptable(now(), m->getModuleSettings()->periodMsec() / 1000L);
    if (!i) {
      ESP.restart();
    } else {
      log(CLASS_MAIN, Warn, "Restart skipped because of interrupt.");
      log(CLASS_MAIN, Warn, "System ready for exploration.");
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
    telnet.begin(apiDeviceLogin()); // Intialize the remote logging framework
    ArduinoOTA.begin();             // Intialize OTA
    firstTime = false;
  }

  m->getSleepinoSettings()->getStatus()->fill("vcc:%0.2f,heap:%d", VCC_FLOAT, ESP.getFreeHeap());
  m->getSleepinoSettings()->getMetadata()->changed();

  if (m->getSleepinoSettings()->fsLogsEnabled()) {
    dumpLogBuffer();
  }
  telnet.handle();     // Handle telnet log server and commands
  ArduinoOTA.handle(); // Handle on the air firmware load
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
  m->command(telnet.getLastCommand().c_str());
}

void heartbeat() {
  delay(1);
}

bool lightSleepInterruptable(time_t cycleBegin, time_t periodSecs) {
  log(CLASS_MAIN, Debug, "Light Sleep(%ds)...", (int)periodSecs);
  if (haveToInterrupt()) { // first quick check before any time considerations
    return true;
  }
  while (now() < cycleBegin + periodSecs) {
    if (haveToInterrupt()) {
      return true;
    }
    heartbeat();
    delay(m->getModuleSettings()->miniPeriodMsec());
  }
  return false;
}

void deepSleepNotInterruptable(time_t cycleBegin, time_t periodSecs) {
	time_t p = (periodSecs > MAX_DEEP_SLEEP_PERIOD_SECS? MAX_DEEP_SLEEP_PERIOD_SECS: periodSecs);
  log(CLASS_MAIN, Debug, "Deep Sleep(%ds)...", (int)p);
  time_t spentSecs = now() - cycleBegin;
  time_t leftSecs = p - spentSecs;
  if (leftSecs > 0) {
    ESP.deepSleep(leftSecs * 1000000L);
  }
}

void handleInterrupt() {
  if (Serial.available()) {
    // Handle serial commands
    Buffer cmdBuffer(COMMAND_MAX_LENGTH);
    log(CLASS_MAIN, Info, "Listening...");
    Serial.readBytesUntil('\n', cmdBuffer.getUnsafeBuffer(), COMMAND_MAX_LENGTH);
    cmdBuffer.replace('\n', 0);
    cmdBuffer.replace('\r', 0);
    CmdExecStatus execStatus = m->command(cmdBuffer.getBuffer());
    bool interrupt = (execStatus == ExecutedInterrupt);
    log(CLASS_MAIN, Debug, "Interrupt: %d", interrupt);
    log(CLASS_MAIN, Debug, "Cmd status: %s", CMD_EXEC_STATUS(execStatus));
    logUser("(%s => %s)", cmdBuffer.getBuffer(), CMD_EXEC_STATUS(execStatus));
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

Buffer *initializeTuningVariable(Buffer **var, const char *filename, int maxLength, const char *defaultContent, bool obfuscate) {
	bool first = false;
  if (*var == NULL) {
  	first = true;
    *var = new Buffer(maxLength);
    bool succAlias = readFile(filename, *var); // preserve the alias
    if (succAlias) {                           // managed to retrieve the alias
      (*var)->replace('\n', 0);                // content already with the alias
    } else if (defaultContent != NULL) {
      (*var)->fill(defaultContent);
    } else {
      abort(filename);
    }
  }
  if (first) {
    if (obfuscate) {
      log(CLASS_MAIN, Info, "Tuning: %s=***", filename);
    } else {
      log(CLASS_MAIN, Info, "Tuning: %s=%s", filename, (*var)->getBuffer());
    }
  }
  return *var;
}

void dumpLogBuffer() {
  if (logBuffer == NULL)
    return;

  Buffer fname(16);
  static int rr = 0;
  rr = (rr + 1) % MAX_ROUND_ROBIN_LOG_FILES;
  fname.fill("%d.log", rr);
  bool suc = writeFile(fname.getBuffer(), logBuffer->getBuffer());
  log(CLASS_MAIN, Warn, "Log: %s %s", fname.getBuffer(), BOOL(suc));
  logBuffer->clear();
}

bool inDeepSleepMode() {
  return (bool)atoi(initializeTuningVariable(&deepSleepMode, DEVICE_DSLEEP_FILENAME, DEVICE_DSLEEP_MAX_LENGTH, "0", false)->getBuffer());
}
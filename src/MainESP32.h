
#include <Adafruit_GFX.h>      // include adafruit graphics library
#include <Adafruit_PCD8544.h>  // include adafruit PCD8544 (Nokia 5110) library
#include <Arduino.h>
#include <ArduinoOTA.h>
#include <HTTPClient.h>
#include <SPIFFS.h>
#include <HTTPUpdate.h>
//#include <EspSaveCrash.h> // not supported for ESP32
#include <FS.h>
#include <Main.h>
#include <Pinout.h>
#include <RemoteDebug.h>
#include <SPI.h>
#include <Wire.h>

#define FORMAT_SPIFFS_IF_FAILED true
#define DELAY_MS_SPI 3
#define ABORT_DELAY_SECS 5
#define HW_STARTUP_DELAY_MSECS 10

#define DEVICE_ALIAS_FILENAME "/alias.tuning"
#define DEVICE_ALIAS_MAX_LENGTH 16

#define DEVICE_PWD_FILENAME "/pass.tuning"
#define DEVICE_PWD_MAX_LENGTH 16

#define DEVICE_DSLEEP_FILENAME "/deepsleep.tuning"
#define DEVICE_DSLEEP_MAX_LENGTH 1

#define SLEEP_PERIOD_UPON_BOOT_SEC 2
#define SLEEP_PERIOD_UPON_ABORT_SEC 600

#define SLEEP_PERIOD_PRE_ABORT_SEC 5

#define MAX_DEEP_SLEEP_PERIOD_SECS 2100 // 35 minutes

#define LCD_PIXEL_WIDTH 6
#define LCD_PIXEL_HEIGHT 8
#define LCD_DEFAULT_CONTRAST 50
#define LCD_DEFAULT_BIAS 0x17

#ifndef WIFI_DELAY_MS
#define WIFI_DELAY_MS 10000
#endif // WIFI_DELAY_MS

#define MAX_ROUND_ROBIN_LOG_FILES 5

#ifndef FIRMWARE_UPDATE_URL
#define FIRMWARE_UPDATE_URL MAIN4INOSERVER_API_HOST_BASE "/firmwares/sleepino/%s.esp32.bin"
#endif // FIRMWARE_UPDATE_URL

#define PRE_DEEP_SLEEP_WINDOW_SECS 5

#define NEXT_LOG_LINE_ALGORITHM ((currentLogLine + 1) % 2)

#define LOG_BUFFER_MAX_LENGTH 1024

#ifndef URL_PRINT_MAX_LENGTH
#define URL_PRINT_MAX_LENGTH 20
#endif // URL_PRINT_MAX_LENGTH

#ifndef USER_DELAY_MS
#define USER_DELAY_MS 2000
#endif // USER_DELAY_MS

#define USER_LCD_FONT_SIZE 2

//#define VCC_FLOAT ((float)ESP.getVcc() / 1024)

#define ONLY_SHOW_MSG true
#define SHOW_MSG_AND_REACT false

#define WAIT_BEFORE_HTTP_MS 100

//extern "C" {
//#include "user_interface.h"
//}

#define HTTP_TIMEOUT_MS 8000

#define HELP_COMMAND_ARCH_CLI                                                                                                              \
  "\n  ESP32 HELP"                                                                                                                        \
  "\n  init              : initialize essential settings (wifi connection, logins, etc.)"                                                  \
  "\n  rm ...            : remove file in FS "                                                                                             \
  "\n  lcdcont ...       : change lcd contrast"                                                                                            \
  "\n  ls                : list files present in FS "                                                                                      \
  "\n  reset             : reset the device"                                                                                               \
  "\n  deepsleep ...     : deep sleep N provided seconds"                                                                                  \
  "\n  lightsleep ...    : light sleep N provided seconds"                                                                                 \
  "\n  clearstack        : clear stack trace "                                                                                             \
  "\n"

HTTPClient httpClient;
RemoteDebug telnet;
Adafruit_PCD8544* lcd = NULL;
int lcdContrast = -1;
Buffer *apiDeviceId = NULL;
Buffer *apiDevicePwd = NULL;
Buffer *deepSleepMode = NULL;
int currentLogLine = 0;
Buffer *logBuffer = NULL;
Buffer *cmdBuffer = NULL;
Buffer *cmdLast = NULL;



void bitmapToLcd(uint8_t bitmap[]);
void reactCommandCustom();
void heartbeat();
bool lightSleepInterruptable(time_t cycleBegin, time_t periodSecs);
void deepSleepNotInterruptableSecs(time_t cycleBegin, time_t periodSecs);
void debugHandle();
bool haveToInterrupt();
void handleInterrupt();
Buffer *initializeTuningVariable(Buffer **var, const char *filename, int maxLength, const char *defaultContent, bool obfuscate);
void dumpLogBuffer();
bool inDeepSleepMode();
void updateLcdContrast();

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
  Serial.setDebugOutput(getLogLevel() == Debug && m->getModuleSettings()->getDebug()); // deep HW logs
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
    int line = currentLogLine + 4;
    lcd->setTextWrap(false);
    lcd->fillRect(0, line * LCD_PIXEL_HEIGHT, 84, LCD_PIXEL_HEIGHT, WHITE);
    lcd->setTextSize(1);
    lcd->setTextColor(BLACK);
    lcd->setCursor(0, line * LCD_PIXEL_HEIGHT);
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

void stopWifi() {
  if (!inDeepSleepMode()) {
  WiFi.disconnect();
  // TODO see if power reduction can be applied
  } else {
    log(CLASS_MAIN, Debug, "Skip wifi off...");
  }
}

WifiNetwork chooseWifi(const char* ssid, const char* ssidb) {
  int n = WiFi.scanNetworks();
  for (int i = 0; i < n; ++i) {
    String s = WiFi.SSID(i);
    if (strcmp(s.c_str(), ssid) == 0) {
      log(CLASS_MAIN, Info, "Wifi found '%s'", ssid);
    	return WifiMainNetwork;
    } else if  (strcmp(s.c_str(), ssidb) == 0) {
      log(CLASS_MAIN, Info, "Wifi found '%s'", ssidb);
    	return WifiBackupNetwork;
    }
  }
  return WifiNoNetwork;
}

bool initWifi(const char *ssid, const char *pass, bool skipIfConnected, int retries) {
  wl_status_t status;

  const char* ssidb = m->getSleepinoSettings()->getBackupWifiSsid()->getBuffer();
  const char* passb = m->getSleepinoSettings()->getBackupWifiPass()->getBuffer();

  log(CLASS_MAIN, Info, "Init wifi '%s' (or '%s')...", ssid, ssidb);


  if (skipIfConnected) { // check if connected
    log(CLASS_MAIN, Info, "Conn. '%s'?", ssid);
    status = WiFi.status();
    if (status == WL_CONNECTED) {
      log(CLASS_MAIN, Info, "IP: %s", WiFi.localIP().toString().c_str());
      return true; // connected
    }
  } else {
  	stopWifi();
  }

  log(CLASS_MAIN, Debug, "Scanning...");
  WifiNetwork w = chooseWifi(ssid, ssidb);

  log(CLASS_MAIN, Debug, "Connecting...");
  WiFi.mode(WIFI_STA);
  delay(WIFI_DELAY_MS);
  switch (w) {
  	case WifiMainNetwork:
      WiFi.begin(ssid, pass);
      break;
  	case WifiBackupNetwork:
      WiFi.begin(ssidb, passb);
      break;
  	default:
  		return false;
  }

  int attemptsLeft = retries;
  while (true) {
    bool interrupt = lightSleepInterruptable(now(), WIFI_DELAY_MS / 1000);
    if (interrupt) {
      log(CLASS_MAIN, Warn, "Wifi init interrupted");
      return false; // not connected
    }
    status = WiFi.status();
    log(CLASS_MAIN, Debug, "..'%s'(%d left)", ssid, attemptsLeft);
    attemptsLeft--;
    if (status == WL_CONNECTED) {
      log(CLASS_MAIN, Debug, "Connected! %s", WiFi.localIP().toString().c_str());
      return true; // connected
    }
    if (attemptsLeft < 0) {
      log(CLASS_MAIN, Warn, "Connection to '%s' failed %d", ssid, status);
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
  updateLcdContrast();
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
  //SPIFFS.format();
  logUser("   rm %s", DEVICE_ALIAS_FILENAME);
  logUser("   rm %s", DEVICE_PWD_FILENAME);
  logUser("   ls");
  logUser("   <remove all .properties>");
  //SaveCrash.clear();
}

bool readFile(const char *fname, Buffer *content) {
  bool success = false;
  File f = SPIFFS.open(fname, "r");
  if (!f) {
    log(CLASS_MAIN, Warn, "File reading failed: %s", fname);
    content->clear();
    success = false;
  } else {
    String s = f.readString();
    content->load(s.c_str());
    log(CLASS_MAIN, Debug, "File read: %s", fname);
    success = true;
  }
  return success;
}

bool writeFile(const char *fname, const char *content) {
  bool success = false;
  File f = SPIFFS.open(fname, "w+");
  if (!f) {
    log(CLASS_MAIN, Warn, "File writing failed: %s", fname);
    success = false;
  } else {
    f.print(content);
    f.close();
    log(CLASS_MAIN, Debug, "File written: %s", fname);
    success = true;
  }
  return success;
}

void infoArchitecture() {}

void testArchitecture() {}

void updateFirmware(const char* descriptor) {
  HTTPUpdate updater;
  Buffer url(64);
  url.fill(FIRMWARE_UPDATE_URL, descriptor);

  Settings *s = m->getModuleSettings();
  bool connected = initWifi(s->getSsid(), s->getPass(), false, 10);
  if (!connected) {
    log(CLASS_MAIN, Error, "Cannot connect to wifi");
    return; // fail fast
  }

  log(CLASS_MAIN, Info, "Updating firmware from '%s'...", url.getBuffer());

  t_httpUpdate_return ret = updater.update(httpClient.getStream(), url.getBuffer(), STRINGIFY(PROJ_VERSION));
  switch (ret) {
    case HTTP_UPDATE_FAILED:
      log(CLASS_MAIN,
          Error,
          "HTTP_UPDATE_FAILD Error (%d): %s\n",
          updater.getLastError(),
          updater.getLastErrorString().c_str());
      break;
    case HTTP_UPDATE_NO_UPDATES:
      log(CLASS_MAIN, Debug, "No updates.");
      break;
    case HTTP_UPDATE_OK:
      log(CLASS_MAIN, Debug, "Done!");
      break;
  }
}

// Execution
///////////////////

void deepSleepNotInterruptable(time_t cycleBegin, time_t periodSecs) {
  Timing t = Timing();
  time_t n = now();

  // light sleep to allow user intervention
  bool inte = lightSleepInterruptable(n, PRE_DEEP_SLEEP_WINDOW_SECS);

	// calculate time to boot regularly at the same moments
  t.setCurrentTime(n);
  t.setFreqEverySecs((int)periodSecs);
  time_t toSleepSecs = t.secsToMatch(MAX_DEEP_SLEEP_PERIOD_SECS);

  if (!inte) {
  	// if no intervention, deep sleep
    deepSleepNotInterruptableSecs(n, toSleepSecs);
  }
}

bool sleepInterruptable(time_t cycleBegin, time_t periodSecs) {
  return lightSleepInterruptable(cycleBegin, periodSecs);
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

  log(CLASS_MAIN, Debug, "Setup SPIFFS");
  SPIFFS.begin(FORMAT_SPIFFS_IF_FAILED);

  log(CLASS_MAIN, Debug, "Setup pins & deepsleep (if failure think of activating deep sleep mode?)");

  log(CLASS_MAIN, Debug, "Setup LCD");
  lcd = new Adafruit_PCD8544(LCD_CLK_PIN, LCD_DIN_PIN, LCD_DC_PIN, LCD_CS_PIN, LCD_RST_PIN);
  lcd->begin(LCD_DEFAULT_CONTRAST, LCD_DEFAULT_BIAS);
  lcd->clearDisplay();
  lcdContrast = LCD_DEFAULT_CONTRAST;
  delay(DELAY_MS_SPI);

  messageFunc(0, 0, 1, false, FullClear, 1, "booting...");

  heartbeat();

  //log(CLASS_MAIN, Debug, "Setup wdt");
  //ESP.wdtEnable(1); // argument not used

  log(CLASS_MAIN, Debug, "Setup wifi");
  WiFi.persistent(false);
  WiFi.setHostname(apiDeviceLogin());
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

  log(CLASS_MAIN, Debug, "Letting user interrupt...");
  bool i = lightSleepInterruptable(now(), SLEEP_PERIOD_UPON_BOOT_SEC);
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

  lcdAux.fill("%s\nV:%s", timeAux.getBuffer(), STRINGIFY(PROJ_VERSION));

  messageFunc(0, 0, 1, false, FullClear, 1, lcdAux.getBuffer());

  // other
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
    logRawUser("   wifipass <password>");
    logRawUser("   lcdcont <contrast-0-100>");
    logRawUser("   (setup of power consumption settings architecture specific if any)");
    logRawUser("   store");
    logRawUser("   ls");
    return Executed;
  } else if (strcmp("ls", c) == 0) {
    File root = SPIFFS.open("/");
    File file = root.openNextFile();
    while(file) {
      logUser("- %s (%d bytes)", file.name(), (int)file.size());
      file = root.openNextFile();
    }
    return Executed;
  } else if (strcmp("rm", c) == 0) {
    const char *f = strtok(NULL, " ");
    bool succ = SPIFFS.remove(f);
    logUser("### File '%s' %s removed", f, (succ?"":"NOT"));
    return Executed;
  } else if (strcmp("lcdcont", c) == 0) {
    const char *c = strtok(NULL, " ");
    int i = atoi(c);
    logUser("Set contrast to: %d", i);
    m->getSleepinoSettings()->setLcdContrast(i);
    updateLcdContrast();
    return Executed;
  } else if (strcmp("reset", c) == 0) {
    ESP.restart(); // it is normal that it fails if invoked the first time after firmware is written
    return Executed;
  } else if (strcmp("deepsleep", c) == 0) {
    int s = atoi(strtok(NULL, " "));
    deepSleepNotInterruptableSecs(now(), s);
    return Executed;
  } else if (strcmp("lightsleep", c) == 0) {
    int s = atoi(strtok(NULL, " "));
    return (lightSleepInterruptable(now(), s)? ExecutedInterrupt: Executed);
  } else if (strcmp("clearstack", c) == 0) {
    //SaveCrash.clear();
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
}

void abort(const char *msg) {
  log(CLASS_MAIN, Error, "Abort: %s", msg);
  if (inDeepSleepMode()) {
    log(CLASS_MAIN, Warn, "Will deep sleep upon abort...");
    bool inte = lightSleepInterruptable(now(), SLEEP_PERIOD_PRE_ABORT_SEC);
    if (!inte) {
      deepSleepNotInterruptableSecs(now(), SLEEP_PERIOD_UPON_ABORT_SEC);
    } else {
      m->getBot()->setMode(ConfigureMode);
      log(CLASS_MAIN, Warn, "Abort skipped");
    }
  } else {
    log(CLASS_MAIN, Warn, "Will light sleep and restart upon abort...");
    bool inte = lightSleepInterruptable(now(), SLEEP_PERIOD_UPON_ABORT_SEC);
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
    telnet.begin(apiDeviceLogin()); // Intialize the remote logging framework
    ArduinoOTA.begin();             // Intialize OTA
    firstTime = false;
  }

  m->getSleepinoSettings()->getStatus()->fill("heap:%d", ESP.getFreeHeap());
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

void heartbeat() { }

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

void deepSleepNotInterruptableSecs(time_t cycleBegin, time_t periodSecs) {
  time_t p = (periodSecs > MAX_DEEP_SLEEP_PERIOD_SECS ? MAX_DEEP_SLEEP_PERIOD_SECS : periodSecs);
  log(CLASS_MAIN, Debug, "Deep Sleep(%ds)...", (int)p);
  time_t spentSecs = now() - cycleBegin;
  time_t leftSecs = p - spentSecs;
  if (leftSecs > 0) {
    //lcd->command(PCD8544_FUNCTIONSET | PCD8544_POWERDOWN);
    ESP.deepSleep(leftSecs * 1000000L);
  }
}

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
        cmdBuffer->load(cmdLast);
      } else if ((c == '\n' || c == '\r') && n == 1) { // if enter is pressed...
        log(CLASS_MAIN, Debug, "Enter");
        if (cmdBuffer->getLength() > 0) {
          CmdExecStatus execStatus = m->command(cmdBuffer->getBuffer());
          bool interrupt = (execStatus == ExecutedInterrupt);
          log(CLASS_MAIN, Debug, "Interrupt: %d", interrupt);
          log(CLASS_MAIN, Debug, "Cmd status: %s", CMD_EXEC_STATUS(execStatus));
          logUser("('%s' => %s)", cmdBuffer->getBuffer(), CMD_EXEC_STATUS(execStatus));
          cmdLast->load(cmdBuffer);
          cmdBuffer->clear();
        }
        break;
      } else if (n == 1){
        cmdBuffer->append(c);
      }
      // echo
      logUser("> %s", cmdBuffer->getBuffer());
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
      log(CLASS_MAIN, Debug, "Tuning: %s=***", filename);
    } else {
      log(CLASS_MAIN, Debug, "Tuning: %s=%s", filename, (*var)->getBuffer());
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

void updateLcdContrast() {
	int tgt = m->getSleepinoSettings()->getLcdContrast();
	if (lcdContrast != tgt) {
		lcd->setContrast(tgt);
    lcdContrast = tgt;
    log(CLASS_MAIN, Debug, "Changed: new contrast %d", tgt);
	}
}

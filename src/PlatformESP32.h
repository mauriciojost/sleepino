#include <Adafruit_GFX.h>      // include adafruit graphics library
#include <Adafruit_PCD8544.h>  // include adafruit PCD8544 (Nokia 5110) library
#include <Arduino.h>
#include <Platform.h>
#ifdef OTA_ENABLED
#include <ArduinoOTA.h>
#endif // OTA_ENABLED
#include <HTTPClient.h>
#include <SPIFFS.h>
#include <HTTPUpdate.h>
//#include <EspSaveCrash.h> // not supported for ESP32
#include <FS.h>
#include <Pinout.h>
#ifdef TELNET_ENABLED
#include <RemoteDebug.h>
#endif // TELNET_ENABLED
#include <SPI.h>
#include <Wire.h>
#include <primitives/BoardESP32.h>
#include <PlatformESP.h>

#ifndef TELNET_HANDLE_DELAY_MS
#define TELNET_HANDLE_DELAY_MS 240000 // 4 minutes
#endif // TELNET_HANDLE_DELAY_MS

#define MAX_SLEEP_CYCLE_SECS 2419200 // 4 weeks

#define FORMAT_SPIFFS_IF_FAILED true


#define LCD_CHAR_WIDTH 6
#define LCD_CHAR_HEIGHT 8
#define LCD_DEFAULT_BIAS 0x17

#define NEXT_LOG_LINE_ALGORITHM ((currentLogLine + 1) % 6)

#define HELP_COMMAND_ARCH_CLI                                                                                                              \
  "\n  ESP32 HELP"                                                                                                                         \
  "\n  init              : initialize essential settings (wifi connection, logins, etc.)"                                                  \
  "\n  rm ...            : remove file in FS "                                                                                             \
  "\n  lcdcont ...       : change lcd contrast"                                                                                            \
  "\n  ls                : list files present in FS "                                                                                      \
  "\n  reset             : reset the device"                                                                                               \
  "\n  deepsleep ...     : deep sleep N provided seconds"                                                                                  \
  "\n  lightsleep ...    : light sleep N provided seconds"                                                                                 \
  "\n"

Adafruit_PCD8544* lcd = NULL;




float vcc();
void reactCommandCustom();
void heartbeat();
bool lightSleepInterruptable(time_t cycleBegin, time_t periodSecs);
void deepSleepNotInterruptableSecs(time_t cycleBegin, time_t periodSecs);
bool haveToInterrupt();
void dumpLogBuffer();
int lcdContrast();

////////////////////////////////////////
// Functions requested for architecture
////////////////////////////////////////

// Callbacks
///////////////////

void servo(int idx, int pos) { }

float vcc() {
  return 3.3; // not supported.
}

void logLine(const char *str, const char *clz, LogLevel l, bool newline) {
  int ts = (int)((millis()/1000) % 10000);
  Buffer time(8);
  time.fill("%04d|", ts);
  // serial print
  /*
Serial.print("HEA:");
Serial.print(ESP.getFreeHeap());
Serial.print("|");
Serial.print("VCC:");
Serial.print(VCC_FLOAT);
Serial.print("|");
*/
  Serial.print(str);
  // telnet print
#ifdef TELNET_ENABLED
  if (telnet.isActive()) {
    for (unsigned int i = 0; i < strlen(str); i++) {
      telnet.write(str[i]);
    }
  }
#endif // TELNET_ENABLED
  bool lcdLogsEnabled = (m==NULL?true:m->getSleepinoSettings()->getLcdLogs());
  bool fsLogsEnabled = (m==NULL?true:m->getSleepinoSettings()->fsLogsEnabled());
  int fsLogsLength = (m==NULL?DEFAULT_FS_LOGS_LENGTH:m->getSleepinoSettings()->getFsLogsLength());

  // lcd print
  if (lcd != NULL && lcdLogsEnabled) { // can be called before LCD initialization
    currentLogLine = NEXT_LOG_LINE_ALGORITHM;
    int line = currentLogLine + 2;
#ifdef LCD_ENABLED
    lcd->setTextWrap(false);
    lcd->fillRect(0, line * LCD_CHAR_HEIGHT, 84, LCD_CHAR_HEIGHT, WHITE);
    lcd->setTextSize(1);
    lcd->setTextColor(BLACK);
    lcd->setCursor(0, line * LCD_CHAR_HEIGHT);
    lcd->print(str);
    lcd->display();
#endif // LCD_ENABLED
    delay(DELAY_MS_SPI);
  }
  // local logs (to be sent via network)
  if (fsLogsEnabled) {
    initLogBuffer();
    if (newline) {
      logBuffer->append(time.getBuffer());
    }
    unsigned int s = (unsigned int)(fsLogsLength) + 1;
    char aux2[s];
    strncpy(aux2, str, s);
    aux2[s - 1] = 0;
    aux2[s - 2] = '\n';
    logBuffer->append(aux2);
  }
}

void clearDevice() {
  log(CLASS_PLATFORM, User, "   rm %s", DEVICE_ALIAS_FILENAME);
  log(CLASS_PLATFORM, User, "   rm %s", DEVICE_PWD_FILENAME);
  log(CLASS_PLATFORM, User, "   ls");
  log(CLASS_PLATFORM, User, "   <remove all .properties>");

}

void infoArchitecture() {
  log(CLASS_PLATFORM, User, "ID:%s", apiDeviceLogin());
  log(CLASS_PLATFORM, User, "V:%s", STRINGIFY(PROJ_VERSION));
  log(CLASS_PLATFORM, User, "IP: %s", WiFi.localIP().toString().c_str());
  log(CLASS_PLATFORM, User, "Uptime:%luh", (millis() / 1000) / 3600);
}

void testArchitecture() {}

// Execution
///////////////////

void setupArchitecture() {

  // Let HW startup
  delay(HW_STARTUP_DELAY_MSECS);

  // Intialize the logging framework
  Serial.begin(115200);     // Initialize serial port
  Serial.setTimeout(1000); // Timeout for read
  setupLog(logLine);

  log(CLASS_PLATFORM, User, "BOOT");
  log(CLASS_PLATFORM, User, "%s", STRINGIFY(PROJ_VERSION));

  log(CLASS_PLATFORM, Debug, "Setup cmds");
  cmdBuffer = new Buffer(COMMAND_MAX_LENGTH);
  cmdLast = new Buffer(COMMAND_MAX_LENGTH);

  log(CLASS_PLATFORM, Debug, "Setup timing");
  setExternalMillis(millis);

  log(CLASS_PLATFORM, Debug, "Setup SPIFFS");
  SPIFFS.begin(FORMAT_SPIFFS_IF_FAILED);


  log(CLASS_PLATFORM, Debug, "Setup wifi");
  WiFi.persistent(false);
  WiFi.setHostname(apiDeviceLogin());
  heartbeat();
  log(CLASS_PLATFORM, Debug, "Setup LCD");
#ifdef LCD_ENABLED
  lcd = new Adafruit_PCD8544(LCD_CLK_PIN, LCD_DIN_PIN, LCD_DC_PIN, LCD_CS_PIN, LCD_RST_PIN);
  lcd->begin(lcdContrast(), LCD_DEFAULT_BIAS);
#endif // LCD_ENABLED
  delay(DELAY_MS_SPI);

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

  Buffer fcontent(ABORT_LOG_MAX_LENGTH);
  bool abrt = readFile(ABORT_LOG_FILENAME, &fcontent);
  if (abrt) {
    log(CLASS_PLATFORM, Warn, "Abort: %s", fcontent.getBuffer());
    SPIFFS.begin();
    SPIFFS.remove(ABORT_LOG_FILENAME);
    SPIFFS.end();
  } else {
    log(CLASS_PLATFORM, Debug, "No abort");
  }

}

void runModeArchitecture() {
  Buffer timeAux(32);
  Timing::humanize(m->getClock()->currentTime(), &timeAux);
  timeAux.replace(' ', '\n');

  Buffer lcdAux(64);

  lcdAux.fill("%s\nV:%s", timeAux.getBuffer(), STRINGIFY(PROJ_VERSION));

  messageFunc(0, 0, 1, false, FullClear, 1, lcdAux.getBuffer());

  // other
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
    File root = SPIFFS.open("/");
    File file = root.openNextFile();
    while(file) {
      log(CLASS_PLATFORM, User, "- %s (%d bytes)", file.name(), (int)file.size());
      file = root.openNextFile();
    }
    return Executed;
  } else if (strcmp("rm", c) == 0) {
    const char *f = strtok(NULL, " ");
    bool succ = SPIFFS.remove(f);
    log(CLASS_PLATFORM, User, "### File '%s' %s removed", f, (succ?"":"NOT"));
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
  } else if (strcmp("deepsleep", c) == 0) {
    int s = atoi(strtok(NULL, " "));
    deepSleepNotInterruptableSecs(now(), s);
    return Executed;
  } else if (strcmp("lightsleep", c) == 0) {
    int s = atoi(strtok(NULL, " "));
    return (sleepInterruptable(now(), s)? ExecutedInterrupt: Executed);
  } else if (strcmp("help", c) == 0 || strcmp("?", c) == 0) {
    logRaw(CLASS_PLATFORM, User, HELP_COMMAND_ARCH_CLI);
    return Executed;
  } else {
    return NotFound;
  }
}

int readRemainingSecs() {
  return -1; // not supported nor needed
}

void writeRemainingSecs(int s) {
  return; // not supported nor needed
}


////////////////////////////////////////
// Architecture specific functions
////////////////////////////////////////

void debugHandle() {
  if (!m->getModuleSettings()->getDebug()) {
    return;
  }
  static bool firstTime = true;
  Serial.setDebugOutput(m->getModuleSettings()->getDebug()); // deep HW logs
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

  m->getSleepinoSettings()->getStatus()->fill("freeheap:%d/%d", ESP.getFreeHeap(), ESP.getHeapSize());
  m->getSleepinoSettings()->getMetadata()->changed();

#ifdef TELNET_ENABLED
  log(CLASS_PLATFORM, User, "telnet?");
  for (int i = 0; i < TELNET_HANDLE_DELAY_MS/1000; i++) {
    telnet.handle();     // Handle telnet log server and commands
    delay(1000);
  }
#endif // TELNET_ENABLED
#ifdef OTA_ENABLED
  ArduinoOTA.handle(); // Handle on the air firmware load
#endif // OTA_ENABLED
}



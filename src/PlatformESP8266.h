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
#include <Servo.h>
#include <primitives/BoardESP8266.h>

#ifndef TELNET_HANDLE_DELAY_MS
#define TELNET_HANDLE_DELAY_MS 240000 // 4 minutes
#endif // TELNET_HANDLE_DELAY_MS

#define MAX_SLEEP_CYCLE_SECS 1800 // 30min



#define STACKTRACE_LOG_FILENAME "/stacktrace.log"

#define LCD_CHAR_WIDTH 6
#define LCD_CHAR_HEIGHT 8
#define LCD_DEFAULT_BIAS 0x17

#define NEXT_LOG_LINE_ALGORITHM ((currentLogLine + 1) % 6)

#define VCC_FLOAT ((float)ESP.getVcc() / 1024)

extern "C" {
#include "user_interface.h"
}

struct { // 512 bytes can be stored
  uint32_t crc32;
  int remainingSecs;
} rtcData;

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

Adafruit_PCD8544* lcd = NULL;

#include <PlatformESP.h>

Servo servo0;

EspSaveCrash espSaveCrash;

ADC_MODE(ADC_VCC);

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

void servo(int idx, int pos) {
  switch (idx) {
    case 0: 
      servo0.attach(SERVO0_PIN);
      for (int i = 0; i < 10; i++) {
        servo0.write(pos);
        delay(60);
      }
      servo0.detach();
      break;
    default:
      break;
  }
}

float vcc() {
  return VCC_FLOAT;
}

void logLine(const char *str, const char *clz, LogLevel l, bool newline) {
  int ts = (int)((millis()/1000) % 10000);
  Buffer time(8);
  time.fill("%04d|", ts);
  // serial print
#ifdef HEAP_VCC_LOG
  //Serial.print("HEA:");
  //Serial.print(ESP.getFreeHeap()); // caused a crash, reenable upon upgrade
  //Serial.print("|");
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
  bool lcdLogsEnabled = (m==NULL?true:m->getSleepinoSettings()->getLcdLogs());
  bool fsLogsEnabled = (m==NULL?true:m->getSleepinoSettings()->fsLogsEnabled());
  int fsLogsLength = (m==NULL?DEFAULT_FS_LOGS_LENGTH:m->getSleepinoSettings()->getFsLogsLength());

  // lcd print
  if (lcd != NULL && lcdLogsEnabled) { // can be called before LCD initialization
    currentLogLine = NEXT_LOG_LINE_ALGORITHM;
#ifdef LCD_ENABLED
    int line = currentLogLine + 2;
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
  espSaveCrash.clear();
}



void infoArchitecture() {
  log(CLASS_PLATFORM, User, "ID:%s", apiDeviceLogin());
  log(CLASS_PLATFORM, User, "V:%s", STRINGIFY(PROJ_VERSION));
  log(CLASS_PLATFORM, User, "Crashes:%d", espSaveCrash.count());
  log(CLASS_PLATFORM, User, "IP: %s", WiFi.localIP().toString().c_str());
  log(CLASS_PLATFORM, User, "Uptime:%luh", (millis() / 1000) / 3600);
  log(CLASS_PLATFORM, User, "Vcc: %0.2f", VCC_FLOAT);
}

void testArchitecture() {}

// Execution
///////////////////


void wakeupCallback() {  // unlike ISRs, you can do a print() from a callback function
  //log(CLASS_PLATFORM, Debug, "cls-wucb");
  //Serial.flush();
}

int failuresInPast() {
  // Useful links for debugging:
  // https://links2004.github.io/Arduino/dc/deb/md_esp8266_doc_exception_causes.html
  // ./packages/framework-arduinoespressif8266@2.20502.0/tools/sdk/include/user_interface.h
  // https://bitbucket.org/mauriciojost/esp8266-stacktrace-translator/src/master/
  int e = espSaveCrash.count();
  Buffer fcontent(16);
  bool abrt = readFile(ABORT_LOG_FILENAME, &fcontent);
  return e + (abrt?1:0);
}

void reportFailureLogs() {
  bool fsLogsEnabled = (m==NULL?true:m->getSleepinoSettings()->fsLogsEnabled());
  if (fsLogsEnabled) {
    initLogBuffer();
    espSaveCrash.print(logBuffer->getUnsafeBuffer(), LOG_BUFFER_MAX_LENGTH);
    writeFile(STACKTRACE_LOG_FILENAME, logBuffer->getBuffer());
  }

  Buffer fcontent(ABORT_LOG_MAX_LENGTH);
  bool abrt = readFile(ABORT_LOG_FILENAME, &fcontent);
  if (abrt) {
    log(CLASS_PLATFORM, Error, "Abort: %s", fcontent.getBuffer());
  } else {
    log(CLASS_PLATFORM, Debug, "No abort");
  }
}

void cleanFailures() {
  SPIFFS.begin();
  SPIFFS.remove(ABORT_LOG_FILENAME);
  SPIFFS.end();
  espSaveCrash.clear();
}

void setupArchitecture() {

  // Let HW startup
  delay(HW_STARTUP_DELAY_MSECS);

  // Intialize the logging framework
  Serial.begin(115200);     // Initialize serial port
  Serial.setTimeout(1000); // Timeout for read
  setupLog(logLine);

  log(CLASS_PLATFORM, Debug, "Setup cmds");
  cmdBuffer = new Buffer(COMMAND_MAX_LENGTH);
  cmdLast = new Buffer(COMMAND_MAX_LENGTH);

  log(CLASS_PLATFORM, Debug, "Setup timing");
  setExternalMillis(millis);

  heartbeat();

  startup(
    PROJECT_ID,
    STRINGIFY(PROJ_VERSION),
    apiDeviceLogin(),
    failuresInPast,
    reportFailureLogs,
    cleanFailures,
    restoreSafeFirmware
  );

  log(CLASS_PLATFORM, Debug, "Setup pins");
  pinMode(POWER_PIN, OUTPUT);
  digitalWrite(POWER_PIN, LOW);
  pinMode(SERVO0_PIN, OUTPUT);
  log(CLASS_PLATFORM, Debug, "Setup wdt");
  //ESP.wdtEnable(1); // argument not used

  log(CLASS_PLATFORM, Debug, "Setup wifi");
  WiFi.persistent(false);
  WiFi.hostname(apiDeviceLogin());
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

}

void runModeArchitecture() {
  Buffer timeAux(32);
  Timing::humanize(m->getClock()->currentTime(), &timeAux);
  timeAux.replace(' ', '\n');

  Buffer lcdAux(200);

  lcdAux.fill("%s\nVcc: %0.4f\nV:%s\n", timeAux.getBuffer(), VCC_FLOAT, STRINGIFY(PROJ_VERSION));
  logRaw(CLASS_PLATFORM, Debug, lcdAux.getBuffer());

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
    //lcd->setContrast(i);
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

int readRemainingSecs() {
  int s;
  if (ESP.rtcUserMemoryRead(0, (uint32_t*) &rtcData, sizeof(rtcData))) {
    if (rtcData.crc32 == 0) {
      return rtcData.remainingSecs;
    } else {
      log(CLASS_PLATFORM, Warn, "Invalid RTC");
      return -1;
    }
  } else {
    log(CLASS_PLATFORM, Debug, "No ds remaining");
    return -1;
  }
}

void writeRemainingSecs(int s) {
  rtcData.crc32 = 0;
  rtcData.remainingSecs = s;
  if (!ESP.rtcUserMemoryWrite(0, (uint32_t*) &rtcData, sizeof(rtcData))) {
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

  //m->getSleepinoSettings()->getStatus()->fill("freeheap:%d", ESP.getFreeHeap()); // made crash, reenable upon upgrade
  m->getSleepinoSettings()->getStatus()->fill("vcc:%0.2f", VCC_FLOAT);
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



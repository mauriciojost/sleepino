#ifndef PLATFORM_ESP_INC
#define PLATFORM_ESP_INC

#define QUESTION_ANSWER_TIMEOUT_MS 60000

#define RESTORE_WIFI_SSID "assid"
#define RESTORE_WIFI_PASS "apassword"
#define RESTORE_URL "http://main4ino.martinenhome.com/main4ino/prd/firmwares/" PROJECT_ID "/" PLATFORM_ID "/content?version=LATEST"
#define RESTORE_RETRIES 10

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

#define ABORT_LOG_FILENAME "/abort.log"
#define ABORT_LOG_MAX_LENGTH 64

#define LOG_BUFFER_MAX_LENGTH 1024

#ifdef TELNET_ENABLED
RemoteDebug telnet;
#endif // TELNET_ENABLED

Buffer *apiDeviceId = NULL;
Buffer *apiDevicePwd = NULL;
Buffer *contrast = NULL;
int currentLogLine = 0;
Buffer *cmdBuffer = NULL;
Buffer *cmdLast = NULL;

void debugHandle();
void handleInterrupt();

const char *apiDeviceLogin() {
  return initializeTuningVariable(&apiDeviceId, DEVICE_ALIAS_FILENAME, DEVICE_ALIAS_MAX_LENGTH, NULL, false)->getBuffer();
}

const char *apiDevicePass() {
  return initializeTuningVariable(&apiDevicePwd, DEVICE_PWD_FILENAME, DEVICE_PWD_MAX_LENGTH, NULL, true)->getBuffer();
}

void initLogBuffer() {
  if (logBuffer == NULL) {
    logBuffer = new Buffer(LOG_BUFFER_MAX_LENGTH);
  }
}


void configureModeArchitecture() {
  handleInterrupt();
  debugHandle();
}

void abort(const char *msg) {
  log(CLASS_PLATFORM, Error, "Abort: %s", msg);
  
  Buffer fcontent(ABORT_LOG_MAX_LENGTH);
  fcontent.fill("time=%ld msg=%s", now(), msg);
  writeFile(ABORT_LOG_FILENAME, fcontent.getBuffer());

  log(CLASS_PLATFORM, Warn, "Will deep sleep upon abort...");
  bool inte = sleepInterruptable(now(), SLEEP_PERIOD_PRE_ABORT_SEC);
  if (!inte) {
    deepSleepNotInterruptableSecs(now(), SLEEP_PERIOD_UPON_ABORT_SEC);
  } else {
    m->getBot()->setMode(ConfigureMode);
    log(CLASS_PLATFORM, Warn, "Abort skipped");
  }
}


void messageFunc(int x, int y, int color, bool wrap, MsgClearMode clearMode, int size, const char *str) {
  switch (clearMode) {
    case FullClear:
#ifdef LCD_ENABLED
      lcd->clearDisplay();
#endif // LCD_ENABLED
      break;
    case LineClear:
#ifdef LCD_ENABLED
      lcd->fillRect(x * size * LCD_CHAR_WIDTH, y * size * LCD_CHAR_HEIGHT, 128, size * LCD_CHAR_HEIGHT, !color);
#endif // LCD_ENABLED
      wrap = false;
      break;
    case NoClear:
      break;
  }
#ifdef LCD_ENABLED
  lcd->setTextWrap(wrap);
  lcd->setTextSize(size);
  lcd->setTextColor(color);
  lcd->setCursor(x * size * LCD_CHAR_WIDTH, y * size * LCD_CHAR_HEIGHT);
  lcd->print(str);
  lcd->display();
#endif // LCD_ENABLED
  log(CLASS_PLATFORM, Debug, "Msg(%d,%d):%s", x, y, str);
  delay(DELAY_MS_SPI);
}


void restoreSafeFirmware() { // to be invoked as last resource when things go wrong
#ifndef RESTORE_SAFE_FIRMWARE_DISABLED
  log(CLASS_PLATFORM, Warn, "RSF disabled");
#else // RESTORE_SAFE_FIRMWARE_DISABLED
  initializeWifi(RESTORE_WIFI_SSID, RESTORE_WIFI_PASS, RESTORE_WIFI_SSID, RESTORE_WIFI_PASS, true, RESTORE_RETRIES);
  updateFirmware(RESTORE_URL, STRINGIFY(PROJ_VERSION));
#endif // RESTORE_SAFE_FIRMWARE_DISABLED
}

void askStringQuestion(const char *question, Buffer *answer) {
  log(CLASS_PLATFORM, User, "Question: %s", question);
  Serial.setTimeout(QUESTION_ANSWER_TIMEOUT_MS);
  Serial.readBytesUntil('\n', answer->getUnsafeBuffer(), answer->getCapacity());
  answer->replace('\n', '\0');
  answer->replace('\r', '\0');
  log(CLASS_PLATFORM, User, "Answer: '%s'", answer->getBuffer());
}

void io(int pin, int val) {
  digitalWrite(pin, val);
}


void reactCommandCustom() { // for the use via telnet
  if (m == NULL) {
    return;
  }
#ifdef TELNET_ENABLED
  m->command(telnet.getLastCommand().c_str());
#endif // TELNET_ENABLED
}

void heartbeat() {
  espWdtFeed();
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
      } else if (c == '\n' && n == 1) { // if enter is pressed...
        log(CLASS_PLATFORM, Debug, "Enter");
        cmdBuffer->replace('\n', 0);
        cmdBuffer->replace('\r', 0);
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


#endif // PLATFORM_ESP_INC


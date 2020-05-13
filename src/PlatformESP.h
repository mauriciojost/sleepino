#ifndef PLATFORM_ESP_INC
#define PLATFORM_ESP_INC

#define QUESTION_ANSWER_TIMEOUT_MS 60000

#define RESTORE_WIFI_SSID "assid"
#define RESTORE_WIFI_PASS "apassword"
#define RESTORE_URL "http://main4ino.martinenhome.com/main4ino/prd/firmwares/" PROJECT_ID "/" PLATFORM_ID "/content?version=LATEST"
#define RESTORE_RETRIES 10

void restoreSafeFirmware() { // to be invoked as last resource when things go wrong
  initializeWifi(RESTORE_WIFI_SSID, RESTORE_WIFI_PASS, RESTORE_WIFI_SSID, RESTORE_WIFI_PASS, true, RESTORE_RETRIES);
  updateFirmware(RESTORE_URL, STRINGIFY(PROJ_VERSION));
}

void askStringQuestion(const char *question, Buffer *answer) {
  log(CLASS_PLATFORM, User, "Question: %s", question);
  Serial.setTimeout(QUESTION_ANSWER_TIMEOUT_MS);
  Serial.readBytesUntil('\n', answer->getUnsafeBuffer(), answer->getCapacity());
  answer->replace('\n', '\0');
  answer->replace('\r', '\0');
}

#endif // PLATFORM_ESP_INC


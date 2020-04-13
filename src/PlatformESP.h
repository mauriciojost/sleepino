#ifndef PLATFORM_ESP_INC
#define PLATFORM_ESP_INC

#define QUESTION_ANSWER_TIMEOUT_MS 60000

void askStringQuestion(const char *question, Buffer *answer) {
  log(CLASS_PLATFORM, User, "Question: %s", question);
  Serial.setTimeout(QUESTION_ANSWER_TIMEOUT_MS);
  Serial.readBytesUntil('\n', answer->getUnsafeBuffer(), answer->getCapacity());
  answer->replace('\n', '\0');
  answer->replace('\r', '\0');
}

#endif // PLATFORM_ESP_INC


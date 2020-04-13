#ifndef PLATFORM_ESP_INC
#define PLATFORM_ESP_INC

void askStringQuestion(const char *question, Buffer *answer) {
  log(CLASS_PLATFORM, User, "Question: %s", question);
  Serial.setTimeout(QUESTION_ANSWER_TIMEOUT_MS);
  Serial.readBytesUntil('\n', answer->getUnsafeBuffer(), QUESTION_ANSWER_MAX_LENGTH);
  answer->replace('\n', '\0');
  answer->replace('\r', '\0');
}

#endif // PLATFORM_ESP_INC


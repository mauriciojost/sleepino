#ifndef NOTIFIER_INC
#define NOTIFIER_INC

/**
 * Notifier
 *
 * Responsible of making sure that the end user each and every message they should have got.
 * Handles the LCD too.
 */

#define CLASS_NOTIFIER "NF"
#define MAX_NOTIF_LENGTH 16
#define MAX_MSG_LENGTH MAX_EFF_STR_LENGTH
#define NOTIF_LINE 7
#define NOTIF_SIZE 1
#define BLACK 0
#define WHITE 1
#define DO_WRAP true
#define DO_NOT_WRAP false

#ifndef LCD_WIDTH
#define LCD_WIDTH 21
#endif // LCD_WIDTH

#include <main4ino/Actor.h>
#include <main4ino/Queue.h>
#include <main4ino/RichBuffer.h>
#include <mod4ino/MsgClearMode.h>

enum NotifierProps {
  NotifierPropsDelimiter = 0 // count of properties
};

class Notifier : public Actor {

private:
  const char *name;
  Metadata *md;
  Buffer *b;
  void (*messageFunc)(int x, int y, int color, bool wrap, MsgClearMode clear, int size, const char *str);

  bool isInitialized() {
    return messageFunc != NULL;
  }

public:
  Notifier(const char *n) {
    name = n;
    messageFunc = NULL;
    md = new Metadata(n);
    md->getTiming()->setFreq("~1m");
    b = new Buffer(64);
  }

  void setMessage(const char* m) {
  	b->load(m);
    message(0, 1, b->getBuffer());
  }

  const char *getName() {
    return name;
  }

  void setup(void (*m)(int x, int y, int color, bool wrap, MsgClearMode clear, int size, const char *str)) {
    messageFunc = m;
  }

  /**
   * Show a message.
   *
   * Initialization safe.
   */
  void message(int line, int size, const char *format, ...) {
    if (!isInitialized()) {
      log(CLASS_NOTIFIER, Warn, "No init!");
      return;
    }
    Buffer buffer(MAX_MSG_LENGTH - 1);
    va_list args;
    va_start(args, format);
    vsnprintf(buffer.getUnsafeBuffer(), MAX_MSG_LENGTH, format, args);
    buffer.getUnsafeBuffer()[MAX_MSG_LENGTH - 1] = 0;
    messageFunc(0, line, BLACK, DO_NOT_WRAP, FullClear, size, buffer.getBuffer());
    va_end(args);

  }

  void act() {
  	if (getTiming()->matches()) {
      message(0, 1, b->getBuffer());
  	}
  }

  const char *getPropName(int propIndex) {
    switch (propIndex) {
      default:
        return "";
    }
  }

  void getSetPropValue(int propIndex, GetSetMode m, const Value *targetValue, Value *actualValue) {}

  int getNroProps() {
    return NotifierPropsDelimiter;
  }

  Metadata *getMetadata() {
    return md;
  }
};

#endif // NOTIFIER_INC

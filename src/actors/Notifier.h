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

#define EMPTY_NOTIF_REPRESENTATION ""
#define NOTIFS_SEPARATOR ':'
#define MAX_NRO_NOTIFS 4 // must be aligned with enum below

enum NotifierProps {
  NotifierNotifsProp = 0,
  NotifierPropsDelimiter // count of properties
};

class Notifier : public Actor {

private:
  const char *name;
  Metadata *md;
  Queue<MAX_NRO_NOTIFS, MAX_NOTIF_LENGTH> queue;
  void (*messageFunc)(int x, int y, int color, bool wrap, MsgClearMode clear, int size, const char *str);
  void((*lcdImgFunc)(char img, uint8_t bitmap[]));

  bool isInitialized() {
    return lcdImgFunc != NULL && messageFunc != NULL;
  }

  void notify(bool forceClean) {
    const char *currentNotif = getNotification();
    Buffer msg(LCD_WIDTH);
    if (currentNotif != NULL) {
      log(CLASS_NOTIFIER, Debug, "Notif(%d): %s", queue.size(), currentNotif);
      msg.fill("(%d) %s", queue.size(), currentNotif);
      messageFunc(0, NOTIF_LINE, WHITE, DO_NOT_WRAP, LineClear, NOTIF_SIZE, msg.center(' ', LCD_WIDTH));
    } else {
      if (forceClean) {
        msg.fill("<>");
        messageFunc(0, NOTIF_LINE, WHITE, DO_NOT_WRAP, LineClear, NOTIF_SIZE, msg.center(' ', LCD_WIDTH));
      }
      log(CLASS_NOTIFIER, Debug, "No notifs");
    }
  }

public:
  Notifier(const char *n) {
    name = n;
    messageFunc = NULL;
    md = new Metadata(n);
    lcdImgFunc = NULL;
    md->getTiming()->setFreq("never");
  }

  const char *getName() {
    return name;
  }

  void setup(void (*i)(char img, uint8_t bitmap[]),
             void (*m)(int x, int y, int color, bool wrap, MsgClearMode clear, int size, const char *str)) {
    lcdImgFunc = i;
    messageFunc = m;
  }

  /**
   * Display an LCD image.
   *
   * Initialization safe.
   */
  void lcdImg(char img, uint8_t bitmap[]) {
    if (!isInitialized()) {
      log(CLASS_NOTIFIER, Warn, "No init!");
      return;
    }
    lcdImgFunc(img, bitmap);
    notify(false); // apart from the image, also notify if notifications are available
  }

  void clearLcd() {
    lcdImg('l', NULL);
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
    messageFunc(0, line, WHITE, DO_WRAP, FullClear, size, buffer.getBuffer());
    va_end(args);

    notify(false); // apart from the message, also notify if notifications are available
  }

  int notification(const char *msg) {
    int i = queue.pushUnique(msg);
    log(CLASS_NOTIFIER, Debug, "New notif: %s (%d notifs)", msg, i);
    getMetadata()->changed();
    notify(false); // update notification
    return i;
  }

  const char *getNotification() {
    return queue.get();
  }

  int notificationRead() {
    int i = queue.pop();
    log(CLASS_NOTIFIER, Debug, "Notif read: %d notifs left", i);
    getMetadata()->changed();
    notify(true); // update notification
    return i;
  }

  void act() {}

  const char *getPropName(int propIndex) {
    switch (propIndex) {
      case (NotifierNotifsProp):
        return STATUS_PROP_PREFIX "ns";
      default:
        return "";
    }
  }

  void bufferToQueue(RichBuffer *b) {
    const char *p;
    while ((p = b->split(NOTIFS_SEPARATOR)) != NULL) {
      if (strcmp(p, EMPTY_NOTIF_REPRESENTATION) != 0) { // filter out empty notifs
        queue.pushUnique(p);                            // add to the existent ones, does not remove
      }
    }
  }

  void queueToBuffer(RichBuffer *b) {
    b->clear();
    for (int i = 0; i < queue.capacity(); i++) {
      b->append(queue.getAt(i, EMPTY_NOTIF_REPRESENTATION));
      b->append(NOTIFS_SEPARATOR);
    }
  }

  void getSetPropValue(int propIndex, GetSetMode m, const Value *targetValue, Value *actualValue) {
    if (propIndex == NotifierNotifsProp) {
      RichBuffer b = RichBuffer((MAX_NOTIF_LENGTH + 1) * MAX_NRO_NOTIFS);
      if (m == SetCustomValue) {
        b.load(targetValue);
        bufferToQueue(&b);
      }
      if (actualValue != NULL) {
        queueToBuffer(&b);
        actualValue->load(&b);
      }
    }
    if (m != GetValue) {
      getMetadata()->changed();
    }
  }

  int getNroProps() {
    return NotifierPropsDelimiter;
  }

  Metadata *getMetadata() {
    return md;
  }
};

#endif // NOTIFIER_INC

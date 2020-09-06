#ifndef SERVON_INC
#define SERVON_INC

#include <log4ino/Log.h>
#include <main4ino/Actor.h>
#include <functional>

#define CLASS_SERVON "SE"

enum ServonProps {
  ServonPropsDelimiter = 0
};

class Servon : public Actor {

private:
  const char *name;
  Metadata *md;
  std::function<void (int d)> rotate;
  std::function<void (bool b)> enable;

public:
  Servon(const char *n) {
    name = n;
    md = new Metadata(n);
    md->getTiming()->setFreq("~1m");
    rotate = NULL;
    enable = NULL;
  }

  void setup(std::function<void (int d)> r, std::function<void (int d)> e){
  	rotate = r;
  	enable = e;
  }

  const char *getName() {
    return name;
  }

  int getNroProps() {
    return ServonPropsDelimiter;
  }

  void act() {
    if (md->getTiming()->matches()) {
      log(CLASS_SERVON, Debug, "Act!");
      if (rotate != NULL) {
        enable(true);
        log(CLASS_SERVON, Debug, "Enabled, moving...");
        for (int i = 0; i <= 180; i = i + 30) {
          rotate(i);
        }
        enable(false);
        log(CLASS_SERVON, Debug, "Disabled");
      } else {
        log(CLASS_SERVON, Warn, "No init!");
      }
    }
  }

  const char *getPropName(int propIndex) {
    return "";
  }

  void getSetPropValue(int propIndex, GetSetMode m, const Value *targetValue, Value *actualValue) { }

  Metadata *getMetadata() {
    return md;
  }

};

#endif // SERVON_INC

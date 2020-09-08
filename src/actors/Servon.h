#ifndef SERVON_INC
#define SERVON_INC

#include <log4ino/Log.h>
#include <main4ino/Actor.h>
#include <functional>

#define CLASS_SERVON "SE"

enum ServonProps {
  ServonFreqProp = 0,   // frequency of synchronization
  ServonPropsDelimiter
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
        for (int i = 0; i <= 180; i = i + 90) {
          rotate(i);
        }
        enable(false);
      } else {
        log(CLASS_SERVON, Warn, "No init!");
      }
    }
  }

public: const char *getPropName(int propIndex) {
    switch (propIndex) {
      case (ServonFreqProp):
        return ADVANCED_PROP_PREFIX "freq";
      default:
        return "";
    }
  }

public: void getSetPropValue(int propIndex, GetSetMode m, const Value *targetValue, Value *actualValue) {
    switch (propIndex) {
      case (ServonFreqProp): {
        setPropTiming(m, targetValue, actualValue, md->getTiming());
      } break;
      default:
        break;
    }
    if (m != GetValue) {
      getMetadata()->changed();
    }
  }


  Metadata *getMetadata() {
    return md;
  }

};

#endif // SERVON_INC

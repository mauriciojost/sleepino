#ifndef BATTERY_INC
#define BATTERY_INC

#include <log4ino/Log.h>
#include <main4ino/Actor.h>

#define CLASS_BATTERY "BY"

enum BatteryProps {
  BatteryChargeProp = 0,        // integer, charge percentage
  BatteryVccNowProp,            // integer, measure of Vcc [mV]
  BatteryVccMaxProp,            // integer, maximum measure of Vcc [mV]
  BatteryVccMinProp,            // integer, minimum measure of Vcc [mV]
  BatteryPropsDelimiter
};

class Battery : public Actor {

private:
  const char *name;
  Metadata *md;
  int charge;
  int vccmVoltsNow;
  int vccmVoltsMin;
  int vccmVoltsMax;
  float (*vcc)();

public:
  SleepinoSettings(const char *n) {
    name = n;
    md = new Metadata(n);
    md->getTiming()->setFreq("~10m");
    charge = 0;
    vccmVoltsNow = 1;
    vccmVoltsMin = 1;
    vccmVoltsMax = 1;
    vcc = NULL;
  }

  void setup(float(*v)()){
  	vcc = v;
  }

  const char *getName() {
    return name;
  }

  int getNroProps() {
    return BatteryPropsDelimiter;
  }

  void act() {
    if (md->getTiming()->matches()) {
      if (vcc != NULL) {
        float v = vcc();
        log(CLASS_BATTERY, Debug, "Vcc: %0.3f", v);
        vccmVoltsNow = v * 1000;
        vccmVoltsMin = MINIM(vccmVoltsNow, vccmVoltsMin);
        vccmVoltsMax = MAXIM(vccmVoltsNow, vccmVoltsMax);
        charge = ((float)(vccmVoltsNow - vccmVoltsMin) / (vccmVoltsMax - vccmVoltsMin)) * 100 ;
        log(CLASS_BATTERY, Debug, "[mvnow=%d <= mvmin=%d <= mvmax=%d]", vccmVoltsMin, vccmVoltsNow, vccmVoltsMax);
        getMetadata()->changed();
      } else {
        log(CLASS_BATTERY, Warn, "No init!");
      }
    }
  }

  const char *getPropName(int propIndex) {
    switch (propIndex) {
      case (BatteryChargeProp):
        return STATUS_PROP_PREFIX "charge";
      case (BatteryVccNowProp):
        return STATUS_PROP_PREFIX "mvcc";
      case (BatteryVccMaxProp):
        return STATUS_PROP_PREFIX "mvccmax";
      case (BatteryVccMinProp):
        return STATUS_PROP_PREFIX "mvccmin";
      default:
        return "";
    }
  }

  void getSetPropValue(int propIndex, GetSetMode m, const Value *targetValue, Value *actualValue) {
    switch (propIndex) {
      case (BatteryChargeProp):
        setPropInteger(m, targetValue, actualValue, &charge);
        break;
      case (BatteryVccNowProp):
        setPropInteger(m, targetValue, actualValue, &vccmVoltsNow);
        break;
      case (BatteryVccMaxProp):
        setPropInteger(m, targetValue, actualValue, &vccmVoltsMax);
        break;
      case (BatteryVccMinProp):
        setPropInteger(m, targetValue, actualValue, &vccmVoltsMin);
        break;
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

#endif // BATTERY_INC

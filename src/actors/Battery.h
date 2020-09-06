#ifndef BATTERY_INC
#define BATTERY_INC

#include <log4ino/Log.h>
#include <main4ino/Actor.h>

#define CLASS_BATTERY "BA"

#define VCC_MVOLTS_MIN_DEFAULT 100000
#define VCC_MVOLTS_MAX_DEFAULT 0
#define VCC_MVOLTS_NOW_DEFAULT 3300 // 3.3v

enum BatteryProps {
  BatteryChargeProp = 0,        // float, charge percentage
  BatteryVccNowProp,            // integer, measure of Vcc [mV]
  BatteryVccMaxProp,            // integer, maximum measure of Vcc [mV]
  BatteryVccMinProp,            // integer, minimum measure of Vcc [mV]
  BatteryPropsDelimiter
};

class Battery : public Actor {

private:
  const char *name;
  Metadata *md;
  float charge;
  int vccmVoltsNow;
  int vccmVoltsMin;
  int vccmVoltsMax;
  float (*vcc)();

public:
  Battery(const char *n) {
    name = n;
    md = new Metadata(n);
    md->getTiming()->setFreq("~10m");
    charge = 0.0;
    vccmVoltsNow = VCC_MVOLTS_NOW_DEFAULT;
    vccmVoltsMin = VCC_MVOLTS_MIN_DEFAULT;
    vccmVoltsMax = VCC_MVOLTS_MAX_DEFAULT;
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
        log(CLASS_BATTERY, Debug, "[mvmin=%d <= mvnow=%d <= mvmax=%d]", vccmVoltsMin, vccmVoltsNow, vccmVoltsMax);
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
        setPropFloat(m, targetValue, actualValue, &charge);
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

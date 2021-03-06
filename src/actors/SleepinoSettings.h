#ifndef MODULE_SETTINGS_INC
#define MODULE_SETTINGS_INC

#include <log4ino/Log.h>
#include <main4ino/Actor.h>

#define STATUS_BUFFER_SIZE 64
#define CLASS_SLEEPINO_SETTINGS "SL"
#define DEFAULT_FS_LOGS_LENGTH 32
#define DEFAULT_LS_DURATION_SECS 60

enum SleepinoSettingsProps {
  SleepinoSettingsLcdLogsProp = 0,   // boolean, define if the device display logs in LCD
  SleepinoSettingsStatusProp,        // string, defines the current general status of the device (vcc level, heap, etc)
  SleepinoSettingsFsLogsProp,        // boolean, define if logs are to be dumped in the file system (only in debug mode)
  SleepinoSettingsFsLengthLogsProp,  // integer, define the length of the line in the logs
  SleepinoSettingsLsDurationSecsProp,// integer, duration to light sleep
  SleepinoSettingsWifiSsidBackupProp,// string, ssid for backup wifi network
  SleepinoSettingsWifiPassBackupProp,// string, pass for backup wifi network
  SleepinoSettingsPropsDelimiter
};

class SleepinoSettings : public Actor {

private:
  const char *name;
  bool lcdLogs;
  Buffer *status;
  bool fsLogs;
  int fsLogsLength;
  int lightSleepDurationSecs;
  Buffer *ssidb;
  Buffer *passb;
  Metadata *md;
  void (*command)(const char*);

public:
  SleepinoSettings(const char *n) {
    name = n;
    lcdLogs = true;
    status = new Buffer(STATUS_BUFFER_SIZE);
    fsLogs = true;
    fsLogsLength = DEFAULT_FS_LOGS_LENGTH;
    lightSleepDurationSecs = DEFAULT_LS_DURATION_SECS;
    ssidb = new Buffer(20);
    ssidb->load("defaultssid");
    passb = new Buffer(20);
    passb->load("defaultssid");
    md = new Metadata(n);
    md->getTiming()->setFreq("~24h");
    command = NULL;
  }

  void setup(void(*cmd)(const char*)){
  	command = cmd;
  }

  const char *getName() {
    return name;
  }

  int getNroProps() {
    return SleepinoSettingsPropsDelimiter;
  }

  void act() { }

  const char *getPropName(int propIndex) {
    switch (propIndex) {
      case (SleepinoSettingsLcdLogsProp):
        return DEBUG_PROP_PREFIX "lcdlogs";
      case (SleepinoSettingsStatusProp):
        return STATUS_PROP_PREFIX "status";
      case (SleepinoSettingsFsLogsProp):
        return DEBUG_PROP_PREFIX "fslogs";
      case (SleepinoSettingsFsLengthLogsProp):
        return DEBUG_PROP_PREFIX "fsll";
      case (SleepinoSettingsLsDurationSecsProp):
        return ADVANCED_PROP_PREFIX "lssecs";
      case (SleepinoSettingsWifiSsidBackupProp):
        return SENSITIVE_PROP_PREFIX "ssidb";
      case (SleepinoSettingsWifiPassBackupProp):
        return SENSITIVE_PROP_PREFIX "passb";
      default:
        return "";
    }
  }

  void getSetPropValue(int propIndex, GetSetMode m, const Value *targetValue, Value *actualValue) {
    switch (propIndex) {
      case (SleepinoSettingsLcdLogsProp):
        setPropBoolean(m, targetValue, actualValue, &lcdLogs);
        break;
      case (SleepinoSettingsStatusProp):
        setPropValue(m, targetValue, actualValue, status);
        break;
      case (SleepinoSettingsFsLogsProp):
        setPropBoolean(m, targetValue, actualValue, &fsLogs);
        break;
      case (SleepinoSettingsFsLengthLogsProp):
        setPropInteger(m, targetValue, actualValue, &fsLogsLength);
        break;
      case (SleepinoSettingsLsDurationSecsProp):
        setPropInteger(m, targetValue, actualValue, &lightSleepDurationSecs);
        break;
      case (SleepinoSettingsWifiSsidBackupProp):
        setPropValue(m, targetValue, actualValue, ssidb);
        break;
      case (SleepinoSettingsWifiPassBackupProp):
        setPropValue(m, targetValue, actualValue, passb);
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

  Buffer *getStatus() {
    return status;
  }

  bool fsLogsEnabled() {
    return fsLogs;
  }

  int getFsLogsLength() {
    return fsLogsLength;
  }

  int getLsDurationSecs() {
    return lightSleepDurationSecs;
  }

  bool getLcdLogs() {
    return lcdLogs;
  }

  Buffer *getBackupWifiSsid() {
    return ssidb;
  }

  Buffer *getBackupWifiPass() {
    return passb;
  }
};

#endif // MODULE_SETTINGS_INC

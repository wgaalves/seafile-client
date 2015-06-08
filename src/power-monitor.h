#ifndef SEAFILE_CLIENT_POWER_MONITOR_H
#define SEAFILE_CLIENT_POWER_MONITOR_H
#include <QObject>

class PowerMonitor : public QObject {
    Q_OBJECT
    PowerMonitor(const PowerMonitor &);            // DELETED
    PowerMonitor &operator=(const PowerMonitor &); // DELETED

  public:
    static PowerMonitor* instance() { 
        if (instance_ == NULL) {
            static PowerMonitor pm;
            instance_ = &pm;
        }
        return instance_;
    }

    // call these methods in a run loop
    void load();
    void unload();

    enum PowerEventType { POWER_STATE_EVENT, RESUME_EVENT, SUSPEND_EVENT };

    bool isOnBatteryPower() { return on_battery_power_; }
    void processPowerEvent(PowerEventType power_event);

  signals:
    void suspend();
    void resume();
    void powerStateChanged(bool battery_in_use);

  private:
    PowerMonitor() : on_battery_power_(false), suspended_(false), impl_(NULL) {}
    static PowerMonitor *instance_;

    bool on_battery_power_;
    bool suspended_;
    void *impl_;
};

#endif // SEAFILE_CLIENT_POWER_MONITOR_H

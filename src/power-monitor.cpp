#include "power-monitor.h"

#ifdef Q_OS_WIN32
#include <windows.h>
#endif
namespace {
#ifdef Q_OS_WIN32
const wchar_t kWindowClassName[] = L"PowerMessageWindow";
HMODULE GetModuleFromAddress(void *address) {
    HMODULE instance = NULL;
    if (!GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                            GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                            static_cast<char *>(address), &instance)) {
        // fatal error
        exit(-1);
    }
    return instance;
}

class PowerMessageWindow {
  public:
    PowerMessageWindow()
        : message_hwnd_(NULL) {
        WNDCLASSEXW window_class;
        WNDPROC window_proc = &WndProcThunk;
        window_class.cbSize = sizeof(WNDCLASSEX);
        window_class.style = 0;
        window_class.lpfnWndProc = window_proc;
        window_class.cbClsExtra = 0;
        window_class.cbWndExtra = 0;
        window_class.hInstance = GetModuleFromAddress((void*)window_proc);
        window_class.hIcon = NULL;
        window_class.hCursor = NULL;
        window_class.hbrBackground = NULL;
        window_class.lpszMenuName = NULL;
        window_class.lpszClassName = kWindowClassName;
        window_class.hIconSm = NULL;

        instance_ = window_class.hInstance;
        if (instance_ == NULL)
            return;
        ATOM clazz = RegisterClassExW(&window_class);
        if (!clazz)
            return;
        message_hwnd_ =
            CreateWindowExW(WS_EX_NOACTIVATE, kWindowClassName, NULL, WS_POPUP,
                            0, 0, 0, 0, NULL, NULL, instance_, NULL);
    }
    ~PowerMessageWindow() {
        if (message_hwnd_) {
            DestroyWindow(message_hwnd_);
            UnregisterClassW(kWindowClassName, instance_);
        }
    }

  private:
    static void ProcessWmPowerBroadcastMessage(WPARAM event_id) {
        PowerMonitor::PowerEventType power_event;
        switch (event_id) {
        case PBT_APMPOWERSTATUSCHANGE: // The power status changed.
            power_event = PowerMonitor::POWER_STATE_EVENT;
            break;
        case PBT_APMRESUMEAUTOMATIC: // Resume from suspend.
            power_event = PowerMonitor::RESUME_EVENT;
            break;
        case PBT_APMSUSPEND: // System has been suspended.
            power_event = PowerMonitor::SUSPEND_EVENT;
            break;
        default:
            return;
        }

        PowerMonitor::instance()->processPowerEvent(power_event);
    }

    static LRESULT CALLBACK WndProcThunk(HWND hwnd, UINT message, WPARAM wparam,
                                         LPARAM lparam) {
        switch (message) {
        case WM_POWERBROADCAST:
            ProcessWmPowerBroadcastMessage(wparam);
            return TRUE;
        default:
            return DefWindowProc(hwnd, message, wparam, lparam);
        }
    }

    HMODULE instance_;
    HWND message_hwnd_;
};

bool isOnBatteryPowerFromSystem() {
    SYSTEM_POWER_STATUS status;
    if (!GetSystemPowerStatus(&status)) {
        return false;
    }
    return (status.ACLineStatus == 0);
}
#elif defined(Q_OS_MAC)
bool isOnBatteryPowerFromSystem() { return false; }
#else
bool isOnBatteryPowerFromSystem() { return false; }
#endif
} // anonymous namespace

PowerMonitor* PowerMonitor::instance_ = NULL;

void PowerMonitor::load() {
    if (impl_)
        return;
#ifdef Q_OS_WIN32
    impl_ = new PowerMessageWindow;
#endif
    on_battery_power_ = false;
    suspended_ = false;
}

void PowerMonitor::unload() {
    if (!impl_)
        return;
#ifdef Q_OS_WIN32
    delete static_cast<PowerMessageWindow *>(impl_);
    impl_ = NULL;
#endif
    on_battery_power_ = false;
    suspended_ = false;
}

void PowerMonitor::processPowerEvent(PowerEventType power_event) {
    switch (power_event) {
    case POWER_STATE_EVENT: {
        bool new_on_battery_power = isOnBatteryPowerFromSystem();
        if (on_battery_power_ != new_on_battery_power) {
            emit powerStateChanged((on_battery_power_ = new_on_battery_power));
        }
    } break;
    case RESUME_EVENT:
        suspended_ = false;
        emit resume();
        break;
    case SUSPEND_EVENT:
        suspended_ = true;
        emit suspend();
        break;
    }
}

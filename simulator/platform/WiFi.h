#pragma once
// WiFi.h shim for desktop simulator

#include <cstdint>
#include "WString.h"

typedef enum {
    WL_IDLE_STATUS = 0,
    WL_NO_SSID_AVAIL = 1,
    WL_SCAN_COMPLETED = 2,
    WL_CONNECTED = 3,
    WL_CONNECT_FAILED = 4,
    WL_CONNECTION_LOST = 5,
    WL_DISCONNECTED = 6
} wl_status_t;

class WiFiClass {
public:
    wl_status_t status() const { return WL_DISCONNECTED; }
    bool isConnected() const { return false; }
    String localIP() const { return String("0.0.0.0"); }
    bool disconnect(bool = false) { return true; }
};

inline WiFiClass WiFi;

#pragma once
// WiFiClientSecure.h shim for desktop simulator

#include <cstdint>
#include "WString.h"

class WiFiClientSecure {
public:
    void setInsecure() {}
    bool connect(const char*, uint16_t, int = 0) { return false; }
    void stop() {}
    bool connected() const { return false; }
    int available() const { return 0; }
    size_t write(const uint8_t*, size_t) { return 0; }
    int read(uint8_t*, size_t) { return -1; }
    String readStringUntil(char) { return String(""); }
};

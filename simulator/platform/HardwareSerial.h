#pragma once
// Minimal HardwareSerial / HWCDC stub for desktop simulator

#include "Print.h"
#include "WString.h"
#include <cstdio>
#include <cstdarg>

class HWCDC : public Print {
public:
    void begin(unsigned long) {}
    operator bool() const { return true; }

    size_t write(uint8_t b) override {
        return fputc(b, stdout) != EOF ? 1 : 0;
    }
    size_t write(const uint8_t* buffer, size_t size) override {
        return fwrite(buffer, 1, size, stdout);
    }
    void flush() override { fflush(stdout); }

    int available() { return 0; }
    String readStringUntil(char) { return String(); }

    size_t printf(const char* format, ...) {
        va_list args;
        va_start(args, format);
        int ret = vfprintf(stdout, format, args);
        va_end(args);
        return ret > 0 ? ret : 0;
    }
};

inline HWCDC Serial;

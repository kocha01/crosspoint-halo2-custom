#pragma once
// Minimal Print base class for Arduino compatibility

#include <cstdint>
#include <cstdio>
#include <cstdarg>
#include <cstring>
#include "WString.h"

class Print {
public:
    virtual ~Print() = default;
    virtual size_t write(uint8_t b) = 0;
    virtual size_t write(const uint8_t* buffer, size_t size) {
        size_t written = 0;
        for (size_t i = 0; i < size; i++) {
            written += write(buffer[i]);
        }
        return written;
    }
    virtual void flush() {}

    size_t print(const char* s) {
        return write(reinterpret_cast<const uint8_t*>(s), strlen(s));
    }
    size_t println(const char* s = "") {
        size_t n = print(s);
        n += write('\n');
        return n;
    }
    size_t printf(const char* format, ...) {
        char buf[256];
        va_list args;
        va_start(args, format);
        int len = vsnprintf(buf, sizeof(buf), format, args);
        va_end(args);
        if (len > 0) {
            return write(reinterpret_cast<const uint8_t*>(buf), len);
        }
        return 0;
    }
};

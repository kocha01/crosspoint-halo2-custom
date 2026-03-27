#pragma once
// Minimal Arduino compatibility shim for desktop simulator

#include <cstdint>
#include <cstddef>
#include <cstring>
#include <cassert>
#include <chrono>
#include <thread>
#include <string>

// Standard Arduino types (already in cstdint)
using byte = uint8_t;

// Timing
inline uint32_t millis() {
    static auto start = std::chrono::steady_clock::now();
    auto now = std::chrono::steady_clock::now();
    return static_cast<uint32_t>(std::chrono::duration_cast<std::chrono::milliseconds>(now - start).count());
}

inline void delay(uint32_t ms) {
    std::this_thread::sleep_for(std::chrono::milliseconds(ms));
}

inline void yield() {
    std::this_thread::yield();
}

// micros (used by FontDecompressor for profiling)
inline uint32_t micros() {
    static auto start = std::chrono::steady_clock::now();
    auto now = std::chrono::steady_clock::now();
    return static_cast<uint32_t>(std::chrono::duration_cast<std::chrono::microseconds>(now - start).count());
}

// GPIO constants
constexpr int HIGH = 1;
constexpr int LOW = 0;
constexpr int INPUT = 0;
constexpr int INPUT_PULLUP = 2;
constexpr int OUTPUT = 1;

inline void pinMode(int, int) {}
inline int digitalRead(int) { return LOW; }
inline void digitalWrite(int, int) {}
inline int analogRead(int) { return 0; }

// PROGMEM (no-op on desktop)
#define PROGMEM
#define memcpy_P memcpy
#define pgm_read_byte(addr) (*(const uint8_t*)(addr))
#define pgm_read_word(addr) (*(const uint16_t*)(addr))
#define pgm_read_dword(addr) (*(const uint32_t*)(addr))

// ESP class stub
class EspClass {
public:
    uint32_t getFreeHeap() const { return 300000; }
    uint32_t getHeapSize() const { return 380000; }
    uint32_t getMinFreeHeap() const { return 200000; }
    uint32_t getMaxAllocHeap() const { return 100000; }
    void restart() {}
};
inline EspClass ESP;

// Include our String and Print shims
#include "WString.h"
#include "Print.h"

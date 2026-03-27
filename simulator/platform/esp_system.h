#pragma once
// ESP-IDF esp_system.h shim for desktop simulator

#include <cstdint>

inline uint32_t esp_get_free_heap_size() {
    return 300000;  // Simulated 300KB free heap
}

inline uint32_t esp_get_minimum_free_heap_size() {
    return 200000;
}

inline void esp_restart() {
    // No-op in simulator
}

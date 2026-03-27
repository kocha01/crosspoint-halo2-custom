#pragma once
// ESP-IDF esp_sntp.h shim for desktop simulator

#include <cstdint>

typedef enum {
    ESP_SNTP_OPMODE_POLL = 0,
    ESP_SNTP_OPMODE_LISTENONLY = 1
} esp_sntp_operatingmode_t;

inline bool esp_sntp_enabled() { return false; }
inline void esp_sntp_stop() {}
inline void esp_sntp_setoperatingmode(esp_sntp_operatingmode_t) {}
inline void esp_sntp_setservername(int, const char*) {}
inline void esp_sntp_init() {}

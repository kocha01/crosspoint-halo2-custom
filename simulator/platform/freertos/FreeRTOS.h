#pragma once
// FreeRTOS type stubs for desktop simulator (single-threaded)

#include <cstdint>

#define pdTRUE  1
#define pdFALSE 0
#define pdPASS  pdTRUE

using BaseType_t = int;
using UBaseType_t = unsigned int;
using TickType_t = uint32_t;

#define portMAX_DELAY 0xFFFFFFFF

// Critical section (no-op in single-threaded simulator)
inline void taskENTER_CRITICAL(void*) {}
inline void taskEXIT_CRITICAL(void*) {}
#define portENTER_CRITICAL_ISR(x)
#define portEXIT_CRITICAL_ISR(x)

// Notification actions
enum eNotifyAction { eNoAction, eSetBits, eIncrement, eSetValueWithOverwrite, eSetValueWithoutOverwrite };

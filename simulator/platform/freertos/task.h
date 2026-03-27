#pragma once
// Task stubs for desktop simulator (single-threaded)

#include "FreeRTOS.h"
#include <cstddef>

using TaskHandle_t = void*;
using TaskFunction_t = void (*)(void*);

inline BaseType_t xTaskCreate(TaskFunction_t, const char*, uint32_t, void*, UBaseType_t, TaskHandle_t* handle) {
    // Store a dummy non-null handle
    if (handle) *handle = reinterpret_cast<void*>(0x2);
    return pdPASS;
}

inline void vTaskDelete(TaskHandle_t) {}
inline void vTaskDelay(TickType_t) {}

inline TaskHandle_t xTaskGetCurrentTaskHandle() { return reinterpret_cast<void*>(0x3); }
inline UBaseType_t uxTaskGetStackHighWaterMark(TaskHandle_t) { return 4096; }

inline BaseType_t xTaskNotify(TaskHandle_t, uint32_t, eNotifyAction) { return pdTRUE; }
inline uint32_t ulTaskNotifyTake(BaseType_t, TickType_t) { return 1; }

#define portYIELD_FROM_ISR(x)

// Queue stubs (used by some FreeRTOS APIs)
using QueueHandle_t = void*;
inline QueueHandle_t xQueueCreate(UBaseType_t, UBaseType_t) { return reinterpret_cast<void*>(0x4); }
inline BaseType_t xQueueSend(QueueHandle_t, const void*, TickType_t) { return pdTRUE; }
inline BaseType_t xQueueSendFromISR(QueueHandle_t, const void*, BaseType_t*) { return pdTRUE; }
inline BaseType_t xQueueReceive(QueueHandle_t, void*, TickType_t) { return pdFALSE; }

#pragma once
// Semaphore/Mutex stubs for desktop simulator (single-threaded, all no-ops)

#include "FreeRTOS.h"

using SemaphoreHandle_t = void*;

// Dummy non-null pointer to satisfy null checks
inline void* _simDummyMutex = reinterpret_cast<void*>(0x1);

inline SemaphoreHandle_t xSemaphoreCreateMutex() { return _simDummyMutex; }
inline BaseType_t xSemaphoreTake(SemaphoreHandle_t, TickType_t) { return pdTRUE; }
inline BaseType_t xSemaphoreGive(SemaphoreHandle_t) { return pdTRUE; }
inline BaseType_t xSemaphoreGiveFromISR(SemaphoreHandle_t, BaseType_t*) { return pdTRUE; }
inline void* xSemaphoreGetMutexHolder(SemaphoreHandle_t) { return nullptr; }
inline BaseType_t xQueuePeek(SemaphoreHandle_t, void*, TickType_t) { return pdTRUE; }

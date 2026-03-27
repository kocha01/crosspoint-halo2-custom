// Desktop HalGPIO implementation — SDL2 keyboard input
#include <HalGPIO.h>
#include <SDL.h>
#include <cstring>

// Button state arrays
static bool currentState[7] = {};
static bool lastState[7] = {};
static bool pressedEvents[7] = {};
static bool releasedEvents[7] = {};
static uint32_t pressStartTime[7] = {};
static uint32_t lastHeldTime = 0;

// Map SDL scancodes to button indices
static int sdlKeyToButton(SDL_Scancode key) {
    switch (key) {
        case SDL_SCANCODE_ESCAPE: return HalGPIO::BTN_BACK;
        case SDL_SCANCODE_RETURN:
        case SDL_SCANCODE_RETURN2: return HalGPIO::BTN_CONFIRM;
        case SDL_SCANCODE_LEFT:   return HalGPIO::BTN_LEFT;
        case SDL_SCANCODE_RIGHT:  return HalGPIO::BTN_RIGHT;
        case SDL_SCANCODE_UP:     return HalGPIO::BTN_UP;
        case SDL_SCANCODE_DOWN:   return HalGPIO::BTN_DOWN;
        case SDL_SCANCODE_P:      return HalGPIO::BTN_POWER;
        default: return -1;
    }
}

void HalGPIO::begin() {
    memset(currentState, 0, sizeof(currentState));
    memset(lastState, 0, sizeof(lastState));
    memset(pressedEvents, 0, sizeof(pressedEvents));
    memset(releasedEvents, 0, sizeof(releasedEvents));
    memset(pressStartTime, 0, sizeof(pressStartTime));
}

void HalGPIO::update() {
    // Clear edge-triggered events
    memset(pressedEvents, 0, sizeof(pressedEvents));
    memset(releasedEvents, 0, sizeof(releasedEvents));

    // Save previous state
    memcpy(lastState, currentState, sizeof(currentState));

    // Poll SDL events (keyboard only — window events handled by main loop)
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        if (event.type == SDL_QUIT) {
            exit(0);
        }
        if (event.type == SDL_KEYDOWN && !event.key.repeat) {
            int btn = sdlKeyToButton(event.key.keysym.scancode);
            if (btn >= 0) {
                currentState[btn] = true;
                pressStartTime[btn] = millis();
            }
        }
        if (event.type == SDL_KEYUP) {
            int btn = sdlKeyToButton(event.key.keysym.scancode);
            if (btn >= 0) {
                currentState[btn] = false;
            }
        }
    }

    // Detect edges
    for (int i = 0; i < 7; i++) {
        if (currentState[i] && !lastState[i]) pressedEvents[i] = true;
        if (!currentState[i] && lastState[i]) releasedEvents[i] = true;
    }

    // Track held time for any pressed button
    for (int i = 0; i < 7; i++) {
        if (currentState[i]) {
            lastHeldTime = millis() - pressStartTime[i];
        }
    }
}

bool HalGPIO::isPressed(uint8_t btn) const { return btn < 7 && currentState[btn]; }
bool HalGPIO::wasPressed(uint8_t btn) const { return btn < 7 && pressedEvents[btn]; }
bool HalGPIO::wasReleased(uint8_t btn) const { return btn < 7 && releasedEvents[btn]; }

bool HalGPIO::wasAnyPressed() const {
    for (int i = 0; i < 7; i++) if (pressedEvents[i]) return true;
    return false;
}
bool HalGPIO::wasAnyReleased() const {
    for (int i = 0; i < 7; i++) if (releasedEvents[i]) return true;
    return false;
}

unsigned long HalGPIO::getHeldTime() const { return lastHeldTime; }
bool HalGPIO::isUsbConnected() const { return true; }
HalGPIO::WakeupReason HalGPIO::getWakeupReason() const { return WakeupReason::AfterFlash; }

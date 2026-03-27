// Simulator stubs for network-dependent activities
// These activities require WiFi/HTTP which is not available in the desktop simulator.
// Each stub shows a "not available" message and allows the user to go back.

#include <I18n.h>
#include <Logging.h>

#include "activities/Activity.h"
#include "components/UITheme.h"
#include "MappedInputManager.h"

// ── WifiSelectionActivity stub ──
#include "activities/network/WifiSelectionActivity.h"

void WifiSelectionActivity::onEnter() {
    Activity::onEnter();
    LOG_INF("SIM", "WifiSelectionActivity — not available in simulator");
    requestUpdate();
}
void WifiSelectionActivity::onExit() { Activity::onExit(); }
void WifiSelectionActivity::loop() {
    if (mappedInput.wasPressed(MappedInputManager::Button::Back) ||
        mappedInput.wasPressed(MappedInputManager::Button::Confirm)) {
        finish();
    }
}
void WifiSelectionActivity::render(RenderLock&&) {
    renderer.clearScreen();
    GUI.drawPopup(renderer, "WiFi not available in simulator");
    renderer.displayBuffer();
}

// ── OtaUpdateActivity stub ──
#include "activities/settings/OtaUpdateActivity.h"

void OtaUpdateActivity::onEnter() {
    Activity::onEnter();
    LOG_INF("SIM", "OtaUpdateActivity — not available in simulator");
    requestUpdate();
}
void OtaUpdateActivity::onExit() { Activity::onExit(); }
void OtaUpdateActivity::loop() {
    if (mappedInput.wasPressed(MappedInputManager::Button::Back) ||
        mappedInput.wasPressed(MappedInputManager::Button::Confirm)) {
        finish();
    }
}
void OtaUpdateActivity::render(RenderLock&&) {
    renderer.clearScreen();
    GUI.drawPopup(renderer, "OTA Update not available in simulator");
    renderer.displayBuffer();
}

// ── KOReaderAuthActivity stub ──
#include "activities/settings/KOReaderAuthActivity.h"

void KOReaderAuthActivity::onEnter() {
    Activity::onEnter();
    LOG_INF("SIM", "KOReaderAuthActivity — not available in simulator");
    requestUpdate();
}
void KOReaderAuthActivity::onExit() { Activity::onExit(); }
void KOReaderAuthActivity::loop() {
    if (mappedInput.wasPressed(MappedInputManager::Button::Back) ||
        mappedInput.wasPressed(MappedInputManager::Button::Confirm)) {
        finish();
    }
}
void KOReaderAuthActivity::render(RenderLock&&) {
    renderer.clearScreen();
    GUI.drawPopup(renderer, "KOReader Auth not available in simulator");
    renderer.displayBuffer();
}

// ── KOReaderSyncActivity stub (requires WiFi, SNTP) ──
#include "activities/reader/KOReaderSyncActivity.h"

void KOReaderSyncActivity::onEnter() {
    Activity::onEnter();
    LOG_INF("SIM", "KOReaderSyncActivity — not available in simulator");
    requestUpdate();
}
void KOReaderSyncActivity::onExit() { Activity::onExit(); }
void KOReaderSyncActivity::loop() {
    if (mappedInput.wasPressed(MappedInputManager::Button::Back) ||
        mappedInput.wasPressed(MappedInputManager::Button::Confirm)) {
        finish();
    }
}
void KOReaderSyncActivity::render(RenderLock&&) {
    renderer.clearScreen();
    GUI.drawPopup(renderer, "KOReader Sync not available in simulator");
    renderer.displayBuffer();
}
void KOReaderSyncActivity::onWifiSelectionComplete(bool) {}
void KOReaderSyncActivity::performSync() {}
void KOReaderSyncActivity::performUpload() {}

// ── QrDisplayActivity stub (requires QR code generation library) ──
#include "activities/reader/QrDisplayActivity.h"

void QrDisplayActivity::onEnter() {
    Activity::onEnter();
    LOG_INF("SIM", "QrDisplayActivity — not available in simulator");
    requestUpdate();
}
void QrDisplayActivity::onExit() { Activity::onExit(); }
void QrDisplayActivity::loop() {
    if (mappedInput.wasPressed(MappedInputManager::Button::Back) ||
        mappedInput.wasPressed(MappedInputManager::Button::Confirm)) {
        finish();
    }
}
void QrDisplayActivity::render(RenderLock&&) {
    renderer.clearScreen();
    GUI.drawPopup(renderer, "QR Display not available in simulator");
    renderer.displayBuffer();
}

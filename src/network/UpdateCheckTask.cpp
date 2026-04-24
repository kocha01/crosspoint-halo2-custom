#include "UpdateCheckTask.h"

#include <Logging.h>
#include <WiFi.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include <string>

#include "CrossPointState.h"
#include "WifiCredentialStore.h"
#include "activities/RenderLock.h"
#include "network/OtaUpdater.h"

namespace {

// Wait up to ~12 seconds for WiFi association. This mirrors the ~15s
// timeout used by WifiSelectionActivity but shaves a few seconds off
// so the user isn't stuck with WiFi hardware active for too long on Home.
constexpr uint32_t WIFI_CONNECT_TIMEOUT_MS = 12000;

// Poll WiFi.status() every 250ms while waiting to connect.
constexpr uint32_t WIFI_POLL_INTERVAL_MS = 250;

// Stack size for the background task. OTA check instantiates an OtaUpdater
// (~8KB HTTP buffer) + JsonDocument + nested call frames on the stack.
// WiFi driver stacks are allocated separately by the driver, not on ours.
// OtaUpdateActivity runs the same check on the main task (stack 8192), so
// match that size here.
constexpr uint32_t TASK_STACK_BYTES = 8192;

void turnWifiOff() {
  WiFi.disconnect(false);
  delay(100);
  WiFi.mode(WIFI_OFF);
  delay(100);
}

void taskBody(void* /*arg*/) {
  LOG_DBG("UPDCHK", "Starting background update check");

  // --- 1. Read saved WiFi credentials (SD access → RenderLock) ---
  std::string lastSsid;
  std::string password;
  bool hasPassword = false;
  {
    RenderLock lock;
    WIFI_STORE.loadFromFile();
    lastSsid = WIFI_STORE.getLastConnectedSsid();
    if (!lastSsid.empty()) {
      const auto* cred = WIFI_STORE.findCredential(lastSsid);
      if (cred) {
        password = cred->password;
        hasPassword = !password.empty();
      } else {
        // Credential removed but lastSsid not cleared — give up.
        lastSsid.clear();
      }
    }
  }

  if (lastSsid.empty()) {
    LOG_DBG("UPDCHK", "No saved WiFi credentials — skipping check");
    APP_STATE.updateCheckFinished = true;
    vTaskDelete(nullptr);
    return;
  }

  // --- 2. Attempt to connect ---
  LOG_DBG("UPDCHK", "Connecting to %s", lastSsid.c_str());
  WiFi.mode(WIFI_STA);
  if (hasPassword) {
    WiFi.begin(lastSsid.c_str(), password.c_str());
  } else {
    WiFi.begin(lastSsid.c_str());
  }

  const uint32_t start = millis();
  bool connected = false;
  while (millis() - start < WIFI_CONNECT_TIMEOUT_MS) {
    const wl_status_t status = WiFi.status();
    if (status == WL_CONNECTED) {
      connected = true;
      break;
    }
    if (status == WL_CONNECT_FAILED || status == WL_NO_SSID_AVAIL) {
      break;
    }
    vTaskDelay(pdMS_TO_TICKS(WIFI_POLL_INTERVAL_MS));
  }

  if (!connected) {
    LOG_DBG("UPDCHK", "WiFi connect timed out or failed — silent exit");
    turnWifiOff();
    APP_STATE.updateCheckFinished = true;
    vTaskDelete(nullptr);
    return;
  }

  LOG_DBG("UPDCHK", "Connected, checking for update");

  // --- 3. Run OTA check ---
  OtaUpdater updater;
  const auto res = updater.checkForUpdate();
  if (res != OtaUpdater::OK) {
    LOG_DBG("UPDCHK", "Update check returned %d — silent exit", res);
    turnWifiOff();
    APP_STATE.updateCheckFinished = true;
    vTaskDelete(nullptr);
    return;
  }

  if (!updater.isUpdateNewer()) {
    LOG_DBG("UPDCHK", "No newer version available — silent exit");
    turnWifiOff();
    APP_STATE.updateCheckFinished = true;
    vTaskDelete(nullptr);
    return;
  }

  // --- 4. Publish result to APP_STATE ---
  // Write `latestVersion` BEFORE flipping the `updateAvailable` flag so
  // HomeActivity can safely read the string once it sees the flag set.
  APP_STATE.latestVersion = updater.getLatestVersion();
  APP_STATE.updateAvailable = true;
  LOG_INF("UPDCHK", "New update available: %s", APP_STATE.latestVersion.c_str());

  // --- 5. Shut down WiFi, self-delete ---
  turnWifiOff();
  APP_STATE.updateCheckFinished = true;
  vTaskDelete(nullptr);
}

}  // namespace

void UpdateCheckTask::start() {
  if (APP_STATE.updateCheckStarted) {
    return;  // Already running or finished this boot
  }
  APP_STATE.updateCheckStarted = true;

  const BaseType_t res = xTaskCreate(&taskBody, "UpdChk", TASK_STACK_BYTES,
                                     /*pvParameters=*/nullptr, /*priority=*/1,
                                     /*taskHandle=*/nullptr);
  if (res != pdPASS) {
    LOG_ERR("UPDCHK", "xTaskCreate failed");
    APP_STATE.updateCheckFinished = true;  // Don't block anything else
  }
}

#pragma once

/**
 * UpdateCheckTask
 *
 * Fires a one-shot FreeRTOS background task that:
 *  1. Reads the last-known WiFi SSID + password from WIFI_STORE
 *  2. Attempts to connect (with a short timeout)
 *  3. If connected, calls OtaUpdater::checkForUpdate() + isUpdateNewer()
 *  4. On "newer" result, writes latest version + sets flag on APP_STATE
 *  5. Shuts WiFi down and self-deletes
 *
 * All failure paths are silent — the user only sees anything if there
 * actually is a newer update available.
 *
 * Called once per boot from HomeActivity::onEnter(). Guarded by
 * APP_STATE.updateCheckStarted so multiple Home entries don't spawn
 * overlapping tasks.
 */
namespace UpdateCheckTask {

// Idempotent. Returns immediately if a check has already been started
// this boot or if no saved WiFi credentials exist.
void start();

}  // namespace UpdateCheckTask

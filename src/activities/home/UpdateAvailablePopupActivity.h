#pragma once

#include <string>

#include "../Activity.h"

/**
 * UpdateAvailablePopupActivity
 *
 * Shown on Home when the background update check found a newer firmware
 * release. Purely informational — returns via ActivityResult so HomeActivity
 * can decide whether to launch the full OTA flow or dismiss the notice.
 *
 * Layout:
 *   New update available!
 *
 *   Current Version: <current>
 *   New Version: <latest>
 *
 *   [ Later ]              [ Update Now ]
 *
 * Right button (Confirm) → isCancelled=false  → caller launches OtaUpdateActivity
 * Left button  (Back)    → isCancelled=true   → caller marks dismissed
 */
class UpdateAvailablePopupActivity final : public Activity {
  std::string latestVersion;

 public:
  UpdateAvailablePopupActivity(GfxRenderer& renderer, MappedInputManager& mappedInput, std::string latestVersion)
      : Activity("UpdatePopup", renderer, mappedInput), latestVersion(std::move(latestVersion)) {}

  void onEnter() override;
  void loop() override;
  void render(RenderLock&&) override;
};

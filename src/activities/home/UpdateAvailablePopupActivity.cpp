#include "UpdateAvailablePopupActivity.h"

#include <GfxRenderer.h>
#include <I18n.h>

#include <string>

#include "../../components/UITheme.h"
#include "../../fontIds.h"
#include "HalDisplay.h"

namespace {
constexpr int kFontId = UI_10_FONT_ID;
constexpr int kHeadingBodyGap = 30;
constexpr int kBodyLineGap = 10;
}  // namespace

void UpdateAvailablePopupActivity::onEnter() {
  Activity::onEnter();
  requestUpdate(true);
}

void UpdateAvailablePopupActivity::render(RenderLock&&) {
  renderer.clearScreen();

  const int pageHeight = renderer.getScreenHeight();
  const int lineHeight = renderer.getLineHeight(kFontId);

  // Layout: heading + gap + 2 body lines (with line gap between them)
  const int totalHeight = lineHeight             // heading
                          + kHeadingBodyGap      // gap under heading
                          + lineHeight           // "Current Version: X"
                          + kBodyLineGap         // small gap between version lines
                          + lineHeight;          // "New Version: Y"
  int y = (pageHeight - totalHeight) / 2;

  // Heading
  renderer.drawCenteredText(kFontId, y, tr(STR_NEW_UPDATE), true, EpdFontFamily::BOLD);
  y += lineHeight + kHeadingBodyGap;

  // Current version
  const std::string currentLine = std::string(tr(STR_CURRENT_VERSION)) + CROSSPOINT_VERSION;
  renderer.drawCenteredText(kFontId, y, currentLine.c_str(), true, EpdFontFamily::REGULAR);
  y += lineHeight + kBodyLineGap;

  // New version
  const std::string newLine = std::string(tr(STR_NEW_VERSION)) + latestVersion;
  renderer.drawCenteredText(kFontId, y, newLine.c_str(), true, EpdFontFamily::REGULAR);

  // Button hints: Left = "Later" (hardcoded English per spec), Right = STR_UPDATE ("Update")
  const auto labels = mappedInput.mapLabels("", "", "Later", tr(STR_UPDATE));
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);

  renderer.displayBuffer(HalDisplay::RefreshMode::FAST_REFRESH);
}

void UpdateAvailablePopupActivity::loop() {
  // Right button (Confirm) → accept update
  if (mappedInput.wasReleased(MappedInputManager::Button::Right) ||
      mappedInput.wasReleased(MappedInputManager::Button::Confirm)) {
    ActivityResult res;
    res.isCancelled = false;
    setResult(std::move(res));
    finish();
    return;
  }

  // Left button or Back → dismiss ("Later")
  if (mappedInput.wasReleased(MappedInputManager::Button::Left) ||
      mappedInput.wasReleased(MappedInputManager::Button::Back)) {
    ActivityResult res;
    res.isCancelled = true;
    setResult(std::move(res));
    finish();
    return;
  }
}

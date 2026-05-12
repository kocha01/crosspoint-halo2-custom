#include "SdCardFontPickerActivity.h"

#include <GfxRenderer.h>
#include <I18n.h>
#include <Logging.h>
#include <SdCardFontRegistry.h>

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <string>

#include "CrossPointSettings.h"
#include "MappedInputManager.h"
#include "SdCardFontGlobals.h"
#include "SdCardFontSystem.h"
#include "components/UITheme.h"
#include "fontIds.h"

void SdCardFontPickerActivity::onEnter() {
  Activity::onEnter();
  loadFamilies();
  requestUpdate();
}

void SdCardFontPickerActivity::onExit() {
  Activity::onExit();
  families.clear();
}

void SdCardFontPickerActivity::loadFamilies() {
  families.clear();
  for (const auto& f : sdFontSystem.registry().getFamilies()) {
    families.emplace_back(f.name);
  }

  // Default selection: "(Built-in)" row when sdFontFamilyName is empty,
  // otherwise the row of the currently selected family.
  if (SETTINGS.sdFontFamilyName[0] == '\0') {
    selectedIndex = 0;  // built-in row
    return;
  }
  const auto it = std::find(families.begin(), families.end(), SETTINGS.sdFontFamilyName);
  if (it != families.end()) {
    selectedIndex = static_cast<int>(it - families.begin()) + 1;  // +1 for built-in row
  } else {
    selectedIndex = 0;
  }
}

void SdCardFontPickerActivity::onBack() {
  ActivityResult result;
  result.isCancelled = true;
  setResult(std::move(result));
  finish();
}

void SdCardFontPickerActivity::handleSelection() {
  // Row 0 is always the synthetic "(Built-in)" entry: clears the SD font choice.
  if (selectedIndex == 0) {
    SETTINGS.sdFontFamilyName[0] = '\0';
    ensureSdFontLoaded();  // unloads any currently-loaded SD font
    finish();
    return;
  }

  if (families.empty()) {
    return;  // nothing to pick
  }

  const auto fileIndex = static_cast<size_t>(selectedIndex - 1);
  if (fileIndex >= families.size()) {
    return;
  }
  std::snprintf(SETTINGS.sdFontFamilyName, sizeof(SETTINGS.sdFontFamilyName), "%s",
                families[fileIndex].c_str());
  ensureSdFontLoaded();  // load + register with renderer
  finish();
}

void SdCardFontPickerActivity::loop() {
  if (mappedInput.wasPressed(MappedInputManager::Button::Back)) {
    onBack();
    return;
  }

  if (mappedInput.wasPressed(MappedInputManager::Button::Confirm)) {
    handleSelection();
    return;
  }

  // +1 for the synthetic "(Built-in)" entry shown above the list.
  const int itemCount = static_cast<int>(families.size()) + 1;
  buttonNavigator.onNextRelease([this, itemCount] {
    selectedIndex = ButtonNavigator::nextIndex(selectedIndex, itemCount);
    requestUpdate();
  });

  buttonNavigator.onPreviousRelease([this, itemCount] {
    selectedIndex = ButtonNavigator::previousIndex(selectedIndex, itemCount);
    requestUpdate();
  });
}

void SdCardFontPickerActivity::render(RenderLock&&) {
  renderer.clearScreen();

  const auto pageWidth = renderer.getScreenWidth();
  const auto pageHeight = renderer.getScreenHeight();
  const auto& metrics = UITheme::getInstance().getMetrics();

  GUI.drawHeader(renderer, Rect{0, metrics.topPadding, pageWidth, metrics.headerHeight}, tr(STR_FONT_FAMILY));

  const int contentTop = metrics.topPadding + metrics.headerHeight + metrics.verticalSpacing;
  const int contentHeight = pageHeight - contentTop - metrics.buttonHintsHeight - metrics.verticalSpacing;

  // Layout: row 0 = "(Built-in)" sentinel, rows 1..N = discovered families.
  const int totalRows = static_cast<int>(families.size()) + 1;
  GUI.drawList(
      renderer, Rect{0, contentTop, pageWidth, contentHeight}, totalRows, selectedIndex,
      [this](int index) {
        if (index == 0) return std::string(tr(STR_DEFAULT_VALUE));
        return families[static_cast<size_t>(index - 1)];
      },
      nullptr, nullptr,
      [this](int index) {
        if (index == 0) {
          return SETTINGS.sdFontFamilyName[0] == '\0' ? std::string(tr(STR_SELECTED)) : std::string{};
        }
        return families[static_cast<size_t>(index - 1)] == SETTINGS.sdFontFamilyName
                   ? std::string(tr(STR_SELECTED))
                   : std::string{};
      },
      true);

  // Empty-state hint shown below the list when no .cpfont files were discovered.
  // Point users at the visible /fonts/ folder rather than the legacy hidden one
  // — drag-and-drop into /fonts/ on the SD card just works.
  if (families.empty()) {
    renderer.drawText(SMALL_FONT_ID, metrics.contentSidePadding, contentTop + 48, "/fonts/");
    renderer.drawText(UI_10_FONT_ID, metrics.contentSidePadding, contentTop + 28, tr(STR_NO_FILES_FOUND));
  }

  const auto labels = mappedInput.mapLabels(tr(STR_BACK), tr(STR_SELECT), tr(STR_DIR_UP), tr(STR_DIR_DOWN));
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);

  renderer.displayBuffer();
}

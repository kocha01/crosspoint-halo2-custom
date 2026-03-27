#include "ClearCacheActivity.h"

#include <GfxRenderer.h>
#include <HalStorage.h>
#include <I18n.h>
#include <Logging.h>

#include "MappedInputManager.h"
#include "RecentBooksStore.h"
#include "components/UITheme.h"
#include "fontIds.h"

void ClearCacheActivity::onEnter() {
  Activity::onEnter();

  state = WARNING;
  requestUpdate();
}

void ClearCacheActivity::onExit() { Activity::onExit(); }

void ClearCacheActivity::render(RenderLock&&) {
  const auto& metrics = UITheme::getInstance().getMetrics();
  const auto pageWidth = renderer.getScreenWidth();
  const auto pageHeight = renderer.getScreenHeight();

  renderer.clearScreen();

  const char* title = tr(STR_CLEAR_READING_CACHE);
  if (mode == Mode::RefreshRecents) {
    title = tr(STR_REFRESH_RECENT_BOOKS);
  } else if (mode == Mode::ClearRecents) {
    title = tr(STR_CLEAR_RECENT_BOOKS);
  }
  GUI.drawHeader(renderer, Rect{0, metrics.topPadding, pageWidth, metrics.headerHeight}, title);

  if (state == WARNING) {
    if (mode == Mode::RefreshRecents) {
      renderer.drawCenteredText(UI_10_FONT_ID, pageHeight / 2 - 40, tr(STR_REFRESH_RECENT_BOOKS_WARNING_1), true);
      renderer.drawCenteredText(UI_10_FONT_ID, pageHeight / 2 - 10, tr(STR_REFRESH_RECENT_BOOKS_WARNING_2), true,
                                EpdFontFamily::BOLD);
      renderer.drawCenteredText(UI_10_FONT_ID, pageHeight / 2 + 20, tr(STR_REFRESH_RECENT_BOOKS_WARNING_3), true);
    } else if (mode == Mode::ClearRecents) {
      renderer.drawCenteredText(UI_10_FONT_ID, pageHeight / 2 - 40, tr(STR_CLEAR_RECENT_BOOKS_WARNING_1), true);
      renderer.drawCenteredText(UI_10_FONT_ID, pageHeight / 2 - 10, tr(STR_CLEAR_RECENT_BOOKS_WARNING_2), true,
                                EpdFontFamily::BOLD);
      renderer.drawCenteredText(UI_10_FONT_ID, pageHeight / 2 + 20, tr(STR_CLEAR_RECENT_BOOKS_WARNING_3), true);
    } else {
      renderer.drawCenteredText(UI_10_FONT_ID, pageHeight / 2 - 60, tr(STR_CLEAR_CACHE_WARNING_1), true);
      renderer.drawCenteredText(UI_10_FONT_ID, pageHeight / 2 - 30, tr(STR_CLEAR_CACHE_WARNING_2), true,
                                EpdFontFamily::BOLD);
      renderer.drawCenteredText(UI_10_FONT_ID, pageHeight / 2 + 10, tr(STR_CLEAR_CACHE_WARNING_3), true);
      renderer.drawCenteredText(UI_10_FONT_ID, pageHeight / 2 + 30, tr(STR_CLEAR_CACHE_WARNING_4), true);
    }

    const char* confirmLabel =
        (mode == Mode::RefreshRecents) ? tr(STR_REFRESH) : tr(STR_CLEAR_BUTTON);
    const auto labels = mappedInput.mapLabels(tr(STR_CANCEL), confirmLabel, "", "");
    GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
    renderer.displayBuffer();
    return;
  }

  if (state == CLEARING) {
    const char* clearingText = tr(STR_CLEARING_CACHE);
    if (mode == Mode::RefreshRecents) {
      clearingText = tr(STR_REFRESHING_RECENT_BOOKS);
    } else if (mode == Mode::ClearRecents) {
      clearingText = tr(STR_CLEARING_RECENT_BOOKS);
    }
    renderer.drawCenteredText(UI_10_FONT_ID, pageHeight / 2, clearingText);
    renderer.displayBuffer();
    return;
  }

  if (state == SUCCESS) {
    const char* successText = tr(STR_CACHE_CLEARED);
    if (mode == Mode::RefreshRecents) {
      successText = tr(STR_RECENT_BOOKS_REFRESHED);
    } else if (mode == Mode::ClearRecents) {
      successText = tr(STR_RECENT_BOOKS_CLEARED);
    }
    renderer.drawCenteredText(UI_10_FONT_ID, pageHeight / 2 - 20, successText, true, EpdFontFamily::BOLD);
    const int totalRemoved = clearedCount + removedRecentCount;
    std::string resultText = std::to_string(totalRemoved) + " " + std::string(tr(STR_ITEMS_REMOVED));
    if (failedCount > 0 && mode == Mode::ReadingCache) {
      resultText += ", " + std::to_string(failedCount) + " " + std::string(tr(STR_FAILED_LOWER));
    }
    renderer.drawCenteredText(UI_10_FONT_ID, pageHeight / 2 + 10, resultText.c_str());

    const auto labels = mappedInput.mapLabels(tr(STR_BACK), "", "", "");
    GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
    renderer.displayBuffer();
    return;
  }

  if (state == FAILED) {
    renderer.drawCenteredText(UI_10_FONT_ID, pageHeight / 2 - 20, tr(STR_CLEAR_CACHE_FAILED), true,
                              EpdFontFamily::BOLD);
    renderer.drawCenteredText(UI_10_FONT_ID, pageHeight / 2 + 10, tr(STR_CHECK_SERIAL_OUTPUT));

    const auto labels = mappedInput.mapLabels(tr(STR_BACK), "", "", "");
    GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
    renderer.displayBuffer();
    return;
  }
}

void ClearCacheActivity::clearCache() {
  removedRecentCount = 0;

  if (mode == Mode::RefreshRecents) {
    removedRecentCount = RECENT_BOOKS.pruneMissingBooks();
    LOG_DBG("CLEAR_CACHE", "Refreshed recents: %d removed", removedRecentCount);
    clearedCount = 0;
    failedCount = 0;
    state = SUCCESS;
    requestUpdate();
    return;
  }

  if (mode == Mode::ClearRecents) {
    removedRecentCount = RECENT_BOOKS.clear();
    LOG_DBG("CLEAR_CACHE", "Cleared recents: %d removed", removedRecentCount);
    clearedCount = 0;
    failedCount = 0;
    state = SUCCESS;
    requestUpdate();
    return;
  }

  LOG_DBG("CLEAR_CACHE", "Clearing cache...");

  // Open .crosspoint directory
  auto root = Storage.open("/.crosspoint");
  if (!root || !root.isDirectory()) {
    LOG_DBG("CLEAR_CACHE", "Failed to open cache directory");
    if (root) root.close();
    state = FAILED;
    requestUpdate();
    return;
  }

  clearedCount = 0;
  failedCount = 0;
  char name[128];

  // Iterate through all entries in the directory
  for (auto file = root.openNextFile(); file; file = root.openNextFile()) {
    file.getName(name, sizeof(name));
    String itemName(name);

    // Only delete directories starting with epub_ or xtc_
    if (file.isDirectory() && (itemName.startsWith("epub_") || itemName.startsWith("xtc_"))) {
      String fullPath = "/.crosspoint/" + itemName;
      LOG_DBG("CLEAR_CACHE", "Removing cache: %s", fullPath.c_str());

      file.close();  // Close before attempting to delete

      if (Storage.removeDir(fullPath.c_str())) {
        clearedCount++;
      } else {
        LOG_ERR("CLEAR_CACHE", "Failed to remove: %s", fullPath.c_str());
        failedCount++;
      }
    } else {
      file.close();
    }
  }
  root.close();

  removedRecentCount = RECENT_BOOKS.pruneMissingBooks();

  LOG_DBG("CLEAR_CACHE", "Cache cleared: %d removed, %d failed", clearedCount, failedCount);

  state = SUCCESS;
  requestUpdate();
}

void ClearCacheActivity::loop() {
  if (state == WARNING) {
    if (mappedInput.wasPressed(MappedInputManager::Button::Confirm)) {
      LOG_DBG("CLEAR_CACHE", "User confirmed, starting cache clear");
      {
        RenderLock lock(*this);
        state = CLEARING;
      }
      requestUpdateAndWait();

      clearCache();
    }

    if (mappedInput.wasPressed(MappedInputManager::Button::Back)) {
      LOG_DBG("CLEAR_CACHE", "User cancelled");
      goBack();
    }
    return;
  }

  if (state == SUCCESS || state == FAILED) {
    if (mappedInput.wasPressed(MappedInputManager::Button::Back)) {
      goBack();
    }
    return;
  }
}

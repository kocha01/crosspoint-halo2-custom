#include "SleepWallpaperPickerActivity.h"

#include <FsHelpers.h>
#include <GfxRenderer.h>
#include <HalStorage.h>
#include <I18n.h>
#include <Logging.h>

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <string>

#include "CrossPointSettings.h"
#include "MappedInputManager.h"
#include "components/UITheme.h"
#include "fontIds.h"

namespace {
// Sleep-wallpaper source directory resolution: prefer "/.sleep" (hidden, matches
// X4's existing convention for device-internal media) and fall back to "/sleep"
// so users who already curated a /sleep folder keep their picks. Mirrors the
// directory probe logic in SleepActivity::renderCustomSleepScreen.
const char* resolveSleepDir() {
  auto dir = Storage.open("/.sleep");
  if (dir && dir.isDirectory()) {
    dir.close();
    return "/.sleep";
  }
  if (dir) dir.close();
  dir = Storage.open("/sleep");
  if (dir && dir.isDirectory()) {
    dir.close();
    return "/sleep";
  }
  if (dir) dir.close();
  return nullptr;
}

// Natural-order comparator so "wallpaper_2.bmp" sorts before "wallpaper_10.bmp"
// rather than after it. Compares digit runs numerically (length-first, then
// lexicographically on equal lengths to avoid overflow on long numbers) and
// non-digit characters case-insensitively.
void sortWallpaperList(std::vector<std::string>& entries) {
  std::sort(entries.begin(), entries.end(), [](const std::string& lhs, const std::string& rhs) {
    const char* left = lhs.c_str();
    const char* right = rhs.c_str();

    while (*left != '\0' && *right != '\0') {
      if (std::isdigit(static_cast<unsigned char>(*left)) && std::isdigit(static_cast<unsigned char>(*right))) {
        while (*left == '0') left++;
        while (*right == '0') right++;

        int leftDigits = 0;
        int rightDigits = 0;
        while (std::isdigit(static_cast<unsigned char>(left[leftDigits]))) leftDigits++;
        while (std::isdigit(static_cast<unsigned char>(right[rightDigits]))) rightDigits++;

        if (leftDigits != rightDigits) {
          return leftDigits < rightDigits;
        }

        for (int i = 0; i < leftDigits; i++) {
          if (left[i] != right[i]) {
            return left[i] < right[i];
          }
        }

        left += leftDigits;
        right += rightDigits;
      } else {
        const auto leftChar = static_cast<char>(std::tolower(static_cast<unsigned char>(*left)));
        const auto rightChar = static_cast<char>(std::tolower(static_cast<unsigned char>(*right)));
        if (leftChar != rightChar) {
          return leftChar < rightChar;
        }
        left++;
        right++;
      }
    }

    return *left == '\0' && *right != '\0';
  });
}
}  // namespace

void SleepWallpaperPickerActivity::onEnter() {
  Activity::onEnter();
  loadWallpapers();
  requestUpdate();
}

void SleepWallpaperPickerActivity::onExit() {
  Activity::onExit();
  files.clear();
}

void SleepWallpaperPickerActivity::loadWallpapers() {
  files.clear();

  const char* sleepDir = resolveSleepDir();
  if (sleepDir == nullptr) {
    selectedIndex = 0;
    return;
  }

  auto dir = Storage.open(sleepDir);
  if (!dir || !dir.isDirectory()) {
    if (dir) dir.close();
    selectedIndex = 0;
    return;
  }

  dir.rewindDirectory();

  // Scratch buffer for SdFat getName — long-filename support means names can be
  // up to 255 chars plus null terminator. Kept on the stack (< 512 B) per the
  // stack-safety guideline.
  char name[500];
  for (auto file = dir.openNextFile(); file; file = dir.openNextFile()) {
    file.getName(name, sizeof(name));
    if (file.isDirectory() || name[0] == '.' || !FsHelpers::hasBmpExtension(name)) {
      file.close();
      continue;
    }

    files.emplace_back(name);
    file.close();
  }

  dir.close();
  sortWallpaperList(files);

  const auto it = std::find(files.begin(), files.end(), SETTINGS.customSleepImagePath);
  selectedIndex = (it != files.end()) ? static_cast<int>(it - files.begin()) : 0;
}

void SleepWallpaperPickerActivity::onBack() {
  ActivityResult result;
  result.isCancelled = true;
  setResult(std::move(result));
  finish();
}

void SleepWallpaperPickerActivity::handleSelection() {
  if (files.empty()) {
    return;
  }

  const std::string& selectedFile = files[static_cast<size_t>(selectedIndex)];
  // Persist the filename only; SleepActivity reconstructs the full path by
  // prefixing the resolved sleep directory at render time.
  std::snprintf(SETTINGS.customSleepImagePath, sizeof(SETTINGS.customSleepImagePath), "%s", selectedFile.c_str());

  finish();
}

void SleepWallpaperPickerActivity::loop() {
  if (mappedInput.wasPressed(MappedInputManager::Button::Back)) {
    onBack();
    return;
  }

  if (mappedInput.wasPressed(MappedInputManager::Button::Confirm)) {
    handleSelection();
    return;
  }

  if (files.empty()) {
    return;
  }

  const int itemCount = static_cast<int>(files.size());
  buttonNavigator.onNextRelease([this, itemCount] {
    selectedIndex = ButtonNavigator::nextIndex(selectedIndex, itemCount);
    requestUpdate();
  });

  buttonNavigator.onPreviousRelease([this, itemCount] {
    selectedIndex = ButtonNavigator::previousIndex(selectedIndex, itemCount);
    requestUpdate();
  });
}

void SleepWallpaperPickerActivity::render(RenderLock&&) {
  renderer.clearScreen();

  const auto pageWidth = renderer.getScreenWidth();
  const auto pageHeight = renderer.getScreenHeight();
  const auto& metrics = UITheme::getInstance().getMetrics();

  GUI.drawHeader(renderer, Rect{0, metrics.topPadding, pageWidth, metrics.headerHeight}, tr(STR_SELECT_WALLPAPER));

  const int contentTop = metrics.topPadding + metrics.headerHeight + metrics.verticalSpacing;
  const int contentHeight = pageHeight - contentTop - metrics.buttonHintsHeight - metrics.verticalSpacing;

  if (files.empty()) {
    // No /sleep or /.sleep directory, or it contains no BMPs. Show where to drop
    // files so the user knows how to populate the picker.
    renderer.drawText(UI_10_FONT_ID, metrics.contentSidePadding, contentTop + 20, tr(STR_NO_FILES_FOUND));
    const char* sleepDir = resolveSleepDir();
    renderer.drawText(SMALL_FONT_ID, metrics.contentSidePadding, contentTop + 48,
                      sleepDir != nullptr ? sleepDir : "/sleep");
  } else {
    GUI.drawList(
        renderer, Rect{0, contentTop, pageWidth, contentHeight}, static_cast<int>(files.size()), selectedIndex,
        [this](int index) { return files[static_cast<size_t>(index)]; }, nullptr, nullptr,
        [this](int index) {
          return files[static_cast<size_t>(index)] == SETTINGS.customSleepImagePath ? std::string(tr(STR_SELECTED))
                                                                                    : std::string("");
        },
        true);
  }

  const auto labels = mappedInput.mapLabels(tr(STR_BACK), tr(STR_SELECT), tr(STR_DIR_UP), tr(STR_DIR_DOWN));
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);

  renderer.displayBuffer();
}

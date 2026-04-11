#include "ThaiDictionaryActivity.h"

#include <HalStorage.h>
#include <I18n.h>
#include <Logging.h>

#include "Epub/hyphenation/ThaiWordBreaker.h"
#include "MappedInputManager.h"
#include "activities/util/KeyboardEntryActivity.h"
#include "components/UITheme.h"
#include "fontIds.h"

static constexpr char DICT_PATH[] = "/crosspoint/thai_dict.txt";
static constexpr size_t MAX_BUF = 8192;
static constexpr size_t MAX_WORDS = 200;

void ThaiDictionaryActivity::onEnter() {
  Activity::onEnter();
  loadWords();
  requestUpdate();
}

void ThaiDictionaryActivity::onExit() {
  if (dirty) {
    saveWords();
    // Reload into the word breaker so changes take effect immediately
    ThaiWordBreaker::setUserDictionary(words);
  }
  Activity::onExit();
}

int ThaiDictionaryActivity::totalItems() const {
  // "Add Word" + "Keyboard Layout" + existing words
  return FIRST_WORD_INDEX + static_cast<int>(words.size());
}

void ThaiDictionaryActivity::loadWords() {
  words.clear();

  if (!Storage.exists(DICT_PATH)) return;

  auto* buf = static_cast<char*>(malloc(MAX_BUF));
  if (!buf) {
    LOG_ERR("DICT", "malloc failed for dict buffer");
    return;
  }

  const size_t bytesRead = Storage.readFileToBuffer(DICT_PATH, buf, MAX_BUF);
  if (bytesRead == 0) {
    free(buf);
    return;
  }

  words.reserve(64);
  size_t lineStart = 0;
  for (size_t i = 0; i <= bytesRead; ++i) {
    if (i == bytesRead || buf[i] == '\n' || buf[i] == '\r') {
      if (i > lineStart) {
        size_t lineEnd = i;
        while (lineEnd > lineStart && (buf[lineEnd - 1] == ' ' || buf[lineEnd - 1] == '\t')) {
          --lineEnd;
        }
        if (lineEnd > lineStart && buf[lineStart] != '#') {
          words.emplace_back(buf + lineStart, lineEnd - lineStart);
        }
      }
      lineStart = i + 1;
    }
  }

  free(buf);
}

void ThaiDictionaryActivity::saveWords() {
  // Build file content
  std::string content;
  content.reserve(words.size() * 30);
  for (const auto& word : words) {
    content += word;
    content += '\n';
  }

  // Ensure /crosspoint/ directory exists
  if (!Storage.exists("/crosspoint")) {
    Storage.mkdir("/crosspoint");
  }

  HalFile file;
  if (!Storage.openFileForWrite("DICT", DICT_PATH, file)) {
    LOG_ERR("DICT", "Failed to open %s for write", DICT_PATH);
    return;
  }
  file.write(content.c_str(), content.size());
  file.close();

  LOG_INF("DICT", "Saved %d words to %s", static_cast<int>(words.size()), DICT_PATH);
}

void ThaiDictionaryActivity::addWord() {
  if (words.size() >= MAX_WORDS) return;

  auto keyboard = std::make_unique<KeyboardEntryActivity>(renderer, mappedInput, tr(STR_ENTER_THAI_WORD), "", 0, false,
                                                          KeyboardLayout::Thai);

  startActivityForResult(std::move(keyboard), [this](const ActivityResult& result) {
    if (result.isCancelled) return;

    const auto& kr = std::get<KeyboardResult>(result.data);
    if (kr.text.empty()) return;

    // Avoid duplicates
    for (const auto& w : words) {
      if (w == kr.text) return;
    }

    words.push_back(kr.text);
    dirty = true;
    selectedIndex = static_cast<int>(words.size());  // select the newly added word
  });
}

void ThaiDictionaryActivity::deleteSelectedWord() {
  const int wordIndex = selectedIndex - FIRST_WORD_INDEX;
  if (wordIndex < 0 || wordIndex >= static_cast<int>(words.size())) return;

  words.erase(words.begin() + wordIndex);
  dirty = true;

  // Adjust selection
  if (selectedIndex >= totalItems()) {
    selectedIndex = totalItems() - 1;
  }
}

void ThaiDictionaryActivity::loop() {
  buttonNavigator.onPressAndContinuous({MappedInputManager::Button::Down, MappedInputManager::Button::Right}, [this] {
    selectedIndex = ButtonNavigator::nextIndex(selectedIndex, totalItems());
    requestUpdate();
  });

  buttonNavigator.onPressAndContinuous({MappedInputManager::Button::Up, MappedInputManager::Button::Left}, [this] {
    selectedIndex = ButtonNavigator::previousIndex(selectedIndex, totalItems());
    requestUpdate();
  });

  if (mappedInput.wasPressed(MappedInputManager::Button::Confirm)) {
    if (selectedIndex == ADD_WORD_INDEX) {
      addWord();
    } else {
      deleteSelectedWord();
    }
    requestUpdate();
  }

  if (mappedInput.wasPressed(MappedInputManager::Button::Back)) {
    finish();
  }
}

void ThaiDictionaryActivity::render(RenderLock&&) {
  renderer.clearScreen();

  const auto pageWidth = renderer.getScreenWidth();
  const auto& metrics = UITheme::getInstance().getMetrics();

  // Header with word count subtitle
  char subtitle[32] = {};
  if (!words.empty()) {
    snprintf(subtitle, sizeof(subtitle), "%d%s", static_cast<int>(words.size()), tr(STR_DICT_WORD_COUNT));
  }
  GUI.drawHeader(renderer, Rect{0, metrics.topPadding, pageWidth, metrics.headerHeight}, tr(STR_THAI_DICTIONARY),
                 words.empty() ? nullptr : subtitle);

  // Draw list
  const int listY = metrics.topPadding + metrics.headerHeight + metrics.verticalSpacing;
  const int listHeight = renderer.getScreenHeight() - listY - metrics.buttonHintsHeight - metrics.verticalSpacing;

  GUI.drawList(
      renderer, Rect{0, listY, pageWidth, listHeight}, totalItems(), selectedIndex,
      // Row title
      [this](const int index) -> std::string {
        if (index == ADD_WORD_INDEX) return std::string("+ ") + tr(STR_ADD_WORD);
        const int wordIdx = index - FIRST_WORD_INDEX;
        if (wordIdx >= 0 && wordIdx < static_cast<int>(words.size())) {
          return words[wordIdx];
        }
        return "";
      },
      // Row subtitle
      nullptr,
      // Row icon
      nullptr,
      // Row value
      [](const int index) -> std::string {
        if (index == ADD_WORD_INDEX) return "";
        return tr(STR_DELETE);
      },
      true);

  // Empty state
  if (words.empty()) {
    const int centerY = listY + listHeight / 2;
    renderer.drawCenteredText(UI_12_FONT_ID, centerY, tr(STR_DICT_EMPTY));
  }

  // Button hints
  const char* confirmLabel;
  if (selectedIndex == ADD_WORD_INDEX)
    confirmLabel = tr(STR_ADD_WORD);
  else
    confirmLabel = tr(STR_DELETE);
  const auto labels = mappedInput.mapLabels(tr(STR_BACK), confirmLabel, tr(STR_DIR_LEFT), tr(STR_DIR_RIGHT));
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
  GUI.drawSideButtonHints(renderer, ">", "<");

  renderer.displayBuffer();
}

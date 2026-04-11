#pragma once
#include <string>
#include <vector>

#include "activities/Activity.h"
#include "util/ButtonNavigator.h"

/**
 * Settings sub-activity for managing the Thai user dictionary.
 * Users can add/delete words that supplement the built-in DAWG dictionary.
 * Words are persisted to /crosspoint/thai_dict.txt on the SD card.
 */
class ThaiDictionaryActivity final : public Activity {
  ButtonNavigator buttonNavigator;

  std::vector<std::string> words;
  int selectedIndex = 0;  // 0 = "Add Word" action, 1..N = word entries
  bool dirty = false;     // true when words have been modified

  void loadWords();
  void saveWords();
  void addWord();
  void deleteSelectedWord();
  int totalItems() const;
  static constexpr int ADD_WORD_INDEX = 0;
  static constexpr int FIRST_WORD_INDEX = 1;

 public:
  explicit ThaiDictionaryActivity(GfxRenderer& renderer, MappedInputManager& mappedInput)
      : Activity("ThaiDictionary", renderer, mappedInput) {}
  void onEnter() override;
  void onExit() override;
  void loop() override;
  void render(RenderLock&&) override;
};

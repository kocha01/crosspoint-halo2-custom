#pragma once
#include <GfxRenderer.h>

#include <functional>
#include <string>
#include <utility>

#include "../Activity.h"
#include "util/ButtonNavigator.h"

enum class KeyboardLayout { English, Thai };

/**
 * Reusable keyboard entry activity for text input.
 * Supports English (QWERTY) and Thai (frequency-ordered) layouts.
 * Can be started from any activity that needs text entry via startActivityForResult()
 */
class KeyboardEntryActivity : public Activity {
 public:
  explicit KeyboardEntryActivity(GfxRenderer& renderer, MappedInputManager& mappedInput,
                                 std::string title = "Enter Text", std::string initialText = "",
                                 const size_t maxLength = 0, const bool isPassword = false,
                                 const KeyboardLayout initialLayout = KeyboardLayout::English)
      : Activity("KeyboardEntry", renderer, mappedInput),
        title(std::move(title)),
        text(std::move(initialText)),
        maxLength(maxLength),
        isPassword(isPassword),
        layout(initialLayout) {}

  void onEnter() override;
  void onExit() override;
  void loop() override;
  void render(RenderLock&&) override;

 private:
  std::string title;
  std::string text;
  size_t maxLength;
  bool isPassword;
  KeyboardLayout layout;

  ButtonNavigator buttonNavigator;

  // Keyboard state
  int selectedRow = 0;
  int selectedCol = 0;
  int shiftState = 0;  // 0 = normal, 1 = shifted, 2 = shift lock

  // Handlers
  void onComplete(std::string text);
  void onCancel();

  // Layout constants
  static constexpr int NUM_ROWS = 5;
  static constexpr int KEYS_PER_ROW = 13;

  // Special key positions (bottom row)
  static constexpr int SPECIAL_ROW = 4;
  static constexpr int SHIFT_COL = 0;
  static constexpr int SPACE_COL = 2;
  static constexpr int BACKSPACE_COL = 7;
  static constexpr int DONE_COL = 9;

  // Layout data — each key is a null-terminated UTF-8 string.
  // English layout
  static const char* const englishKeys[NUM_ROWS][KEYS_PER_ROW];
  static const char* const englishKeysShift[NUM_ROWS][KEYS_PER_ROW];
  static const int englishRowLengths[NUM_ROWS];

  // Thai Kedmanee layout (standard Thai computer keyboard)
  static const char* const thaiKeysKed[NUM_ROWS][KEYS_PER_ROW];
  static const char* const thaiKeysKedShift[NUM_ROWS][KEYS_PER_ROW];
  static const int thaiRowLengths[NUM_ROWS];

  static const char* const shiftString[3];

  // Long-press threshold for Back button cancel (Thai keyboard)
  static constexpr unsigned long LONG_PRESS_MS = 500;

  // Key access for current layout
  const char* getKeyAt(int row, int col) const;
  int getRowLength(int row) const;
  bool handleKeyPress();

  // UTF-8 aware text manipulation
  void appendKey(const char* key);
  void backspaceUtf8();
};

#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "HyphenationCommon.h"

class ThaiWordBreaker {
 public:
  // Returns legal Thai line-break positions as codepoint indexes inside cps.
  // Breaks are emitted only at dictionary-derived word boundaries unless
  // includeFallback is enabled, in which case unknown runs additionally expose
  // safe Thai cluster boundaries as a last resort.
  static std::vector<size_t> breakIndexes(const std::vector<CodepointInfo>& cps, bool includeFallback);

  // Load a user dictionary from a list of UTF-8 Thai words.
  // Words are stored as codepoint sequences for O(1) per-character matching.
  // Maximum 200 words to stay within ESP32-C3 memory constraints (~12 KB).
  static void setUserDictionary(const std::vector<std::string>& words);

  // Returns true if a user dictionary has been loaded.
  static bool hasUserDictionary();

  // Provides read access to the user dictionary for the DAWG matching function.
  static const std::vector<std::vector<uint16_t>>& getUserDictWords() { return userDictWords_; }

 private:
  // Each entry is a word stored as a vector of Unicode codepoints.
  static std::vector<std::vector<uint16_t>> userDictWords_;
};

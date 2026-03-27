#pragma once

#include <cstdint>
#include <string>
#define REPLACEMENT_GLYPH 0xFFFD

uint32_t utf8NextCodepoint(const unsigned char** string);
// Remove the last UTF-8 codepoint from a std::string and return the new size.
size_t utf8RemoveLastChar(std::string& str);
// Truncate string by removing N UTF-8 codepoints from the end.
void utf8TruncateChars(std::string& str, size_t numChars);

// Truncate a raw char buffer to the last complete UTF-8 codepoint boundary.
// Returns the new length (<= len). If the buffer ends mid-sequence, the
// incomplete trailing bytes are excluded.
int utf8SafeTruncateBuffer(const char* buf, int len);

// Returns true for Unicode combining diacritical marks that should not advance the cursor.
inline bool utf8IsThaiLowerCombiningMark(const uint32_t cp) {
  return cp >= 0x0E38 && cp <= 0x0E3A;  // Thai below vowels + Phinthu
}

inline bool utf8IsThaiUpperCombiningMark(const uint32_t cp) {
  return cp == 0x0E31                     // Thai Mai Han Akat
         || (cp >= 0x0E34 && cp <= 0x0E37)   // Thai upper vowels
         || (cp >= 0x0E47 && cp <= 0x0E4E);  // Thai tone marks and related diacritics
}

inline bool utf8IsThaiUpperLevelTwoMark(const uint32_t cp) {
  return cp == 0x0E31                    // Thai Mai Han Akat
         || (cp >= 0x0E34 && cp <= 0x0E37)  // Thai upper vowels
         || cp == 0x0E47                 // Thai Mai Tai Khu
         || cp == 0x0E4D;                // Thai Nikhahit
}

inline bool utf8IsThaiUpperLevelThreeMark(const uint32_t cp) {
  return (cp >= 0x0E48 && cp <= 0x0E4C)  // Thai tone marks + Thanthakhat
         || cp == 0x0E4E;                // Thai Yamakkan
}

inline bool utf8IsThaiCombiningMark(const uint32_t cp) {
  return utf8IsThaiLowerCombiningMark(cp) || utf8IsThaiUpperCombiningMark(cp);
}

inline bool utf8IsThaiCodepoint(const uint32_t cp) { return cp >= 0x0E00 && cp <= 0x0E7F; }

inline bool utf8IsCombiningMark(const uint32_t cp) {
  return (cp >= 0x0300 && cp <= 0x036F)      // Combining Diacritical Marks
         || (cp >= 0x1DC0 && cp <= 0x1DFF)   // Combining Diacritical Marks Supplement
         || (cp >= 0x20D0 && cp <= 0x20FF)   // Combining Diacritical Marks for Symbols
         || (cp >= 0xFE20 && cp <= 0xFE2F)   // Combining Half Marks
         || utf8IsThaiCombiningMark(cp);
}

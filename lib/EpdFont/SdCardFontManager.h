#pragma once

#include <cstdint>
#include <string>
#include <vector>

class GfxRenderer;
class SdCardFont;
struct SdCardFontFamilyInfo;

class SdCardFontManager {
 public:
  SdCardFontManager() = default;
  ~SdCardFontManager();
  SdCardFontManager(const SdCardFontManager&) = delete;
  SdCardFontManager& operator=(const SdCardFontManager&) = delete;

  // Load the single size closest to targetPtSize for a discovered family.
  // Only one .cpfont file is loaded; other sizes remain on disk. This keeps
  // resident interval + kern/ligature tables to one size's worth of memory
  // (see PR #1327 discussion re: Literata OOM).
  // Returns true on success.
  bool loadFamily(const SdCardFontFamilyInfo& family, GfxRenderer& renderer, uint8_t targetPtSize);

  // Unload everything, unregister from renderer.
  void unloadAll(GfxRenderer& renderer);

  // Free the mini-cache (page-resident glyph bitmaps + kern matrix) on every
  // loaded SD font WITHOUT unloading the font itself.  The headers, intervals,
  // and kern-class tables stay resident so a subsequent prewarm() can rebuild
  // the page cache from SD without re-reading the file header.
  //
  // Used by EpubReaderActivity::onExit to give back ~17-30KB of heap to the
  // upcoming Home activity, whose cover BMP LRU cache (~96KB) plus recent-book
  // parsing pushes RAM close to the wall on the 320KB ESP32-C3.  Without this,
  // exiting Reader → Home with an SD font selected can OOM during cover-cache
  // population and reboot the device.  Re-entering the Reader pays a one-page
  // prewarm cost (already paid on every initial page-turn anyway).
  void clearAllMiniCaches();

  // Look up the font ID for the loaded family. Returns 0 if nothing loaded
  // or familyName doesn't match.
  int getFontId(const std::string& familyName) const;

  // Get name of currently loaded family (empty if none).
  const std::string& currentFamilyName() const { return loadedFamilyName_; };

  // Point size that was actually loaded (closest match to targetPtSize).
  // 0 if nothing loaded.
  uint8_t currentPointSize() const { return loadedPointSize_; };

 private:
  struct LoadedFont {
    SdCardFont* font;  // heap-allocated, owned
    int fontId;
    uint8_t size;
  };
  static int computeFontId(uint32_t contentHash, const char* familyName, uint8_t pointSize);

  std::string loadedFamilyName_;
  uint8_t loadedPointSize_ = 0;
  std::vector<LoadedFont> loaded_;
};

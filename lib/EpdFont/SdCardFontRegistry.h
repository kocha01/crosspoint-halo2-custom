#pragma once

#include <cstdint>
#include <string>
#include <vector>

struct SdCardFontFileInfo {
  std::string path;   // Full SD path to the .cpfont file. Three layouts work:
                      //   1. Flat (recommended for new users):
                      //        /fonts/<Family>_<size>.cpfont
                      //   2. Subfolder-per-family:
                      //        /fonts/<Family>/<Family>_<size>.cpfont
                      //   3. Legacy hidden (kept for backward-compat):
                      //        /.crosspoint/fonts/<Family>/<Family>_<size>.cpfont
                      //   e.g. "/fonts/PKNakhonSawan_18.cpfont"
  uint8_t pointSize;  // parsed from filename suffix: 18
  uint8_t style;      // always 0 in v4 (all 4 styles bundled in one file);
                      // kept for potential future formats
};

struct SdCardFontFamilyInfo {
  std::string name;  // directory name, e.g. "NotoSansCJK"
  std::vector<SdCardFontFileInfo> files;

  const SdCardFontFileInfo* findFile(uint8_t size, uint8_t style = 0) const;
  bool hasSize(uint8_t size) const;
  std::vector<uint8_t> availableSizes() const;
};

class SdCardFontRegistry {
 public:
  static constexpr int MAX_SD_FAMILIES = 128;
  /// Primary visible directory.  Drop .cpfont files here directly (flat) or
  /// in a per-family subfolder.  No hidden-folder gymnastics required.
  static constexpr const char* FONTS_DIR = "/fonts";
  /// Legacy hidden directory still scanned for backward compatibility with
  /// devices that were set up before the visible /fonts path existed.
  static constexpr const char* LEGACY_FONTS_DIR = "/.crosspoint/fonts";

  // Scan SD card, populate families_. Returns true if any families found.
  bool discover();

  const std::vector<SdCardFontFamilyInfo>& getFamilies() const { return families_; }
  const SdCardFontFamilyInfo* findFamily(const std::string& name) const;
  int getFamilyIndex(const std::string& name) const;
  int getFamilyCount() const { return static_cast<int>(families_.size()); }

 private:
  std::vector<SdCardFontFamilyInfo> families_;  // sorted alphabetically

  static bool parseFilename(const char* filename, uint8_t& size, uint8_t& style);
  void scanDirectory(const char* dirPath, SdCardFontFamilyInfo& family);
  /// Walks one root (e.g. "/fonts" or "/.crosspoint/fonts") and merges any
  /// flat .cpfont files (family name parsed from filename prefix) plus any
  /// family subfolders into families_.
  void scanFontsRoot(const char* rootPath);
  /// Merge a freshly-discovered family into families_, deduping by name so
  /// that the same family name appearing in both /fonts/ and the legacy
  /// directory is unified.
  void mergeFamily(SdCardFontFamilyInfo&& family);
  /// Add a single flat-layout file to its family bucket (creating the bucket
  /// when this is the first file for that family name).
  void mergeFlatFile(const std::string& familyName, SdCardFontFileInfo&& file);
};

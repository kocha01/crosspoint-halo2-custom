#include "SdCardFontRegistry.h"

#include <HalStorage.h>
#include <Logging.h>

#include <algorithm>
#include <cstring>

// --- SdCardFontFamilyInfo helpers ---

const SdCardFontFileInfo* SdCardFontFamilyInfo::findFile(uint8_t size, uint8_t style) const {
  for (const auto& f : files) {
    if (f.pointSize == size && f.style == style) return &f;
  }
  return nullptr;
}

bool SdCardFontFamilyInfo::hasSize(uint8_t size) const {
  for (const auto& f : files) {
    if (f.pointSize == size) return true;
  }
  return false;
}

std::vector<uint8_t> SdCardFontFamilyInfo::availableSizes() const {
  std::vector<uint8_t> sizes;
  for (const auto& f : files) {
    bool found = false;
    for (uint8_t s : sizes) {
      if (s == f.pointSize) {
        found = true;
        break;
      }
    }
    if (!found) sizes.push_back(f.pointSize);
  }
  std::sort(sizes.begin(), sizes.end());
  return sizes;
}

// --- SdCardFontRegistry ---

bool SdCardFontRegistry::parseFilename(const char* filename, uint8_t& size, uint8_t& style) {
  // V4 naming: <name>_<size>.cpfont (e.g. Bookerly-SD_14.cpfont)
  // Use an ends-with check rather than strstr() so that in-progress downloads
  // like "Foo_14.cpfont.tmp" or backups like "Foo_14.cpfont~" aren't accepted.
  static constexpr char kExt[] = ".cpfont";
  static constexpr size_t kExtLen = sizeof(kExt) - 1;
  const size_t nameLen = strlen(filename);
  if (nameLen <= kExtLen) return false;
  if (strcmp(filename + nameLen - kExtLen, kExt) != 0) return false;
  const char* ext = filename + nameLen - kExtLen;

  size_t baseLen = ext - filename;
  if (baseLen == 0 || baseLen > 127) return false;

  char base[128];
  memcpy(base, filename, baseLen);
  base[baseLen] = '\0';

  char* lastUnderscore = strrchr(base, '_');
  if (!lastUnderscore || lastUnderscore == base) return false;

  const char* sizeStr = lastUnderscore + 1;
  char* endPtr;
  long sizeVal = strtol(sizeStr, &endPtr, 10);
  if (endPtr == sizeStr || *endPtr != '\0' || sizeVal < 1 || sizeVal > 255) return false;
  size = static_cast<uint8_t>(sizeVal);
  style = 0;
  return true;
}

void SdCardFontRegistry::scanDirectory(const char* dirPath, SdCardFontFamilyInfo& family) {
  FsFile dir = Storage.open(dirPath);
  if (!dir || !dir.isDirectory()) return;

  char nameBuffer[128];
  while (true) {
    FsFile entry = dir.openNextFile();
    if (!entry) break;
    if (entry.isDirectory()) {
      entry.close();
      continue;
    }

    entry.getName(nameBuffer, sizeof(nameBuffer));
    entry.close();

    // Skip macOS resource fork files (._*) and other hidden files
    if (nameBuffer[0] == '.' || nameBuffer[0] == '_') continue;

    uint8_t size, style;
    if (!parseFilename(nameBuffer, size, style)) continue;

    SdCardFontFileInfo info;
    info.path = std::string(dirPath) + "/" + nameBuffer;
    info.pointSize = size;
    info.style = style;
    family.files.push_back(std::move(info));
  }
  dir.close();
}

void SdCardFontRegistry::mergeFamily(SdCardFontFamilyInfo&& family) {
  if (family.files.empty()) return;
  // If we already have this family (e.g. from another root), merge files in.
  for (auto& existing : families_) {
    if (existing.name == family.name) {
      for (auto& f : family.files) {
        existing.files.push_back(std::move(f));
      }
      return;
    }
  }
  if (static_cast<int>(families_.size()) >= MAX_SD_FAMILIES) return;
  families_.push_back(std::move(family));
}

void SdCardFontRegistry::mergeFlatFile(const std::string& familyName, SdCardFontFileInfo&& file) {
  for (auto& existing : families_) {
    if (existing.name == familyName) {
      existing.files.push_back(std::move(file));
      return;
    }
  }
  if (static_cast<int>(families_.size()) >= MAX_SD_FAMILIES) return;
  SdCardFontFamilyInfo family;
  family.name = familyName;
  family.files.push_back(std::move(file));
  families_.push_back(std::move(family));
}

void SdCardFontRegistry::scanFontsRoot(const char* rootPath) {
  FsFile root = Storage.open(rootPath);
  if (!root) {
    LOG_DBG("SDREG", "Fonts root not present (skipping): %s", rootPath);
    return;
  }
  if (!root.isDirectory()) {
    LOG_ERR("SDREG", "Fonts root is not a directory: %s", rootPath);
    root.close();
    return;
  }

  char nameBuffer[128];
  while (true) {
    FsFile entry = root.openNextFile();
    if (!entry) break;
    const bool entryIsDir = entry.isDirectory();
    entry.getName(nameBuffer, sizeof(nameBuffer));
    entry.close();

    // Skip hidden/system entries (macOS ._*, .Trashes, etc.)
    if (nameBuffer[0] == '.' || nameBuffer[0] == '_') continue;

    if (entryIsDir) {
      // Subfolder layout: directory name = family name; scan inner .cpfont files.
      SdCardFontFamilyInfo family;
      family.name = nameBuffer;
      std::string subDirPath = std::string(rootPath) + "/" + nameBuffer;
      scanDirectory(subDirPath.c_str(), family);
      mergeFamily(std::move(family));
    } else {
      // Flat layout: file directly under the root.  Filename pattern is
      // "<Family>_<size>.cpfont" — derive family name from everything before
      // the final underscore.
      uint8_t size, style;
      if (!parseFilename(nameBuffer, size, style)) continue;
      std::string fname = nameBuffer;
      const size_t dotPos = fname.rfind(".cpfont");
      if (dotPos == std::string::npos) continue;
      const size_t underscorePos = fname.rfind('_', dotPos);
      if (underscorePos == std::string::npos || underscorePos == 0) continue;
      std::string familyName = fname.substr(0, underscorePos);

      SdCardFontFileInfo info;
      info.path = std::string(rootPath) + "/" + nameBuffer;
      info.pointSize = size;
      info.style = style;
      mergeFlatFile(familyName, std::move(info));
    }
  }
  root.close();
}

bool SdCardFontRegistry::discover() {
  families_.clear();
  families_.reserve(MAX_SD_FAMILIES);

  // Scan the visible /fonts/ directory first (preferred for new users).
  scanFontsRoot(FONTS_DIR);
  // Then merge anything still living in the legacy hidden directory.
  scanFontsRoot(LEGACY_FONTS_DIR);

  // Sort families alphabetically for deterministic picker order.
  std::sort(families_.begin(), families_.end(),
            [](const SdCardFontFamilyInfo& a, const SdCardFontFamilyInfo& b) { return a.name < b.name; });

  // Within each family, sort files by point size (small→large) and dedupe.
  for (auto& fam : families_) {
    std::sort(fam.files.begin(), fam.files.end(),
              [](const SdCardFontFileInfo& a, const SdCardFontFileInfo& b) { return a.pointSize < b.pointSize; });
    fam.files.erase(std::unique(fam.files.begin(), fam.files.end(),
                                [](const SdCardFontFileInfo& a, const SdCardFontFileInfo& b) {
                                  return a.pointSize == b.pointSize && a.style == b.style;
                                }),
                    fam.files.end());
  }

  // Cap at MAX_SD_FAMILIES (defensive — mergeFamily/mergeFlatFile already cap).
  if (static_cast<int>(families_.size()) > MAX_SD_FAMILIES) {
    families_.resize(MAX_SD_FAMILIES);
  }

  for (const auto& fam : families_) {
    LOG_DBG("SDREG", "Family: %s (%d files)", fam.name.c_str(), static_cast<int>(fam.files.size()));
  }
  LOG_DBG("SDREG", "Discovery complete: %d families", static_cast<int>(families_.size()));
  return !families_.empty();
}

const SdCardFontFamilyInfo* SdCardFontRegistry::findFamily(const std::string& name) const {
  for (const auto& f : families_) {
    if (f.name == name) return &f;
  }
  return nullptr;
}

int SdCardFontRegistry::getFamilyIndex(const std::string& name) const {
  for (int i = 0; i < static_cast<int>(families_.size()); i++) {
    if (families_[i].name == name) return i;
  }
  return -1;
}

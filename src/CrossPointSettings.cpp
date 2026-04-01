#include "CrossPointSettings.h"

#include <HalStorage.h>
#include <JsonSettingsIO.h>
#include <Logging.h>
#include <Serialization.h>

#include <cstring>
#include <string>

#include "fontIds.h"

// Initialize the static instance
CrossPointSettings CrossPointSettings::instance;

void readAndValidate(FsFile& file, uint8_t& member, const uint8_t maxValue) {
  uint8_t tempValue;
  serialization::readPod(file, tempValue);
  if (tempValue < maxValue) {
    member = tempValue;
  }
}

namespace {
constexpr uint8_t SETTINGS_FILE_VERSION = 1;
constexpr char SETTINGS_FILE_BIN[] = "/.crosspoint/settings.bin";
constexpr char SETTINGS_FILE_JSON[] = "/.crosspoint/settings.json";
constexpr char SETTINGS_FILE_BAK[] = "/.crosspoint/settings.bin.bak";
constexpr uint8_t LEGACY_FONT_FAMILY_COUNT = 4;
constexpr uint8_t LEGACY_FONT_SIZE_COUNT = 4;

uint8_t migrateLegacyFontFamily(const uint8_t legacyValue) {
  return legacyValue == 3 ? CrossPointSettings::CLOUDLOOP : CrossPointSettings::BAIJAMJUREE;
}

uint8_t migrateLegacyFontSize(const uint8_t legacyValue) {
  switch (legacyValue) {
    case 0:
      return CrossPointSettings::FONT_12;
    case 1:
    default:
      return CrossPointSettings::FONT_14;
    case 2:
      return CrossPointSettings::FONT_16;
    case 3:
      return CrossPointSettings::FONT_18;
  }
}

uint8_t normalizeFontSize(const uint8_t sizeValue) {
  if (sizeValue <= CrossPointSettings::FONT_SIZE_MIN) return CrossPointSettings::FONT_SIZE_MIN;
  if (sizeValue >= CrossPointSettings::FONT_SIZE_MAX) return CrossPointSettings::FONT_SIZE_MAX;

  const uint8_t delta = sizeValue - CrossPointSettings::FONT_SIZE_MIN;
  const uint8_t roundedSteps = static_cast<uint8_t>((delta + 1) / CrossPointSettings::FONT_SIZE_STEP);
  return static_cast<uint8_t>(CrossPointSettings::FONT_SIZE_MIN +
                              roundedSteps * CrossPointSettings::FONT_SIZE_STEP);
}

// Convert legacy front button layout into explicit logical->hardware mapping.
void applyLegacyFrontButtonLayout(CrossPointSettings& settings) {
  switch (static_cast<CrossPointSettings::FRONT_BUTTON_LAYOUT>(settings.frontButtonLayout)) {
    case CrossPointSettings::LEFT_RIGHT_BACK_CONFIRM:
      settings.frontButtonBack = CrossPointSettings::FRONT_HW_LEFT;
      settings.frontButtonConfirm = CrossPointSettings::FRONT_HW_RIGHT;
      settings.frontButtonLeft = CrossPointSettings::FRONT_HW_BACK;
      settings.frontButtonRight = CrossPointSettings::FRONT_HW_CONFIRM;
      break;
    case CrossPointSettings::LEFT_BACK_CONFIRM_RIGHT:
      settings.frontButtonBack = CrossPointSettings::FRONT_HW_CONFIRM;
      settings.frontButtonConfirm = CrossPointSettings::FRONT_HW_LEFT;
      settings.frontButtonLeft = CrossPointSettings::FRONT_HW_BACK;
      settings.frontButtonRight = CrossPointSettings::FRONT_HW_RIGHT;
      break;
    case CrossPointSettings::BACK_CONFIRM_RIGHT_LEFT:
      settings.frontButtonBack = CrossPointSettings::FRONT_HW_BACK;
      settings.frontButtonConfirm = CrossPointSettings::FRONT_HW_CONFIRM;
      settings.frontButtonLeft = CrossPointSettings::FRONT_HW_RIGHT;
      settings.frontButtonRight = CrossPointSettings::FRONT_HW_LEFT;
      break;
    case CrossPointSettings::BACK_CONFIRM_LEFT_RIGHT:
    default:
      settings.frontButtonBack = CrossPointSettings::FRONT_HW_BACK;
      settings.frontButtonConfirm = CrossPointSettings::FRONT_HW_CONFIRM;
      settings.frontButtonLeft = CrossPointSettings::FRONT_HW_LEFT;
      settings.frontButtonRight = CrossPointSettings::FRONT_HW_RIGHT;
      break;
  }
}

}  // namespace

void CrossPointSettings::validateFrontButtonMapping(CrossPointSettings& settings) {
  const uint8_t mapping[] = {settings.frontButtonBack, settings.frontButtonConfirm, settings.frontButtonLeft,
                             settings.frontButtonRight};
  for (size_t i = 0; i < 4; i++) {
    for (size_t j = i + 1; j < 4; j++) {
      if (mapping[i] == mapping[j]) {
        settings.frontButtonBack = FRONT_HW_BACK;
        settings.frontButtonConfirm = FRONT_HW_CONFIRM;
        settings.frontButtonLeft = FRONT_HW_LEFT;
        settings.frontButtonRight = FRONT_HW_RIGHT;
        return;
      }
    }
  }
}

bool CrossPointSettings::saveToFile() const {
  Storage.mkdir("/.crosspoint");
  return JsonSettingsIO::saveSettings(*this, SETTINGS_FILE_JSON);
}

bool CrossPointSettings::loadFromFile() {
  // Try JSON first
  if (Storage.exists(SETTINGS_FILE_JSON)) {
    String json = Storage.readFile(SETTINGS_FILE_JSON);
    if (!json.isEmpty()) {
      bool resave = false;
      bool result = JsonSettingsIO::loadSettings(*this, json.c_str(), &resave);
      if (result && resave) {
        if (saveToFile()) {
          LOG_DBG("CPS", "Resaved settings to update format");
        } else {
          LOG_ERR("CPS", "Failed to resave settings after format update");
        }
      }
      return result;
    }
  }

  // Fall back to binary migration
  if (Storage.exists(SETTINGS_FILE_BIN)) {
    if (loadFromBinaryFile()) {
      if (saveToFile()) {
        Storage.rename(SETTINGS_FILE_BIN, SETTINGS_FILE_BAK);
        LOG_DBG("CPS", "Migrated settings.bin to settings.json");
        return true;
      } else {
        LOG_ERR("CPS", "Failed to save migrated settings to JSON");
        return false;
      }
    }
  }

  return false;
}

bool CrossPointSettings::loadFromBinaryFile() {
  FsFile inputFile;
  if (!Storage.openFileForRead("CPS", SETTINGS_FILE_BIN, inputFile)) {
    return false;
  }

  uint8_t version;
  serialization::readPod(inputFile, version);
  if (version != SETTINGS_FILE_VERSION) {
    LOG_ERR("CPS", "Deserialization failed: Unknown version %u", version);
    inputFile.close();
    return false;
  }

  uint8_t fileSettingsCount = 0;
  serialization::readPod(inputFile, fileSettingsCount);

  uint8_t settingsRead = 0;
  bool frontButtonMappingRead = false;
  do {
    readAndValidate(inputFile, sleepScreen, SLEEP_SCREEN_MODE_COUNT);
    if (++settingsRead >= fileSettingsCount) break;
    serialization::readPod(inputFile, extraParagraphSpacing);
    if (++settingsRead >= fileSettingsCount) break;
    readAndValidate(inputFile, shortPwrBtn, SHORT_PWRBTN_COUNT);
    if (++settingsRead >= fileSettingsCount) break;
    readAndValidate(inputFile, statusBar, STATUS_BAR_MODE_COUNT);  // legacy
    if (++settingsRead >= fileSettingsCount) break;
    readAndValidate(inputFile, orientation, ORIENTATION_COUNT);
    if (++settingsRead >= fileSettingsCount) break;
    readAndValidate(inputFile, frontButtonLayout, FRONT_BUTTON_LAYOUT_COUNT);
    if (++settingsRead >= fileSettingsCount) break;
    readAndValidate(inputFile, sideButtonLayout, SIDE_BUTTON_LAYOUT_COUNT);
    if (++settingsRead >= fileSettingsCount) break;
    {
      uint8_t rawFontFamily = 0;
      serialization::readPod(inputFile, rawFontFamily);
      if (rawFontFamily < LEGACY_FONT_FAMILY_COUNT) {
        fontFamily = migrateLegacyFontFamily(rawFontFamily);
      }
    }
    if (++settingsRead >= fileSettingsCount) break;
    {
      uint8_t rawFontSize = 0;
      serialization::readPod(inputFile, rawFontSize);
      if (rawFontSize < LEGACY_FONT_SIZE_COUNT) {
        fontSize = migrateLegacyFontSize(rawFontSize);
      }
    }
    if (++settingsRead >= fileSettingsCount) break;
    readAndValidate(inputFile, lineSpacing, LINE_COMPRESSION_COUNT);
    if (++settingsRead >= fileSettingsCount) break;
    readAndValidate(inputFile, paragraphAlignment, PARAGRAPH_ALIGNMENT_COUNT);
    if (++settingsRead >= fileSettingsCount) break;
    readAndValidate(inputFile, sleepTimeout, SLEEP_TIMEOUT_COUNT);
    if (++settingsRead >= fileSettingsCount) break;
    readAndValidate(inputFile, refreshFrequency, REFRESH_FREQUENCY_COUNT);
    if (++settingsRead >= fileSettingsCount) break;
    serialization::readPod(inputFile, screenMargin);
    if (++settingsRead >= fileSettingsCount) break;
    readAndValidate(inputFile, sleepScreenCoverMode, SLEEP_SCREEN_COVER_MODE_COUNT);
    if (++settingsRead >= fileSettingsCount) break;
    {
      std::string urlStr;
      serialization::readString(inputFile, urlStr);
      strncpy(opdsServerUrl, urlStr.c_str(), sizeof(opdsServerUrl) - 1);
      opdsServerUrl[sizeof(opdsServerUrl) - 1] = '\0';
    }
    if (++settingsRead >= fileSettingsCount) break;
    serialization::readPod(inputFile, textAntiAliasing);
    if (++settingsRead >= fileSettingsCount) break;
    readAndValidate(inputFile, hideBatteryPercentage, HIDE_BATTERY_PERCENTAGE_COUNT);
    if (++settingsRead >= fileSettingsCount) break;
    serialization::readPod(inputFile, longPressChapterSkip);
    if (++settingsRead >= fileSettingsCount) break;
    serialization::readPod(inputFile, hyphenationEnabled);
    if (++settingsRead >= fileSettingsCount) break;
    {
      std::string usernameStr;
      serialization::readString(inputFile, usernameStr);
      strncpy(opdsUsername, usernameStr.c_str(), sizeof(opdsUsername) - 1);
      opdsUsername[sizeof(opdsUsername) - 1] = '\0';
    }
    if (++settingsRead >= fileSettingsCount) break;
    {
      std::string passwordStr;
      serialization::readString(inputFile, passwordStr);
      strncpy(opdsPassword, passwordStr.c_str(), sizeof(opdsPassword) - 1);
      opdsPassword[sizeof(opdsPassword) - 1] = '\0';
    }
    if (++settingsRead >= fileSettingsCount) break;
    readAndValidate(inputFile, sleepScreenCoverFilter, SLEEP_SCREEN_COVER_FILTER_COUNT);
    if (++settingsRead >= fileSettingsCount) break;
    serialization::readPod(inputFile, uiTheme);
    if (++settingsRead >= fileSettingsCount) break;
    readAndValidate(inputFile, frontButtonBack, FRONT_BUTTON_HARDWARE_COUNT);
    if (++settingsRead >= fileSettingsCount) break;
    readAndValidate(inputFile, frontButtonConfirm, FRONT_BUTTON_HARDWARE_COUNT);
    if (++settingsRead >= fileSettingsCount) break;
    readAndValidate(inputFile, frontButtonLeft, FRONT_BUTTON_HARDWARE_COUNT);
    if (++settingsRead >= fileSettingsCount) break;
    readAndValidate(inputFile, frontButtonRight, FRONT_BUTTON_HARDWARE_COUNT);
    frontButtonMappingRead = true;
    if (++settingsRead >= fileSettingsCount) break;
    serialization::readPod(inputFile, fadingFix);
    if (++settingsRead >= fileSettingsCount) break;
    serialization::readPod(inputFile, embeddedStyle);
    if (++settingsRead >= fileSettingsCount) break;
  } while (false);

  if (frontButtonMappingRead) {
    CrossPointSettings::validateFrontButtonMapping(*this);
  } else {
    applyLegacyFrontButtonLayout(*this);
  }

  inputFile.close();
  LOG_DBG("CPS", "Settings loaded from binary file");
  return true;
}

float CrossPointSettings::getReaderLineCompression() const {
  switch (fontFamily) {
    case BAIJAMJUREE:
    default:
      switch (lineSpacing) {
        case TIGHT:
          return 1.20f;
        case NORMAL:
        default:
          return 1.40f;
        case WIDE:
          return 1.60f;
      }
    case CLOUDLOOP:
      switch (lineSpacing) {
        case TIGHT:
          return 0.90f;
        case NORMAL:
        default:
          return 0.95f;
        case WIDE:
          return 1.05f;
      }
  }
}

unsigned long CrossPointSettings::getSleepTimeoutMs() const {
  switch (sleepTimeout) {
    case SLEEP_1_MIN:
      return 1UL * 60 * 1000;
    case SLEEP_5_MIN:
      return 5UL * 60 * 1000;
    case SLEEP_10_MIN:
    default:
      return 10UL * 60 * 1000;
    case SLEEP_15_MIN:
      return 15UL * 60 * 1000;
    case SLEEP_30_MIN:
      return 30UL * 60 * 1000;
  }
}

int CrossPointSettings::getRefreshFrequency() const {
  switch (refreshFrequency) {
    case REFRESH_1:
      return 1;
    case REFRESH_5:
      return 5;
    case REFRESH_10:
      return 10;
    case REFRESH_15:
    default:
      return 15;
    case REFRESH_30:
      return 30;
  }
}

int CrossPointSettings::getReaderFontId() const {
  switch (fontFamily) {
    case BAIJAMJUREE:
    default:
      switch (normalizeFontSize(fontSize)) {
        case FONT_12: return BAIJAMJUREE_12_FONT_ID;
        case FONT_14: return BAIJAMJUREE_14_FONT_ID;
        case FONT_16: return BAIJAMJUREE_16_FONT_ID;
        case FONT_18: return BAIJAMJUREE_18_FONT_ID;
        case FONT_20: default: return BAIJAMJUREE_20_FONT_ID;
      }
    case CLOUDLOOP:
      switch (normalizeFontSize(fontSize)) {
        case FONT_12: return CLOUDLOOP_12_FONT_ID;
        case FONT_14: return CLOUDLOOP_14_FONT_ID;
        case FONT_16: return CLOUDLOOP_16_FONT_ID;
        case FONT_18: return CLOUDLOOP_18_FONT_ID;
        case FONT_20: default: return CLOUDLOOP_20_FONT_ID;
      }
    case BOOKERLY:
      switch (normalizeFontSize(fontSize)) {
        case FONT_12: return BOOKERLY_12_FONT_ID;
        case FONT_14: return BOOKERLY_14_FONT_ID;
        case FONT_16: return BOOKERLY_16_FONT_ID;
        case FONT_18: return BOOKERLY_18_FONT_ID;
        case FONT_20: default: return BOOKERLY_20_FONT_ID;
      }
  }
}

static bool isThaiLanguage(const std::string& language) {
  if (language.empty()) return false;
  // Match "th", "tha", "th-TH", "th-*" etc.
  return language == "th" || language == "tha" || (language.size() >= 3 && language[0] == 't' && language[1] == 'h' && language[2] == '-');
}

static bool containsThaiChars(const std::string& text) {
  // Scan for Thai Unicode codepoints (U+0E01-U+0E3A, U+0E40-U+0E4E)
  const auto* p = reinterpret_cast<const uint8_t*>(text.c_str());
  while (*p) {
    if (p[0] == 0xE0 && p[1] >= 0xB8 && p[1] <= 0xB9) {
      return true;  // Thai block: U+0E00-U+0E7F = UTF-8 0xE0 0xB8/0xB9 xx
    }
    // Skip UTF-8 multibyte sequences
    if (*p < 0x80) p += 1;
    else if (*p < 0xE0) p += 2;
    else if (*p < 0xF0) p += 3;
    else p += 4;
  }
  return false;
}

int CrossPointSettings::getThaiFallbackFontId() const {
  // Use Noto Serif (serif Latin + Thai via NotoSansThaiLooped font stack)
  switch (normalizeFontSize(fontSize)) {
    case FONT_12: return NOTOSERIF_12_FONT_ID;
    case FONT_14: return NOTOSERIF_14_FONT_ID;
    case FONT_16: return NOTOSERIF_16_FONT_ID;
    case FONT_18: return NOTOSERIF_18_FONT_ID;
    case FONT_20: default: return NOTOSERIF_20_FONT_ID;
  }
}

int CrossPointSettings::getReaderFontIdForLanguage(const std::string& language) const {
  if (fontFamily != BOOKERLY) return getReaderFontId();
  if (isThaiLanguage(language)) return getThaiFallbackFontId();
  return getReaderFontId();
}

int CrossPointSettings::getReaderFontIdForThaiContent(const std::string& language, const std::string& title) const {
  // Bai Jamjuree and CloudLoop all have native Thai glyphs — no fallback needed.
  if (fontFamily != BOOKERLY) return getReaderFontId();
  // Bookerly lacks Thai glyphs; auto-switch to Noto Serif (Thai font stack).
  if (isThaiLanguage(language) || containsThaiChars(title)) return getThaiFallbackFontId();
  return getReaderFontId();
}

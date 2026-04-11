#include <Arduino.h>
#include <Epub.h>
#include <FontCacheManager.h>
#include <FontDecompressor.h>
#include <GfxRenderer.h>
#include <HalDisplay.h>
#include <HalGPIO.h>
#include <HalPowerManager.h>
#include <HalStorage.h>
#include <HalSystem.h>
#include <I18n.h>
#include <Logging.h>
#include <SPI.h>
#include <builtinFonts/all.h>

#include <cstring>

#include "BleTimeSync.h"
#include "CrossPointSettings.h"
#include "CrossPointState.h"
#include "Epub/hyphenation/ThaiWordBreaker.h"
#include "KOReaderCredentialStore.h"
#include "MappedInputManager.h"
#include "RecentBooksStore.h"
#include "activities/Activity.h"
#include "activities/ActivityManager.h"
#include "components/UITheme.h"
#include "fontIds.h"
#include "util/ButtonNavigator.h"
#include "util/ScreenshotUtil.h"

HalDisplay display;
HalGPIO gpio;
MappedInputManager mappedInputManager(gpio);
GfxRenderer renderer(display);
ActivityManager activityManager(renderer, mappedInputManager);
FontDecompressor fontDecompressor;
FontCacheManager fontCacheManager(renderer.getFontMap());

// Fonts
EpdFont smallFont(&baijamjuree_8_regular);
EpdFontFamily smallFontFamily(&smallFont);

EpdFont ui10RegularFont(&baijamjuree_10_regular);
EpdFontFamily ui10FontFamily(&ui10RegularFont);  // faux bold via renderer

EpdFont ui12RegularFont(&baijamjuree_12_regular);
EpdFont ui12BoldFont(&baijamjuree_12_bold);
EpdFontFamily ui12FontFamily(&ui12RegularFont, &ui12BoldFont);

EpdFont cjk8RegularFont(&notosanssc_8_regular);
EpdFontFamily cjk8FontFamily(&cjk8RegularFont);

EpdFont cjk10RegularFont(&notosanssc_10_regular);
EpdFont cjk10BoldFont(&notosanssc_10_bold);
EpdFontFamily cjk10FontFamily(&cjk10RegularFont, &cjk10BoldFont);

EpdFont cjk12RegularFont(&notosanssc_12_regular);
EpdFont cjk12BoldFont(&notosanssc_12_bold);
EpdFontFamily cjk12FontFamily(&cjk12RegularFont, &cjk12BoldFont);

#ifndef OMIT_FONTS
// Bai Jamjuree (Thai + Latin sans-serif — reader font + UI number display + Thai UI fallback)
EpdFont baijamjuree8RegularFont(&baijamjuree_8_regular);
EpdFontFamily baijamjuree8FontFamily(&baijamjuree8RegularFont);
EpdFont baijamjuree10RegularFont(&baijamjuree_10_regular);
EpdFontFamily baijamjuree10FontFamily(&baijamjuree10RegularFont);
EpdFont baijamjuree12RegularFont(&baijamjuree_12_regular);
EpdFont baijamjuree12BoldFont(&baijamjuree_12_bold);
EpdFontFamily baijamjuree12FontFamily(&baijamjuree12RegularFont, &baijamjuree12BoldFont);
EpdFont baijamjuree14RegularFont(&baijamjuree_14_regular);
EpdFont baijamjuree14BoldFont(&baijamjuree_14_bold);
EpdFontFamily baijamjuree14FontFamily(&baijamjuree14RegularFont, &baijamjuree14BoldFont);
EpdFont baijamjuree16RegularFont(&baijamjuree_16_regular);
EpdFont baijamjuree16BoldFont(&baijamjuree_16_bold);
EpdFontFamily baijamjuree16FontFamily(&baijamjuree16RegularFont, &baijamjuree16BoldFont);
EpdFont baijamjuree18RegularFont(&baijamjuree_18_regular);
EpdFont baijamjuree18BoldFont(&baijamjuree_18_bold);
EpdFontFamily baijamjuree18FontFamily(&baijamjuree18RegularFont, &baijamjuree18BoldFont);
EpdFont baijamjuree20RegularFont(&baijamjuree_20_regular);
EpdFont baijamjuree20BoldFont(&baijamjuree_20_bold);
EpdFontFamily baijamjuree20FontFamily(&baijamjuree20RegularFont, &baijamjuree20BoldFont);
EpdFont cloudloop36Font(&cloudloop_36_regular);
EpdFontFamily cloudloop36FontFamily(&cloudloop36Font);  // UI number display (font-size selector)

EpdFont cloudloop12RegularFont(&cloudloop_12_regular);
EpdFontFamily cloudloop12FontFamily(&cloudloop12RegularFont);
EpdFont cloudloop14RegularFont(&cloudloop_14_regular);
EpdFontFamily cloudloop14FontFamily(&cloudloop14RegularFont);
EpdFont cloudloop16RegularFont(&cloudloop_16_regular);
EpdFontFamily cloudloop16FontFamily(&cloudloop16RegularFont);
EpdFont cloudloop18RegularFont(&cloudloop_18_regular);
EpdFontFamily cloudloop18FontFamily(&cloudloop18RegularFont);
EpdFont cloudloop20RegularFont(&cloudloop_20_regular);
EpdFontFamily cloudloop20FontFamily(&cloudloop20RegularFont);

// Bookerly (Latin serif — fallback to Literata for Thai)
EpdFont bookerly12RegularFont(&bookerly_12_regular);
EpdFont bookerly12BoldFont(&bookerly_12_bold);
EpdFontFamily bookerly12FontFamily(&bookerly12RegularFont, &bookerly12BoldFont);
EpdFont bookerly14RegularFont(&bookerly_14_regular);
EpdFont bookerly14BoldFont(&bookerly_14_bold);
EpdFontFamily bookerly14FontFamily(&bookerly14RegularFont, &bookerly14BoldFont);
EpdFont bookerly16RegularFont(&bookerly_16_regular);
EpdFont bookerly16BoldFont(&bookerly_16_bold);
EpdFontFamily bookerly16FontFamily(&bookerly16RegularFont, &bookerly16BoldFont);
EpdFont bookerly18RegularFont(&bookerly_18_regular);
EpdFont bookerly18BoldFont(&bookerly_18_bold);
EpdFontFamily bookerly18FontFamily(&bookerly18RegularFont, &bookerly18BoldFont);
EpdFont bookerly20RegularFont(&bookerly_20_regular);
EpdFont bookerly20BoldFont(&bookerly_20_bold);
EpdFontFamily bookerly20FontFamily(&bookerly20RegularFont, &bookerly20BoldFont);

// Noto Serif (serif + Thai — auto-selected when Bookerly + Thai book)
EpdFont notoserif12RegularFont(&notoserif_12_regular);
EpdFont notoserif12BoldFont(&notoserif_12_bold);
EpdFontFamily notoserif12FontFamily(&notoserif12RegularFont, &notoserif12BoldFont);
EpdFont notoserif14RegularFont(&notoserif_14_regular);
EpdFont notoserif14BoldFont(&notoserif_14_bold);
EpdFontFamily notoserif14FontFamily(&notoserif14RegularFont, &notoserif14BoldFont);
EpdFont notoserif16RegularFont(&notoserif_16_regular);
EpdFont notoserif16BoldFont(&notoserif_16_bold);
EpdFontFamily notoserif16FontFamily(&notoserif16RegularFont, &notoserif16BoldFont);
EpdFont notoserif18RegularFont(&notoserif_18_regular);
EpdFont notoserif18BoldFont(&notoserif_18_bold);
EpdFontFamily notoserif18FontFamily(&notoserif18RegularFont, &notoserif18BoldFont);
EpdFont notoserif20RegularFont(&notoserif_20_regular);
EpdFont notoserif20BoldFont(&notoserif_20_bold);
EpdFontFamily notoserif20FontFamily(&notoserif20RegularFont, &notoserif20BoldFont);
#endif  // OMIT_FONTS

// measurement of power button press duration calibration value
unsigned long t1 = 0;
unsigned long t2 = 0;

// Verify power button press duration on wake-up from deep sleep
// Pre-condition: isWakeupByPowerButton() == true
void verifyPowerButtonDuration() {
  // Give the user up to 1000ms to start holding the power button, and must hold for SETTINGS.getPowerButtonDuration()
  const auto start = millis();
  bool abort = false;
  // Subtract the current time, because inputManager only starts counting the HeldTime from the first update()
  // This way, we remove the time we already took to reach here from the duration,
  // assuming the button was held until now from millis()==0 (i.e. device start time).
  const uint16_t calibration = start;
  const uint16_t calibratedPressDuration =
      (calibration < SETTINGS.getPowerButtonDuration()) ? SETTINGS.getPowerButtonDuration() - calibration : 1;

  gpio.update();
  // Needed because inputManager.isPressed() may take up to ~500ms to return the correct state
  while (!gpio.isPressed(HalGPIO::BTN_POWER) && millis() - start < 1000) {
    delay(10);  // only wait 10ms each iteration to not delay too much in case of short configured duration.
    gpio.update();
  }

  t2 = millis();
  if (gpio.isPressed(HalGPIO::BTN_POWER)) {
    do {
      delay(10);
      gpio.update();
    } while (gpio.isPressed(HalGPIO::BTN_POWER) && gpio.getHeldTime() < calibratedPressDuration);
    abort = gpio.getHeldTime() < calibratedPressDuration;
  } else {
    abort = true;
  }

  if (abort) {
    // Button released too early. Returning to sleep.
    // IMPORTANT: Re-arm the wakeup trigger before sleeping again
    powerManager.startDeepSleep(gpio);
  }
}

void waitForPowerRelease() {
  gpio.update();
  while (gpio.isPressed(HalGPIO::BTN_POWER)) {
    delay(50);
    gpio.update();
  }
}

// Enter deep sleep mode
void enterDeepSleep() {
  HalPowerManager::Lock powerLock;  // Ensure we are at normal CPU frequency for sleep preparation
  APP_STATE.lastSleepFromReader = activityManager.isReaderActivity();
  APP_STATE.saveToFile();

  activityManager.goToSleep();

  display.deepSleep();
  LOG_DBG("MAIN", "Power button press calibration value: %lu ms", t2 - t1);
  LOG_DBG("MAIN", "Entering deep sleep");

  powerManager.startDeepSleep(gpio);
}

// Load user-supplied Thai dictionary words from SD card (/crosspoint/thai_dict.txt).
// Each line should contain one Thai word. Lines starting with '#' are comments.
// Maximum 200 words to stay within ESP32-C3 memory constraints.
void loadThaiUserDictionary() {
  static constexpr char DICT_PATH[] = "/crosspoint/thai_dict.txt";
  static constexpr size_t MAX_BUF = 8192;  // 8 KB max file size

  if (!Storage.exists(DICT_PATH)) {
    return;
  }

  // Stack buffer would be too large (> 256 bytes), so use heap allocation.
  auto* buf = static_cast<char*>(malloc(MAX_BUF));
  if (!buf) {
    LOG_ERR("MAIN", "Failed to allocate buffer for Thai user dictionary");
    return;
  }

  const size_t bytesRead = Storage.readFileToBuffer(DICT_PATH, buf, MAX_BUF);
  if (bytesRead == 0) {
    free(buf);
    return;
  }

  std::vector<std::string> words;
  words.reserve(64);

  // Parse line by line
  size_t lineStart = 0;
  for (size_t i = 0; i <= bytesRead; ++i) {
    if (i == bytesRead || buf[i] == '\n' || buf[i] == '\r') {
      if (i > lineStart) {
        // Trim trailing whitespace
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
  buf = nullptr;

  if (!words.empty()) {
    ThaiWordBreaker::setUserDictionary(words);
    LOG_DBG("MAIN", "Thai user dictionary: %d words from %s", static_cast<int>(words.size()), DICT_PATH);
  }
}

void setupDisplayAndFonts() {
  display.begin();
  renderer.begin();
  activityManager.begin();
  LOG_DBG("MAIN", "Display initialized");

  // Initialize font decompressor for compressed reader fonts
  if (!fontDecompressor.init()) {
    LOG_ERR("MAIN", "Font decompressor init failed");
  }
  fontCacheManager.setFontDecompressor(&fontDecompressor);
  renderer.setFontCacheManager(&fontCacheManager);
#ifndef OMIT_FONTS
  // Bai Jamjuree (reader + UI number display)
  renderer.insertFont(BAIJAMJUREE_12_FONT_ID, baijamjuree12FontFamily);
  renderer.insertFont(BAIJAMJUREE_14_FONT_ID, baijamjuree14FontFamily);
  renderer.insertFont(BAIJAMJUREE_16_FONT_ID, baijamjuree16FontFamily);
  renderer.insertFont(BAIJAMJUREE_18_FONT_ID, baijamjuree18FontFamily);
  renderer.insertFont(BAIJAMJUREE_20_FONT_ID, baijamjuree20FontFamily);
  renderer.insertFont(NOTOSANS_18_FONT_ID, baijamjuree18FontFamily);  // font-name display in reader menu
  renderer.insertFont(NOTOSANS_36_FONT_ID, cloudloop36FontFamily);    // font-size display in reader menu
  // CloudLoop
  renderer.insertFont(CLOUDLOOP_12_FONT_ID, cloudloop12FontFamily);
  renderer.insertFont(CLOUDLOOP_14_FONT_ID, cloudloop14FontFamily);
  renderer.insertFont(CLOUDLOOP_16_FONT_ID, cloudloop16FontFamily);
  renderer.insertFont(CLOUDLOOP_18_FONT_ID, cloudloop18FontFamily);
  renderer.insertFont(CLOUDLOOP_20_FONT_ID, cloudloop20FontFamily);
  // Bookerly
  renderer.insertFont(BOOKERLY_12_FONT_ID, bookerly12FontFamily);
  renderer.insertFont(BOOKERLY_14_FONT_ID, bookerly14FontFamily);
  renderer.insertFont(BOOKERLY_16_FONT_ID, bookerly16FontFamily);
  renderer.insertFont(BOOKERLY_18_FONT_ID, bookerly18FontFamily);
  renderer.insertFont(BOOKERLY_20_FONT_ID, bookerly20FontFamily);
  // Noto Serif (serif + Thai — auto-selected when Bookerly + Thai book)
  renderer.insertFont(NOTOSERIF_12_FONT_ID, notoserif12FontFamily);
  renderer.insertFont(NOTOSERIF_14_FONT_ID, notoserif14FontFamily);
  renderer.insertFont(NOTOSERIF_16_FONT_ID, notoserif16FontFamily);
  renderer.insertFont(NOTOSERIF_18_FONT_ID, notoserif18FontFamily);
  renderer.insertFont(NOTOSERIF_20_FONT_ID, notoserif20FontFamily);
  // Noto Serif as fallback for Bookerly (provides Thai glyphs for mixed-language text)
  renderer.setFallbackFont(BOOKERLY_12_FONT_ID, &notoserif12FontFamily);
  renderer.setFallbackFont(BOOKERLY_14_FONT_ID, &notoserif14FontFamily);
  renderer.setFallbackFont(BOOKERLY_16_FONT_ID, &notoserif16FontFamily);
  renderer.setFallbackFont(BOOKERLY_18_FONT_ID, &notoserif18FontFamily);
  renderer.setFallbackFont(BOOKERLY_20_FONT_ID, &notoserif20FontFamily);
#endif  // OMIT_FONTS
  renderer.insertFont(UI_10_FONT_ID, ui10FontFamily);
  renderer.insertFont(UI_12_FONT_ID, ui12FontFamily);
  renderer.insertFont(SMALL_FONT_ID, smallFontFamily);
  renderer.setFallbackFont(SMALL_FONT_ID, &cjk8FontFamily);
  renderer.setFallbackFont(UI_10_FONT_ID, &cjk10FontFamily);
  renderer.setFallbackFont(UI_12_FONT_ID, &cjk12FontFamily);

  LOG_DBG("MAIN", "Fonts setup");
}

void setup() {
  t1 = millis();

  HalSystem::begin();
  gpio.begin();
  powerManager.begin();

  // Only start serial if USB connected
  if (gpio.isUsbConnected()) {
    Serial.begin(115200);
    // Wait up to 3 seconds for Serial to be ready to catch early logs
    unsigned long start = millis();
    while (!Serial && (millis() - start) < 3000) {
      delay(10);
    }
  }

  // SD Card Initialization
  // We need 6 open files concurrently when parsing a new chapter
  if (!Storage.begin()) {
    LOG_ERR("MAIN", "SD card initialization failed");
    setupDisplayAndFonts();
    activityManager.goToFullScreenMessage("SD card error", EpdFontFamily::BOLD);
    return;
  }

  HalSystem::checkPanic();
  HalSystem::clearPanic();  // TODO: move this to an activity when we have one to display the panic info

  SETTINGS.loadFromFile();
  I18N.loadSettings();
  KOREADER_STORE.loadFromFile();
  UITheme::getInstance().reload();
  ButtonNavigator::setMappedInputManager(mappedInputManager);

  // Load optional Thai user dictionary from SD card (non-blocking, silently skips if absent)
  loadThaiUserDictionary();

  switch (gpio.getWakeupReason()) {
    case HalGPIO::WakeupReason::PowerButton:
      // For normal wakeups, verify power button press duration
      LOG_DBG("MAIN", "Verifying power button press duration");
      verifyPowerButtonDuration();
      break;
    case HalGPIO::WakeupReason::AfterUSBPower:
      // If USB power caused a cold boot, go back to sleep
      LOG_DBG("MAIN", "Wakeup reason: After USB Power");
      powerManager.startDeepSleep(gpio);
      break;
    case HalGPIO::WakeupReason::AfterFlash:
      // After flashing, just proceed to boot
    case HalGPIO::WakeupReason::Other:
    default:
      break;
  }

  // First serial output only here to avoid timing inconsistencies for power button press duration verification
  LOG_DBG("MAIN", "Starting CrossPoint version " CROSSPOINT_VERSION);

  setupDisplayAndFonts();

  // BLE time sync at boot — disabled until clock UI is ready
  // To re-enable: uncomment the block below
  // {
  //   renderer.clearScreen();
  //   renderer.drawCenteredText(UI_10_FONT_ID, renderer.getScreenHeight() / 2 - 10, "CROSSPOINT", true,
  //   EpdFontFamily::BOLD); renderer.drawCenteredText(SMALL_FONT_ID, renderer.getScreenHeight() / 2 + 15, "Syncing
  //   time via Bluetooth..."); renderer.displayBuffer(); const bool synced = BleTimeSync::sync(10000);
  //   LOG_INF("MAIN", "BLE time sync: %s", synced ? "OK" : "timeout");
  // }

  activityManager.goToBoot();

  APP_STATE.loadFromFile();
  RECENT_BOOKS.loadFromFile();

  // Boot to home screen if no book is open, last sleep was not from reader, back button is held, or reader activity
  // crashed (indicated by readerActivityLoadCount > 0)
  if (APP_STATE.openEpubPath.empty() || !APP_STATE.lastSleepFromReader ||
      mappedInputManager.isPressed(MappedInputManager::Button::Back) || APP_STATE.readerActivityLoadCount > 0) {
    activityManager.goHome();
  } else {
    // Clear app state to avoid getting into a boot loop if the epub doesn't load
    const auto path = APP_STATE.openEpubPath;
    APP_STATE.openEpubPath = "";
    APP_STATE.readerActivityLoadCount++;
    APP_STATE.saveToFile();
    activityManager.goToReader(path);
  }

  // Ensure we're not still holding the power button before leaving setup
  waitForPowerRelease();
}

void loop() {
  static unsigned long maxLoopDuration = 0;
  const unsigned long loopStartTime = millis();
  static unsigned long lastMemPrint = 0;

  gpio.update();

  renderer.setFadingFix(SETTINGS.fadingFix);

  if (Serial && millis() - lastMemPrint >= 10000) {
    LOG_INF("MEM", "Free: %d bytes, Total: %d bytes, Min Free: %d bytes, MaxAlloc: %d bytes", ESP.getFreeHeap(),
            ESP.getHeapSize(), ESP.getMinFreeHeap(), ESP.getMaxAllocHeap());
    lastMemPrint = millis();
  }

  // Handle incoming serial commands,
  // nb: we use logSerial from logging to avoid deprecation warnings
  if (logSerial.available() > 0) {
    String line = logSerial.readStringUntil('\n');
    if (line.startsWith("CMD:")) {
      String cmd = line.substring(4);
      cmd.trim();
      if (cmd == "SCREENSHOT") {
        logSerial.printf("SCREENSHOT_START:%d\n", HalDisplay::BUFFER_SIZE);
        uint8_t* buf = display.getFrameBuffer();
        logSerial.write(buf, HalDisplay::BUFFER_SIZE);
        logSerial.printf("SCREENSHOT_END\n");
      }
    }
  }

  // Check for any user activity (button press or release) or active background work
  static unsigned long lastActivityTime = millis();
  if (gpio.wasAnyPressed() || gpio.wasAnyReleased() || activityManager.preventAutoSleep()) {
    lastActivityTime = millis();         // Reset inactivity timer
    powerManager.setPowerSaving(false);  // Restore normal CPU frequency on user activity
  }

  static bool screenshotButtonsReleased = true;
  if (gpio.isPressed(HalGPIO::BTN_POWER) && gpio.isPressed(HalGPIO::BTN_DOWN)) {
    if (screenshotButtonsReleased) {
      screenshotButtonsReleased = false;
      {
        RenderLock lock;
        ScreenshotUtil::takeScreenshot(renderer);
      }
    }
    return;
  } else {
    screenshotButtonsReleased = true;
  }

  const unsigned long sleepTimeoutMs = SETTINGS.getSleepTimeoutMs();
  if (millis() - lastActivityTime >= sleepTimeoutMs) {
    LOG_DBG("SLP", "Auto-sleep triggered after %lu ms of inactivity", sleepTimeoutMs);
    enterDeepSleep();
    // This should never be hit as `enterDeepSleep` calls esp_deep_sleep_start
    return;
  }

  if (gpio.isPressed(HalGPIO::BTN_POWER) && gpio.getHeldTime() > SETTINGS.getPowerButtonDuration()) {
    // If the screenshot combination is potentially being pressed, don't sleep
    if (gpio.isPressed(HalGPIO::BTN_DOWN)) {
      return;
    }
    enterDeepSleep();
    // This should never be hit as `enterDeepSleep` calls esp_deep_sleep_start
    return;
  }

  const unsigned long activityStartTime = millis();
  activityManager.loop();
  const unsigned long activityDuration = millis() - activityStartTime;

  const unsigned long loopDuration = millis() - loopStartTime;
  if (loopDuration > maxLoopDuration) {
    maxLoopDuration = loopDuration;
    if (maxLoopDuration > 50) {
      LOG_DBG("LOOP", "New max loop duration: %lu ms (activity: %lu ms)", maxLoopDuration, activityDuration);
    }
  }

  // Add delay at the end of the loop to prevent tight spinning
  // When an activity requests skip loop delay (e.g., webserver running), use yield() for faster response
  // Otherwise, use longer delay to save power
  if (activityManager.skipLoopDelay()) {
    powerManager.setPowerSaving(false);  // Make sure we're at full performance when skipLoopDelay is requested
    yield();                             // Give FreeRTOS a chance to run tasks, but return immediately
  } else {
    if (millis() - lastActivityTime >= HalPowerManager::IDLE_POWER_SAVING_MS) {
      // If we've been inactive for a while, increase the delay to save power
      powerManager.setPowerSaving(true);  // Lower CPU frequency after extended inactivity
      delay(50);
    } else {
      // Short delay to prevent tight loop while still being responsive
      delay(10);
    }
  }
}

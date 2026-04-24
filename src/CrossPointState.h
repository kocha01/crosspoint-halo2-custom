#pragma once
#include <cstdint>
#include <iosfwd>
#include <string>

class CrossPointState {
  // Static instance
  static CrossPointState instance;

 public:
  std::string openEpubPath;
  uint8_t lastSleepImage = UINT8_MAX;  // UINT8_MAX = unset sentinel
  uint8_t readerActivityLoadCount = 0;
  bool lastSleepFromReader = false;

  // --- Runtime-only update-check fields (not persisted) ---
  // Written by UpdateCheckTask background task, read by HomeActivity.
  // Simple bool/string reads are safe without mutex on single-core ESP32-C3
  // because the task only flips `updateAvailable` from false -> true once,
  // after `latestVersion` has been written.
  bool updateCheckStarted = false;    // Guards against launching task twice per boot
  bool updateCheckFinished = false;   // Check completed (either outcome)
  bool updateAvailable = false;       // Newer version found — show popup
  bool updateDismissed = false;       // User tapped "Later" this session
  std::string latestVersion;          // Valid only when updateAvailable == true
  ~CrossPointState() = default;

  // Get singleton instance
  static CrossPointState& getInstance() { return instance; }

  bool saveToFile() const;

  bool loadFromFile();

 private:
  bool loadFromBinaryFile();
};

// Helper macro to access settings
#define APP_STATE CrossPointState::getInstance()

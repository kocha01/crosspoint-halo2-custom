#pragma once

#include <functional>

#include "activities/Activity.h"

class ClearCacheActivity final : public Activity {
 public:
  enum class Mode { ReadingCache, RefreshRecents, ClearRecents };

  explicit ClearCacheActivity(GfxRenderer& renderer, MappedInputManager& mappedInput,
                              const Mode mode = Mode::ReadingCache)
      : Activity("ClearCache", renderer, mappedInput), mode(mode) {}

  void onEnter() override;
  void onExit() override;
  void loop() override;
  bool skipLoopDelay() override { return true; }  // Prevent power-saving mode
  void render(RenderLock&&) override;

 private:
  enum State { WARNING, CLEARING, SUCCESS, FAILED };

  Mode mode = Mode::ReadingCache;
  State state = WARNING;

  void goBack() { finish(); }

  int clearedCount = 0;
  int failedCount = 0;
  int removedRecentCount = 0;
  void clearCache();
};

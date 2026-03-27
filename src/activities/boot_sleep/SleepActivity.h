#pragma once
#include "../Activity.h"

class Bitmap;
struct RecentBook;

class SleepActivity final : public Activity {
 public:
  explicit SleepActivity(GfxRenderer& renderer, MappedInputManager& mappedInput)
      : Activity("Sleep", renderer, mappedInput) {}
  void onEnter() override;

 private:
  void renderDefaultSleepScreen() const;
  void renderCustomSleepScreen() const;
  void renderCoverSleepScreen() const;
  void renderBitmapSleepScreen(const Bitmap& bitmap) const;
  void renderHalo2SleepScreen(const Bitmap& bitmap, const RecentBook& book) const;
  RecentBook getCurrentSleepBookData() const;
  void renderBlankSleepScreen() const;
};

#pragma once

#include <string>
#include <vector>

#include "../Activity.h"
#include "util/ButtonNavigator.h"

class SleepWallpaperPickerActivity final : public Activity {
 public:
  explicit SleepWallpaperPickerActivity(GfxRenderer& renderer, MappedInputManager& mappedInput)
      : Activity("SleepWallpaperPicker", renderer, mappedInput) {}

  void onEnter() override;
  void onExit() override;
  void loop() override;
  void render(RenderLock&&) override;

 private:
  void loadWallpapers();
  void handleSelection();
  void onBack();

  ButtonNavigator buttonNavigator;
  std::vector<std::string> files;
  int selectedIndex = 0;
};

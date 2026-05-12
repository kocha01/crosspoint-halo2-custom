#pragma once

#include <string>
#include <vector>

#include "../Activity.h"
#include "util/ButtonNavigator.h"

/// List picker for SD card font families discovered under /.crosspoint/fonts/.
/// Row 0 is always "(Built-in)" — selecting it clears SETTINGS.sdFontFamilyName so
/// the reader falls back to the built-in fontFamily enum. Rows 1..N are the
/// discovered families in alphabetical order.  Confirm writes the chosen name
/// to settings, calls ensureSdFontLoaded() to load+register the family with the
/// renderer, then finishes.
class SdCardFontPickerActivity final : public Activity {
 public:
  explicit SdCardFontPickerActivity(GfxRenderer& renderer, MappedInputManager& mappedInput)
      : Activity("SdCardFontPicker", renderer, mappedInput) {}

  void onEnter() override;
  void onExit() override;
  void loop() override;
  void render(RenderLock&&) override;

 private:
  void loadFamilies();
  void handleSelection();
  void onBack();

  ButtonNavigator buttonNavigator;
  std::vector<std::string> families;  // populated from sdFontSystem.registry()
  int selectedIndex = 0;
};

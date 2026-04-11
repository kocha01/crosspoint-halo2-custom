#pragma once
#include <Epub.h>
#include <GfxRenderer.h>
#include <I18n.h>

#include <string>
#include <vector>

#include "../Activity.h"
#include "util/ButtonNavigator.h"

class EpubReaderMenuActivity final : public Activity {
 public:
  // Menu actions available from the reader menu.
  enum class MenuAction {
    SELECT_CHAPTER,
    FOOTNOTES,
    FONT_FAMILY,
    FONT_SIZE,
    LINE_SPACING,
    SCREEN_MARGIN,
    CUSTOMISE_STATUS_BAR,
    GO_TO_PERCENT,
    AUTO_PAGE_TURN,
    ROTATE_SCREEN,
    DARK_MODE,
    SCREENSHOT,
    DISPLAY_QR,
    GO_HOME,
    SYNC,
    DELETE_CACHE,
    BACKGROUND_TEXTURE,
    BOLD_TEXT,
    THAI_DICTIONARY
  };

  // Two modes: quick settings (default) and full menu (More)
  enum class Mode : uint8_t { QUICK, FULL_MENU };

  // Background texture styles
  enum class Texture : uint8_t {
    DOT_GRID = 0,
    DARK,
    TEXTURE_COUNT
  };

  explicit EpubReaderMenuActivity(GfxRenderer& renderer, MappedInputManager& mappedInput, const std::string& title,
                                  const int currentPage, const int totalPages, const int bookProgressPercent,
                                  const uint8_t currentOrientation, const bool hasFootnotes,
                                  const uint8_t currentFontFamily, const uint8_t currentFontSize,
                                  const uint8_t currentLineSpacing, const uint8_t currentScreenMargin);

  void onEnter() override;
  void onExit() override;
  void loop() override;
  void render(RenderLock&&) override;

 private:
  struct MenuItem {
    MenuAction action;
    StrId labelId;
  };

  static std::vector<MenuItem> buildFullMenuItems(bool hasFootnotes);

  // Quick settings rendering
  void renderQuickSettings(int x, int y, int w, int h) const;
  // Full menu rendering
  void renderFullMenu(int x, int y, int w, int h) const;
  // Get display value for a menu action
  const char* getItemValue(MenuAction action) const;

  Mode currentMode = Mode::QUICK;
  const std::vector<MenuItem> fullMenuItems;
  int fullMenuSelectedIndex = 0;

  ButtonNavigator buttonNavigator;
  std::string title = "Reader Menu";
  uint8_t pendingOrientation = 0;
  uint8_t selectedPageTurnOption = 0;
  uint8_t pendingFontFamily = 0;
  uint8_t pendingFontSize = 0;
  uint8_t pendingLineSpacing = 0;
  uint8_t pendingScreenMargin = 0;
  const std::vector<StrId> orientationLabels = {StrId::STR_PORTRAIT, StrId::STR_LANDSCAPE_CW, StrId::STR_INVERTED,
                                                StrId::STR_LANDSCAPE_CCW};
  const std::vector<const char*> pageTurnLabels = {I18N.get(StrId::STR_STATE_OFF), "1", "3", "6", "12"};
  int currentPage = 0;
  int totalPages = 0;
  int bookProgressPercent = 0;
  int8_t activeArrow = 0;           // 0=none, 1=up pressed, -1=down pressed
  unsigned long arrowPressTime = 0; // millis() when arrow was pressed
  bool arrowResetPending = false;   // waiting to reset arrow to gray
  Texture currentTexture = Texture::DOT_GRID;
  GfxRenderer::Orientation savedOrientation = GfxRenderer::Orientation::Portrait;
};

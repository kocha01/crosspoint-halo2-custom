#pragma once

#include "components/themes/BaseTheme.h"

class GfxRenderer;

// Modern theme metrics — stacked cover carousel, 2x2 menu grid, DarkGray pill selection
namespace ModernMetrics {
// Mockup: 480×800 e-paper at 50% CSS scale (HTML values ×2 = actual pixels)
constexpr ThemeMetrics values = {.batteryWidth = 16,
                                 .batteryHeight = 12,
                                 .topPadding = 5,
                                 .batteryBarHeight = 28,   // Mockup: status-bar 14px CSS → 28
                                 .headerHeight = 82,       // ~3mm more than before
                                 .verticalSpacing = 14,    // Match visual spacing between list rows
                                 .contentSidePadding = 28, // Mockup: 14px CSS → 28
                                 .listRowHeight = 52,
                                 .listWithSubtitleRowHeight = 68,
                                 .menuRowHeight = 72,
                                 .menuSpacing = 10,        // Mockup: 5px CSS → 10
                                 .tabSpacing = 6,
                                 .tabBarHeight = 40,
                                 .scrollBarWidth = 3,
                                 .scrollBarRightOffset = 5,
                                 .homeTopPadding = 28,     // Right after battery bar
                                 .homeCoverHeight = 260,   // Mockup: center 130px CSS → 260
                                 .homeSideCoverHeight = 200,  // Side covers: 70% detail (100×200)
                                 .homeFarCoverHeight = 150,   // Far covers: 50% detail (75×150)
                                 .homeCoverTileHeight = 460,// Covers + title + author + progress + dots + divider
                                 .homeRecentBooksCount = 5,
                                 .buttonHintsHeight = 30,  // Mockup: ~15px CSS → 30
                                 .sideButtonHintsWidth = 24,// Mockup: ~12px CSS → 24 (narrower)
                                 .progressBarHeight = 16,
                                 .progressBarMarginTop = 1,
                                 .statusBarHorizontalMargin = 5,
                                 .statusBarVerticalMargin = 19,
                                 .keyboardKeyWidth = 31,
                                 .keyboardKeyHeight = 50,
                                 .keyboardKeySpacing = 0,
                                 .keyboardBottomAligned = true,
                                 .keyboardCenteredText = true};
}

class ModernTheme : public BaseTheme {
 public:
  void drawBatteryLeft(const GfxRenderer& renderer, Rect rect, bool showPercentage = true) const override;
  void drawBatteryRight(const GfxRenderer& renderer, Rect rect, bool showPercentage = true) const override;
  void drawHeader(const GfxRenderer& renderer, Rect rect, const char* title, const char* subtitle) const override;
  void drawSubHeader(const GfxRenderer& renderer, Rect rect, const char* label,
                     const char* rightLabel = nullptr) const override;
  void drawTabBar(const GfxRenderer& renderer, Rect rect, const std::vector<TabInfo>& tabs,
                  bool selected) const override;
  void drawList(const GfxRenderer& renderer, Rect rect, int itemCount, int selectedIndex,
                const std::function<std::string(int index)>& rowTitle,
                const std::function<std::string(int index)>& rowSubtitle,
                const std::function<UIIcon(int index)>& rowIcon, const std::function<std::string(int index)>& rowValue,
                bool highlightValue) const override;
  void drawButtonHints(GfxRenderer& renderer, const char* btn1, const char* btn2, const char* btn3,
                       const char* btn4) const override;
  void drawSideButtonHints(const GfxRenderer& renderer, const char* topBtn, const char* bottomBtn) const override;
  void drawButtonMenu(GfxRenderer& renderer, Rect rect, int buttonCount, int selectedIndex,
                      const std::function<std::string(int index)>& buttonLabel,
                      const std::function<UIIcon(int index)>& rowIcon) const override;
  void drawRecentBookCover(GfxRenderer& renderer, Rect rect, const std::vector<RecentBook>& recentBooks,
                           const int selectorIndex, bool& coverRendered, bool& coverBufferStored, bool& bufferRestored,
                           std::function<bool()> storeCoverBuffer) const override;
  Rect drawPopup(const GfxRenderer& renderer, const char* message) const override;
  void fillPopupProgress(const GfxRenderer& renderer, const Rect& layout, const int progress) const override;
  void drawTextField(const GfxRenderer& renderer, Rect rect, const int textWidth) const override;
  void drawKeyboardKey(const GfxRenderer& renderer, Rect rect, const char* label, const bool isSelected) const override;

 private:
  void drawEmptyRecents(const GfxRenderer& renderer, Rect rect) const;
};

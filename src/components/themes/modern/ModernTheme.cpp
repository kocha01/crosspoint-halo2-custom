#include "ModernTheme.h"

#include <algorithm>
#include <GfxRenderer.h>
#include <HalPowerManager.h>
#include <HalStorage.h>
#include <I18n.h>

#include <cstdint>
#include <string>
#include <vector>

#include "CrossPointSettings.h"
#include "RecentBooksStore.h"
#include "components/UITheme.h"
#include "components/icons/book.h"
#include "components/icons/book24.h"
#include "components/icons/cover.h"
#include "components/icons/file24.h"
#include "components/icons/folder.h"
#include "components/icons/folder24.h"
#include "components/icons/hotspot.h"
#include "components/icons/image24.h"
#include "components/icons/library.h"
#include "components/icons/recent.h"
#include "components/icons/settings2.h"
#include "components/icons/text24.h"
#include "components/icons/transfer.h"
#include "components/icons/wifi.h"
#include "fontIds.h"

namespace {
constexpr int batteryPercentSpacing = 4;
constexpr int hPaddingInSelection = 8;
constexpr int cornerRadius = 6;
constexpr int topHintButtonY = 345;
constexpr int popupMarginX = 20;
constexpr int popupMarginY = 14;
constexpr int maxSubtitleWidth = 100;
constexpr int maxListValueWidth = 200;
constexpr int mainMenuIconSize = 32;
constexpr int listIconSize = 24;
constexpr int mainMenuColumns = 2;
constexpr int centerCoverSourceH = 260;
constexpr int sideCoverSourceH = 200;
constexpr int farCoverSourceH = 150;
int coverWidth = 0;

const uint8_t* iconForName(UIIcon icon, int size) {
  if (size == 24) {
    switch (icon) {
      case UIIcon::Folder:
        return Folder24Icon;
      case UIIcon::Text:
        return Text24Icon;
      case UIIcon::Image:
        return Image24Icon;
      case UIIcon::Book:
        return Book24Icon;
      case UIIcon::File:
        return File24Icon;
      default:
        return nullptr;
    }
  } else if (size == 32) {
    switch (icon) {
      case UIIcon::Folder:
        return FolderIcon;
      case UIIcon::Book:
        return BookIcon;
      case UIIcon::Recent:
        return RecentIcon;
      case UIIcon::Settings:
        return Settings2Icon;
      case UIIcon::Transfer:
        return TransferIcon;
      case UIIcon::Library:
        return LibraryIcon;
      case UIIcon::Wifi:
        return WifiIcon;
      case UIIcon::Hotspot:
        return HotspotIcon;
      default:
        return nullptr;
    }
  }
  return nullptr;
}
}  // namespace

// ============================================================
// Battery
// ============================================================

void ModernTheme::drawBatteryLeft(const GfxRenderer& renderer, Rect rect, const bool showPercentage) const {
  const uint16_t percentage = powerManager.getBatteryPercentage();
  const int y = rect.y + 6;
  const int battWidth = ModernMetrics::values.batteryWidth;

  if (showPercentage) {
    const auto percentageText = std::to_string(percentage) + "%";
    renderer.drawText(SMALL_FONT_ID, rect.x + batteryPercentSpacing + battWidth, rect.y, percentageText.c_str(), true,
                      EpdFontFamily::BOLD);
  }

  const int x = rect.x;
  renderer.drawLine(x + 1, y, x + battWidth - 3, y);
  renderer.drawLine(x + 1, y + rect.height - 1, x + battWidth - 3, y + rect.height - 1);
  renderer.drawLine(x, y + 1, x, y + rect.height - 2);
  renderer.drawLine(x + battWidth - 2, y + 1, x + battWidth - 2, y + rect.height - 2);
  renderer.drawPixel(x + battWidth - 1, y + 3);
  renderer.drawPixel(x + battWidth - 1, y + rect.height - 4);
  renderer.drawLine(x + battWidth - 0, y + 4, x + battWidth - 0, y + rect.height - 5);

  if (percentage > 10) {
    renderer.fillRect(x + 2, y + 2, 3, rect.height - 4);
  }
  if (percentage > 40) {
    renderer.fillRect(x + 6, y + 2, 3, rect.height - 4);
  }
  if (percentage > 70) {
    renderer.fillRect(x + 10, y + 2, 3, rect.height - 4);
  }
}

void ModernTheme::drawBatteryRight(const GfxRenderer& renderer, Rect rect, const bool showPercentage) const {
  const uint16_t percentage = powerManager.getBatteryPercentage();
  const int y = rect.y + 6;
  const int battWidth = ModernMetrics::values.batteryWidth;

  if (showPercentage) {
    const auto percentageText = std::to_string(percentage) + "%";
    const int textWidth = renderer.getTextWidth(SMALL_FONT_ID, percentageText.c_str(), EpdFontFamily::BOLD);
    const auto textHeight = renderer.getTextHeight(SMALL_FONT_ID);
    renderer.fillRect(rect.x - textWidth - batteryPercentSpacing, rect.y, textWidth, textHeight, false);
    renderer.drawText(SMALL_FONT_ID, rect.x - textWidth - batteryPercentSpacing, rect.y, percentageText.c_str(), true,
                      EpdFontFamily::BOLD);
  }

  const int x = rect.x;
  renderer.drawLine(x + 1, y, x + battWidth - 3, y);
  renderer.drawLine(x + 1, y + rect.height - 1, x + battWidth - 3, y + rect.height - 1);
  renderer.drawLine(x, y + 1, x, y + rect.height - 2);
  renderer.drawLine(x + battWidth - 2, y + 1, x + battWidth - 2, y + rect.height - 2);
  renderer.drawPixel(x + battWidth - 1, y + 3);
  renderer.drawPixel(x + battWidth - 1, y + rect.height - 4);
  renderer.drawLine(x + battWidth - 0, y + 4, x + battWidth - 0, y + rect.height - 5);

  if (percentage > 10) {
    renderer.fillRect(x + 2, y + 2, 3, rect.height - 4);
  }
  if (percentage > 40) {
    renderer.fillRect(x + 6, y + 2, 3, rect.height - 4);
  }
  if (percentage > 70) {
    renderer.fillRect(x + 10, y + 2, 3, rect.height - 4);
  }
}

// ============================================================
// Header — Slim, left-aligned title, 1px separator
// ============================================================

void ModernTheme::drawHeader(const GfxRenderer& renderer, Rect rect, const char* title, const char* subtitle) const {
  renderer.fillRect(rect.x, rect.y, rect.width, rect.height, false);

  const bool showBatteryPercentage =
      SETTINGS.hideBatteryPercentage != CrossPointSettings::HIDE_BATTERY_PERCENTAGE::HIDE_ALWAYS;
  const int batteryX = rect.x + rect.width - 12 - ModernMetrics::values.batteryWidth;
  drawBatteryRight(renderer,
                   Rect{batteryX, rect.y + 5, ModernMetrics::values.batteryWidth, ModernMetrics::values.batteryHeight},
                   showBatteryPercentage);

  int maxTitleWidth =
      rect.width - ModernMetrics::values.contentSidePadding * 2 - (subtitle != nullptr ? maxSubtitleWidth : 0);

  if (title) {
    auto truncatedTitle = renderer.truncatedText(UI_12_FONT_ID, title, maxTitleWidth, EpdFontFamily::BOLD);
    renderer.drawText(UI_12_FONT_ID, rect.x + ModernMetrics::values.contentSidePadding,
                      rect.y + ModernMetrics::values.batteryBarHeight + 3, truncatedTitle.c_str(), true,
                      EpdFontFamily::BOLD);
    // Thin 1px separator instead of thick 3px line
    renderer.drawLine(rect.x, rect.y + rect.height - 1, rect.x + rect.width - 1, rect.y + rect.height - 1, true);
  }

  if (subtitle) {
    auto truncatedSubtitle = renderer.truncatedText(SMALL_FONT_ID, subtitle, maxSubtitleWidth, EpdFontFamily::REGULAR);
    int truncatedSubtitleWidth = renderer.getTextWidth(SMALL_FONT_ID, truncatedSubtitle.c_str());
    renderer.drawText(SMALL_FONT_ID,
                      rect.x + rect.width - ModernMetrics::values.contentSidePadding - truncatedSubtitleWidth,
                      rect.y + ModernMetrics::values.batteryBarHeight + 6, truncatedSubtitle.c_str(), true);
  }
}

// ============================================================
// SubHeader
// ============================================================

void ModernTheme::drawSubHeader(const GfxRenderer& renderer, Rect rect, const char* label,
                                const char* rightLabel) const {
  int currentX = rect.x + ModernMetrics::values.contentSidePadding;
  int rightSpace = ModernMetrics::values.contentSidePadding;
  if (rightLabel) {
    auto truncatedRightLabel =
        renderer.truncatedText(SMALL_FONT_ID, rightLabel, maxListValueWidth, EpdFontFamily::REGULAR);
    int rightLabelWidth = renderer.getTextWidth(SMALL_FONT_ID, truncatedRightLabel.c_str());
    renderer.drawText(SMALL_FONT_ID, rect.x + rect.width - ModernMetrics::values.contentSidePadding - rightLabelWidth,
                      rect.y + 7, truncatedRightLabel.c_str());
    rightSpace += rightLabelWidth + hPaddingInSelection;
  }

  auto truncatedLabel = renderer.truncatedText(UI_10_FONT_ID, label,
                                               rect.width - ModernMetrics::values.contentSidePadding - rightSpace);
  renderer.drawText(UI_10_FONT_ID, currentX, rect.y + 6, truncatedLabel.c_str(), true, EpdFontFamily::REGULAR);
  renderer.drawLine(rect.x, rect.y + rect.height - 1, rect.x + rect.width - 1, rect.y + rect.height - 1, true);
}

// ============================================================
// Tab Bar — Pill-based selection (no underline)
// ============================================================

void ModernTheme::drawTabBar(const GfxRenderer& renderer, Rect rect, const std::vector<TabInfo>& tabs,
                             bool selected) const {
  int currentX = rect.x + ModernMetrics::values.contentSidePadding;
  const int lineHeight = renderer.getLineHeight(UI_10_FONT_ID);
  constexpr int pillPadX = hPaddingInSelection + 5;
  constexpr int pillInsetTop = 1;
  constexpr int pillInsetBottom = 1;
  const int textY = rect.y + (rect.height - lineHeight) / 2 + 1;

  for (const auto& tab : tabs) {
    const int textWidth = renderer.getTextWidth(UI_10_FONT_ID, tab.label, EpdFontFamily::REGULAR);
    const int pillWidth = textWidth + 2 * pillPadX;

    if (tab.selected) {
      if (selected) {
        // Focused + selected: Black pill
        renderer.fillRoundedRect(currentX, rect.y + pillInsetTop, pillWidth, rect.height - pillInsetTop - pillInsetBottom,
                                 cornerRadius, Color::Black);
      } else {
        // Not focused but selected: Black pill
        renderer.fillRoundedRect(currentX, rect.y + pillInsetTop, pillWidth, rect.height - pillInsetTop - pillInsetBottom,
                                 cornerRadius, Color::Black);
      }
    }

    renderer.drawText(UI_10_FONT_ID, currentX + pillPadX, textY, tab.label, !tab.selected,
                      EpdFontFamily::REGULAR);

    currentX += pillWidth + ModernMetrics::values.tabSpacing;
  }

  // Thin separator at bottom
  renderer.drawLine(rect.x, rect.y + rect.height - 1, rect.x + rect.width - 1, rect.y + rect.height - 1, true);
}

// ============================================================
// List — DarkGray pill selection, thin separators
// ============================================================

void ModernTheme::drawList(const GfxRenderer& renderer, Rect rect, int itemCount, int selectedIndex,
                           const std::function<std::string(int index)>& rowTitle,
                           const std::function<std::string(int index)>& rowSubtitle,
                           const std::function<UIIcon(int index)>& rowIcon,
                           const std::function<std::string(int index)>& rowValue, bool highlightValue) const {
  int rowHeight =
      (rowSubtitle != nullptr) ? ModernMetrics::values.listWithSubtitleRowHeight : ModernMetrics::values.listRowHeight;
  int pageItems = rect.height / rowHeight;

  const int totalPages = (itemCount + pageItems - 1) / pageItems;
  if (totalPages > 1) {
    const int scrollAreaHeight = rect.height;
    const int scrollBarHeight = (scrollAreaHeight * pageItems) / itemCount;
    const int currentPage = selectedIndex / pageItems;
    const int scrollBarY = rect.y + ((scrollAreaHeight - scrollBarHeight) * currentPage) / (totalPages - 1);
    const int scrollBarX = rect.x + rect.width - ModernMetrics::values.scrollBarRightOffset;
    renderer.drawLine(scrollBarX, rect.y, scrollBarX, rect.y + scrollAreaHeight, true);
    renderer.fillRect(scrollBarX - ModernMetrics::values.scrollBarWidth, scrollBarY,
                      ModernMetrics::values.scrollBarWidth, scrollBarHeight, true);
  }

  int contentWidth =
      rect.width -
      (totalPages > 1 ? (ModernMetrics::values.scrollBarWidth + ModernMetrics::values.scrollBarRightOffset) : 1);

  // Black pill selection
  if (selectedIndex >= 0) {
    renderer.fillRoundedRect(ModernMetrics::values.contentSidePadding, rect.y + selectedIndex % pageItems * rowHeight,
                             contentWidth - ModernMetrics::values.contentSidePadding * 2, rowHeight, cornerRadius,
                             Color::Black);
  }

  int textX = rect.x + ModernMetrics::values.contentSidePadding + hPaddingInSelection;
  int textWidth = contentWidth - ModernMetrics::values.contentSidePadding * 2 - hPaddingInSelection * 2;
  int iconSize = 0;
  if (rowIcon != nullptr) {
    iconSize = (rowSubtitle != nullptr) ? mainMenuIconSize : listIconSize;
    textX += iconSize + hPaddingInSelection;
    textWidth -= iconSize + hPaddingInSelection;
  }

  const auto pageStartIndex = selectedIndex / pageItems * pageItems;
  const int titleLineHeight = renderer.getLineHeight(UI_10_FONT_ID);
  const int subtitleLineHeight = renderer.getLineHeight(SMALL_FONT_ID);
  const int subtitleGap = 4;
  for (int i = pageStartIndex; i < itemCount && i < pageStartIndex + pageItems; i++) {
    const int itemY = rect.y + (i % pageItems) * rowHeight;
    const bool isSelected = (i == selectedIndex);
    int rowTextWidth = textWidth;
    const bool hasSubtitle = rowSubtitle != nullptr;
    const int textBlockHeight = hasSubtitle ? (titleLineHeight + subtitleGap + subtitleLineHeight) : titleLineHeight;
    const int textTop = itemY + std::max(0, (rowHeight - textBlockHeight) / 2);
    const int titleY = textTop;
    const int subtitleY = textTop + titleLineHeight + subtitleGap;
    const int iconY = itemY + std::max(0, (rowHeight - iconSize) / 2);

    // Draw separator line (except for selected item and item after selected)
    if (!isSelected && i != pageStartIndex && (i - 1) != selectedIndex) {
      renderer.drawLine(rect.x + ModernMetrics::values.contentSidePadding + hPaddingInSelection, itemY,
                        rect.x + contentWidth - ModernMetrics::values.contentSidePadding - hPaddingInSelection, itemY,
                        true);
    }

    int valueWidth = 0;
    std::string valueText = "";
    if (rowValue != nullptr) {
      valueText = rowValue(i);
      valueText = renderer.truncatedText(UI_10_FONT_ID, valueText.c_str(), maxListValueWidth);
      valueWidth = renderer.getTextWidth(UI_10_FONT_ID, valueText.c_str()) + hPaddingInSelection;
      rowTextWidth -= valueWidth;
    }

    auto itemName = rowTitle(i);
    auto item = renderer.truncatedText(UI_10_FONT_ID, itemName.c_str(), rowTextWidth);
    renderer.drawText(UI_10_FONT_ID, textX, titleY, item.c_str(), !isSelected);

    if (rowIcon != nullptr) {
      UIIcon icon = rowIcon(i);
      const uint8_t* iconBitmap = iconForName(icon, iconSize);
      if (iconBitmap != nullptr) {
        const int iconX = rect.x + ModernMetrics::values.contentSidePadding + hPaddingInSelection;
        if (isSelected) {
          renderer.drawIconInverted(iconBitmap, iconX, iconY, iconSize, iconSize);
        } else {
          renderer.drawIcon(iconBitmap, iconX, iconY, iconSize, iconSize);
        }
      }
    }

    if (hasSubtitle) {
      std::string subtitleText = rowSubtitle(i);
      auto subtitle = renderer.truncatedText(SMALL_FONT_ID, subtitleText.c_str(), rowTextWidth);
      renderer.drawText(SMALL_FONT_ID, textX, subtitleY, subtitle.c_str(), !isSelected);
    }

    if (!valueText.empty()) {
      if (isSelected && highlightValue) {
        renderer.fillRoundedRect(
            contentWidth - ModernMetrics::values.contentSidePadding - hPaddingInSelection - valueWidth, itemY,
            valueWidth + hPaddingInSelection, rowHeight, cornerRadius, Color::Black);
      }
      renderer.drawText(UI_10_FONT_ID, rect.x + contentWidth - ModernMetrics::values.contentSidePadding - valueWidth,
                        titleY, valueText.c_str(), !(isSelected && highlightValue));
    }
  }
}

// ============================================================
// Button Hints — Same layout as Lyra
// ============================================================

/// Draw a small left-pointing arrow (◀) centered at (cx, cy)
static void drawLeftArrow(const GfxRenderer& renderer, int cx, int cy, int size) {
  const int xPts[] = {cx - size / 2, cx + size / 2, cx + size / 2};
  const int yPts[] = {cy, cy - size / 2, cy + size / 2};
  renderer.fillPolygon(xPts, yPts, 3, true);
}

/// Draw a small right-pointing arrow (▶) centered at (cx, cy)
static void drawRightArrow(const GfxRenderer& renderer, int cx, int cy, int size) {
  const int xPts[] = {cx + size / 2, cx - size / 2, cx - size / 2};
  const int yPts[] = {cy, cy - size / 2, cy + size / 2};
  renderer.fillPolygon(xPts, yPts, 3, true);
}

/// Draw a small up-pointing arrow (▲) centered at (cx, cy)
static void drawUpArrow(const GfxRenderer& renderer, int cx, int cy, int size) {
  const int xPts[] = {cx, cx - size / 2, cx + size / 2};
  const int yPts[] = {cy - size / 2, cy + size / 2, cy + size / 2};
  renderer.fillPolygon(xPts, yPts, 3, true);
}

/// Draw a small down-pointing arrow (▼) centered at (cx, cy)
static void drawDownArrow(const GfxRenderer& renderer, int cx, int cy, int size) {
  const int xPts[] = {cx, cx - size / 2, cx + size / 2};
  const int yPts[] = {cy + size / 2, cy - size / 2, cy - size / 2};
  renderer.fillPolygon(xPts, yPts, 3, true);
}

/// Check if a label matches a directional hint (Left/Right/Up/Down in any language)
static bool isDirectionLabel(const char* label, const char* dirStr) {
  return label != nullptr && dirStr != nullptr && strcmp(label, dirStr) == 0;
}

void ModernTheme::drawButtonHints(GfxRenderer& renderer, const char* btn1, const char* btn2, const char* btn3,
                                  const char* btn4) const {
  const GfxRenderer::Orientation orig_orientation = renderer.getOrientation();
  renderer.setOrientation(GfxRenderer::Orientation::Portrait);

  const int pageHeight = renderer.getScreenHeight();
  const int screenWidth = renderer.getScreenWidth();

  // Mockup: .btn-hints { padding: 3px 8px; gap: 3px } → ×2 = 6px 16px, gap 6px
  constexpr int hPad = 16;       // Mockup: 8px CSS → 16px
  constexpr int vPad = 6;        // Mockup: 3px CSS → 6px
  constexpr int btnGap = 6;      // Mockup: 3px CSS → 6px
  constexpr int btnRadius = 6;   // Mockup: 3px CSS → 6px
  constexpr int areaHeight = ModernMetrics::values.buttonHintsHeight;
  constexpr int smallBtnH = 15;

  // Flex-like equal-width buttons: (screenWidth - 2*hPad - 3*gap) / 4
  const int availableW = screenWidth - hPad * 2;
  const int btnWidth = (availableW - btnGap * 3) / 4;
  const int btnHeight = areaHeight - vPad * 2;
  const int areaTop = pageHeight - areaHeight;

  const char* labels[] = {btn1, btn2, btn3, btn4};
  const char* dirLeft = tr(STR_DIR_LEFT);
  const char* dirRight = tr(STR_DIR_RIGHT);

  // 1px separator at top of hints area (mockup: border-top: 0.5px → 1px)
  renderer.drawLine(0, areaTop, screenWidth - 1, areaTop, true);

  for (int i = 0; i < 4; i++) {
    const int x = hPad + i * (btnWidth + btnGap);
    const int btnTop = areaTop + vPad;

    if (labels[i] != nullptr && labels[i][0] != '\0') {
      renderer.fillRoundedRect(x, btnTop, btnWidth, btnHeight, btnRadius, Color::White);
      renderer.drawRoundedRect(x, btnTop, btnWidth, btnHeight, 1, btnRadius, true, true, false, false, true);

      // Replace directional text with arrow icons
      if (isDirectionLabel(labels[i], dirLeft)) {
        drawLeftArrow(renderer, x + btnWidth / 2, btnTop + btnHeight / 2, 10);
      } else if (isDirectionLabel(labels[i], dirRight)) {
        drawRightArrow(renderer, x + btnWidth / 2, btnTop + btnHeight / 2, 10);
      } else {
        const int textWidth = renderer.getTextWidth(SMALL_FONT_ID, labels[i], EpdFontFamily::BOLD);
        const int textX = x + (btnWidth - textWidth) / 2;
        const int textY = btnTop + (btnHeight - renderer.getLineHeight(SMALL_FONT_ID)) / 2;
        renderer.drawText(SMALL_FONT_ID, textX, textY, labels[i], true, EpdFontFamily::BOLD);
      }
    } else {
      // Empty button: smaller stub at bottom
      renderer.fillRoundedRect(x, pageHeight - smallBtnH, btnWidth, smallBtnH, btnRadius, Color::White);
      renderer.drawRoundedRect(x, pageHeight - smallBtnH, btnWidth, smallBtnH, 1, btnRadius, true, true, false, false,
                               true);
    }
  }

  renderer.setOrientation(orig_orientation);
}

// ============================================================
// Side Button Hints
// ============================================================

void ModernTheme::drawSideButtonHints(const GfxRenderer& renderer, const char* topBtn, const char* bottomBtn) const {
  // Modern theme: no side button hints — cleaner look
  (void)renderer;
  (void)topBtn;
  (void)bottomBtn;
}

// ============================================================
// Button Menu — 2x2 grid with icons, Black pill for selection
// ============================================================

void ModernTheme::drawButtonMenu(GfxRenderer& renderer, Rect rect, int buttonCount, int selectedIndex,
                                 const std::function<std::string(int index)>& buttonLabel,
                                 const std::function<UIIcon(int index)>& rowIcon) const {
  const int sidePadding = ModernMetrics::values.contentSidePadding;
  const int totalWidth = rect.width - sidePadding * 2;
  const int colGap = ModernMetrics::values.menuSpacing;
  const int rowGap = ModernMetrics::values.menuSpacing;
  const int colWidth = (totalWidth - colGap) / mainMenuColumns;

  // Calculate number of rows and expand row height to fill available space (capped)
  const int rows = (buttonCount + mainMenuColumns - 1) / mainMenuColumns;
  const int availablePerRow = (rect.height - (rows - 1) * rowGap) / rows;
  // Expand up to 1.5x the base menu row height to fill space without looking sparse
  constexpr int maxRowHeight = ModernMetrics::values.menuRowHeight * 3 / 2;
  const int rowHeight = std::min(std::max(availablePerRow, static_cast<int>(ModernMetrics::values.menuRowHeight)),
                                 maxRowHeight);

  // Center grid vertically within the available rect
  const int totalGridHeight = rows * rowHeight + (rows - 1) * rowGap;
  const int yOffset = (rect.height - totalGridHeight) / 2;

  for (int i = 0; i < buttonCount; ++i) {
    int col = i % mainMenuColumns;
    int row = i / mainMenuColumns;
    int tileX = rect.x + sidePadding + col * (colWidth + colGap);
    int tileY = rect.y + yOffset + row * (rowHeight + rowGap);

    // selectedIndex can be negative when book is selected (selectorIndex < recentBooks.size())
    const bool selected = (selectedIndex >= 0 && selectedIndex == i);

    if (selected) {
      // BLACK fill for selected — matches mockup (white text on black)
      renderer.fillRoundedRect(tileX, tileY, colWidth, rowHeight, cornerRadius, Color::Black);
    } else {
      // Border for unselected — fill Black then white interior (2px border)
      renderer.fillRoundedRect(tileX, tileY, colWidth, rowHeight, cornerRadius, Color::Black);
      renderer.fillRoundedRect(tileX + 2, tileY + 2, colWidth - 4, rowHeight - 4, cornerRadius - 2, Color::White);
    }

    // Draw icon and label centered vertically as a unit within the cell.
    // Mockup: .menu-btn gap 3px CSS → 6px between icon and text
    const int textHeight = renderer.getLineHeight(UI_10_FONT_ID);
    constexpr int iconTextGap = 6;
    const int contentHeight = mainMenuIconSize + iconTextGap + textHeight;
    const int contentStartY = tileY + (rowHeight - contentHeight) / 2 + 10;

    int iconX = tileX + (colWidth - mainMenuIconSize) / 2;
    int iconY = contentStartY;
    if (rowIcon != nullptr) {
      UIIcon icon = rowIcon(i);
      const uint8_t* iconBitmap = iconForName(icon, mainMenuIconSize);
      if (iconBitmap != nullptr) {
        if (selected) {
          // White icon on black background
          renderer.drawIconInverted(iconBitmap, iconX, iconY, mainMenuIconSize, mainMenuIconSize);
        } else {
          renderer.drawIcon(iconBitmap, iconX, iconY, mainMenuIconSize, mainMenuIconSize);
        }
      }
    }

    // Draw label centered horizontally, below icon
    std::string labelStr = buttonLabel(i);
    const char* label = labelStr.c_str();
    const int textWidth = renderer.getTextWidth(UI_10_FONT_ID, label);
    const int textX = tileX + (colWidth - textWidth) / 2;
    const int textY = iconY + mainMenuIconSize + iconTextGap;
    renderer.drawText(UI_10_FONT_ID, textX, textY, label, !selected);
  }
}

// ============================================================
// Recent Book Cover — Stacked cover carousel
//
// Drawing order (back to front): far → side → CENTER
// This ensures center cover overlaps side covers visually.
// ============================================================

void ModernTheme::drawRecentBookCover(GfxRenderer& renderer, Rect rect, const std::vector<RecentBook>& recentBooks,
                                      const int selectorIndex, bool& coverRendered, bool& coverBufferStored,
                                      bool& bufferRestored, std::function<bool()> storeCoverBuffer) const {
  const bool hasContinueReading = !recentBooks.empty();
  if (!hasContinueReading) {
    drawEmptyRecents(renderer, rect);
    return;
  }

  const int bookCount = static_cast<int>(recentBooks.size());
  // Remember last selected book when switching to menu
  static int lastCenterIndex = 0;
  int centerIndex;
  if (selectorIndex < bookCount) {
    centerIndex = selectorIndex;
    lastCenterIndex = selectorIndex;
  } else {
    centerIndex = lastCenterIndex;
  }
  const bool bookIsSelected = (selectorIndex < bookCount);

  // --- Cover dimensions from mockup (CSS ×2) ---
  // Center: 90×130 CSS → 180×260 actual
  constexpr int centerCoverH = 260;
  constexpr int centerCoverW = 180;
  // Side: 70×100 CSS → 140×200 actual
  constexpr int sideCoverH = 200;
  constexpr int sideCoverW = 140;
  // Far: 50×75 CSS → 100×150 actual
  constexpr int farCoverH = 150;
  constexpr int farCoverW = 100;

  if (coverWidth == 0) {
    coverWidth = centerCoverW;
  }

  // --- Carousel area: covers are centered vertically ---
  // Reduced from 400 to 340 to leave room for title+author+progress+dots below
  constexpr int carouselH = 340;
  const int carouselY = rect.y;
  const int centerX = rect.x + (rect.width - centerCoverW) / 2;
  const int centerY = carouselY + (carouselH - centerCoverH) / 2;

  // Cover cards follow the real cover aspect ratio while keeping a consistent frame treatment.
  constexpr int coverBorder = 2;
  constexpr int coverCardRadius = cornerRadius;
  constexpr int coverCardOutline = 2;
  constexpr int coverContentInset = 2;
  constexpr int centerCoverHalo = 5;  // ~0.5mm visual halo on this display

  // When buffer is restored, all cover layers are already correct — skip to info section
  if (!bufferRestored) {
    auto displayWidthForBitmap = [&](const Bitmap& bmp, int targetH, int fallbackW, int maxW) {
      const int bmpW = bmp.getWidth();
      const int bmpH = bmp.getHeight();
      if (bmpW <= 0 || bmpH <= 0 || targetH <= 0) {
        return fallbackW;
      }

      const int scaledW = std::max(1, (bmpW * targetH + bmpH / 2) / bmpH);
      return std::clamp(scaledW, 1, maxW);
    };

    // --- Helper lambda: draw a rounded cover card that follows the cover's real aspect ratio ---
    auto drawCoverAt = [&](int idx, int centerAnchorX, int topY, int targetH, int fallbackW, int maxW, int thumbHeight,
                           int outlineWidth) {
      int displayW = fallbackW;
      bool hasCover = false;

      const std::string& coverPath = recentBooks[idx].coverBmpPath;
      if (!coverPath.empty()) {
        const std::string bmpPath = UITheme::getCoverThumbPath(coverPath, thumbHeight);
        FsFile file;
        if (Storage.openFileForRead("HOME", bmpPath, file)) {
          Bitmap bmp(file);
          if (bmp.parseHeaders() == BmpReaderError::Ok) {
            displayW = displayWidthForBitmap(bmp, targetH, fallbackW, maxW);
            hasCover = true;
          }
          file.close();
        }
      }

      const int cardX = centerAnchorX - displayW / 2 - coverBorder;
      const int cardY = topY - coverBorder;
      const int cardW = displayW + coverBorder * 2;
      const int cardH = targetH + coverBorder * 2;
      const int contentX = cardX + coverContentInset;
      const int contentY = cardY + coverContentInset;
      const int contentW = cardW - coverContentInset * 2;
      const int contentH = cardH - coverContentInset * 2;

      renderer.fillRoundedRect(cardX, cardY, cardW, cardH, coverCardRadius, Color::Black);
      renderer.fillRect(contentX, contentY, contentW, contentH, false);

      if (!coverPath.empty()) {
        const std::string bmpPath = UITheme::getCoverThumbPath(coverPath, thumbHeight);
        FsFile file;
        if (Storage.openFileForRead("HOME", bmpPath, file)) {
          Bitmap bmp(file);
          if (bmp.parseHeaders() == BmpReaderError::Ok) {
            renderer.drawBitmap(bmp, contentX, contentY, contentW, contentH, 0.0f, 0.0f);
            hasCover = true;
          }
          file.close();
        }
      }
      if (!hasCover) {
        renderer.fillRectDither(contentX, contentY, contentW, contentH, Color::LightGray);
        renderer.drawIcon(CoverIcon, contentX + (contentW - 24) / 2, contentY + contentH / 3, 24, 24);
      }
      renderer.drawRoundedRect(cardX, cardY, cardW, cardH, outlineWidth, coverCardRadius, true);
      return displayW;
    };

    // --- LAYER 1: Far covers (centerIndex ± 2) ---
    // Uses smaller thumbnails (150px) for faster SD I/O
    if (bookCount > 2) {
      const int farY = centerY + (centerCoverH - farCoverH) / 2;
      constexpr int farInset = 28;  // Near screen edge, matching contentSidePadding
      const int farLeftAnchorX = rect.x + farInset + farCoverW / 2;
      const int farRightAnchorX = rect.x + rect.width - farInset - farCoverW / 2;
      drawCoverAt((centerIndex - 2 + bookCount) % bookCount, farLeftAnchorX, farY, farCoverH, farCoverW, farCoverH,
                  farCoverSourceH, coverCardOutline);
      drawCoverAt((centerIndex + 2) % bookCount, farRightAnchorX, farY, farCoverH, farCoverW, farCoverH,
                  farCoverSourceH, coverCardOutline);
    }

    // --- LAYER 2: Side covers (centerIndex ± 1) ---
    // Uses medium thumbnails (200px) for balanced quality/speed
    if (bookCount > 1) {
      const int sideY = centerY + (centerCoverH - sideCoverH) / 2;
      constexpr int sideInset = 64;
      const int sideLeftAnchorX = rect.x + sideInset + sideCoverW / 2;
      const int sideRightAnchorX = rect.x + rect.width - sideInset - sideCoverW / 2;
      drawCoverAt((centerIndex - 1 + bookCount) % bookCount, sideLeftAnchorX, sideY, sideCoverH, sideCoverW,
                  sideCoverH, sideCoverSourceH, coverCardOutline);
      drawCoverAt((centerIndex + 1) % bookCount, sideRightAnchorX, sideY, sideCoverH, sideCoverW, sideCoverH,
                  sideCoverSourceH, coverCardOutline);
    }
  } // end !bufferRestored

  // --- LAYER 3: CENTER cover (on top, with white border) ---
  {
    const RecentBook& book = recentBooks[centerIndex];

    if (!coverRendered) {
      std::string coverPath = book.coverBmpPath;
      bool hasCover = false;
      int displayW = centerCoverW;

      if (!coverPath.empty()) {
        const std::string coverBmpPath = UITheme::getCoverThumbPath(coverPath, centerCoverSourceH);
        FsFile probeFile;
        if (Storage.openFileForRead("HOME", coverBmpPath, probeFile)) {
          Bitmap probeBitmap(probeFile);
          if (probeBitmap.parseHeaders() == BmpReaderError::Ok) {
            displayW = std::clamp(std::max(1, (probeBitmap.getWidth() * centerCoverH + probeBitmap.getHeight() / 2) /
                                                  probeBitmap.getHeight()),
                                  1, centerCoverH);
          }
          probeFile.close();
        }
      }

      const int cardX = rect.x + rect.width / 2 - displayW / 2 - coverBorder;
      const int cardY = centerY - coverBorder;
      const int cardW = displayW + coverBorder * 2;
      const int cardH = centerCoverH + coverBorder * 2;
      const int contentX = cardX + coverContentInset;
      const int contentY = cardY + coverContentInset;
      const int contentW = cardW - coverContentInset * 2;
      const int contentH = cardH - coverContentInset * 2;

      renderer.fillRoundedRect(cardX - centerCoverHalo, cardY - centerCoverHalo, cardW + centerCoverHalo * 2,
                               cardH + centerCoverHalo * 2, coverCardRadius + centerCoverHalo / 2, Color::White);
      renderer.fillRoundedRect(cardX, cardY, cardW, cardH, coverCardRadius, Color::Black);
      renderer.fillRect(contentX, contentY, contentW, contentH, false);

      if (!coverPath.empty()) {
        const std::string coverBmpPath = UITheme::getCoverThumbPath(coverPath, centerCoverSourceH);
        FsFile file;
        if (Storage.openFileForRead("HOME", coverBmpPath, file)) {
          Bitmap bitmap(file);
          if (bitmap.parseHeaders() == BmpReaderError::Ok) {
            renderer.drawBitmap(bitmap, contentX, contentY, contentW, contentH, 0.0f, 0.0f);
            coverWidth = displayW;
            hasCover = true;
          }
          file.close();
        }
      }

      if (!hasCover) {
        coverWidth = displayW;
        renderer.fillRect(contentX, contentY, contentW, contentH, true);
        renderer.drawIcon(CoverIcon, contentX + (contentW - 32) / 2, contentY + contentH / 3, 32, 32);
      }

      renderer.drawRoundedRect(cardX, cardY, cardW, cardH, bookIsSelected ? 3 : coverCardOutline, coverCardRadius,
                               true);

      coverBufferStored = storeCoverBuffer();
      coverRendered = coverBufferStored;
    }

    // --- Book info below covers: FIXED positions ---
    // All elements at fixed Y offsets so layout never shifts between books.
    // Info area = carouselH → rect.height (120px with carousel=340, tile=460)
    const int infoStart = carouselY + carouselH;
    const int textAreaWidth = rect.width - 40 * 2;
    const int textCenterX = rect.x + rect.width / 2;

    // Fixed Y offsets within info area (120px total):
    //  +4   title (1 line, truncated, UI_10 ~14px + Thai diacritics headroom)
    //  +32  author (1 line, truncated, SMALL ~10px)
    //  +76  progress bar
    //  +94  dots
    //  +119 divider
    constexpr int titleOffset = 4;
    constexpr int authorOffset = 32;
    constexpr int progressOffset = 76;
    constexpr int dotsOffset = 94;

    // When book is selected: black background for info area, inverted text/bar/dots
    const bool inverted = bookIsSelected;
    if (inverted) {
      // Black rounded pill behind title + author + progress + dots
      // Horizontal padding matches grid (contentSidePadding), vertical +1mm (~10px)
      constexpr int pillPadH = ModernMetrics::values.contentSidePadding;
      constexpr int pillPadV = 14;
      const int pillY = infoStart + titleOffset - pillPadV;
      const int pillH = dotsOffset + 6 + pillPadV - titleOffset + pillPadV;
      renderer.fillRoundedRect(rect.x + pillPadH, pillY, rect.width - pillPadH * 2, pillH, cornerRadius, Color::Black);
    }

    // Title: 1 line only, truncated, centered
    auto title = renderer.truncatedText(UI_10_FONT_ID, book.title.c_str(), textAreaWidth, EpdFontFamily::REGULAR);
    const int titleW = renderer.getTextWidth(UI_10_FONT_ID, title.c_str(), EpdFontFamily::REGULAR);
    renderer.drawText(UI_10_FONT_ID, textCenterX - titleW / 2, infoStart + titleOffset, title.c_str(), !inverted,
                      EpdFontFamily::REGULAR);

    // Author: 1 line only, truncated, centered
    if (!book.author.empty()) {
      auto author = renderer.truncatedText(SMALL_FONT_ID, book.author.c_str(), textAreaWidth);
      const int authorW = renderer.getTextWidth(SMALL_FONT_ID, author.c_str());
      renderer.drawText(SMALL_FONT_ID, textCenterX - authorW / 2, infoStart + authorOffset, author.c_str(), !inverted);
    }

    // Progress bar: fixed position, 240px wide, 5px tall
    constexpr int barHeight = 5;
    constexpr int barMaxWidth = 240;
    const int barY = infoStart + progressOffset;
    if (book.progressPercent > 0) {
      constexpr int percentGap = 12;
      char pctBuf[8];
      snprintf(pctBuf, sizeof(pctBuf), "%d%%", book.progressPercent);
      const int maxPctWidth = renderer.getTextWidth(SMALL_FONT_ID, "100%");
      const int totalBarUnit = barMaxWidth + percentGap + maxPctWidth;
      const int barX = textCenterX - totalBarUnit / 2;

      // Track: DarkGray when selected (on white bg it's LightGray)
      renderer.fillRoundedRect(barX, barY, barMaxWidth, barHeight, barHeight / 2,
                               inverted ? Color::DarkGray : Color::LightGray);
      // Fill: White when selected (inverted), Black when normal
      const int fillWidth = std::max(barHeight, barMaxWidth * book.progressPercent / 100);
      renderer.fillRoundedRect(barX, barY, fillWidth, barHeight, barHeight / 2,
                               inverted ? Color::White : Color::Black);
      const int pctTextWidth = renderer.getTextWidth(SMALL_FONT_ID, pctBuf);
      const int pctTextX = barX + barMaxWidth + percentGap + (maxPctWidth - pctTextWidth);
      const int pctTextY = barY + (barHeight - renderer.getLineHeight(SMALL_FONT_ID)) / 2;
      renderer.drawText(SMALL_FONT_ID, pctTextX, pctTextY, pctBuf, !inverted);
    }
  }

  // Dot indicators: fixed position, inverted when book selected
  if (bookCount > 1) {
    constexpr int dotDiam = 6;
    constexpr int dotGap = 8;
    constexpr int activeDotW = 16;
    constexpr int dotsOffset = 94;
    const int dotY = rect.y + carouselH + dotsOffset;
    const int maxDots = std::min(bookCount, 7);
    const bool previewsRight = SETTINGS.previewDirection == CrossPointSettings::PREVIEW_RIGHT;
    const int activeDotIndex =
        previewsRight ? (maxDots - 1) - (centerIndex % maxDots) : (centerIndex % maxDots);
    const int totalDotsWidth = (maxDots - 1) * (dotDiam + dotGap) + activeDotW;
    int dotX = rect.x + (rect.width - totalDotsWidth) / 2;

    for (int i = 0; i < maxDots; i++) {
      if (i == activeDotIndex) {
        renderer.fillRoundedRect(dotX, dotY, activeDotW, dotDiam, dotDiam / 2,
                                 bookIsSelected ? Color::White : Color::Black);
        dotX += activeDotW + dotGap;
      } else {
        renderer.fillRoundedRect(dotX, dotY, dotDiam, dotDiam, dotDiam / 2,
                                 bookIsSelected ? Color::DarkGray : Color::LightGray);
        dotX += dotDiam + dotGap;
      }
    }
  }

  // Divider: between selection pill and grid, shifted 2mm (~18px) down, shortened 2mm (~18px) each side
  constexpr int pillBottomOffset = 94 + 6 + 14;  // dotsOffset + dotDiam + pillPadV
  const int pillBottom = rect.y + carouselH + 6 - 14 + pillBottomOffset;
  const int dividerY = (pillBottom + rect.y + static_cast<int>(rect.height)) / 2 + 18;
  constexpr int dividerInset = 28 + 18;  // contentSidePadding + 2mm
  renderer.drawLine(rect.x + dividerInset, dividerY, rect.x + rect.width - dividerInset, dividerY, true);
}

void ModernTheme::drawEmptyRecents(const GfxRenderer& renderer, const Rect rect) const {
  constexpr int padding = 48;
  renderer.drawText(UI_12_FONT_ID, rect.x + padding,
                    rect.y + rect.height / 2 - renderer.getLineHeight(UI_12_FONT_ID) - 2, tr(STR_NO_OPEN_BOOK), true,
                    EpdFontFamily::BOLD);
  renderer.drawText(UI_10_FONT_ID, rect.x + padding, rect.y + rect.height / 2 + 2, tr(STR_START_READING), true);
}

// ============================================================
// Popup — White card with Black border, rounded
// ============================================================

Rect ModernTheme::drawPopup(const GfxRenderer& renderer, const char* message) const {
  constexpr int y = 132;
  constexpr int outline = 2;
  const int textWidth = renderer.getTextWidth(UI_12_FONT_ID, message, EpdFontFamily::REGULAR);
  const int textHeight = renderer.getLineHeight(UI_12_FONT_ID);
  const int w = textWidth + popupMarginX * 2;
  const int h = textHeight + popupMarginY * 2;
  const int x = (renderer.getScreenWidth() - w) / 2;

  renderer.fillRoundedRect(x - outline, y - outline, w + outline * 2, h + outline * 2, cornerRadius + outline,
                           Color::White);
  renderer.fillRoundedRect(x, y, w, h, cornerRadius, Color::Black);

  const int textX = x + (w - textWidth) / 2;
  const int textY = y + popupMarginY - 2;
  renderer.drawText(UI_12_FONT_ID, textX, textY, message, false, EpdFontFamily::REGULAR);
  renderer.displayBuffer();

  return Rect{x, y, w, h};
}

void ModernTheme::fillPopupProgress(const GfxRenderer& renderer, const Rect& layout, const int progress) const {
  constexpr int barHeight = 4;
  const int barWidth = layout.width - popupMarginX * 2;
  const int barX = layout.x + (layout.width - barWidth) / 2;
  const int barY = layout.y + layout.height - popupMarginY / 2 - barHeight / 2 - 1;

  int fillWidth = barWidth * progress / 100;
  // DarkGray progress bar instead of solid black
  renderer.fillRect(barX, barY, fillWidth, barHeight, false);

  renderer.displayBuffer(HalDisplay::FAST_REFRESH);
}

// ============================================================
// TextField — Material-style underline
// ============================================================

void ModernTheme::drawTextField(const GfxRenderer& renderer, Rect rect, const int textWidth) const {
  int lineY = rect.y + rect.height + renderer.getLineHeight(UI_12_FONT_ID) + ModernMetrics::values.verticalSpacing;
  int lineW = textWidth + hPaddingInSelection * 2;
  renderer.drawLine(rect.x + (rect.width - lineW) / 2, lineY, rect.x + (rect.width + lineW) / 2, lineY, 2);
}

// ============================================================
// Keyboard Key — Black pill selection
// ============================================================

void ModernTheme::drawKeyboardKey(const GfxRenderer& renderer, Rect rect, const char* label,
                                  const bool isSelected) const {
  if (isSelected) {
    renderer.fillRoundedRect(rect.x, rect.y, rect.width, rect.height, cornerRadius, Color::Black);
  }

  const int textWidth = renderer.getTextWidth(UI_12_FONT_ID, label);
  const int textX = rect.x + (rect.width - textWidth) / 2;
  const int textY = rect.y + (rect.height - renderer.getLineHeight(UI_12_FONT_ID)) / 2;
  renderer.drawText(UI_12_FONT_ID, textX, textY, label, !isSelected);
}

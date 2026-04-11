#include "EpubReaderMenuActivity.h"

#include <CrossPointSettings.h>
#include <GfxRenderer.h>
#include <I18n.h>

#include "MappedInputManager.h"
#include "components/UITheme.h"
#include "fontIds.h"

namespace {
uint8_t nextReaderFontSize(uint8_t currentValue) {
  switch (currentValue) {
    case CrossPointSettings::FONT_12:
      return CrossPointSettings::FONT_14;
    case CrossPointSettings::FONT_14:
      return CrossPointSettings::FONT_16;
    case CrossPointSettings::FONT_16:
      return CrossPointSettings::FONT_18;
    case CrossPointSettings::FONT_18:
      return CrossPointSettings::FONT_20;
    case CrossPointSettings::FONT_20:
    default:
      return CrossPointSettings::FONT_12;
  }
}

uint8_t nextScreenMargin(uint8_t currentValue) {
  constexpr uint8_t kMinMargin = 5;
  constexpr uint8_t kMaxMargin = 40;
  constexpr uint8_t kStep = 5;
  if (currentValue + kStep > kMaxMargin) {
    return kMinMargin;
  }
  return currentValue + kStep;
}
}  // namespace

EpubReaderMenuActivity::EpubReaderMenuActivity(GfxRenderer& renderer, MappedInputManager& mappedInput,
                                               const std::string& title, const int currentPage, const int totalPages,
                                               const int bookProgressPercent, const uint8_t currentOrientation,
                                               const bool hasFootnotes, const uint8_t currentFontFamily,
                                               const uint8_t currentFontSize, const uint8_t currentLineSpacing,
                                               const uint8_t currentScreenMargin)
    : Activity("EpubReaderMenu", renderer, mappedInput),
      fullMenuItems(buildFullMenuItems(hasFootnotes)),
      title(title),
      pendingOrientation(currentOrientation),
      pendingFontFamily(currentFontFamily),
      pendingFontSize(currentFontSize),
      pendingLineSpacing(currentLineSpacing),
      pendingScreenMargin(currentScreenMargin),
      currentPage(currentPage),
      totalPages(totalPages),
      bookProgressPercent(bookProgressPercent),
      currentTexture(static_cast<Texture>(SETTINGS.menuTexture)) {}

// Full menu items (shown when user presses "More")
std::vector<EpubReaderMenuActivity::MenuItem> EpubReaderMenuActivity::buildFullMenuItems(bool hasFootnotes) {
  std::vector<MenuItem> items;
  items.reserve(14);
  items.push_back({MenuAction::BOLD_TEXT, StrId::STR_BOLD});
  items.push_back({MenuAction::LINE_SPACING, StrId::STR_LINE_SPACING});
  items.push_back({MenuAction::SCREEN_MARGIN, StrId::STR_SCREEN_MARGIN});
  items.push_back({MenuAction::ROTATE_SCREEN, StrId::STR_ORIENTATION});
  items.push_back({MenuAction::AUTO_PAGE_TURN, StrId::STR_AUTO_TURN_PAGES_PER_MIN});
  items.push_back({MenuAction::SELECT_CHAPTER, StrId::STR_SELECT_CHAPTER});
  items.push_back({MenuAction::GO_TO_PERCENT, StrId::STR_GO_TO_PERCENT});
  if (hasFootnotes) {
    items.push_back({MenuAction::FOOTNOTES, StrId::STR_FOOTNOTES});
  }
  items.push_back({MenuAction::CUSTOMISE_STATUS_BAR, StrId::STR_CUSTOMISE_STATUS_BAR});
  items.push_back({MenuAction::SCREENSHOT, StrId::STR_SCREENSHOT_BUTTON});
  items.push_back({MenuAction::DISPLAY_QR, StrId::STR_DISPLAY_QR});
  items.push_back({MenuAction::SYNC, StrId::STR_SYNC_PROGRESS});
  items.push_back({MenuAction::GO_HOME, StrId::STR_GO_HOME_BUTTON});
  items.push_back({MenuAction::DELETE_CACHE, StrId::STR_DELETE_CACHE});
  return items;
}

void EpubReaderMenuActivity::onEnter() {
  Activity::onEnter();
  // Always show menu in portrait, regardless of reader orientation
  savedOrientation = renderer.getOrientation();
  renderer.setOrientation(GfxRenderer::Orientation::Portrait);
  requestUpdate();
}

void EpubReaderMenuActivity::onExit() {
  // Restore reader orientation
  renderer.setOrientation(savedOrientation);
  Activity::onExit();
}

void EpubReaderMenuActivity::loop() {
  // Arrow bounce-back: reset to gray after 500ms
  if (arrowResetPending && millis() - arrowPressTime > 200) {
    activeArrow = 0;
    arrowResetPending = false;
    requestUpdate();
  }

  if (currentMode == Mode::QUICK) {
    // ══ QUICK SETTINGS MODE ══
    // Power = cycle font family
    if (mappedInput.wasReleased(MappedInputManager::Button::Power)) {
      pendingFontFamily = (pendingFontFamily + 1) % CrossPointSettings::FONT_FAMILY_COUNT;
      requestUpdate();
      return;
    }
    // Up = font size up
    if (mappedInput.wasReleased(MappedInputManager::Button::Up)) {
      activeArrow = 1;
      arrowPressTime = millis();
      arrowResetPending = true;
      pendingFontSize = nextReaderFontSize(pendingFontSize);
      requestUpdate();
      return;
    }
    // Down = font size down (reverse cycle)
    if (mappedInput.wasReleased(MappedInputManager::Button::Down)) {
      activeArrow = -1;
      arrowPressTime = millis();
      arrowResetPending = true;
      // Reverse: 20→18→16→14→12→20
      switch (pendingFontSize) {
        case CrossPointSettings::FONT_20:
          pendingFontSize = CrossPointSettings::FONT_18;
          break;
        case CrossPointSettings::FONT_18:
          pendingFontSize = CrossPointSettings::FONT_16;
          break;
        case CrossPointSettings::FONT_16:
          pendingFontSize = CrossPointSettings::FONT_14;
          break;
        case CrossPointSettings::FONT_14:
          pendingFontSize = CrossPointSettings::FONT_12;
          break;
        default:
          pendingFontSize = CrossPointSettings::FONT_20;
          break;
      }
      requestUpdate();
      return;
    }
    // Left = toggle dark mode
    if (mappedInput.wasReleased(MappedInputManager::Button::Left)) {
      SETTINGS.readerDarkMode = !SETTINGS.readerDarkMode;
      SETTINGS.saveToFile();
      requestUpdate();
      return;
    }
    // Right = open Thai dictionary
    if (mappedInput.wasReleased(MappedInputManager::Button::Right)) {
      setResult(MenuResult{static_cast<int>(MenuAction::THAI_DICTIONARY), pendingOrientation, selectedPageTurnOption,
                           pendingFontFamily, pendingFontSize, pendingLineSpacing, pendingScreenMargin});
      finish();
      return;
    }
    // Confirm = enter full menu
    if (mappedInput.wasReleased(MappedInputManager::Button::Confirm)) {
      currentMode = Mode::FULL_MENU;
      requestUpdate();
      return;
    }
    // Back = exit and apply
    if (mappedInput.wasReleased(MappedInputManager::Button::Back)) {
      ActivityResult result;
      result.isCancelled = true;
      result.data = MenuResult{-1, pendingOrientation, selectedPageTurnOption, pendingFontFamily, pendingFontSize,
                               pendingLineSpacing, pendingScreenMargin};
      setResult(std::move(result));
      finish();
      return;
    }
  } else {
    // ══ FULL MENU MODE ══
    // Up/Down = navigate items
    buttonNavigator.onNext([this] {
      fullMenuSelectedIndex = ButtonNavigator::nextIndex(fullMenuSelectedIndex, static_cast<int>(fullMenuItems.size()));
      requestUpdate();
    });
    buttonNavigator.onPrevious([this] {
      fullMenuSelectedIndex =
          ButtonNavigator::previousIndex(fullMenuSelectedIndex, static_cast<int>(fullMenuItems.size()));
      requestUpdate();
    });

    // Confirm = select/cycle item
    if (mappedInput.wasReleased(MappedInputManager::Button::Confirm)) {
      const auto selectedAction = fullMenuItems[fullMenuSelectedIndex].action;

      if (selectedAction == MenuAction::BOLD_TEXT) {
        SETTINGS.readerBoldText = !SETTINGS.readerBoldText;
        SETTINGS.saveToFile();
        requestUpdate();
        return;
      }
      if (selectedAction == MenuAction::LINE_SPACING) {
        pendingLineSpacing = (pendingLineSpacing + 1) % CrossPointSettings::LINE_COMPRESSION_COUNT;
        requestUpdate();
        return;
      }
      if (selectedAction == MenuAction::SCREEN_MARGIN) {
        pendingScreenMargin = nextScreenMargin(pendingScreenMargin);
        requestUpdate();
        return;
      }
      if (selectedAction == MenuAction::AUTO_PAGE_TURN) {
        selectedPageTurnOption = (selectedPageTurnOption + 1) % pageTurnLabels.size();
        requestUpdate();
        return;
      }
      if (selectedAction == MenuAction::ROTATE_SCREEN) {
        pendingOrientation = (pendingOrientation + 1) % CrossPointSettings::ORIENTATION_COUNT;
        requestUpdate();
        return;
      }
      if (selectedAction == MenuAction::BACKGROUND_TEXTURE) {
        currentTexture = static_cast<Texture>((static_cast<uint8_t>(currentTexture) + 1) %
                                               static_cast<uint8_t>(Texture::TEXTURE_COUNT));
        SETTINGS.menuTexture = static_cast<uint8_t>(currentTexture);
        SETTINGS.saveToFile();
        requestUpdate();
        return;
      }

      // Action items → finish with result
      setResult(MenuResult{static_cast<int>(selectedAction), pendingOrientation, selectedPageTurnOption,
                           pendingFontFamily, pendingFontSize, pendingLineSpacing, pendingScreenMargin});
      finish();
      return;
    }

    // Back = return to quick settings
    if (mappedInput.wasReleased(MappedInputManager::Button::Back)) {
      currentMode = Mode::QUICK;
      requestUpdate();
      return;
    }
  }
}

// ══ Render ══
void EpubReaderMenuActivity::render(RenderLock&&) {
  renderer.clearScreen();
  const auto pageWidth = renderer.getScreenWidth();
  const auto pageHeight = renderer.getScreenHeight();
  const auto orientation = renderer.getOrientation();

  const bool isLandscapeCw = orientation == GfxRenderer::Orientation::LandscapeClockwise;
  const bool isLandscapeCcw = orientation == GfxRenderer::Orientation::LandscapeCounterClockwise;
  const bool isPortraitInverted = orientation == GfxRenderer::Orientation::PortraitInverted;
  const int hintGutterWidth = (isLandscapeCw || isLandscapeCcw) ? 30 : 0;
  const int contentX = isLandscapeCw ? hintGutterWidth : 0;
  const int contentWidth = pageWidth - hintGutterWidth;
  const int hintGutterHeight = isPortraitInverted ? 50 : 0;
  const int topY = hintGutterHeight + 60;  // +60px (~1.5cm) push content down
  const int contentHeight = pageHeight - 40 - hintGutterHeight;  // 40 = button hints

  if (currentMode == Mode::QUICK) {
    renderQuickSettings(contentX, topY, contentWidth, contentHeight);
  } else {
    renderFullMenu(contentX, topY, contentWidth, contentHeight);
  }

  // Button hints
  if (currentMode == Mode::QUICK) {
    const auto labels = mappedInput.mapLabels(tr(STR_BACK), "More >", "Dark Mode", "Dictionary");
    GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
  } else {
    const auto labels = mappedInput.mapLabels(tr(STR_BACK), tr(STR_SELECT), tr(STR_DIR_UP), tr(STR_DIR_DOWN));
    GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
  }

  renderer.displayBuffer();
}

// ── Texture pattern fills ──
using Tex = EpubReaderMenuActivity::Texture;

static void drawTexture(GfxRenderer const& r, int rx, int ry, int rw, int rh, Tex style) {
  switch (style) {
    case Tex::DOT_GRID: {
      // Small 2×2 dots on a 6px grid
      constexpr int spacing = 6, dot = 2;
      for (int dy = 0; dy < rh; dy += spacing) {
        for (int dx = 0; dx < rw; dx += spacing) {
          const int dw = std::min(dot, rw - dx);
          const int dh = std::min(dot, rh - dy);
          r.fillRect(rx + dx, ry + dy, dw, dh, true);
        }
      }
      break;
    }
    case Tex::DARK: {
      r.fillRect(rx, ry, rw, rh, true);
      break;
    }
    default: break;
  }
}

// ── Text with white stroke outline ──
static void drawStrokedText(GfxRenderer const& r, int fontId, int tx, int ty,
                            const char* text, int stroke = 2,
                            EpdFontFamily::Style style = EpdFontFamily::BOLD) {
  // Draw white outline by rendering text offset in 8 directions
  for (int dx = -stroke; dx <= stroke; ++dx) {
    for (int dy = -stroke; dy <= stroke; ++dy) {
      if (dx == 0 && dy == 0) continue;
      r.drawText(fontId, tx + dx, ty + dy, text, false, style);  // white
    }
  }
  // Draw black text on top
  r.drawText(fontId, tx, ty, text, true, style);
}

// ══ Quick Settings Screen ══
// Minimal design: only values, no redundant labels or instructions.
// Layout matches physical button positions (calibrated Y positions):
//   Power button (top)     → Font Family  (center ≈ Y=151)
//   Side Up/Down           → Font Size    (center ≈ Y=406)
//   Front buttons (bottom) → Dark Mode / Orientation (Y=595 → Y=715)
void EpubReaderMenuActivity::renderQuickSettings(int x, int /*y*/, int w, int /*h*/) const {
  const auto pageHeight = renderer.getScreenHeight();
  constexpr int hintsH = 40;
  constexpr int pad = 20;
  constexpr int radius = 6;
  const int boxW = w - pad * 2;
  const int boxX = x + pad;

  // ── Thin separator under header area ──
  renderer.drawLine(x + 60, 60, x + w - 60, 60, true);

  // ══ Font Family (center ≈ Y=151) — just the name, big and bold ══
  const char* fontName = getItemValue(MenuAction::FONT_FAMILY);
  const int fontNameH = renderer.getTextHeight(NOTOSANS_18_FONT_ID);
  const int fontNameY = 151 - fontNameH / 2 - 10;  // +15px lower
  renderer.drawText(NOTOSANS_18_FONT_ID,
                    x + (w - renderer.getTextWidth(NOTOSANS_18_FONT_ID, fontName, EpdFontFamily::BOLD)) / 2,
                    fontNameY, fontName, true, EpdFontFamily::BOLD);

  // ── Thin separator between zones ──
  renderer.drawLine(x + 60, 250, x + w - 60, 250, true);

  // ══ Font Size (center ≈ Y=406) — just the number with arrows ══
  char sizeBuf[8];
  snprintf(sizeBuf, sizeof(sizeBuf), "%d", pendingFontSize);
  const int sizeH = renderer.getTextHeight(NOTOSANS_36_FONT_ID);
  const int sizeY = 406 - sizeH / 2;
  {
    const int textW = renderer.getTextWidth(NOTOSANS_36_FONT_ID, sizeBuf, EpdFontFamily::REGULAR);
    const int cx = x + (w - textW) / 2;
    // Stroke effect: draw at 8 neighbours first (outline), then solid on top
    for (int dx = -1; dx <= 1; ++dx) {
      for (int dy = -1; dy <= 1; ++dy) {
        if (dx == 0 && dy == 0) continue;
        renderer.drawText(NOTOSANS_36_FONT_ID, cx + dx, sizeY + dy, sizeBuf, true, EpdFontFamily::REGULAR);
      }
    }
    renderer.drawText(NOTOSANS_36_FONT_ID, cx, sizeY, sizeBuf, true, EpdFontFamily::REGULAR);
  }

  // ▲ Up triangle (gray, black when pressed)
  constexpr int triW = 20, triH = 12;
  const int triCx = x + w / 2;
  const int upTriY = sizeY - triH - 16;  // 20px higher than before
  const bool upActive = (activeArrow == 1);
  for (int row = 0; row < triH; ++row) {
    const int halfSpan = (row * triW) / (2 * (triH - 1));
    if (upActive) {
      renderer.fillRect(triCx - halfSpan, upTriY + row, halfSpan * 2 + 1, 1, true);
    } else {
      renderer.fillRectDither(triCx - halfSpan, upTriY + row, halfSpan * 2 + 1, 1, DarkGray);
    }
  }

  // ▼ Down triangle
  const int dnTriY = sizeY + sizeH + 32;  // +10px lower
  const bool dnActive = (activeArrow == -1);
  for (int row = 0; row < triH; ++row) {
    const int halfSpan = ((triH - 1 - row) * triW) / (2 * (triH - 1));
    if (dnActive) {
      renderer.fillRect(triCx - halfSpan, dnTriY + row, halfSpan * 2 + 1, 1, true);
    } else {
      renderer.fillRectDither(triCx - halfSpan, dnTriY + row, halfSpan * 2 + 1, 1, DarkGray);
    }
  }

  // ── Thin separator ──
  renderer.drawLine(x + 60, 555, x + w - 60, 555, true);

  // ══ Dark Mode + Orientation (Y=595 → Y=715, two boxes) ══
  constexpr int dmY = 595, dmH = 120;
  const int halfBoxW = (boxW - 10) / 2;

  // Dark Mode box (left)
  renderer.drawRoundedRect(boxX, dmY, halfBoxW, dmH, 1, radius, true);
  const int dmMid = dmY + dmH / 2;
  // Label small
  renderer.drawText(SMALL_FONT_ID,
                    boxX + (halfBoxW - renderer.getTextWidth(SMALL_FONT_ID, "Dark Mode")) / 2,
                    dmMid - 22, "Dark Mode", true);
  // Value bold
  const char* darkVal = SETTINGS.readerDarkMode ? I18N.get(StrId::STR_STATE_ON) : I18N.get(StrId::STR_STATE_OFF);
  renderer.drawText(UI_12_FONT_ID,
                    boxX + (halfBoxW - renderer.getTextWidth(UI_12_FONT_ID, darkVal, EpdFontFamily::BOLD)) / 2,
                    dmMid - 2, darkVal, true, EpdFontFamily::BOLD);

  // Dictionary box (right)
  const int orBoxX = boxX + halfBoxW + 10;
  renderer.drawRoundedRect(orBoxX, dmY, halfBoxW, dmH, 1, radius, true);
  renderer.drawText(SMALL_FONT_ID,
                    orBoxX + (halfBoxW - renderer.getTextWidth(SMALL_FONT_ID, "Dictionary")) / 2,
                    dmMid - 10, "Dictionary", true);

  // ══ Footer ══
  const char* moreHint = "More >";
  renderer.drawText(SMALL_FONT_ID,
                    x + (w - renderer.getTextWidth(SMALL_FONT_ID, moreHint)) / 2,
                    dmY + dmH + 15, moreHint, true);
}

// ══ Full Menu Screen (traditional list) ══
void EpubReaderMenuActivity::renderFullMenu(int x, int y, int w, int h) const {
  // Title
  renderer.drawText(UI_12_FONT_ID,
                    x + (w - renderer.getTextWidth(UI_12_FONT_ID, "More Settings", EpdFontFamily::BOLD)) / 2,
                    y + 12, "More Settings", true, EpdFontFamily::BOLD);

  const int startY = y + 50;
  constexpr int lineHeight = 30;

  for (size_t i = 0; i < fullMenuItems.size(); ++i) {
    const int displayY = startY + (i * lineHeight);
    const bool isSelected = (static_cast<int>(i) == fullMenuSelectedIndex);

    if (isSelected) {
      renderer.fillRect(x, displayY, w - 1, lineHeight, true);
    }

    const char* label = (fullMenuItems[i].action == MenuAction::BACKGROUND_TEXTURE)
                             ? "Background"
                             : I18N.get(fullMenuItems[i].labelId);
    renderer.drawText(UI_10_FONT_ID, x + 20, displayY, label, !isSelected);

    const char* value = getItemValue(fullMenuItems[i].action);
    if (value != nullptr) {
      const auto valW = renderer.getTextWidth(UI_10_FONT_ID, value);
      renderer.drawText(UI_10_FONT_ID, x + w - 20 - valW, displayY, value, !isSelected);
    }
  }
}

// ══ Get display value for a menu action ══
const char* EpubReaderMenuActivity::getItemValue(MenuAction action) const {
  static char numBuf[8];

  switch (action) {
    case MenuAction::FONT_FAMILY: {
      StrId label;
      switch (pendingFontFamily) {
        case CrossPointSettings::CLOUDLOOP:
          label = StrId::STR_CLOUD_LOOP;
          break;
        case CrossPointSettings::BOOKERLY:
          label = StrId::STR_BOOKERLY;
          break;
        default:  // BAIJAMJUREE
          label = StrId::STR_BAI_JAMJUREE;
          break;
      }
      return I18N.get(label);
    }
    case MenuAction::FONT_SIZE:
      snprintf(numBuf, sizeof(numBuf), "%d", pendingFontSize);
      return numBuf;
    case MenuAction::LINE_SPACING: {
      const StrId label = pendingLineSpacing == CrossPointSettings::TIGHT
                              ? StrId::STR_TIGHT
                              : (pendingLineSpacing == CrossPointSettings::WIDE ? StrId::STR_WIDE : StrId::STR_NORMAL);
      return I18N.get(label);
    }
    case MenuAction::SCREEN_MARGIN:
      snprintf(numBuf, sizeof(numBuf), "%d", pendingScreenMargin);
      return numBuf;
    case MenuAction::DARK_MODE:
      return SETTINGS.readerDarkMode ? I18N.get(StrId::STR_STATE_ON) : I18N.get(StrId::STR_STATE_OFF);
    case MenuAction::BOLD_TEXT:
      return SETTINGS.readerBoldText ? I18N.get(StrId::STR_STATE_ON) : I18N.get(StrId::STR_STATE_OFF);
    case MenuAction::ROTATE_SCREEN:
      return I18N.get(orientationLabels[pendingOrientation]);
    case MenuAction::AUTO_PAGE_TURN:
      return pageTurnLabels[selectedPageTurnOption];
    case MenuAction::GO_TO_PERCENT:
      snprintf(numBuf, sizeof(numBuf), "%d%%", bookProgressPercent);
      return numBuf;
    case MenuAction::BACKGROUND_TEXTURE: {
      static constexpr const char* texNames[] = {"Dots", "Dark"};
      return texNames[static_cast<uint8_t>(currentTexture)];
    }
    default:
      return nullptr;
  }
}

// CrossPoint Reader Desktop Simulator
// Renders the actual firmware UI in an SDL2 window on macOS.
// Auto-saves screenshot.png on every displayBuffer() call.

#include <SDL.h>

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

#include <builtinFonts/all.h>

#include "CrossPointSettings.h"
#include "CrossPointState.h"
#include "KOReaderCredentialStore.h"
#include "MappedInputManager.h"
#include "RecentBooksStore.h"
#include "activities/Activity.h"
#include "activities/ActivityManager.h"
#include "components/UITheme.h"
#include "fontIds.h"
#include "util/ButtonNavigator.h"

// Forward decl from SimDisplay.cpp
void simDisplaySetSDL(SDL_Window* window, SDL_Renderer* renderer, SDL_Texture* texture);

// ── Global singletons (same as firmware main.cpp) ──
HalDisplay display;
HalGPIO gpio;
MappedInputManager mappedInputManager(gpio);
GfxRenderer renderer(display);
ActivityManager activityManager(renderer, mappedInputManager);
FontDecompressor fontDecompressor;
FontCacheManager fontCacheManager(renderer.getFontMap());

// ── Fonts (same as firmware) ──
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
EpdFontFamily cloudloop36FontFamily(&cloudloop36Font);

// CloudLoop
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
#endif  // OMIT_FONTS

// ── Font + display setup (same as firmware) ──
void setupDisplayAndFonts() {
    display.begin();
    renderer.begin();
    activityManager.begin();

    if (!fontDecompressor.init()) {
        LOG_ERR("SIM", "Font decompressor init failed");
    }
    fontCacheManager.setFontDecompressor(&fontDecompressor);
    renderer.setFontCacheManager(&fontCacheManager);

#ifndef OMIT_FONTS
    renderer.insertFont(BAIJAMJUREE_12_FONT_ID, baijamjuree12FontFamily);
    renderer.insertFont(BAIJAMJUREE_14_FONT_ID, baijamjuree14FontFamily);
    renderer.insertFont(BAIJAMJUREE_16_FONT_ID, baijamjuree16FontFamily);
    renderer.insertFont(BAIJAMJUREE_18_FONT_ID, baijamjuree18FontFamily);
    renderer.insertFont(BAIJAMJUREE_20_FONT_ID, baijamjuree20FontFamily);
    renderer.insertFont(NOTOSANS_18_FONT_ID, baijamjuree18FontFamily);
    renderer.insertFont(NOTOSANS_36_FONT_ID, cloudloop36FontFamily);
    renderer.insertFont(CLOUDLOOP_12_FONT_ID, cloudloop12FontFamily);
    renderer.insertFont(CLOUDLOOP_14_FONT_ID, cloudloop14FontFamily);
    renderer.insertFont(CLOUDLOOP_16_FONT_ID, cloudloop16FontFamily);
    renderer.insertFont(CLOUDLOOP_18_FONT_ID, cloudloop18FontFamily);
    renderer.insertFont(CLOUDLOOP_20_FONT_ID, cloudloop20FontFamily);
#endif
    renderer.insertFont(UI_10_FONT_ID, ui10FontFamily);
    renderer.insertFont(UI_12_FONT_ID, ui12FontFamily);
    renderer.insertFont(SMALL_FONT_ID, smallFontFamily);
    renderer.setFallbackFont(SMALL_FONT_ID, &cjk8FontFamily);
    renderer.setFallbackFont(UI_10_FONT_ID, &cjk10FontFamily);
    renderer.setFallbackFont(UI_12_FONT_ID, &cjk12FontFamily);

    LOG_INF("SIM", "Fonts loaded");
}

// ── Main ──
int main(int argc, char* argv[]) {
    LOG_INF("SIM", "CrossPoint Reader Desktop Simulator starting...");

    // Initialize SDL2
    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
        return 1;
    }

    // Create window — 480×800 portrait (half-size: 240×400 for comfortable viewing)
    constexpr int SCALE = 1;  // 1:1 pixel mapping, change to 2 for retina
    SDL_Window* window = SDL_CreateWindow(
        "CrossPoint Simulator — Halo 2 Theme",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        480 / SCALE, 800 / SCALE,
        SDL_WINDOW_SHOWN | SDL_WINDOW_ALLOW_HIGHDPI
    );
    if (!window) {
        fprintf(stderr, "SDL_CreateWindow failed: %s\n", SDL_GetError());
        SDL_Quit();
        return 1;
    }

    SDL_Renderer* sdlRenderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
    SDL_Texture* texture = SDL_CreateTexture(sdlRenderer, SDL_PIXELFORMAT_ARGB8888,
                                             SDL_TEXTUREACCESS_STREAMING, 480, 800);

    // Connect SDL to our display
    simDisplaySetSDL(window, sdlRenderer, texture);

    // Initialize HAL
    HalSystem::begin();
    gpio.begin();
    powerManager.begin();

    // Initialize storage (create sd_data/ if not exists)
    if (!Storage.begin()) {
        LOG_ERR("SIM", "Storage init failed");
    }

    // Load settings and set theme to Modern (Halo 2)
    SETTINGS.loadFromFile();
    SETTINGS.uiTheme = CrossPointSettings::UI_THEME::MODERN;
    I18N.loadSettings();
    UITheme::getInstance().reload();
    ButtonNavigator::setMappedInputManager(mappedInputManager);

    // Setup display and fonts
    setupDisplayAndFonts();

    // Load recent books for carousel display
    APP_STATE.loadFromFile();
    RECENT_BOOKS.loadFromFile();

    // Go to home screen (or directly to reader if book path provided)
    if (argc > 1) {
        LOG_INF("SIM", "Auto-opening book: %s", argv[1]);
        activityManager.goToReader(argv[1]);
    } else {
        activityManager.goHome();
    }

    LOG_INF("SIM", "Simulator running — use arrow keys to navigate, Enter to select, Esc to go back");
    LOG_INF("SIM", "Screenshot saved to: screenshot.png (auto-updated on every render)");

    // ── Event loop ──
    bool running = true;
    while (running) {
        // GPIO update polls SDL events internally
        gpio.update();

        // Activity manager loop (handles input + renders if dirty)
        activityManager.loop();

        // Cap at ~30 FPS to prevent tight spinning
        SDL_Delay(33);
    }

    // Cleanup
    SDL_DestroyTexture(texture);
    SDL_DestroyRenderer(sdlRenderer);
    SDL_DestroyWindow(window);
    SDL_Quit();

    return 0;
}

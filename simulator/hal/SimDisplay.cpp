// Desktop HalDisplay implementation — SDL2-backed framebuffer + PNG auto-save
#define HAL_STORAGE_IMPL  // prevent FsFile alias collision
#include <HalDisplay.h>

#include <SDL.h>
#include <cstring>
#include <cstdio>
#include <cstdlib>

// ── stb_image_write (header-only PNG writer) ──
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

// ── Static framebuffer (48KB for 800×480 @ 1bpp) ──
static uint8_t frameBuffer0[HalDisplay::BUFFER_SIZE];

// ── Grayscale support buffers ──
static uint8_t* grayLsbBuffer = nullptr;
static uint8_t* grayMsbBuffer = nullptr;

// ── SDL2 window/renderer (set by simulator main.cpp) ──
static SDL_Window* simWindow = nullptr;
static SDL_Renderer* simRenderer = nullptr;
static SDL_Texture* simTexture = nullptr;
static bool simInitialized = false;

// Called from main.cpp after SDL_Init
void simDisplaySetSDL(SDL_Window* window, SDL_Renderer* renderer, SDL_Texture* texture) {
    simWindow = window;
    simRenderer = renderer;
    simTexture = texture;
    simInitialized = true;
}

// ── Convert 1bpp framebuffer to RGBA pixels and present to SDL ──
// The physical framebuffer is 800×480 (landscape).
// GfxRenderer maps logical Portrait (480×800) via rotateCoordinates:
//   Portrait: phyX = logY, phyY = 479 - logX
// We display the SDL window as 480×800 (portrait) by rotating the buffer.
static void presentFramebuffer() {
    if (!simInitialized) return;

    // 480×800 portrait pixel buffer (RGBA)
    static uint32_t pixels[480 * 800];

    // Physical buffer: row = phyY (0..479), col = phyX (0..799), 1 bit per pixel
    // Portrait mapping: logX = 479 - phyY, logY = phyX
    // So portrait pixel [logX][logY] = physical bit at [phyY=479-logX][phyX=logY]
    for (int logY = 0; logY < 800; logY++) {       // portrait rows (= phyX)
        for (int logX = 0; logX < 480; logX++) {    // portrait cols
            int phyX = logY;
            int phyY = 479 - logX;
            int byteIndex = phyY * 100 + phyX / 8;  // 100 = 800/8
            int bitIndex = 7 - (phyX % 8);
            bool isWhite = (frameBuffer0[byteIndex] >> bitIndex) & 1;
            pixels[logY * 480 + logX] = isWhite ? 0xFFF0EDE6 : 0xFF1A1A1A;
        }
    }

    SDL_UpdateTexture(simTexture, nullptr, pixels, 480 * sizeof(uint32_t));
    SDL_RenderClear(simRenderer);
    SDL_RenderCopy(simRenderer, simTexture, nullptr, nullptr);
    SDL_RenderPresent(simRenderer);
}

// ── Save screenshot as PNG (portrait orientation) ──
static void saveScreenshotPNG() {
    // Convert to 8-bit grayscale PNG (portrait 480×800)
    static uint8_t gray[480 * 800];

    for (int logY = 0; logY < 800; logY++) {
        for (int logX = 0; logX < 480; logX++) {
            int phyX = logY;
            int phyY = 479 - logX;
            int byteIndex = phyY * 100 + phyX / 8;
            int bitIndex = 7 - (phyX % 8);
            bool isWhite = (frameBuffer0[byteIndex] >> bitIndex) & 1;
            gray[logY * 480 + logX] = isWhite ? 0xF0 : 0x1A;
        }
    }

    stbi_write_png("screenshot.png", 480, 800, 1, gray, 480);
}

// ── HalDisplay implementation ──

HalDisplay::HalDisplay() {}
HalDisplay::~HalDisplay() {
    free(grayLsbBuffer);
    free(grayMsbBuffer);
    grayLsbBuffer = nullptr;
    grayMsbBuffer = nullptr;
}

void HalDisplay::begin() {
    memset(frameBuffer0, 0xFF, BUFFER_SIZE);  // White screen
}

uint8_t* HalDisplay::getFrameBuffer() const {
    return frameBuffer0;
}

void HalDisplay::clearScreen(uint8_t color) const {
    memset(frameBuffer0, color, BUFFER_SIZE);
}

void HalDisplay::drawImage(const uint8_t* imageData, uint16_t x, uint16_t y, uint16_t w, uint16_t h,
                           bool) const {
    const uint16_t wBytes = (w + 7) / 8;
    for (uint16_t row = 0; row < h; row++) {
        uint16_t py = y + row;
        if (py >= DISPLAY_HEIGHT) break;
        for (uint16_t col = 0; col < w; col++) {
            uint16_t px = x + col;
            if (px >= DISPLAY_WIDTH) break;
            // Source bit
            bool srcBit = (imageData[row * wBytes + col / 8] >> (7 - (col % 8))) & 1;
            // Destination
            int dstByte = py * DISPLAY_WIDTH_BYTES + px / 8;
            int dstBit = 7 - (px % 8);
            if (srcBit)
                frameBuffer0[dstByte] |= (1 << dstBit);
            else
                frameBuffer0[dstByte] &= ~(1 << dstBit);
        }
    }
}

void HalDisplay::drawImageTransparent(const uint8_t* imageData, uint16_t x, uint16_t y, uint16_t w, uint16_t h,
                                      bool) const {
    // Transparent draw: only set black pixels, don't overwrite with white
    const uint16_t wBytes = (w + 7) / 8;
    for (uint16_t row = 0; row < h; row++) {
        uint16_t py = y + row;
        if (py >= DISPLAY_HEIGHT) break;
        for (uint16_t col = 0; col < w; col++) {
            uint16_t px = x + col;
            if (px >= DISPLAY_WIDTH) break;
            bool srcBit = (imageData[row * wBytes + col / 8] >> (7 - (col % 8))) & 1;
            if (!srcBit) {
                int dstByte = py * DISPLAY_WIDTH_BYTES + px / 8;
                int dstBit = 7 - (px % 8);
                frameBuffer0[dstByte] &= ~(1 << dstBit);  // Set black
            }
        }
    }
}

void HalDisplay::displayBuffer(RefreshMode, bool) {
    presentFramebuffer();
    saveScreenshotPNG();
}

void HalDisplay::refreshDisplay(RefreshMode mode, bool turnOff) {
    displayBuffer(mode, turnOff);
}

void HalDisplay::deepSleep() {}

void HalDisplay::copyGrayscaleBuffers(const uint8_t* lsb, const uint8_t* msb) {
    if (!grayLsbBuffer) grayLsbBuffer = static_cast<uint8_t*>(malloc(BUFFER_SIZE));
    if (!grayMsbBuffer) grayMsbBuffer = static_cast<uint8_t*>(malloc(BUFFER_SIZE));
    if (grayLsbBuffer && lsb) memcpy(grayLsbBuffer, lsb, BUFFER_SIZE);
    if (grayMsbBuffer && msb) memcpy(grayMsbBuffer, msb, BUFFER_SIZE);
}

void HalDisplay::copyGrayscaleLsbBuffers(const uint8_t* lsb) {
    if (!grayLsbBuffer) grayLsbBuffer = static_cast<uint8_t*>(malloc(BUFFER_SIZE));
    if (grayLsbBuffer && lsb) memcpy(grayLsbBuffer, lsb, BUFFER_SIZE);
}

void HalDisplay::copyGrayscaleMsbBuffers(const uint8_t* msb) {
    if (!grayMsbBuffer) grayMsbBuffer = static_cast<uint8_t*>(malloc(BUFFER_SIZE));
    if (grayMsbBuffer && msb) memcpy(grayMsbBuffer, msb, BUFFER_SIZE);
}

void HalDisplay::cleanupGrayscaleBuffers(const uint8_t*) {
    free(grayLsbBuffer);
    free(grayMsbBuffer);
    grayLsbBuffer = nullptr;
    grayMsbBuffer = nullptr;
}

void HalDisplay::displayGrayBuffer(bool) {
    // For grayscale display in simulator: combine LSB+MSB into 4-level gray
    // then present. For now, just present the BW buffer.
    presentFramebuffer();
    saveScreenshotPNG();
}

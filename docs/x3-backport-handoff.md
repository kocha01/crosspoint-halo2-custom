# X3 Back-port Handoff — SD Font System Fixes (X4 → X3)

**Audience**: an agent/engineer back-porting recent SD card font / Font Builder
bug fixes from this X4-targeting fork into the X3 port (the same repo whose
`docs/x4-port-handoff.md` originally ported these features TO X4).

**Origin tag**: `v2.2.15` on `kocha01/crosspoint-halo2-custom`, branch
`codex/halo-2-ui-ver2`. **Date**: 2026-05-14.

**TL;DR**: The SD card font system and the in-browser Font Builder were both
present in the X3 port but contained at least six bugs that made them fail in
ways ranging from "subtle" (selected font sometimes silently ignored) to
"catastrophic" (boot loop after web-flash). They are now all fixed on the X4
side. The fixes are nearly 1:1 portable because the affected files are shared
in lineage. Critical bugs to triage on X3 are flagged 🚨 below — fix those
first.

If you only have time for one paragraph: **the single highest-impact fix is
[Critical Bug 1](#critical-bug-1-negative-font-ids-rejected-by-resolver-check)
— a `if (id > 0)` check in `CrossPointSettings.cpp` that silently rejects
roughly half of all custom SD fonts (whichever ones hash to a negative
`int32_t`)**. If X3 ships with the same file, it has the same bug.

---

## 0. Source of truth

Every fix is an individual commit on the X4 branch. Direct browse:

  https://github.com/kocha01/crosspoint-halo2-custom/commits/codex/halo-2-ui-ver2

Each section below cites the commit short-sha so you can `git show <sha>` for
the full diff. The patches are also reproduced inline so this doc stands alone
when only the markdown is shared.

---

## 1. Critical bugs — port these immediately

### Critical bug 1: negative font IDs rejected by resolver check

🚨 **Symptom**: User uploads `.cpfont`, picks it in Settings → reader, opens a
book, sees the built-in font instead of the custom one. Sometimes it works,
sometimes it doesn't, with no apparent pattern — different fonts behave
differently, even when both are valid `.cpfont` files.

**Root cause**: `SdCardFontManager::computeFontId()` builds an FNV-1a hash of
`(contentHash, familyName, pointSize)` and casts the 32-bit result to `int`:

```cpp
int id = static_cast<int>(hash);
return id != 0 ? id : 1;  // 0 is reserved as "not found"
```

Roughly half of all hashes have the MSB set, so `id` is **negative** about
50% of the time. The three resolver call sites in
`CrossPointSettings.cpp` then did:

```cpp
int id = sdFontIdResolver(sdFontResolverCtx, sdFontFamilyName, fontSize);
if (id > 0) return id;   // ← silently drops negative ids!
```

A negative-id font fell through to the built-in `fontFamily` switch as if the
SD font had never been loaded. **PKNakhonSawan hashes positive → works**.
**ChakraPetch hashes to `-2119622908` → silently uses CloudLoop instead.**

**Fix**: `if (id > 0)` → `if (id != 0)` in all three call sites. Commit
`f16da77`:

```cpp
// src/CrossPointSettings.cpp — three identical sites:
//   getReaderFontId()
//   getReaderFontIdForLanguage()
//   getReaderFontIdForThaiContent()

if (sdFontFamilyName[0] != '\0' && sdFontIdResolver) {
  int id = sdFontIdResolver(sdFontResolverCtx, sdFontFamilyName, fontSize);
  // SD font IDs are FNV-1a hashes cast to int32_t and CAN BE NEGATIVE — the
  // hash's MSB ends up as the sign bit ~half the time, so e.g. ChakraPetch
  // computes to -2119622908.  The earlier `if (id > 0)` check silently
  // rejected those legitimate IDs and fell through to the built-in font
  // switch, so picking a custom font whose hash happened to be negative
  // looked exactly like "the selection didn't take effect."  Only 0 is
  // reserved as the "not loaded" sentinel (computeFontId remaps it to 1).
  if (id != 0) return id;
}
```

How to find them on X3:
```bash
grep -n "sdFontIdResolver" src/CrossPointSettings.cpp
# Each match is followed by `if (id > 0)` — change all three to `if (id != 0)`.
```

### Critical bug 2: Font Builder bitmap packing was row-aligned

🚨 **Symptom**: Every `.cpfont` produced by the in-browser Font Builder rendered
as garbled noise on device. Python-converted fonts work, JS-built fonts don't.

**Root cause**: `.cpfont` 2bpp bitmap data is packed **continuously** (4 pixels
per byte across rows; total = `ceil(width * height / 4)` bytes). The firmware
walks pixels with a single `pixelPosition` counter and indexes
`bitmap[pixelPosition >> 2]` without ever resetting per row. The JS rasterizer
was instead writing `ceil(width/4) × height` bytes — one full byte's worth of
bits per row, leaving the last byte of each row padded with zeros when the
width wasn't a multiple of 4. For a 23-px-wide 'A' the JS file was 162 bytes
where Python emits 156, and every row past row 0 shifted by 1 pixel.

Widths that happened to be multiples of 4 worked by accident; everything else
rendered as scrambled mosaic.

**Fix**: replace the row-loop packer with a single linear loop. Commit
`6335dc2`:

```js
// src/network/html/FontBuilder.html — inside renderGlyph(), replace the
// per-row packing loop with this continuous version:

const imgData = ctx.getImageData(0, 0, width, height);
const pixels = imgData.data;
const totalPixels = width * height;
const bitmapBytes = new Uint8Array(Math.ceil(totalPixels / 4));

let px = 0;
for (let pos = 0; pos < totalPixels; pos++) {
  const x = pos % width;
  const y = (pos - x) / width;
  const lum = pixels[(y * width + x) * 4];
  const ink = 255 - lum;
  const q = ink >= 192 ? 3 : ink >= 128 ? 2 : ink >= 64 ? 1 : 0;
  px = ((px << 2) | q) & 0xFF;   // MSB-first within byte
  if ((pos & 3) === 3) {
    bitmapBytes[pos >> 2] = px;
    px = 0;
  }
}
// Tail flush: shift remaining pixels into the high bits.
if ((totalPixels & 3) !== 0) {
  const remaining = totalPixels & 3;
  px = (px << ((4 - remaining) * 2)) & 0xFF;
  bitmapBytes[bitmapBytes.length - 1] = px;
}
```

Verification harness — Python-equivalent of the JS algorithm produces
**byte-for-byte identical output** to `fontconvert_sdcard.py`'s
`pixels2b` loop, across single-pixel / 4-multiple / non-4-multiple
widths and total counts. Both should be checked when porting.

### Critical bug 3: Font Builder y-axis convention inverted

🚨 **Symptom**: Latin glyphs rendered as empty blank rectangles. Thai
combining marks (which sit fully above the baseline) disappeared entirely.

**Root cause**: opentype.js' `Path.getBoundingBox()` returns the bbox in
**y-down** screen coordinates with the baseline at y=0. `bbox.y1` is the top
edge (smaller y = visually higher), `bbox.y2` is the bottom edge. The
firmware's `EpdGlyph.top` field uses the FreeType convention — y-up, positive
above baseline — matching FreeType's `bitmap_top`.

The old code set `top = ceil(bbox.y2)`, which for an 'A' sitting on the
baseline (y1=-16, y2=0) gave `top = 0`. It also passed this value as the
baseline-y argument to `glyph.getPath(x, top, ...)` for the canvas draw,
which placed the baseline at canvas row 0 and sent the entire glyph off the
top of the supersample surface. Result: bitmap mostly white, glyph.top in the
.cpfont also wrong by 16-30 px depending on the glyph.

For Latin uppercase this looked "almost correct" because the bitmap's bottom
edge aligned with the baseline (the all-white output looked like reasonable
sub-pixel rendering). For Thai marks above the baseline, the entire mark
disappeared because nothing was ever drawn inside the canvas surface.

**Fix**: use `topYUp = -bbox.y1` everywhere. Commit `0216fce`:

```js
function renderGlyph(font, glyph, fontSize) {
  const path = glyph.getPath(0, 0, fontSize);
  const bbox = path.getBoundingBox();

  // opentype.js Path: y-down screen coords, baseline at y=0.  A glyph above
  // the baseline has bbox.y1 < bbox.y2 < 0; a descender pushes bbox.y2 +ve.
  // FreeType / firmware convention (EpdGlyph.top): y-up with baseline at 0;
  // `top` is the pixel distance from baseline to the top edge, POSITIVE above.
  const left = Math.floor(bbox.x1);
  const right = Math.ceil(bbox.x2);
  const topYUp = Math.ceil(-bbox.y1);       // ← was Math.ceil(bbox.y2)
  const bottomYUp = Math.floor(-bbox.y2);
  const width = Math.max(0, right - left);
  const height = Math.max(0, topYUp - bottomYUp);
  // ...
  // Place the baseline at canvas-y = topYUp so the top edge of the glyph
  // lands on canvas row 0:
  const ssPath = glyph.getPath(-left * ss, topYUp * ss, fontSize * ss);
  // ...
  return {
    width, height,
    advanceX: Math.min(0xFFFF, advanceXFp),
    left,
    top: topYUp,  // ← was `top` (= bbox.y2)
    bitmapBytes,
  };
}
```

### Critical bug 4: Font Builder ppem scale off by 2.083×

🚨 **Symptom**: Font Builder size 24 looks visibly smaller than built-in size
14. Every JS-built font is ~2× too small. After Critical Bugs 1-3 are fixed
this is the next thing the user will notice.

**Root cause**: The reference Python converter
(`lib/EpdFont/scripts/fontconvert_sdcard.py`) treats the user-typed `size` as
points and rasterizes at 150 DPI:

```python
face.set_char_size(size << 6, size << 6, 150, 150)
```

…so the effective pixels-per-em is `size × 150 / 72 ≈ size × 2.083`. All
built-in fonts ship at the same 150 DPI convention, so the firmware's reader
layout assumes "size 18" means roughly 37.5 ppem, not 18 ppem.

The JS builder was passing `size` straight through as the ppem argument to
`opentype.js.getPath(x, y, fontSize)` (where `fontSize` IS the ppem).
Result: ChakraPetch_24 looked smaller than CloudLoop_14 — the exact 2.083
ratio.

**Fix**: scale every metric by `PT_TO_PX = 150 / 72`. Commit `e05eeec`:

```js
const PT_TO_PX = 150 / 72;  // ≈ 2.083 — matches Python's 150 DPI convention

function renderGlyph(font, glyph, fontSize) {
  const ppem = fontSize * PT_TO_PX;
  const path = glyph.getPath(0, 0, ppem);            // ← was getPath(0, 0, fontSize)
  // ...
  const advanceXPx = (glyph.advanceWidth || 0) * (ppem / font.unitsPerEm);   // ← was fontSize / unitsPerEm
  // ...
  const ssPath = glyph.getPath(-left * ss, topYUp * ss, ppem * ss);          // ← was fontSize * ss
}

// In the build loop, when computing per-style font metrics:
const ppem = size * PT_TO_PX;
const scale = ppem / currentFont.unitsPerEm;                                  // ← was size / unitsPerEm
const ascender = Math.ceil(currentFont.ascender * scale);
const descender = Math.floor(currentFont.descender * scale);
const advanceY = Math.max(1, Math.ceil((currentFont.ascender - currentFont.descender) * scale));
```

All FOUR places that touch ppem need to scale, not just one — easy to miss the
metric-pack site.

### Critical bug 5: Web-flash leaves device in deep-sleep loop

🚨 **Symptom**: After flashing via the crosspointreader.com web flasher (or any
flasher that issues an esptool `hard_reset` at the end), the device USB-CDC
briefly enumerates then drops, then re-enumerates, then drops — looks like a
reboot loop from the host. The device cannot be recovered without a 3-second
power-button hold.

**Root cause**: `HalGPIO::getWakeupReason()` used the reset-reason heuristic to
detect "the user just plugged USB into a fully-discharged device" and route
that case into an `AfterUSBPower` branch in `main.cpp`'s setup() that
immediately starts deep sleep (the intended battery-saver behavior). The
heuristic was:

```cpp
if (wakeupCause == ESP_SLEEP_WAKEUP_UNDEFINED &&
    resetReason == ESP_RST_POWERON && usbConnected) {
  return WakeupReason::AfterUSBPower;
}
```

On ESP32-C3 with USB-CDC, an esptool hard_reset comes back as
`ESP_RST_POWERON` with USB still connected — indistinguishable from the
intended "plug-in to charge" signature. Every web flash → deep sleep → user
panic.

**Fix**: check the running OTA partition state BEFORE the reset-reason
heuristic. `ESP_OTA_IMG_NEW` / `ESP_OTA_IMG_PENDING_VERIFY` is unambiguous
evidence that a flasher just wrote this image. Commit `d03e026`:

```cpp
#include <esp_ota_ops.h>   // add to HalGPIO.cpp

HalGPIO::WakeupReason HalGPIO::getWakeupReason() const {
  // ...existing usbConnected/wakeupCause/resetReason setup...

  // First-boot-after-flash detection takes precedence over the reset-reason
  // heuristic: a fresh OTA write leaves otadata in state=NEW (the
  // crosspointreader.com web flasher writes exactly that), which we can read
  // back regardless of how the chip's reset register lies about its source.
  const esp_partition_t* running = esp_ota_get_running_partition();
  if (running) {
    esp_ota_img_states_t otaState = ESP_OTA_IMG_UNDEFINED;
    if (esp_ota_get_state_partition(running, &otaState) == ESP_OK) {
      if (otaState == ESP_OTA_IMG_NEW || otaState == ESP_OTA_IMG_PENDING_VERIFY) {
        return WakeupReason::AfterFlash;
      }
    }
  }

  // ...rest of existing heuristic...
}
```

And in `src/main.cpp`, the AfterFlash case must call
`esp_ota_mark_app_valid_cancel_rollback()` so the NEXT boot sees `VALID` and
the AfterUSBPower → sleep optimization can work normally:

```cpp
case HalGPIO::WakeupReason::AfterFlash: {
  esp_err_t err = esp_ota_mark_app_valid_cancel_rollback();
  if (err == ESP_OK) {
    LOG_DBG("MAIN", "OTA image marked valid");
  } else if (err != ESP_ERR_NOT_FOUND) {
    LOG_ERR("MAIN", "esp_ota_mark_app_valid failed: %d", err);
  }
  break;
}
```

The X3 port likely has a similar HalGPIO file — if it shares the same
ESP32-C3 / USB-CDC setup the bug applies identically.

### Critical bug 6: FontDecompressor OOM aborts → boot loop

🚨 **Symptom**: Device cold-boots with an SD font selected, gets through Home
init, then aborts mid-page-render. Symptoms manifest only when the heap is
crowded (SD font ~30 KB resident + cover BMP cache + Wi-Fi stack starting up).

**Root cause**: `FontDecompressor::getBitmap()` resizes a `std::vector<uint8_t>`
to hold the decompressed glyph group. `std::vector::resize` under
`-fno-exceptions` calls `operator new` which on OOM calls `abort()` — the
existing `if (hotGroup.empty())` check was dead code because the abort came
first. The result was a panic backtrace ending in
`__cxxabiv1::__terminate → panic_abort`.

**Fix**: pre-flight `ESP.getFreeHeap()` before resize, return nullptr on tight
heap (the renderer skips that glyph for one frame). Commit `71513f8`:

```cpp
// lib/EpdFont/FontDecompressor.cpp — getBitmap(), in the "not in hot group"
// branch, BEFORE hotGroup.resize(group.uncompressedSize):

hotGroup.clear();
hotGroup.shrink_to_fit();

constexpr uint32_t FDC_HEAP_SAFETY_MARGIN = 8 * 1024;
const uint32_t free = ESP.getFreeHeap();
if (free < group.uncompressedSize + FDC_HEAP_SAFETY_MARGIN) {
  LOG_ERR("FDC", "OOM avoided: need %u + %u margin, free %u",
          group.uncompressedSize, FDC_HEAP_SAFETY_MARGIN, free);
  hotGroupFont = nullptr;
  hotGroupIndex = UINT16_MAX;
  return nullptr;
}

hotGroup.resize(group.uncompressedSize);
// ...existing logic...
```

The smaller `hotGlyphBuf.resize(glyph->dataLength)` site below has the same
issue — apply the same pre-flight pattern with a smaller margin (4 KB is
fine).

---

## 2. Should-port — user-visible improvements

### Bug 7: SD font ignored when fontFamily=BOOKERLY on Thai content

**Symptom**: User picks SD font, but Thai books still render with the built-in
Noto Serif Thai fallback. Only manifests when the device's built-in
`fontFamily` is BOOKERLY (the default on some flashing setups) and the book
language/title is Thai.

**Root cause**: `getReaderFontIdForLanguage` and `getReaderFontIdForThaiContent`
short-circuit to `getThaiFallbackFontId()` (Noto Serif) when the user is
on Bookerly + Thai content. That branch never consults the SD font resolver.

**Fix**: probe the SD resolver at the TOP of both functions, before the
Bookerly heuristic. Commit `fd8c54f`:

```cpp
int CrossPointSettings::getReaderFontIdForLanguage(const std::string& language) const {
  // SD font is an explicit user choice — wins over the Bookerly→Thai-fallback
  // heuristic.  Without this, picking a custom Thai font on top of a device
  // whose built-in fontFamily is still BOOKERLY silently routes Thai pages
  // back to the built-in Noto Serif Thai fallback.
  if (sdFontFamilyName[0] != '\0' && sdFontIdResolver) {
    int id = sdFontIdResolver(sdFontResolverCtx, sdFontFamilyName, fontSize);
    if (id != 0) return id;
  }
  if (fontFamily != BOOKERLY) return getReaderFontId();
  if (isThaiLanguage(language)) return getThaiFallbackFontId();
  return getReaderFontId();
}

// Apply the same SD-first probe at the top of getReaderFontIdForThaiContent.
```

### Bug 8: Reader → Home OOM reboot when SD font is loaded

**Symptom**: User selects custom SD font, opens a book, presses Back to Home,
device reboots. Only happens when SD font is active (the resident mini-cache
~17-30 KB combined with the 96 KB cover BMP LRU cache pushed past the heap
ceiling).

**Fix**: free each loaded SD font's page-resident glyph cache on reader exit
without unloading the font. The font itself stays registered with the
renderer; the mini-cache rebuilds on the first page-turn of the next reader
entry (same cost as the initial render). Commit `d76c68c`:

```cpp
// lib/EpdFont/SdCardFontManager.h:
class SdCardFontManager {
 public:
  // ...existing API...
  void clearAllMiniCaches();   // ← new
};

// lib/EpdFont/SdCardFontManager.cpp:
void SdCardFontManager::clearAllMiniCaches() {
  for (auto& lf : loaded_) {
    if (lf.font) lf.font->clearCache();
  }
}

// src/SdCardFontSystem.h:
void releaseReaderHeap() { manager_.clearAllMiniCaches(); }   // ← new

// src/activities/reader/EpubReaderActivity.cpp::onExit, after the existing
// section.reset() / epub.reset() pair:
sdFontSystem.releaseReaderHeap();
```

Also useful: make the cover BMP LRU cache **heap-aware** so it bails out below
a safety floor instead of allocating-then-aborting. In the cover-cache load
function (ours lives in `ModernTheme.cpp`; X3 may have a different theme):

```cpp
constexpr size_t BMP_CACHE_MIN_FREE_HEAP_BYTES = 40 * 1024;
if (ESP.getFreeHeap() < fileSize + BMP_CACHE_MIN_FREE_HEAP_BYTES) {
  file.close();
  return nullptr;   // fall back to uncached render
}
```

### Bug 9: Font Builder "Thai" preset was Latin-less and missing replacement glyph

**Symptom**: User builds with Thai-only checkbox, English digits and
punctuation render as nothing.

**Fix**: The Thai preset should mirror the Python reference: Latin + Latin-1 +
Thai block + general punctuation + super/subscripts. AND every build should
include U+FFFD (REPLACEMENT_GLYPH) so the firmware has its fallback. Commit
`0216fce`:

```js
const PRESETS = {
  latin: [[0x20, 0x7E]],
  latin1: [[0x80, 0xFF]],
  'latin-ext': [[0x0100, 0x024F], [0x1E00, 0x1EFF]],
  thai: [
    [0x0020, 0x007E],  // Latin
    [0x00A0, 0x00FF],  // Latin-1 supplement
    [0x0E00, 0x0E7F],  // Thai block
    [0x2010, 0x2027],  // General punctuation (—, ", ', …)
    [0x2030, 0x203A],
    [0x2070, 0x209F],  // Super/subscripts
  ],
  punctuation: [[0x2000, 0x206F], [0x20A0, 0x20CF]],
};

const REPLACEMENT_INTERVAL = [0xFFFD, 0xFFFD];

function collectIntervals() {
  // ...gather from user selection...
  list.push([...REPLACEMENT_INTERVAL]);   // ← always include
  // ...merge + return...
}
```

### Bug 10: Settings font-size cycle didn't honor SD font's available sizes

**Symptom**: User selects a custom SD font whose files are at 18 / 22 / 26 pt,
but the settings font-size row cycles 12-14-16-18-20 in step-2 increments.
`ensureLoaded()`'s closest-match logic snaps every step back to the same
file, so the user can't perceive the cycle changing anything.

**Fix**: in `SettingsActivity::toggleCurrentSetting()`, when
`setting.nameId == STR_FONT_SIZE` and `sdFontFamilyName` is non-empty, cycle
through the family's discovered .cpfont sizes (sorted + deduped) instead of
the built-in step list. Commit `0216fce`:

```cpp
if (setting.nameId == StrId::STR_FONT_SIZE && SETTINGS.sdFontFamilyName[0] != '\0') {
  const auto* family = sdFontSystem.registry().findFamily(SETTINGS.sdFontFamilyName);
  if (family && !family->files.empty()) {
    std::vector<uint8_t> sizes;
    sizes.reserve(family->files.size());
    for (const auto& f : family->files) sizes.push_back(f.pointSize);
    std::sort(sizes.begin(), sizes.end());
    sizes.erase(std::unique(sizes.begin(), sizes.end()), sizes.end());
    const uint8_t current = SETTINGS.*(setting.valuePtr);
    uint8_t next = sizes.front();
    for (uint8_t s : sizes) {
      if (s > current) { next = s; break; }
    }
    SETTINGS.*(setting.valuePtr) = next;
    ensureSdFontLoaded();
    handled = true;
  }
}
```

---

## 3. Nice-to-have

### Diagnostic log promotions

Several `LOG_DBG` calls in the SD font lifecycle were promoted to `LOG_INF`
so a single serial capture in `gh_release` (LOG_LEVEL=1) can pinpoint where
the load failed. Touched files:

- `src/SdCardFontSystem.cpp` — `ensureLoaded` entry trace + all success/fail
  lines
- `lib/EpdFont/SdCardFont.cpp` — post-load summary
- `lib/EpdFont/SdCardFontManager.cpp` — registration success line
- `lib/EpdFont/SdCardFontRegistry.cpp` — discover results
- `src/activities/settings/SdCardFontPickerActivity.cpp` — picker confirm log
  + heap free at pick time
- `src/activities/reader/EpubReaderActivity.cpp` — `getEffectiveFontId` trace
  (throttled to 3 s)

Commit `67833f0`. Easy to cherry-pick; entirely additive.

### Recovery image generation

`scripts/build_recovery_image.py` was already in the X3 port — no action
needed (the X4 port-handoff explicitly mentioned X3 had it first).

### Release-workflow recovery.bin upload

`.github/workflows/release.yml` and `release_candidate.yml` now upload
`recovery.bin` alongside `firmware.bin` so web flashers that erase the whole
flash can fetch the merged image. Commit `5914a49`.

### One-click "Thai + English" Font Builder UX

The per-block checkbox UI in `FontBuilder.html` was replaced with a 3-option
radio (recommended / English-only / custom). Selecting custom auto-expands a
`<details>` with the old per-block checkboxes and a custom hex-range input.
Commit `66794a7`. Pure UX, no logic change beyond `collectIntervals()`.

---

## 4. X3-specific cautions when applying

- **`ModernTheme.cpp`** — the heap-aware cover-cache guard is X4-specific in
  this branch (X4 uses Modern theme with a 96 KB BMP cache). If X3 has its
  own theme implementation, apply the heap check there instead, around any
  similar eager-load pattern.
- **`platformio.ini`** — the `board_upload.offset_address` removal is X4-only
  and was already documented in `x4-port-handoff.md`. Skip on X3.
- **`scripts/build_recovery_image.py`** — already exists on X3, leave alone.
- **Web Font Builder HTML embed** — X4 added a `/fonts/builder` route to
  serve the standalone HTML from the device's web server. If X3 doesn't have
  this route yet but wants it, the route registration is in
  `CrossPointWebServer.cpp` (commit `d76c68c`). Otherwise users can keep
  opening the standalone `tools/web-font-builder/index.html` from disk.

---

## 5. Verification checklist

Apply Critical bugs 1-4 first (font ID + Font Builder), then 5-6 (boot), then
7-10 (UX). After each:

1. **Build clean**: `pio run -e gh_release` should pass with no new warnings.
2. **Smoke test on hardware** with the same TTF that previously failed
   (ChakraPetch-Regular is a good test — it specifically hashes to a negative
   font ID, triggering Critical Bug 1; it's also Thai + has combining marks,
   exercising Critical Bugs 2-3).
3. **Capture serial during font pick** — should see this sequence in
   `gh_release`:

   ```
   [SDREG] Family: <name> (N files)
   [SDREG] Discovery complete: N families
   [SDFP]  User picked SD font: <name> (heap free=...)
   [SDFS]  ensureLoaded enter: wanted='<name>' current='(empty)' targetPt=N
   [SDCF]  Loaded: /fonts/<name>/<name>_<size>.cpfont (v4, 1 styles, hash=0x...)
   [SDCF]    style[0]: <N> intervals, <N> glyphs, advY=..., ascender=..., descender=...
   [SDMGR] Loaded /fonts/.../<name>_<size>.cpfont size=N id=<negative-or-positive> styles=1
   [SDFS]  Loaded SD font family: <name>
   ```

4. **Open a Thai book** — should see:

   ```
   [READER] getEffectiveFontId=<the-sd-font-id> (sdFont='<name>' ...)
   ```

   If `getEffectiveFontId` returns a built-in fontId after pick, Critical Bug
   1 isn't fully patched.
5. **Visual size check**: a Font-Builder-built `<Name>_18.cpfont` should
   render at the same vertical density as the device's built-in size-18
   font. If it's noticeably smaller, Critical Bug 4 isn't patched.

---

## 6. Quick-apply patch (machine-readable)

For an agent that prefers to apply diffs directly, the X4 branch's diff from
the X3 baseline lives at:

  https://github.com/kocha01/crosspoint-halo2-custom/compare/v2.2.13...v2.2.15

`v2.2.13` was the initial port from X3 with the bugs still in place;
`v2.2.15` has all of section 1 + 2 fixed. The 11 commits in between are each
small and self-contained — `git cherry-pick` should work for any subset.

If `cherry-pick` conflicts, the per-bug code excerpts above are usually
enough to apply by hand, since the touched lines are pinpointed and the
surrounding context is small.

---

Good luck. If anything in this doc is unclear, the commit messages on the X4
branch include longer "why" prose in each.

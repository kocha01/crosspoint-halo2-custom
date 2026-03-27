cmake_minimum_required(VERSION 3.20)
project(crosspoint-simulator LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_C_STANDARD 11)

# Find SDL2
find_package(SDL2 REQUIRED)

# ── Source files ──────────────────────────────────────────────

# Simulator-specific sources
set(SIM_SOURCES
    main.cpp
    hal/SimDisplay.cpp
    hal/SimGPIO.cpp
    hal/SimStorage.cpp
    hal/SimPowerManager.cpp
    hal/SimSystem.cpp
    core/SimLogging.cpp
    core/SimActivityManager.cpp
    core/SimKOReader.cpp
    core/SimStubs.cpp
    core/SimNetworkStubs.cpp
)

# Reused firmware sources (compiled as-is)
set(FW_ROOT ${CMAKE_CURRENT_SOURCE_DIR}/..)
set(FW_SOURCES
    # ── Core libraries ──
    ${FW_ROOT}/lib/GfxRenderer/GfxRenderer.cpp
    ${FW_ROOT}/lib/GfxRenderer/Bitmap.cpp
    ${FW_ROOT}/lib/GfxRenderer/BitmapHelpers.cpp
    ${FW_ROOT}/lib/GfxRenderer/FontCacheManager.cpp
    ${FW_ROOT}/lib/EpdFont/EpdFont.cpp
    ${FW_ROOT}/lib/EpdFont/EpdFontFamily.cpp
    ${FW_ROOT}/lib/EpdFont/FontDecompressor.cpp
    ${FW_ROOT}/lib/InflateReader/InflateReader.cpp
    ${FW_ROOT}/lib/Utf8/Utf8.cpp
    ${FW_ROOT}/lib/I18n/I18nStrings.cpp
    ${FW_ROOT}/lib/I18n/I18n.cpp
    ${FW_ROOT}/lib/FsHelpers/FsHelpers.cpp
    # ── EPUB engine (full) ──
    ${FW_ROOT}/lib/Epub/Epub.cpp
    ${FW_ROOT}/lib/Epub/Epub/BookMetadataCache.cpp
    ${FW_ROOT}/lib/Epub/Epub/Page.cpp
    ${FW_ROOT}/lib/Epub/Epub/ParsedText.cpp
    ${FW_ROOT}/lib/Epub/Epub/Section.cpp
    ${FW_ROOT}/lib/Epub/Epub/blocks/ImageBlock.cpp
    ${FW_ROOT}/lib/Epub/Epub/blocks/TextBlock.cpp
    ${FW_ROOT}/lib/Epub/Epub/converters/ImageDecoderFactory.cpp
    ${FW_ROOT}/lib/Epub/Epub/converters/ImageToFramebufferDecoder.cpp
    ${FW_ROOT}/lib/Epub/Epub/converters/JpegToFramebufferConverter.cpp
    ${FW_ROOT}/lib/Epub/Epub/converters/PngToFramebufferConverter.cpp
    ${FW_ROOT}/lib/Epub/Epub/css/CssParser.cpp
    ${FW_ROOT}/lib/Epub/Epub/htmlEntities.cpp
    ${FW_ROOT}/lib/Epub/Epub/hyphenation/HyphenationCommon.cpp
    ${FW_ROOT}/lib/Epub/Epub/hyphenation/Hyphenator.cpp
    ${FW_ROOT}/lib/Epub/Epub/hyphenation/LanguageRegistry.cpp
    ${FW_ROOT}/lib/Epub/Epub/hyphenation/LiangHyphenation.cpp
    ${FW_ROOT}/lib/Epub/Epub/hyphenation/ThaiWordBreaker.cpp
    ${FW_ROOT}/lib/Epub/Epub/parsers/ChapterHtmlSlimParser.cpp
    ${FW_ROOT}/lib/Epub/Epub/parsers/ContainerParser.cpp
    ${FW_ROOT}/lib/Epub/Epub/parsers/ContentOpfParser.cpp
    ${FW_ROOT}/lib/Epub/Epub/parsers/TocNavParser.cpp
    ${FW_ROOT}/lib/Epub/Epub/parsers/TocNcxParser.cpp
    # ── ZIP file handling ──
    ${FW_ROOT}/lib/ZipFile/ZipFile.cpp
    # ── Image format converters ──
    ${FW_ROOT}/lib/JpegToBmpConverter/JpegToBmpConverter.cpp
    ${FW_ROOT}/lib/PngToBmpConverter/PngToBmpConverter.cpp
    # ── Image decoders ──
    ${FW_ROOT}/.pio/libdeps/default/JPEGDEC/src/JPEGDEC.cpp
    ${FW_ROOT}/.pio/libdeps/default/PNGdec/src/PNGdec.cpp
    # ── UI Theme ──
    ${FW_ROOT}/src/components/UITheme.cpp
    ${FW_ROOT}/src/components/themes/BaseTheme.cpp
    ${FW_ROOT}/src/components/themes/modern/ModernTheme.cpp
    ${FW_ROOT}/src/components/themes/lyra/LyraTheme.cpp
    ${FW_ROOT}/src/components/themes/lyra/Lyra3CoversTheme.cpp
    # ── App core ──
    ${FW_ROOT}/src/CrossPointSettings.cpp
    ${FW_ROOT}/src/CrossPointState.cpp
    ${FW_ROOT}/src/RecentBooksStore.cpp
    ${FW_ROOT}/src/JsonSettingsIO.cpp
    ${FW_ROOT}/src/MappedInputManager.cpp
    ${FW_ROOT}/src/util/ButtonNavigator.cpp
    ${FW_ROOT}/src/util/ScreenshotUtil.cpp
    ${FW_ROOT}/src/activities/Activity.cpp
    # ── Home activities ──
    ${FW_ROOT}/src/activities/home/HomeActivity.cpp
    ${FW_ROOT}/src/activities/home/FileBrowserActivity.cpp
    ${FW_ROOT}/src/activities/home/RecentBooksActivity.cpp
    # ── Reader activities ──
    ${FW_ROOT}/src/activities/reader/ReaderActivity.cpp
    ${FW_ROOT}/src/activities/reader/EpubReaderActivity.cpp
    ${FW_ROOT}/src/activities/reader/EpubReaderMenuActivity.cpp
    ${FW_ROOT}/src/activities/reader/EpubReaderChapterSelectionActivity.cpp
    ${FW_ROOT}/src/activities/reader/EpubReaderPercentSelectionActivity.cpp
    ${FW_ROOT}/src/activities/reader/EpubReaderFootnotesActivity.cpp
    # KOReaderSyncActivity and QrDisplayActivity are stubbed in SimNetworkStubs.cpp
    # (they depend on WiFi, SNTP, QR lib that aren't available in simulator)
    ${FW_ROOT}/src/activities/reader/TxtReaderActivity.cpp
    ${FW_ROOT}/src/activities/reader/XtcReaderActivity.cpp
    ${FW_ROOT}/src/activities/reader/XtcReaderChapterSelectionActivity.cpp
    # ── Settings activities ──
    ${FW_ROOT}/src/activities/settings/SettingsActivity.cpp
    ${FW_ROOT}/src/activities/settings/ButtonRemapActivity.cpp
    ${FW_ROOT}/src/activities/settings/StatusBarSettingsActivity.cpp
    ${FW_ROOT}/src/activities/settings/KOReaderSettingsActivity.cpp
    ${FW_ROOT}/src/activities/settings/CalibreSettingsActivity.cpp
    ${FW_ROOT}/src/activities/settings/ClearCacheActivity.cpp
    ${FW_ROOT}/src/activities/settings/LanguageSelectActivity.cpp
    # ── Util activities ──
    ${FW_ROOT}/src/activities/util/FullScreenMessageActivity.cpp
    ${FW_ROOT}/src/activities/util/ConfirmationActivity.cpp
    ${FW_ROOT}/src/activities/util/KeyboardEntryActivity.cpp
    ${FW_ROOT}/src/activities/util/BmpViewerActivity.cpp
)

# C sources (expat XML parser, uzlib, PNGdec zlib, picojpeg)
set(C_SOURCES
    ${FW_ROOT}/lib/expat/xmlparse.c
    ${FW_ROOT}/lib/expat/xmlrole.c
    ${FW_ROOT}/lib/expat/xmltok.c
    ${FW_ROOT}/lib/picojpeg/picojpeg.c
    ${FW_ROOT}/.pio/libdeps/default/PNGdec/src/adler32.c
    ${FW_ROOT}/.pio/libdeps/default/PNGdec/src/crc32.c
    ${FW_ROOT}/.pio/libdeps/default/PNGdec/src/infback.c
    ${FW_ROOT}/.pio/libdeps/default/PNGdec/src/inffast.c
    ${FW_ROOT}/.pio/libdeps/default/PNGdec/src/inflate.c
    ${FW_ROOT}/.pio/libdeps/default/PNGdec/src/inftrees.c
    ${FW_ROOT}/.pio/libdeps/default/PNGdec/src/zutil.c
)

# uzlib C sources
file(GLOB UZLIB_SOURCES ${FW_ROOT}/lib/uzlib/src/*.c)

add_executable(crosspoint-simulator ${SIM_SOURCES} ${FW_SOURCES} ${C_SOURCES} ${UZLIB_SOURCES})

# ── Include directories ──────────────────────────────────────
# Simulator platform shims FIRST (override Arduino/FreeRTOS headers)
target_include_directories(crosspoint-simulator PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/platform
    ${CMAKE_CURRENT_SOURCE_DIR}/core
    ${CMAKE_CURRENT_SOURCE_DIR}/hal
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${FW_ROOT}/lib/hal
    ${FW_ROOT}/lib/GfxRenderer
    ${FW_ROOT}/lib/EpdFont
    ${FW_ROOT}/lib/EpdFont/builtinFonts
    ${FW_ROOT}/lib/InflateReader
    ${FW_ROOT}/lib/uzlib/src
    ${FW_ROOT}/lib/Utf8
    ${FW_ROOT}/lib/I18n
    ${FW_ROOT}/lib/FsHelpers
    ${FW_ROOT}/lib/Logging
    ${FW_ROOT}/lib/KOReaderSync
    ${FW_ROOT}/lib/Serialization
    ${FW_ROOT}/lib/Epub
    ${FW_ROOT}/lib/Epub/Epub
    ${FW_ROOT}/lib/Epub/Epub/converters
    ${FW_ROOT}/lib/Epub/Epub/parsers
    ${FW_ROOT}/lib/Epub/Epub/css
    ${FW_ROOT}/lib/Epub/Epub/blocks
    ${FW_ROOT}/lib/Epub/Epub/hyphenation
    ${FW_ROOT}/lib/Xtc
    ${FW_ROOT}/lib/Txt
    ${FW_ROOT}/lib/expat
    ${FW_ROOT}/lib/ZipFile
    ${FW_ROOT}/lib/JpegToBmpConverter
    ${FW_ROOT}/lib/PngToBmpConverter
    ${FW_ROOT}/lib/picojpeg
    ${FW_ROOT}/.pio/libdeps/default/ArduinoJson/src
    ${FW_ROOT}/.pio/libdeps/default/JPEGDEC/src
    ${FW_ROOT}/.pio/libdeps/default/PNGdec/src
    ${FW_ROOT}/src
    ${FW_ROOT}/src/components
    ${FW_ROOT}/src/activities
    ${FW_ROOT}/src/activities/reader
    ${FW_ROOT}/src/network
)

# ── Compile definitions ──────────────────────────────────────
target_compile_definitions(crosspoint-simulator PRIVATE
    CROSSPOINT_EMULATED=1
    EINK_DISPLAY_SINGLE_BUFFER_MODE=1
    ENABLE_SERIAL_LOG
    LOG_LEVEL=2
    CROSSPOINT_VERSION="sim-dev"
    USE_UTF8_LONG_NAMES=1
    XML_CONTEXT_BYTES=1024
    MINIZ_NO_ZLIB_COMPATIBLE_NAMES=1
    XML_GE=0
    # expat needs these
    XML_POOR_ENTROPY=1
    HAVE_MEMMOVE=1
)

# ── Link SDL2 ────────────────────────────────────────────────
target_link_libraries(crosspoint-simulator PRIVATE SDL2::SDL2)

# ── Symlink sd_data into build directory ─────────────────────
add_custom_command(TARGET crosspoint-simulator POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E create_symlink
        ${CMAKE_CURRENT_SOURCE_DIR}/sd_data
        ${CMAKE_CURRENT_BINARY_DIR}/sd_data
    COMMENT "Symlinking sd_data into build directory"
)

# ── Compiler flags ───────────────────────────────────────────
target_compile_options(crosspoint-simulator PRIVATE
    -Wno-deprecated-declarations
    -Wno-unused-variable
    -Wno-unused-parameter
    -Wno-sign-compare
    -Wno-missing-field-initializers
    -Wno-inconsistent-missing-override
)

# Force-include Arduino.h shim for C++ files only (matches Arduino framework behavior)
# Can't apply to C files since Arduino.h uses C++ headers
target_compile_options(crosspoint-simulator PRIVATE
    $<$<COMPILE_LANGUAGE:CXX>:-include${CMAKE_CURRENT_SOURCE_DIR}/platform/Arduino.h>
)

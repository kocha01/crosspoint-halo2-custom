// Stubs for heavy dependencies not needed in the UI simulator

#include <string>
#include <memory>
#include <cstdint>
#include <cstdlib>
#include <vector>
#include <functional>

// ── Epub: now compiled from real source (not stubbed) ──

// ── Xtc stubs ──
// Constructor is inline in Xtc.h, destructor is defaulted inline too
#include "Xtc.h"

static const std::vector<xtc::ChapterInfo> emptyChapters;

bool Xtc::load() { return false; }
bool Xtc::clearCache() const { return false; }
void Xtc::setupCacheDir() const {}
std::string Xtc::getTitle() const { return ""; }
std::string Xtc::getAuthor() const { return ""; }
bool Xtc::hasChapters() const { return false; }
const std::vector<xtc::ChapterInfo>& Xtc::getChapters() const { return emptyChapters; }
std::string Xtc::getCoverBmpPath() const { return ""; }
bool Xtc::generateCoverBmp() const { return false; }
std::string Xtc::getThumbBmpPath() const { return ""; }
std::string Xtc::getThumbBmpPath(int) const { return ""; }
bool Xtc::generateThumbBmp(int) const { return false; }
uint32_t Xtc::getPageCount() const { return 0; }
uint16_t Xtc::getPageWidth() const { return 0; }
uint16_t Xtc::getPageHeight() const { return 0; }
uint8_t Xtc::getBitDepth() const { return 1; }
size_t Xtc::loadPage(uint32_t, uint8_t*, size_t) const { return 0; }
xtc::XtcError Xtc::loadPageStreaming(uint32_t, std::function<void(const uint8_t*, size_t, size_t)>, size_t) const {
    return xtc::XtcError::FILE_NOT_FOUND;
}
uint8_t Xtc::calculateProgress(uint32_t) const { return 0; }
xtc::XtcError Xtc::getLastError() const { return xtc::XtcError::OK; }

// ── XtcParser stubs ──
namespace xtc {
XtcParser::XtcParser()
    : m_isOpen(false), m_header{}, m_defaultWidth(0), m_defaultHeight(0),
      m_bitDepth(1), m_hasChapters(false), m_lastError(XtcError::OK) {}
XtcParser::~XtcParser() {}
XtcError XtcParser::open(const char*) { return XtcError::FILE_NOT_FOUND; }
void XtcParser::close() { m_isOpen = false; }
bool XtcParser::getPageInfo(uint32_t, PageInfo&) const { return false; }
size_t XtcParser::loadPage(uint32_t, uint8_t*, size_t) { return 0; }
XtcError XtcParser::loadPageStreaming(uint32_t, std::function<void(const uint8_t*, size_t, size_t)>, size_t) {
    return XtcError::FILE_NOT_FOUND;
}
bool XtcParser::isValidXtcFile(const char*) { return false; }
XtcError XtcParser::readHeader() { return XtcError::FILE_NOT_FOUND; }
XtcError XtcParser::readPageTable() { return XtcError::FILE_NOT_FOUND; }
XtcError XtcParser::readTitle() { return XtcError::FILE_NOT_FOUND; }
XtcError XtcParser::readAuthor() { return XtcError::FILE_NOT_FOUND; }
XtcError XtcParser::readChapters() { return XtcError::FILE_NOT_FOUND; }
}

// ── obfuscation stubs (real code depends on ESP32 mbedtls/base64) ──
#include "ObfuscationUtils.h"
namespace obfuscation {
void xorTransform(std::string&) {}
void xorTransform(std::string&, const uint8_t*, size_t) {}
String obfuscateToBase64(const std::string&) { return String(""); }
std::string deobfuscateFromBase64(const char*, bool* ok) {
    if (ok) *ok = true;
    return "";
}
void selfTest() {}
}

// ── WifiCredentialStore stubs ──
#include "WifiCredentialStore.h"
WifiCredentialStore WifiCredentialStore::instance;
bool WifiCredentialStore::saveToFile() const { return true; }
bool WifiCredentialStore::loadFromFile() { return true; }
bool WifiCredentialStore::loadFromBinaryFile() { return false; }
bool WifiCredentialStore::addCredential(const std::string&, const std::string&) { return true; }
bool WifiCredentialStore::removeCredential(const std::string&) { return true; }
const WifiCredential* WifiCredentialStore::findCredential(const std::string&) const { return nullptr; }
bool WifiCredentialStore::hasSavedCredential(const std::string&) const { return false; }
void WifiCredentialStore::setLastConnectedSsid(const std::string&) {}
const std::string& WifiCredentialStore::getLastConnectedSsid() const { return lastConnectedSsid; }
void WifiCredentialStore::clearLastConnectedSsid() {}
void WifiCredentialStore::clearAll() {}

// ── Txt stubs ──
#include "Txt.h"

Txt::Txt(std::string path, std::string cacheBase) : filepath(std::move(path)), cacheBasePath(std::move(cacheBase)) {}
bool Txt::load() { return false; }
std::string Txt::getTitle() const { return ""; }
void Txt::setupCacheDir() const {}
std::string Txt::getCoverBmpPath() const { return ""; }
bool Txt::generateCoverBmp() const { return false; }
std::string Txt::findCoverImage() const { return ""; }
bool Txt::readContent(uint8_t*, size_t, size_t) const { return false; }

// ── OtaUpdater stubs ──
#include "network/OtaUpdater.h"

bool OtaUpdater::isUpdateNewer() const { return false; }
const std::string& OtaUpdater::getLatestVersion() const {
    static const std::string v = "sim-dev";
    return v;
}
OtaUpdater::OtaUpdaterError OtaUpdater::checkForUpdate() { return NO_UPDATE; }
OtaUpdater::OtaUpdaterError OtaUpdater::installUpdate() { return INTERNAL_UPDATE_ERROR; }

// ── KOReaderCredentialStore stubs are in SimKOReader.cpp ──

// ── uzlib checksum stubs ──
// These are declared in uzlib.h but not provided in the minimal uzlib source tree
extern "C" {
#include "uzlib.h"
uint32_t uzlib_adler32(const void *data, unsigned int length, uint32_t prev_sum) {
    (void)data; (void)length;
    return prev_sum;
}
uint32_t uzlib_crc32(const void *data, unsigned int length, uint32_t crc) {
    (void)data; (void)length;
    return crc;
}
}

// ── KOReaderSyncClient stubs (network-dependent) ──
#include "KOReaderSyncClient.h"

KOReaderSyncClient::Error KOReaderSyncClient::authenticate() { return NETWORK_ERROR; }
KOReaderSyncClient::Error KOReaderSyncClient::getProgress(const std::string&, KOReaderProgress&) {
    return NETWORK_ERROR;
}
KOReaderSyncClient::Error KOReaderSyncClient::updateProgress(const KOReaderProgress&) {
    return NETWORK_ERROR;
}
const char* KOReaderSyncClient::errorString(Error error) {
    switch (error) {
        case OK: return "OK";
        case NO_CREDENTIALS: return "No credentials";
        case NETWORK_ERROR: return "Network error (simulator)";
        case AUTH_FAILED: return "Auth failed";
        case SERVER_ERROR: return "Server error";
        case JSON_ERROR: return "JSON error";
        case NOT_FOUND: return "Not found";
        default: return "Unknown error";
    }
}

// ── KOReaderDocumentId stubs (uses MD5 which needs ESP32 mbedtls) ──
#include "KOReaderDocumentId.h"

std::string KOReaderDocumentId::calculate(const std::string&) { return "sim-document-hash"; }
std::string KOReaderDocumentId::calculateFromFilename(const std::string&) { return "sim-filename-hash"; }
size_t KOReaderDocumentId::getOffset(int i) { return 1024UL << (2 * (i + 1)); }

// ── ProgressMapper stubs ──
#include "ProgressMapper.h"

KOReaderPosition ProgressMapper::toKOReader(const std::shared_ptr<Epub>&, const CrossPointPosition& pos) {
    float pct = (pos.totalPages > 0) ? static_cast<float>(pos.pageNumber) / pos.totalPages : 0.0f;
    return {"/body/DocFragment[1]/body", pct};
}

CrossPointPosition ProgressMapper::toCrossPoint(const std::shared_ptr<Epub>&, const KOReaderPosition&,
                                                  int, int) {
    return {0, 0, 1};
}

std::string ProgressMapper::generateXPath(int spineIndex, int, int) {
    return "/body/DocFragment[" + std::to_string(spineIndex + 1) + "]/body";
}

// ── QrUtils stubs (QR code generation not available in simulator) ──
#include "util/QrUtils.h"
#include <GfxRenderer.h>

namespace QrUtils {
void drawQrCode(const GfxRenderer& renderer, const Rect& bounds, const std::string&) {
    // Draw a placeholder box instead of a real QR code
    const_cast<GfxRenderer&>(renderer).drawRect(bounds.x, bounds.y, bounds.width, bounds.height, 2, true);
    const_cast<GfxRenderer&>(renderer).drawText(0, bounds.x + 10, bounds.y + bounds.height / 2,
                                                  "QR N/A in sim", true);
}
}  // namespace QrUtils

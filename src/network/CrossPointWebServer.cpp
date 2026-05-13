#include "CrossPointWebServer.h"

#include <ArduinoJson.h>
#include <Epub.h>
#include <FsHelpers.h>
#include <HalStorage.h>
#include <Logging.h>
#include <WiFi.h>
#include <esp_task_wdt.h>

#include <algorithm>

#include "CrossPointSettings.h"
#include "SdCardFontGlobals.h"
#include "SettingsList.h"
#include "WebDAVHandler.h"
#include "html/FilesPageHtml.generated.h"
#include "html/FontBuilderHtml.generated.h"
#include "html/FontsPageHtml.generated.h"
#include "html/HomePageHtml.generated.h"
#include "html/SettingsPageHtml.generated.h"

namespace {
// Folders/files to hide from the web interface file browser
// Note: Items starting with "." are automatically hidden
const char* HIDDEN_ITEMS[] = {"System Volume Information", "XTCache"};
constexpr size_t HIDDEN_ITEMS_COUNT = sizeof(HIDDEN_ITEMS) / sizeof(HIDDEN_ITEMS[0]);
constexpr uint16_t UDP_PORTS[] = {54982, 48123, 39001, 44044, 59678};
constexpr uint16_t LOCAL_UDP_PORT = 8134;

// Static pointer for WebSocket callback (WebSocketsServer requires C-style callback)
CrossPointWebServer* wsInstance = nullptr;

// WebSocket upload state
FsFile wsUploadFile;
String wsUploadFileName;
String wsUploadPath;
size_t wsUploadSize = 0;
size_t wsUploadReceived = 0;
unsigned long wsUploadStartTime = 0;
bool wsUploadInProgress = false;
String wsLastCompleteName;
size_t wsLastCompleteSize = 0;
unsigned long wsLastCompleteAt = 0;

// Helper function to clear epub cache after upload
void clearEpubCacheIfNeeded(const String& filePath) {
  // Only clear cache for .epub files
  if (FsHelpers::hasEpubExtension(filePath)) {
    Epub(filePath.c_str(), "/.crosspoint").clearCache();
    LOG_DBG("WEB", "Cleared epub cache for: %s", filePath.c_str());
  }
}

String normalizeWebPath(const String& inputPath) {
  if (inputPath.isEmpty() || inputPath == "/") {
    return "/";
  }
  std::string normalized = FsHelpers::normalisePath(inputPath.c_str());
  String result = normalized.c_str();
  if (result.isEmpty()) {
    return "/";
  }
  if (!result.startsWith("/")) {
    result = "/" + result;
  }
  if (result.length() > 1 && result.endsWith("/")) {
    result = result.substring(0, result.length() - 1);
  }
  return result;
}

bool isProtectedItemName(const String& name) {
  if (name.startsWith(".")) {
    return true;
  }
  for (size_t i = 0; i < HIDDEN_ITEMS_COUNT; i++) {
    if (name.equals(HIDDEN_ITEMS[i])) {
      return true;
    }
  }
  return false;
}
}  // namespace

// Forward declaration so the `/fonts` route lambda in begin() can reference
// this helper, which is defined later in the file (~line 320).
static void sendHtmlContent(WebServer* server, const char* data, size_t len);

// File listing page template - now using generated headers:
// - HomePageHtml (from html/HomePage.html)
// - FilesPageHeaderHtml (from html/FilesPageHeader.html)
// - FilesPageFooterHtml (from html/FilesPageFooter.html)
CrossPointWebServer::CrossPointWebServer() {}

CrossPointWebServer::~CrossPointWebServer() { stop(); }

void CrossPointWebServer::begin() {
  if (running) {
    LOG_DBG("WEB", "Web server already running");
    return;
  }

  // Check if we have a valid network connection (either STA connected or AP mode)
  const wifi_mode_t wifiMode = WiFi.getMode();
  const bool isStaConnected = (wifiMode & WIFI_MODE_STA) && (WiFi.status() == WL_CONNECTED);
  const bool isInApMode = (wifiMode & WIFI_MODE_AP) && (WiFi.softAPgetStationNum() >= 0);  // AP is running

  if (!isStaConnected && !isInApMode) {
    LOG_DBG("WEB", "Cannot start webserver - no valid network (mode=%d, status=%d)", wifiMode, WiFi.status());
    return;
  }

  // Store AP mode flag for later use (e.g., in handleStatus)
  apMode = isInApMode;

  LOG_DBG("WEB", "[MEM] Free heap before begin: %d bytes", ESP.getFreeHeap());
  LOG_DBG("WEB", "Network mode: %s", apMode ? "AP" : "STA");

  LOG_DBG("WEB", "Creating web server on port %d...", port);
  server.reset(new WebServer(port));

  // Disable WiFi sleep to improve responsiveness and prevent 'unreachable' errors.
  // This is critical for reliable web server operation on ESP32.
  WiFi.setSleep(false);

  // Note: WebServer class doesn't have setNoDelay() in the standard ESP32 library.
  // We rely on disabling WiFi sleep for responsiveness.

  LOG_DBG("WEB", "[MEM] Free heap after WebServer allocation: %d bytes", ESP.getFreeHeap());

  if (!server) {
    LOG_ERR("WEB", "Failed to create WebServer!");
    return;
  }

  // Setup routes
  LOG_DBG("WEB", "Setting up routes...");
  server->on("/", HTTP_GET, [this] { handleRoot(); });
  server->on("/files", HTTP_GET, [this] { handleFileList(); });

  server->on("/api/status", HTTP_GET, [this] { handleStatus(); });
  server->on("/api/files", HTTP_GET, [this] { handleFileListData(); });
  server->on("/download", HTTP_GET, [this] { handleDownload(); });

  // Upload endpoint with special handling for multipart form data
  server->on("/upload", HTTP_POST, [this] { handleUploadPost(upload); }, [this] { handleUpload(upload); });

  // Create folder endpoint
  server->on("/mkdir", HTTP_POST, [this] { handleCreateFolder(); });

  // Rename file endpoint
  server->on("/rename", HTTP_POST, [this] { handleRename(); });

  // Move file endpoint
  server->on("/move", HTTP_POST, [this] { handleMove(); });

  // Delete file/folder endpoint
  server->on("/delete", HTTP_POST, [this] { handleDelete(); });

  // Settings endpoints
  server->on("/settings", HTTP_GET, [this] { handleSettingsPage(); });
  server->on("/api/settings", HTTP_GET, [this] { handleGetSettings(); });
  server->on("/api/settings", HTTP_POST, [this] { handlePostSettings(); });

  // SD card font upload — drop-zone web UI + multipart upload endpoint. The
  // server auto-routes uploaded `.cpfont` files to /fonts/<Family>/ based on
  // the parsed filename. Pairs with /api/fonts/list (registry snapshot) and
  // /api/fonts/delete (remove a family directory).
  server->on("/fonts", HTTP_GET,
             [this] { sendHtmlContent(server.get(), FontsPageHtml, sizeof(FontsPageHtml)); });
  // In-browser TTF/OTF → .cpfont builder.  Same standalone page as
  // tools/web-font-builder/index.html, embedded so users on the device's WiFi
  // can convert + upload in one round trip without needing the source repo.
  server->on("/fonts/builder", HTTP_GET,
             [this] { sendHtmlContent(server.get(), FontBuilderHtml, sizeof(FontBuilderHtml)); });
  server->on(
      "/api/fonts/upload", HTTP_POST, [this] { handleFontUploadPost(fontUpload); },
      [this] { handleFontUpload(fontUpload); });
  server->on("/api/fonts/list", HTTP_GET, [this] { handleFontList(); });
  server->on("/api/fonts/delete", HTTP_POST, [this] { handleFontDelete(); });

  server->onNotFound([this] { handleNotFound(); });
  LOG_DBG("WEB", "[MEM] Free heap after route setup: %d bytes", ESP.getFreeHeap());

  // Collect WebDAV headers and register handler
  const char* davHeaders[] = {"Depth", "Destination", "Overwrite", "If", "Lock-Token", "Timeout"};
  server->collectHeaders(davHeaders, 6);
  server->addHandler(new WebDAVHandler());  // Note: WebDAVHandler will be deleted by WebServer when server is stopped
  LOG_DBG("WEB", "WebDAV handler initialized");

  server->begin();

  // Start WebSocket server for fast binary uploads
  LOG_DBG("WEB", "Starting WebSocket server on port %d...", wsPort);
  wsServer.reset(new WebSocketsServer(wsPort));
  wsInstance = const_cast<CrossPointWebServer*>(this);
  wsServer->begin();
  wsServer->onEvent(wsEventCallback);
  LOG_DBG("WEB", "WebSocket server started");

  udpActive = udp.begin(LOCAL_UDP_PORT);
  LOG_DBG("WEB", "Discovery UDP %s on port %d", udpActive ? "enabled" : "failed", LOCAL_UDP_PORT);

  running = true;

  LOG_DBG("WEB", "Web server started on port %d", port);
  // Show the correct IP based on network mode
  const String ipAddr = apMode ? WiFi.softAPIP().toString() : WiFi.localIP().toString();
  LOG_DBG("WEB", "Access at http://%s/", ipAddr.c_str());
  LOG_DBG("WEB", "WebSocket at ws://%s:%d/", ipAddr.c_str(), wsPort);
  LOG_DBG("WEB", "[MEM] Free heap after server.begin(): %d bytes", ESP.getFreeHeap());
}

void CrossPointWebServer::stop() {
  if (!running || !server) {
    LOG_DBG("WEB", "stop() called but already stopped (running=%d, server=%p)", running, server.get());
    return;
  }

  LOG_DBG("WEB", "STOP INITIATED - setting running=false first");
  running = false;  // Set this FIRST to prevent handleClient from using server

  LOG_DBG("WEB", "[MEM] Free heap before stop: %d bytes", ESP.getFreeHeap());

  // Close any in-progress WebSocket upload
  if (wsUploadInProgress && wsUploadFile) {
    wsUploadFile.close();
    wsUploadInProgress = false;
  }

  // Stop WebSocket server
  if (wsServer) {
    LOG_DBG("WEB", "Stopping WebSocket server...");
    wsServer->close();
    wsServer.reset();
    wsInstance = nullptr;
    LOG_DBG("WEB", "WebSocket server stopped");
  }

  if (udpActive) {
    udp.stop();
    udpActive = false;
  }

  // Brief delay to allow any in-flight handleClient() calls to complete
  delay(20);

  server->stop();
  LOG_DBG("WEB", "[MEM] Free heap after server->stop(): %d bytes", ESP.getFreeHeap());

  // Brief delay before deletion
  delay(10);

  server.reset();
  LOG_DBG("WEB", "Web server stopped and deleted");
  LOG_DBG("WEB", "[MEM] Free heap after delete server: %d bytes", ESP.getFreeHeap());

  // Note: Static upload variables (uploadFileName, uploadPath, uploadError) are declared
  // later in the file and will be cleared when they go out of scope or on next upload
  LOG_DBG("WEB", "[MEM] Free heap final: %d bytes", ESP.getFreeHeap());
}

void CrossPointWebServer::handleClient() {
  static unsigned long lastDebugPrint = 0;

  // Check running flag FIRST before accessing server
  if (!running) {
    return;
  }

  // Double-check server pointer is valid
  if (!server) {
    LOG_DBG("WEB", "WARNING: handleClient called with null server!");
    return;
  }

  // Print debug every 10 seconds to confirm handleClient is being called
  if (millis() - lastDebugPrint > 10000) {
    LOG_DBG("WEB", "handleClient active, server running on port %d", port);
    lastDebugPrint = millis();
  }

  server->handleClient();

  // Handle WebSocket events
  if (wsServer) {
    wsServer->loop();
  }

  // Respond to discovery broadcasts
  if (udpActive) {
    int packetSize = udp.parsePacket();
    if (packetSize > 0) {
      char buffer[16];
      int len = udp.read(buffer, sizeof(buffer) - 1);
      if (len > 0) {
        buffer[len] = '\0';
        if (strcmp(buffer, "hello") == 0) {
          String hostname = WiFi.getHostname();
          if (hostname.isEmpty()) {
            hostname = "crosspoint";
          }
          String message = "crosspoint (on " + hostname + ");" + String(wsPort);
          udp.beginPacket(udp.remoteIP(), udp.remotePort());
          udp.write(reinterpret_cast<const uint8_t*>(message.c_str()), message.length());
          udp.endPacket();
        }
      }
    }
  }
}

CrossPointWebServer::WsUploadStatus CrossPointWebServer::getWsUploadStatus() const {
  WsUploadStatus status;
  status.inProgress = wsUploadInProgress;
  status.received = wsUploadReceived;
  status.total = wsUploadSize;
  status.filename = wsUploadFileName.c_str();
  status.lastCompleteName = wsLastCompleteName.c_str();
  status.lastCompleteSize = wsLastCompleteSize;
  status.lastCompleteAt = wsLastCompleteAt;
  return status;
}

static void sendHtmlContent(WebServer* server, const char* data, size_t len) {
  server->sendHeader("Content-Encoding", "gzip");
  server->send_P(200, "text/html", data, len);
}

void CrossPointWebServer::handleRoot() const {
  sendHtmlContent(server.get(), HomePageHtml, sizeof(HomePageHtml));
  LOG_DBG("WEB", "Served root page");
}

void CrossPointWebServer::handleNotFound() const {
  String message = "404 Not Found\n\n";
  message += "URI: " + server->uri() + "\n";
  server->send(404, "text/plain", message);
}

void CrossPointWebServer::handleStatus() const {
  // Get correct IP based on AP vs STA mode
  const String ipAddr = apMode ? WiFi.softAPIP().toString() : WiFi.localIP().toString();

  JsonDocument doc;
  doc["version"] = CROSSPOINT_VERSION;
  doc["ip"] = ipAddr;
  doc["mode"] = apMode ? "AP" : "STA";
  doc["rssi"] = apMode ? 0 : WiFi.RSSI();
  doc["freeHeap"] = ESP.getFreeHeap();
  doc["uptime"] = millis() / 1000;

  String json;
  serializeJson(doc, json);
  server->send(200, "application/json", json);
}

void CrossPointWebServer::scanFiles(const char* path, const std::function<void(FileInfo)>& callback) const {
  FsFile root = Storage.open(path);
  if (!root) {
    LOG_DBG("WEB", "Failed to open directory: %s", path);
    return;
  }

  if (!root.isDirectory()) {
    LOG_DBG("WEB", "Not a directory: %s", path);
    root.close();
    return;
  }

  LOG_DBG("WEB", "Scanning files in: %s", path);

  FsFile file = root.openNextFile();
  char name[500];
  while (file) {
    file.getName(name, sizeof(name));
    auto fileName = String(name);

    // Skip hidden items (starting with ".")
    bool shouldHide = fileName.startsWith(".");

    // Check against explicitly hidden items list
    if (!shouldHide) {
      for (size_t i = 0; i < HIDDEN_ITEMS_COUNT; i++) {
        if (fileName.equals(HIDDEN_ITEMS[i])) {
          shouldHide = true;
          break;
        }
      }
    }

    if (!shouldHide) {
      FileInfo info;
      info.name = fileName;
      info.isDirectory = file.isDirectory();

      if (info.isDirectory) {
        info.size = 0;
        info.isEpub = false;
      } else {
        info.size = file.size();
        info.isEpub = isEpubFile(info.name);
      }

      callback(info);
    }

    file.close();
    yield();               // Yield to allow WiFi and other tasks to process during long scans
    esp_task_wdt_reset();  // Reset watchdog to prevent timeout on large directories
    file = root.openNextFile();
  }
  root.close();
}

bool CrossPointWebServer::isEpubFile(const String& filename) const { return FsHelpers::hasEpubExtension(filename); }

void CrossPointWebServer::handleFileList() const {
  sendHtmlContent(server.get(), FilesPageHtml, sizeof(FilesPageHtml));
}

void CrossPointWebServer::handleFileListData() const {
  // Get current path from query string (default to root)
  String currentPath = "/";
  if (server->hasArg("path")) {
    currentPath = server->arg("path");
    // Ensure path starts with /
    if (!currentPath.startsWith("/")) {
      currentPath = "/" + currentPath;
    }
    // Remove trailing slash unless it's root
    if (currentPath.length() > 1 && currentPath.endsWith("/")) {
      currentPath = currentPath.substring(0, currentPath.length() - 1);
    }
  }

  server->setContentLength(CONTENT_LENGTH_UNKNOWN);
  server->send(200, "application/json", "");
  server->sendContent("[");
  char output[512];
  constexpr size_t outputSize = sizeof(output);
  bool seenFirst = false;
  JsonDocument doc;

  scanFiles(currentPath.c_str(), [this, &output, &doc, seenFirst](const FileInfo& info) mutable {
    doc.clear();
    doc["name"] = info.name;
    doc["size"] = info.size;
    doc["isDirectory"] = info.isDirectory;
    doc["isEpub"] = info.isEpub;

    const size_t written = serializeJson(doc, output, outputSize);
    if (written >= outputSize) {
      // JSON output truncated; skip this entry to avoid sending malformed JSON
      LOG_DBG("WEB", "Skipping file entry with oversized JSON for name: %s", info.name.c_str());
      return;
    }

    if (seenFirst) {
      server->sendContent(",");
    } else {
      seenFirst = true;
    }
    server->sendContent(output);
  });
  server->sendContent("]");
  // End of streamed response, empty chunk to signal client
  server->sendContent("");
  LOG_DBG("WEB", "Served file listing page for path: %s", currentPath.c_str());
}

void CrossPointWebServer::handleDownload() const {
  if (!server->hasArg("path")) {
    server->send(400, "text/plain", "Missing path");
    return;
  }

  String itemPath = server->arg("path");
  if (itemPath.isEmpty() || itemPath == "/") {
    server->send(400, "text/plain", "Invalid path");
    return;
  }
  if (!itemPath.startsWith("/")) {
    itemPath = "/" + itemPath;
  }

  const String itemName = itemPath.substring(itemPath.lastIndexOf('/') + 1);
  if (itemName.startsWith(".")) {
    server->send(403, "text/plain", "Cannot access system files");
    return;
  }
  for (size_t i = 0; i < HIDDEN_ITEMS_COUNT; i++) {
    if (itemName.equals(HIDDEN_ITEMS[i])) {
      server->send(403, "text/plain", "Cannot access protected items");
      return;
    }
  }

  if (!Storage.exists(itemPath.c_str())) {
    server->send(404, "text/plain", "Item not found");
    return;
  }

  FsFile file = Storage.open(itemPath.c_str());
  if (!file) {
    server->send(500, "text/plain", "Failed to open file");
    return;
  }
  if (file.isDirectory()) {
    file.close();
    server->send(400, "text/plain", "Path is a directory");
    return;
  }

  String contentType = "application/octet-stream";
  if (isEpubFile(itemPath)) {
    contentType = "application/epub+zip";
  }

  char nameBuf[128] = {0};
  String filename = "download";
  if (file.getName(nameBuf, sizeof(nameBuf))) {
    filename = nameBuf;
  }

  server->setContentLength(file.size());
  server->sendHeader("Content-Disposition", "attachment; filename=\"" + filename + "\"");
  server->send(200, contentType.c_str(), "");

  NetworkClient client = server->client();
  client.write(file);
  file.close();
}

// Diagnostic counters for upload performance analysis
static unsigned long uploadStartTime = 0;
static unsigned long totalWriteTime = 0;
static size_t writeCount = 0;

static bool flushUploadBuffer(CrossPointWebServer::UploadState& state) {
  if (state.bufferPos > 0 && state.file) {
    esp_task_wdt_reset();  // Reset watchdog before potentially slow SD write
    const unsigned long writeStart = millis();
    const size_t written = state.file.write(state.buffer.data(), state.bufferPos);
    totalWriteTime += millis() - writeStart;
    writeCount++;
    esp_task_wdt_reset();  // Reset watchdog after SD write

    if (written != state.bufferPos) {
      LOG_DBG("WEB", "[UPLOAD] Buffer flush failed: expected %d, wrote %d", state.bufferPos, written);
      state.bufferPos = 0;
      return false;
    }
    state.bufferPos = 0;
  }
  return true;
}

void CrossPointWebServer::handleUpload(UploadState& state) const {
  static size_t lastLoggedSize = 0;

  // Reset watchdog at start of every upload callback - HTTP parsing can be slow
  esp_task_wdt_reset();

  // Safety check: ensure server is still valid
  if (!running || !server) {
    LOG_DBG("WEB", "[UPLOAD] ERROR: handleUpload called but server not running!");
    return;
  }

  const HTTPUpload& upload = server->upload();

  if (upload.status == UPLOAD_FILE_START) {
    // Reset watchdog - this is the critical 1% crash point
    esp_task_wdt_reset();

    state.fileName = upload.filename;
    state.size = 0;
    state.success = false;
    state.error = "";
    uploadStartTime = millis();
    lastLoggedSize = 0;
    state.bufferPos = 0;
    totalWriteTime = 0;
    writeCount = 0;

    // Get upload path from query parameter (defaults to root if not specified)
    // Note: We use query parameter instead of form data because multipart form
    // fields aren't available until after file upload completes
    if (server->hasArg("path")) {
      state.path = server->arg("path");
      // Ensure path starts with /
      if (!state.path.startsWith("/")) {
        state.path = "/" + state.path;
      }
      // Remove trailing slash unless it's root
      if (state.path.length() > 1 && state.path.endsWith("/")) {
        state.path = state.path.substring(0, state.path.length() - 1);
      }
    } else {
      state.path = "/";
    }

    LOG_DBG("WEB", "[UPLOAD] START: %s to path: %s", state.fileName.c_str(), state.path.c_str());
    LOG_DBG("WEB", "[UPLOAD] Free heap: %d bytes", ESP.getFreeHeap());

    // Create file path
    String filePath = state.path;
    if (!filePath.endsWith("/")) filePath += "/";
    filePath += state.fileName;

    // Check if file already exists - SD operations can be slow
    esp_task_wdt_reset();
    if (Storage.exists(filePath.c_str())) {
      LOG_DBG("WEB", "[UPLOAD] Overwriting existing file: %s", filePath.c_str());
      esp_task_wdt_reset();
      Storage.remove(filePath.c_str());
    }

    // Open file for writing - this can be slow due to FAT cluster allocation
    esp_task_wdt_reset();
    if (!Storage.openFileForWrite("WEB", filePath, state.file)) {
      state.error = "Failed to create file on SD card";
      LOG_DBG("WEB", "[UPLOAD] FAILED to create file: %s", filePath.c_str());
      return;
    }
    esp_task_wdt_reset();

    LOG_DBG("WEB", "[UPLOAD] File created successfully: %s", filePath.c_str());
  } else if (upload.status == UPLOAD_FILE_WRITE) {
    if (state.file && state.error.isEmpty()) {
      // Buffer incoming data and flush when buffer is full
      // This reduces SD card write operations and improves throughput
      const uint8_t* data = upload.buf;
      size_t remaining = upload.currentSize;

      while (remaining > 0) {
        const size_t space = UploadState::UPLOAD_BUFFER_SIZE - state.bufferPos;
        const size_t toCopy = (remaining < space) ? remaining : space;

        memcpy(state.buffer.data() + state.bufferPos, data, toCopy);
        state.bufferPos += toCopy;
        data += toCopy;
        remaining -= toCopy;

        // Flush buffer when full
        if (state.bufferPos >= UploadState::UPLOAD_BUFFER_SIZE) {
          if (!flushUploadBuffer(state)) {
            state.error = "Failed to write to SD card - disk may be full";
            state.file.close();
            return;
          }
        }
      }

      state.size += upload.currentSize;

      // Log progress every 100KB
      if (state.size - lastLoggedSize >= 102400) {
        const unsigned long elapsed = millis() - uploadStartTime;
        const float kbps = (elapsed > 0) ? (state.size / 1024.0) / (elapsed / 1000.0) : 0;
        LOG_DBG("WEB", "[UPLOAD] %d bytes (%.1f KB), %.1f KB/s, %d writes", state.size, state.size / 1024.0, kbps,
                writeCount);
        lastLoggedSize = state.size;
      }
    }
  } else if (upload.status == UPLOAD_FILE_END) {
    if (state.file) {
      // Flush any remaining buffered data
      if (!flushUploadBuffer(state)) {
        state.error = "Failed to write final data to SD card";
      }
      state.file.close();

      if (state.error.isEmpty()) {
        state.success = true;
        const unsigned long elapsed = millis() - uploadStartTime;
        const float avgKbps = (elapsed > 0) ? (state.size / 1024.0) / (elapsed / 1000.0) : 0;
        const float writePercent = (elapsed > 0) ? (totalWriteTime * 100.0 / elapsed) : 0;
        LOG_DBG("WEB", "[UPLOAD] Complete: %s (%d bytes in %lu ms, avg %.1f KB/s)", state.fileName.c_str(), state.size,
                elapsed, avgKbps);
        LOG_DBG("WEB", "[UPLOAD] Diagnostics: %d writes, total write time: %lu ms (%.1f%%)", writeCount, totalWriteTime,
                writePercent);

        // Clear epub cache to prevent stale metadata issues when overwriting files
        String filePath = state.path;
        if (!filePath.endsWith("/")) filePath += "/";
        filePath += state.fileName;
        clearEpubCacheIfNeeded(filePath);
      }
    }
  } else if (upload.status == UPLOAD_FILE_ABORTED) {
    state.bufferPos = 0;  // Discard buffered data
    if (state.file) {
      state.file.close();
      // Try to delete the incomplete file
      String filePath = state.path;
      if (!filePath.endsWith("/")) filePath += "/";
      filePath += state.fileName;
      Storage.remove(filePath.c_str());
    }
    state.error = "Upload aborted";
    LOG_DBG("WEB", "Upload aborted");
  }
}

void CrossPointWebServer::handleUploadPost(UploadState& state) const {
  if (state.success) {
    server->send(200, "text/plain", "File uploaded successfully: " + state.fileName);
  } else {
    const String error = state.error.isEmpty() ? "Unknown error during upload" : state.error;
    server->send(400, "text/plain", error);
  }
}

void CrossPointWebServer::handleCreateFolder() const {
  // Get folder name from form data
  if (!server->hasArg("name")) {
    server->send(400, "text/plain", "Missing folder name");
    return;
  }

  const String folderName = server->arg("name");

  // Validate folder name
  if (folderName.isEmpty()) {
    server->send(400, "text/plain", "Folder name cannot be empty");
    return;
  }

  // Get parent path
  String parentPath = "/";
  if (server->hasArg("path")) {
    parentPath = server->arg("path");
    if (!parentPath.startsWith("/")) {
      parentPath = "/" + parentPath;
    }
    if (parentPath.length() > 1 && parentPath.endsWith("/")) {
      parentPath = parentPath.substring(0, parentPath.length() - 1);
    }
  }

  // Build full folder path
  String folderPath = parentPath;
  if (!folderPath.endsWith("/")) folderPath += "/";
  folderPath += folderName;

  LOG_DBG("WEB", "Creating folder: %s", folderPath.c_str());

  // Check if already exists
  if (Storage.exists(folderPath.c_str())) {
    server->send(400, "text/plain", "Folder already exists");
    return;
  }

  // Create the folder
  if (Storage.mkdir(folderPath.c_str())) {
    LOG_DBG("WEB", "Folder created successfully: %s", folderPath.c_str());
    server->send(200, "text/plain", "Folder created: " + folderName);
  } else {
    LOG_DBG("WEB", "Failed to create folder: %s", folderPath.c_str());
    server->send(500, "text/plain", "Failed to create folder");
  }
}

void CrossPointWebServer::handleRename() const {
  if (!server->hasArg("path") || !server->hasArg("name")) {
    server->send(400, "text/plain", "Missing path or new name");
    return;
  }

  String itemPath = normalizeWebPath(server->arg("path"));
  String newName = server->arg("name");
  newName.trim();

  if (itemPath.isEmpty() || itemPath == "/") {
    server->send(400, "text/plain", "Invalid path");
    return;
  }
  if (newName.isEmpty()) {
    server->send(400, "text/plain", "New name cannot be empty");
    return;
  }
  if (newName.indexOf('/') >= 0 || newName.indexOf('\\') >= 0) {
    server->send(400, "text/plain", "Invalid file name");
    return;
  }
  if (isProtectedItemName(newName)) {
    server->send(403, "text/plain", "Cannot rename to protected name");
    return;
  }

  const String itemName = itemPath.substring(itemPath.lastIndexOf('/') + 1);
  if (isProtectedItemName(itemName)) {
    server->send(403, "text/plain", "Cannot rename protected item");
    return;
  }
  if (newName == itemName) {
    server->send(200, "text/plain", "Name unchanged");
    return;
  }

  if (!Storage.exists(itemPath.c_str())) {
    server->send(404, "text/plain", "Item not found");
    return;
  }

  FsFile file = Storage.open(itemPath.c_str());
  if (!file) {
    server->send(500, "text/plain", "Failed to open file");
    return;
  }
  if (file.isDirectory()) {
    file.close();
    server->send(400, "text/plain", "Only files can be renamed");
    return;
  }

  String parentPath = itemPath.substring(0, itemPath.lastIndexOf('/'));
  if (parentPath.isEmpty()) {
    parentPath = "/";
  }
  String newPath = parentPath;
  if (!newPath.endsWith("/")) {
    newPath += "/";
  }
  newPath += newName;

  if (Storage.exists(newPath.c_str())) {
    file.close();
    server->send(409, "text/plain", "Target already exists");
    return;
  }

  clearEpubCacheIfNeeded(itemPath);
  const bool success = file.rename(newPath.c_str());
  file.close();

  if (success) {
    LOG_DBG("WEB", "Renamed file: %s -> %s", itemPath.c_str(), newPath.c_str());
    server->send(200, "text/plain", "Renamed successfully");
  } else {
    LOG_ERR("WEB", "Failed to rename file: %s -> %s", itemPath.c_str(), newPath.c_str());
    server->send(500, "text/plain", "Failed to rename file");
  }
}

void CrossPointWebServer::handleMove() const {
  if (!server->hasArg("path") || !server->hasArg("dest")) {
    server->send(400, "text/plain", "Missing path or destination");
    return;
  }

  String itemPath = normalizeWebPath(server->arg("path"));
  String destPath = normalizeWebPath(server->arg("dest"));

  if (itemPath.isEmpty() || itemPath == "/") {
    server->send(400, "text/plain", "Invalid path");
    return;
  }
  if (destPath.isEmpty()) {
    server->send(400, "text/plain", "Invalid destination");
    return;
  }

  const String itemName = itemPath.substring(itemPath.lastIndexOf('/') + 1);
  if (isProtectedItemName(itemName)) {
    server->send(403, "text/plain", "Cannot move protected item");
    return;
  }
  if (destPath != "/") {
    const String destName = destPath.substring(destPath.lastIndexOf('/') + 1);
    if (isProtectedItemName(destName)) {
      server->send(403, "text/plain", "Cannot move into protected folder");
      return;
    }
  }

  if (!Storage.exists(itemPath.c_str())) {
    server->send(404, "text/plain", "Item not found");
    return;
  }

  FsFile file = Storage.open(itemPath.c_str());
  if (!file) {
    server->send(500, "text/plain", "Failed to open file");
    return;
  }
  if (file.isDirectory()) {
    file.close();
    server->send(400, "text/plain", "Only files can be moved");
    return;
  }

  if (!Storage.exists(destPath.c_str())) {
    file.close();
    server->send(404, "text/plain", "Destination not found");
    return;
  }
  FsFile destDir = Storage.open(destPath.c_str());
  if (!destDir || !destDir.isDirectory()) {
    if (destDir) {
      destDir.close();
    }
    file.close();
    server->send(400, "text/plain", "Destination is not a folder");
    return;
  }
  destDir.close();

  String newPath = destPath;
  if (!newPath.endsWith("/")) {
    newPath += "/";
  }
  newPath += itemName;

  if (newPath == itemPath) {
    file.close();
    server->send(200, "text/plain", "Already in destination");
    return;
  }
  if (Storage.exists(newPath.c_str())) {
    file.close();
    server->send(409, "text/plain", "Target already exists");
    return;
  }

  clearEpubCacheIfNeeded(itemPath);
  const bool success = file.rename(newPath.c_str());
  file.close();

  if (success) {
    LOG_DBG("WEB", "Moved file: %s -> %s", itemPath.c_str(), newPath.c_str());
    server->send(200, "text/plain", "Moved successfully");
  } else {
    LOG_ERR("WEB", "Failed to move file: %s -> %s", itemPath.c_str(), newPath.c_str());
    server->send(500, "text/plain", "Failed to move file");
  }
}

void CrossPointWebServer::handleDelete() const {
  // Check if 'paths' argument is provided
  if (!server->hasArg("paths")) {
    server->send(400, "text/plain", "Missing paths");
    return;
  }

  // Parse paths
  String pathsArg = server->arg("paths");
  JsonDocument doc;
  DeserializationError error = deserializeJson(doc, pathsArg);
  if (error) {
    server->send(400, "text/plain", "Invalid paths format");
    return;
  }

  auto paths = doc.as<JsonArray>();
  if (paths.isNull() || paths.size() == 0) {
    server->send(400, "text/plain", "No paths provided");
    return;
  }

  // Iterate over paths and delete each item
  bool allSuccess = true;
  String failedItems;

  for (const auto& p : paths) {
    auto itemPath = p.as<String>();

    // Validate path
    if (itemPath.isEmpty() || itemPath == "/") {
      failedItems += itemPath + " (cannot delete root); ";
      allSuccess = false;
      continue;
    }

    // Ensure path starts with /
    if (!itemPath.startsWith("/")) {
      itemPath = "/" + itemPath;
    }

    // Security check: prevent deletion of protected items
    const String itemName = itemPath.substring(itemPath.lastIndexOf('/') + 1);

    // Hidden/system files are protected
    if (itemName.startsWith(".")) {
      failedItems += itemPath + " (hidden/system file); ";
      allSuccess = false;
      continue;
    }

    // Check against explicitly protected items
    bool isProtected = false;
    for (size_t i = 0; i < HIDDEN_ITEMS_COUNT; i++) {
      if (itemName.equals(HIDDEN_ITEMS[i])) {
        isProtected = true;
        break;
      }
    }
    if (isProtected) {
      failedItems += itemPath + " (protected file); ";
      allSuccess = false;
      continue;
    }

    // Check if item exists
    if (!Storage.exists(itemPath.c_str())) {
      failedItems += itemPath + " (not found); ";
      allSuccess = false;
      continue;
    }

    // Decide whether it's a directory or file by opening it
    bool success = false;
    FsFile f = Storage.open(itemPath.c_str());
    if (f && f.isDirectory()) {
      // For folders, ensure empty before removing
      FsFile entry = f.openNextFile();
      if (entry) {
        entry.close();
        f.close();
        failedItems += itemPath + " (folder not empty); ";
        allSuccess = false;
        continue;
      }
      f.close();
      success = Storage.rmdir(itemPath.c_str());
    } else {
      // It's a file (or couldn't open as dir) — remove file
      if (f) f.close();
      success = Storage.remove(itemPath.c_str());
      clearEpubCacheIfNeeded(itemPath);
    }

    if (!success) {
      failedItems += itemPath + " (deletion failed); ";
      allSuccess = false;
    }
  }

  if (allSuccess) {
    server->send(200, "text/plain", "All items deleted successfully");
  } else {
    server->send(500, "text/plain", "Failed to delete some items: " + failedItems);
  }
}

void CrossPointWebServer::handleSettingsPage() const {
  sendHtmlContent(server.get(), SettingsPageHtml, sizeof(SettingsPageHtml));
  LOG_DBG("WEB", "Served settings page");
}

void CrossPointWebServer::handleGetSettings() const {
  const auto& settings = getSettingsList();

  server->setContentLength(CONTENT_LENGTH_UNKNOWN);
  server->send(200, "application/json", "");
  server->sendContent("[");

  char output[512];
  constexpr size_t outputSize = sizeof(output);
  bool seenFirst = false;
  JsonDocument doc;

  for (const auto& s : settings) {
    if (!s.key) continue;  // Skip ACTION-only entries

    doc.clear();
    doc["key"] = s.key;
    doc["name"] = I18N.get(s.nameId);
    doc["category"] = I18N.get(s.category);

    switch (s.type) {
      case SettingType::TOGGLE: {
        doc["type"] = "toggle";
        if (s.valuePtr) {
          doc["value"] = static_cast<int>(SETTINGS.*(s.valuePtr));
        }
        break;
      }
      case SettingType::ENUM: {
        doc["type"] = "enum";
        if (s.valuePtr) {
          doc["value"] = static_cast<int>(SETTINGS.*(s.valuePtr));
        } else if (s.valueGetter) {
          doc["value"] = static_cast<int>(s.valueGetter());
        }
        JsonArray options = doc["options"].to<JsonArray>();
        for (const auto& opt : s.enumValues) {
          options.add(I18N.get(opt));
        }
        break;
      }
      case SettingType::VALUE: {
        doc["type"] = "value";
        if (s.valuePtr) {
          doc["value"] = static_cast<int>(SETTINGS.*(s.valuePtr));
        }
        doc["min"] = s.valueRange.min;
        doc["max"] = s.valueRange.max;
        doc["step"] = s.valueRange.step;
        break;
      }
      case SettingType::STRING: {
        doc["type"] = "string";
        if (s.stringGetter) {
          doc["value"] = s.stringGetter();
        } else if (s.stringOffset > 0) {
          doc["value"] = reinterpret_cast<const char*>(&SETTINGS) + s.stringOffset;
        }
        break;
      }
      default:
        continue;
    }

    const size_t written = serializeJson(doc, output, outputSize);
    if (written >= outputSize) {
      LOG_DBG("WEB", "Skipping oversized setting JSON for: %s", s.key);
      continue;
    }

    if (seenFirst) {
      server->sendContent(",");
    } else {
      seenFirst = true;
    }
    server->sendContent(output);
  }

  server->sendContent("]");
  server->sendContent("");
  LOG_DBG("WEB", "Served settings API");
}

void CrossPointWebServer::handlePostSettings() {
  if (!server->hasArg("plain")) {
    server->send(400, "text/plain", "Missing JSON body");
    return;
  }

  const String body = server->arg("plain");
  JsonDocument doc;
  const DeserializationError err = deserializeJson(doc, body);
  if (err) {
    server->send(400, "text/plain", String("Invalid JSON: ") + err.c_str());
    return;
  }

  const auto& settings = getSettingsList();
  int applied = 0;

  for (const auto& s : settings) {
    if (!s.key) continue;
    if (!doc[s.key].is<JsonVariant>()) continue;

    switch (s.type) {
      case SettingType::TOGGLE: {
        const int val = doc[s.key].as<int>() ? 1 : 0;
        if (s.valuePtr) {
          SETTINGS.*(s.valuePtr) = val;
        }
        applied++;
        break;
      }
      case SettingType::ENUM: {
        const int val = doc[s.key].as<int>();
        if (val >= 0 && val < static_cast<int>(s.enumValues.size())) {
          if (s.valuePtr) {
            SETTINGS.*(s.valuePtr) = static_cast<uint8_t>(val);
          } else if (s.valueSetter) {
            s.valueSetter(static_cast<uint8_t>(val));
          }
          applied++;
        }
        break;
      }
      case SettingType::VALUE: {
        const int val = doc[s.key].as<int>();
        if (val >= s.valueRange.min && val <= s.valueRange.max) {
          if (s.valuePtr) {
            SETTINGS.*(s.valuePtr) = static_cast<uint8_t>(val);
          }
          applied++;
        }
        break;
      }
      case SettingType::STRING: {
        const std::string val = doc[s.key].as<std::string>();
        if (s.stringSetter) {
          s.stringSetter(val);
        } else if (s.stringOffset > 0 && s.stringMaxLen > 0) {
          char* ptr = reinterpret_cast<char*>(&SETTINGS) + s.stringOffset;
          strncpy(ptr, val.c_str(), s.stringMaxLen - 1);
          ptr[s.stringMaxLen - 1] = '\0';
        }
        applied++;
        break;
      }
      default:
        break;
    }
  }

  SETTINGS.saveToFile();

  LOG_DBG("WEB", "Applied %d setting(s)", applied);
  server->send(200, "text/plain", String("Applied ") + String(applied) + " setting(s)");
}

// WebSocket callback trampoline
void CrossPointWebServer::wsEventCallback(uint8_t num, WStype_t type, uint8_t* payload, size_t length) {
  if (wsInstance) {
    wsInstance->onWebSocketEvent(num, type, payload, length);
  }
}

// WebSocket event handler for fast binary uploads
// Protocol:
//   1. Client sends TEXT message: "START:<filename>:<size>:<path>"
//   2. Client sends BINARY messages with file data chunks
//   3. Server sends TEXT "PROGRESS:<received>:<total>" after each chunk
//   4. Server sends TEXT "DONE" or "ERROR:<message>" when complete
void CrossPointWebServer::onWebSocketEvent(uint8_t num, WStype_t type, uint8_t* payload, size_t length) {
  switch (type) {
    case WStype_DISCONNECTED:
      LOG_DBG("WS", "Client %u disconnected", num);
      // Clean up any in-progress upload
      if (wsUploadInProgress && wsUploadFile) {
        wsUploadFile.close();
        // Delete incomplete file
        String filePath = wsUploadPath;
        if (!filePath.endsWith("/")) filePath += "/";
        filePath += wsUploadFileName;
        Storage.remove(filePath.c_str());
        LOG_DBG("WS", "Deleted incomplete upload: %s", filePath.c_str());
      }
      wsUploadInProgress = false;
      break;

    case WStype_CONNECTED: {
      LOG_DBG("WS", "Client %u connected", num);
      break;
    }

    case WStype_TEXT: {
      // Parse control messages
      String msg = String((char*)payload);
      LOG_DBG("WS", "Text from client %u: %s", num, msg.c_str());

      if (msg.startsWith("START:")) {
        // Parse: START:<filename>:<size>:<path>
        int firstColon = msg.indexOf(':', 6);
        int secondColon = msg.indexOf(':', firstColon + 1);

        if (firstColon > 0 && secondColon > 0) {
          wsUploadFileName = msg.substring(6, firstColon);
          wsUploadSize = msg.substring(firstColon + 1, secondColon).toInt();
          wsUploadPath = msg.substring(secondColon + 1);
          wsUploadReceived = 0;
          wsUploadStartTime = millis();

          // Ensure path is valid
          if (!wsUploadPath.startsWith("/")) wsUploadPath = "/" + wsUploadPath;
          if (wsUploadPath.length() > 1 && wsUploadPath.endsWith("/")) {
            wsUploadPath = wsUploadPath.substring(0, wsUploadPath.length() - 1);
          }

          // Build file path
          String filePath = wsUploadPath;
          if (!filePath.endsWith("/")) filePath += "/";
          filePath += wsUploadFileName;

          LOG_DBG("WS", "Starting upload: %s (%d bytes) to %s", wsUploadFileName.c_str(), wsUploadSize,
                  filePath.c_str());

          // Check if file exists and remove it
          esp_task_wdt_reset();
          if (Storage.exists(filePath.c_str())) {
            Storage.remove(filePath.c_str());
          }

          // Open file for writing
          esp_task_wdt_reset();
          if (!Storage.openFileForWrite("WS", filePath, wsUploadFile)) {
            wsServer->sendTXT(num, "ERROR:Failed to create file");
            wsUploadInProgress = false;
            return;
          }
          esp_task_wdt_reset();

          wsUploadInProgress = true;
          wsServer->sendTXT(num, "READY");
        } else {
          wsServer->sendTXT(num, "ERROR:Invalid START format");
        }
      }
      break;
    }

    case WStype_BIN: {
      if (!wsUploadInProgress || !wsUploadFile) {
        wsServer->sendTXT(num, "ERROR:No upload in progress");
        return;
      }

      // Write binary data directly to file
      esp_task_wdt_reset();
      size_t written = wsUploadFile.write(payload, length);
      esp_task_wdt_reset();

      if (written != length) {
        wsUploadFile.close();
        wsUploadInProgress = false;
        wsServer->sendTXT(num, "ERROR:Write failed - disk full?");
        return;
      }

      wsUploadReceived += written;

      // Send progress update (every 64KB or at end)
      static size_t lastProgressSent = 0;
      if (wsUploadReceived - lastProgressSent >= 65536 || wsUploadReceived >= wsUploadSize) {
        String progress = "PROGRESS:" + String(wsUploadReceived) + ":" + String(wsUploadSize);
        wsServer->sendTXT(num, progress);
        lastProgressSent = wsUploadReceived;
      }

      // Check if upload complete
      if (wsUploadReceived >= wsUploadSize) {
        wsUploadFile.close();
        wsUploadInProgress = false;

        wsLastCompleteName = wsUploadFileName;
        wsLastCompleteSize = wsUploadSize;
        wsLastCompleteAt = millis();

        unsigned long elapsed = millis() - wsUploadStartTime;
        float kbps = (elapsed > 0) ? (wsUploadSize / 1024.0) / (elapsed / 1000.0) : 0;

        LOG_DBG("WS", "Upload complete: %s (%d bytes in %lu ms, %.1f KB/s)", wsUploadFileName.c_str(), wsUploadSize,
                elapsed, kbps);

        // Clear epub cache to prevent stale metadata issues when overwriting files
        String filePath = wsUploadPath;
        if (!filePath.endsWith("/")) filePath += "/";
        filePath += wsUploadFileName;
        clearEpubCacheIfNeeded(filePath);

        wsServer->sendTXT(num, "DONE");
        lastProgressSent = 0;
      }
      break;
    }

    default:
      break;
  }
}

// ────────────────────────────────────────────────────────────────────────────
// SD card font upload
//
// Endpoints:
//   GET  /fonts             — drop-zone web UI (FontsPage.html)
//   POST /api/fonts/upload  — multipart upload, filename = <Family>_<size>.cpfont
//   GET  /api/fonts/list    — JSON of discovered families + sizes + selection
//   POST /api/fonts/delete  — ?family=<name> removes the entire family dir
//
// Filename convention: `<Family>_<size>.cpfont` (e.g. PKNakhonSawan_18.cpfont).
// The server parses the family name, mkdir's `/fonts/<Family>/` if needed, and
// writes the file there.  Mirrors the SdCardFontRegistry's discover layout
// exactly, so re-running discover() after upload picks up the new family.
// ────────────────────────────────────────────────────────────────────────────

namespace {
// Validate + parse a .cpfont filename like "PKNakhonSawan_18.cpfont".
// On success: outFamily = "PKNakhonSawan", outSize = 18, returns true.
// On failure (wrong extension, missing underscore, non-numeric size): false.
bool parseFontFilename(const String& filename, String& outFamily, uint8_t& outSize) {
  if (!filename.endsWith(".cpfont")) return false;
  const int extDot = filename.lastIndexOf(".cpfont");
  if (extDot <= 0) return false;
  const int lastUnderscore = filename.lastIndexOf('_', extDot);
  if (lastUnderscore <= 0 || lastUnderscore >= extDot - 1) return false;

  outFamily = filename.substring(0, lastUnderscore);
  const String sizeStr = filename.substring(lastUnderscore + 1, extDot);
  const long sizeVal = sizeStr.toInt();
  if (sizeVal < 1 || sizeVal > 255 || sizeStr.length() == 0) return false;

  // toInt returns 0 for non-numeric — guard against "_abc.cpfont"
  for (size_t i = 0; i < sizeStr.length(); i++) {
    if (sizeStr[i] < '0' || sizeStr[i] > '9') return false;
  }

  outSize = static_cast<uint8_t>(sizeVal);
  if (outFamily.length() == 0 || outFamily.length() > 31) return false;  // family name fits in SETTINGS field
  return true;
}
}  // namespace

void CrossPointWebServer::handleFontUpload(UploadState& state) const {
  esp_task_wdt_reset();
  if (!running || !server) return;

  const HTTPUpload& upload = server->upload();

  if (upload.status == UPLOAD_FILE_START) {
    esp_task_wdt_reset();
    state.fileName = upload.filename;
    state.size = 0;
    state.success = false;
    state.error = "";
    state.bufferPos = 0;

    // Parse filename to derive target family directory.
    String family;
    uint8_t pointSize = 0;
    if (!parseFontFilename(state.fileName, family, pointSize)) {
      state.error = "Filename must be <Family>_<size>.cpfont (e.g. Lexend_16.cpfont)";
      LOG_ERR("WEB", "[FONT] Bad filename: %s", state.fileName.c_str());
      return;
    }

    // Ensure /fonts/ and /fonts/<Family>/ exist.
    Storage.mkdir("/fonts");
    String familyDir = String("/fonts/") + family;
    Storage.mkdir(familyDir.c_str());
    state.path = familyDir;

    // Build full target path and overwrite if present.
    String filePath = state.path + "/" + state.fileName;
    if (Storage.exists(filePath.c_str())) {
      LOG_DBG("WEB", "[FONT] Overwriting %s", filePath.c_str());
      Storage.remove(filePath.c_str());
    }

    if (!Storage.openFileForWrite("WEB", filePath, state.file)) {
      state.error = "Failed to create font file on SD card";
      LOG_ERR("WEB", "[FONT] open failed: %s", filePath.c_str());
      return;
    }
    LOG_DBG("WEB", "[FONT] Upload start: %s (family=%s, size=%u)", filePath.c_str(), family.c_str(),
            static_cast<unsigned>(pointSize));
  } else if (upload.status == UPLOAD_FILE_WRITE) {
    if (state.file && state.error.isEmpty()) {
      const uint8_t* data = upload.buf;
      size_t remaining = upload.currentSize;
      while (remaining > 0) {
        const size_t space = UploadState::UPLOAD_BUFFER_SIZE - state.bufferPos;
        const size_t toCopy = remaining < space ? remaining : space;
        memcpy(state.buffer.data() + state.bufferPos, data, toCopy);
        state.bufferPos += toCopy;
        data += toCopy;
        remaining -= toCopy;
        state.size += toCopy;
        if (state.bufferPos >= UploadState::UPLOAD_BUFFER_SIZE) {
          if (!flushUploadBuffer(state)) return;
        }
      }
    }
  } else if (upload.status == UPLOAD_FILE_END) {
    if (state.file && state.error.isEmpty()) {
      if (!flushUploadBuffer(state)) return;
      state.file.close();
      state.success = true;
      LOG_DBG("WEB", "[FONT] Upload complete: %s (%u bytes)", state.fileName.c_str(),
              static_cast<unsigned>(state.size));
    }
  } else if (upload.status == UPLOAD_FILE_ABORTED) {
    if (state.file) {
      state.file.close();
      String filePath = state.path + "/" + state.fileName;
      Storage.remove(filePath.c_str());
    }
    state.error = "Upload aborted";
  }
}

void CrossPointWebServer::handleFontUploadPost(UploadState& state) const {
  if (!state.success) {
    const String error = state.error.isEmpty() ? "Unknown error during font upload" : state.error;
    server->send(400, "text/plain", error);
    return;
  }

  // Re-scan the registry so the new file appears in Settings → Custom Font (SD)
  // without a reboot or SD eject.  rediscover() is fast — just a directory walk
  // over /fonts and /.crosspoint/fonts, no file I/O for glyph data.
  sdFontSystem.rediscover();
  // If the user already has this family selected, ensureLoaded picks the closest
  // .cpfont file (which may have just changed if they uploaded a new size).
  ensureSdFontLoaded();

  String family;
  uint8_t pointSize = 0;
  parseFontFilename(state.fileName, family, pointSize);

  String json = "{\"success\":true,\"family\":\"";
  json += family;
  json += "\",\"size\":";
  json += String(static_cast<unsigned>(pointSize));
  json += ",\"path\":\"";
  json += state.path + "/" + state.fileName;
  json += "\",\"familiesDiscovered\":";
  json += String(sdFontSystem.registry().getFamilyCount());
  json += "}";
  server->send(200, "application/json", json);
}

void CrossPointWebServer::handleFontDelete() const {
  if (!server->hasArg("family")) {
    server->send(400, "text/plain", "Missing 'family' parameter");
    return;
  }

  const String family = server->arg("family");
  if (family.isEmpty() || family.length() > 31) {
    server->send(400, "text/plain", "Invalid family name");
    return;
  }
  // Reject path-traversal attempts.  Family names are pure directory names —
  // no slashes, no leading dots that would escape into hidden system paths.
  if (family.indexOf('/') >= 0 || family.indexOf('\\') >= 0 || family.startsWith(".")) {
    server->send(400, "text/plain", "Invalid family name");
    return;
  }

  // Try both the visible and legacy paths; succeed if either is removed.
  bool removed = false;
  String visiblePath = String("/fonts/") + family;
  String legacyPath = String("/.crosspoint/fonts/") + family;
  if (Storage.exists(visiblePath.c_str())) {
    if (Storage.removeDir(visiblePath.c_str())) {
      LOG_DBG("WEB", "[FONT] Deleted %s", visiblePath.c_str());
      removed = true;
    }
  }
  if (Storage.exists(legacyPath.c_str())) {
    if (Storage.removeDir(legacyPath.c_str())) {
      LOG_DBG("WEB", "[FONT] Deleted %s", legacyPath.c_str());
      removed = true;
    }
  }

  if (!removed) {
    server->send(404, "text/plain", "Family not found or could not be removed");
    return;
  }

  // If the deleted family was the user's current selection, clear it so the
  // reader cleanly falls back to the built-in fontFamily.
  if (strcmp(SETTINGS.sdFontFamilyName, family.c_str()) == 0) {
    SETTINGS.sdFontFamilyName[0] = '\0';
    SETTINGS.saveToFile();
  }
  // Refresh the registry + reload current selection so Settings UI picks up
  // the change immediately.
  sdFontSystem.rediscover();
  ensureSdFontLoaded();

  String json = "{\"success\":true,\"family\":\"";
  json += family;
  json += "\",\"familiesDiscovered\":";
  json += String(sdFontSystem.registry().getFamilyCount());
  json += "}";
  server->send(200, "application/json", json);
}

void CrossPointWebServer::handleFontList() const {
  // JSON shape:
  //   { "families": [
  //       { "name": "PKNakhonSawan", "sizes": [18, 20, 22, 24, 26] },
  //       ...
  //     ],
  //     "selected": "PKNakhonSawan"  // empty string if using built-in
  //   }
  String json = "{\"families\":[";
  const auto& families = sdFontSystem.registry().getFamilies();
  for (size_t i = 0; i < families.size(); i++) {
    const auto& family = families[i];
    if (i > 0) json += ",";
    json += "{\"name\":\"" + String(family.name.c_str()) + "\",\"sizes\":[";
    auto sizes = family.availableSizes();
    for (size_t s = 0; s < sizes.size(); s++) {
      if (s > 0) json += ",";
      json += String(static_cast<unsigned>(sizes[s]));
    }
    json += "]}";
  }
  json += "],\"selected\":\"";
  json += SETTINGS.sdFontFamilyName;
  json += "\"}";
  server->send(200, "application/json", json);
}

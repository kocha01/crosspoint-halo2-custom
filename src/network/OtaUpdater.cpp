#include "OtaUpdater.h"

#include <ArduinoJson.h>
#include <Logging.h>

#include "esp_http_client.h"
#include "esp_ota_ops.h"
#include "esp_wifi.h"

namespace {
constexpr char latestReleaseUrl[] = "https://api.github.com/repos/kocha01/crosspoint-halo2-custom/releases/latest";
constexpr int kUrlResolveMaxHops = 5;
constexpr int kUrlBufSize = 768;  // Long enough for pre-signed S3 CDN URLs

/* This is buffer and size holder to keep upcoming data from latestReleaseUrl */
char* local_buf;
int output_len;

/*
 * When esp_crt_bundle.h included, it is pointing wrong header file
 * which is something under WifiClientSecure because of our framework based on arduno platform.
 * To manage this obstacle, don't include anything, just extern and it will point correct one.
 */
extern "C" {
extern esp_err_t esp_crt_bundle_attach(void* conf);
}

esp_err_t http_client_set_header_cb(esp_http_client_handle_t http_client) {
  return esp_http_client_set_header(http_client, "User-Agent", "CrossPoint-ESP32-" CROSSPOINT_VERSION);
}

/* Context for capturing Location response headers during redirect resolution */
struct UrlResolveCtx {
  char location[kUrlBufSize];
};

/* Captures Location header from each HTTP response during redirect chain */
esp_err_t url_resolve_event(esp_http_client_event_t* evt) {
  if (evt->event_id != HTTP_EVENT_ON_HEADER) return ESP_OK;
  auto* ctx = static_cast<UrlResolveCtx*>(evt->user_data);
  if (strcasecmp(evt->header_key, "location") == 0) {
    strncpy(ctx->location, evt->header_value, sizeof(ctx->location) - 1);
    ctx->location[sizeof(ctx->location) - 1] = '\0';
  }
  return ESP_OK;
}

/*
 * Follow HTTP redirect chain manually using HEAD requests to obtain the final
 * CDN URL without auto-following redirects in esp_http_client.
 *
 * GitHub browser_download_url redirects: github.com → objects.githubusercontent.com (S3).
 * esp_https_ota uses esp_http_client_open internally which does NOT follow redirects,
 * so we pre-resolve the URL here to avoid the problem entirely.
 *
 * Returns the resolved CDN URL, or the original URL if resolution fails.
 */
std::string resolveRedirectUrl(const std::string& startUrl) {
  std::string currentUrl = startUrl;

  for (int hop = 0; hop < kUrlResolveMaxHops; hop++) {
    UrlResolveCtx ctx = {};

    esp_http_client_config_t config = {
        .url = currentUrl.c_str(),
        .timeout_ms = 10000,
        .max_redirection_count = 0,  // Manual redirect following
        .event_handler = url_resolve_event,
        .buffer_size = 4096,
        .buffer_size_tx = 1024,
        .user_data = &ctx,
        .skip_cert_common_name_check = true,
        .crt_bundle_attach = esp_crt_bundle_attach,
    };

    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (!client) {
      LOG_ERR("OTA", "URL resolve: client init failed at hop %d", hop);
      break;
    }

    esp_http_client_set_method(client, HTTP_METHOD_HEAD);
    esp_http_client_set_header(client, "User-Agent", "CrossPoint-ESP32-" CROSSPOINT_VERSION);

    /* open + fetch_headers: triggers HTTP_EVENT_ON_HEADER for each response header */
    esp_err_t err = esp_http_client_open(client, 0);
    if (err != ESP_OK) {
      LOG_ERR("OTA", "URL resolve: open failed at hop %d: %s", hop, esp_err_to_name(err));
      esp_http_client_cleanup(client);
      break;
    }

    esp_http_client_fetch_headers(client);
    const int status = esp_http_client_get_status_code(client);
    esp_http_client_cleanup(client);

    LOG_DBG("OTA", "URL resolve hop %d: status=%d", hop, status);

    if (status == 200 || status == 204) {
      /* currentUrl is already the direct download URL */
      LOG_DBG("OTA", "URL resolve: final URL found after %d hops", hop);
      break;
    } else if (status >= 300 && status < 400 && ctx.location[0] != '\0') {
      LOG_DBG("OTA", "URL resolve: redirect to %.80s...", ctx.location);
      currentUrl = ctx.location;
    } else {
      LOG_ERR("OTA", "URL resolve: unexpected status=%d at hop %d, using original URL", status, hop);
      return startUrl;
    }
  }

  return currentUrl;
}

esp_err_t event_handler(esp_http_client_event_t* event) {
  /* We do interested in only HTTP_EVENT_ON_DATA event only */
  if (event->event_id != HTTP_EVENT_ON_DATA) return ESP_OK;

  if (!esp_http_client_is_chunked_response(event->client)) {
    int content_len = esp_http_client_get_content_length(event->client);
    int copy_len = 0;

    if (local_buf == NULL) {
      /* local_buf life span is tracked by caller checkForUpdate */
      local_buf = static_cast<char*>(calloc(content_len + 1, sizeof(char)));
      output_len = 0;
      if (local_buf == NULL) {
        LOG_ERR("OTA", "HTTP Client Out of Memory Failed, Allocation %d", content_len);
        return ESP_ERR_NO_MEM;
      }
    }
    copy_len = min(event->data_len, (content_len - output_len));
    if (copy_len) {
      memcpy(local_buf + output_len, event->data, copy_len);
    }
    output_len += copy_len;
  } else {
    /* Code might be hits here, It happened once (for version checking) but I need more logs to handle that */
    int chunked_len;
    esp_http_client_get_chunk_length(event->client, &chunked_len);
    LOG_DBG("OTA", "esp_http_client_is_chunked_response failed, chunked_len: %d", chunked_len);
  }

  return ESP_OK;
} /* event_handler */
} /* namespace */

OtaUpdater::OtaUpdaterError OtaUpdater::checkForUpdate() {
  JsonDocument filter;
  esp_err_t esp_err;
  JsonDocument doc;

  esp_http_client_config_t client_config = {
      .url = latestReleaseUrl,
      .event_handler = event_handler,
      /* Default HTTP client buffer size 512 byte only */
      .buffer_size = 8192,
      .buffer_size_tx = 8192,
      .skip_cert_common_name_check = true,
      .crt_bundle_attach = esp_crt_bundle_attach,
      .keep_alive_enable = true,
  };

  /* To track life time of local_buf, dtor will be called on exit from that function */
  struct localBufCleaner {
    char** bufPtr;
    ~localBufCleaner() {
      if (*bufPtr) {
        free(*bufPtr);
        *bufPtr = NULL;
      }
    }
  } localBufCleaner = {&local_buf};

  esp_http_client_handle_t client_handle = esp_http_client_init(&client_config);
  if (!client_handle) {
    LOG_ERR("OTA", "HTTP Client Handle Failed");
    return INTERNAL_UPDATE_ERROR;
  }

  esp_err = esp_http_client_set_header(client_handle, "User-Agent", "CrossPoint-ESP32-" CROSSPOINT_VERSION);
  if (esp_err != ESP_OK) {
    LOG_ERR("OTA", "esp_http_client_set_header Failed : %s", esp_err_to_name(esp_err));
    esp_http_client_cleanup(client_handle);
    return INTERNAL_UPDATE_ERROR;
  }

  esp_err = esp_http_client_perform(client_handle);
  if (esp_err != ESP_OK) {
    LOG_ERR("OTA", "esp_http_client_perform Failed : %s", esp_err_to_name(esp_err));
    esp_http_client_cleanup(client_handle);
    return HTTP_ERROR;
  }

  /* esp_http_client_close will be called inside cleanup as well*/
  esp_err = esp_http_client_cleanup(client_handle);
  if (esp_err != ESP_OK) {
    LOG_ERR("OTA", "esp_http_client_cleanup Failed : %s", esp_err_to_name(esp_err));
    return INTERNAL_UPDATE_ERROR;
  }

  filter["tag_name"] = true;
  filter["assets"][0]["name"] = true;
  filter["assets"][0]["browser_download_url"] = true;
  filter["assets"][0]["size"] = true;
  const DeserializationError error = deserializeJson(doc, local_buf, DeserializationOption::Filter(filter));
  if (error) {
    LOG_ERR("OTA", "JSON parse failed: %s", error.c_str());
    return JSON_PARSE_ERROR;
  }

  if (!doc["tag_name"].is<std::string>()) {
    LOG_ERR("OTA", "No tag_name found");
    return JSON_PARSE_ERROR;
  }

  if (!doc["assets"].is<JsonArray>()) {
    LOG_ERR("OTA", "No assets found");
    return JSON_PARSE_ERROR;
  }

  latestVersion = doc["tag_name"].as<std::string>();

  for (int i = 0; i < doc["assets"].size(); i++) {
    if (doc["assets"][i]["name"] == "firmware.bin") {
      otaUrl = doc["assets"][i]["browser_download_url"].as<std::string>();
      otaSize = doc["assets"][i]["size"].as<size_t>();
      totalSize = otaSize;
      updateAvailable = true;
      break;
    }
  }

  if (!updateAvailable) {
    LOG_ERR("OTA", "No firmware.bin asset found");
    return NO_UPDATE;
  }

  LOG_DBG("OTA", "Found update: %s at %s", latestVersion.c_str(), otaUrl.c_str());

  /* Pre-resolve GitHub redirect → CDN URL.
   * esp_https_ota uses esp_http_client_open which does not follow redirects.
   * Resolving here lets installUpdate() use the CDN URL directly with no redirect. */
  const std::string resolvedUrl = resolveRedirectUrl(otaUrl);
  if (!resolvedUrl.empty() && resolvedUrl != otaUrl) {
    LOG_DBG("OTA", "Resolved CDN URL: %.80s...", resolvedUrl.c_str());
    otaUrl = resolvedUrl;
  } else {
    LOG_DBG("OTA", "URL unchanged after resolve (will attempt direct download)");
  }

  return OK;
}

bool OtaUpdater::isUpdateNewer() const {
  if (!updateAvailable || latestVersion.empty() || latestVersion == CROSSPOINT_VERSION) {
    return false;
  }

  int currentMajor = 0, currentMinor = 0, currentPatch = 0;
  int latestMajor = 0, latestMinor = 0, latestPatch = 0;

  const auto currentVersion = CROSSPOINT_VERSION;

  // semantic version check (only match on 3 segments)
  const int latestParsed = sscanf(latestVersion.c_str(), "%d.%d.%d", &latestMajor, &latestMinor, &latestPatch);
  const int currentParsed = sscanf(currentVersion, "%d.%d.%d", &currentMajor, &currentMinor, &currentPatch);

  // If either version string failed to parse as semver, treat as newer to allow update
  if (latestParsed < 3 || currentParsed < 3) {
    LOG_ERR("OTA", "Version parse failed (latest=%d, current=%d), allowing update", latestParsed, currentParsed);
    return true;
  }

  /*
   * Compare major versions.
   * If they differ, return true if latest major version greater than current major version
   * otherwise return false.
   */
  if (latestMajor != currentMajor) return latestMajor > currentMajor;

  /*
   * Compare minor versions.
   * If they differ, return true if latest minor version greater than current minor version
   * otherwise return false.
   */
  if (latestMinor != currentMinor) return latestMinor > currentMinor;

  /*
   * Check patch versions.
   */
  if (latestPatch != currentPatch) return latestPatch > currentPatch;

  // If we reach here, it means all segments are equal.
  // If we're on a pre-release build (-rc or -dev), consider the release version as newer
  // so dev/rc builds can always OTA to the matching release.
  if (strstr(currentVersion, "-rc") != nullptr || strstr(currentVersion, "-dev") != nullptr) {
    return true;
  }

  return false;
}

const std::string& OtaUpdater::getLatestVersion() const { return latestVersion; }

OtaUpdater::OtaUpdaterError OtaUpdater::installUpdate() {
  if (!isUpdateNewer()) {
    return UPDATE_OLDER_ERROR;
  }

  render = false;

  /* Disable WiFi power saving for reliable high-throughput download */
  esp_wifi_set_ps(WIFI_PS_NONE);

  /* Find the next OTA partition to write to */
  const esp_partition_t* partition = esp_ota_get_next_update_partition(NULL);
  if (!partition) {
    LOG_ERR("OTA", "No OTA partition available");
    esp_wifi_set_ps(WIFI_PS_MIN_MODEM);
    return INTERNAL_UPDATE_ERROR;
  }

  /* Begin OTA write session */
  esp_ota_handle_t ota_handle;
  esp_err_t err = esp_ota_begin(partition, OTA_SIZE_UNKNOWN, &ota_handle);
  if (err != ESP_OK) {
    LOG_ERR("OTA", "esp_ota_begin failed: %s", esp_err_to_name(err));
    esp_wifi_set_ps(WIFI_PS_MIN_MODEM);
    return INTERNAL_UPDATE_ERROR;
  }

  /*
   * Download firmware with manual redirect following.
   * Uses GET requests (not HEAD) and esp_http_client in streaming mode.
   * On 3xx, captures Location header via event handler and retries.
   * On 200, streams data directly into the OTA partition.
   *
   * This replaces esp_https_ota which could not follow GitHub CDN redirects
   * (esp_http_client_open does not follow redirects internally).
   */
  std::string currentUrl = otaUrl;
  bool downloadOk = false;
  constexpr int kMaxRedirects = 10;
  constexpr int kBufSize = 8192;

  for (int hop = 0; hop < kMaxRedirects && !downloadOk; hop++) {
    UrlResolveCtx ctx = {};

    esp_http_client_config_t cfg = {
        .url = currentUrl.c_str(),
        .timeout_ms = 60000,
        .event_handler = url_resolve_event,
        .buffer_size = kBufSize,
        .buffer_size_tx = 4096,
        .user_data = &ctx,
        .skip_cert_common_name_check = true,
        .crt_bundle_attach = esp_crt_bundle_attach,
        .keep_alive_enable = true,
    };

    esp_http_client_handle_t client = esp_http_client_init(&cfg);
    if (!client) {
      LOG_ERR("OTA", "Client init failed at hop %d", hop);
      break;
    }

    esp_http_client_set_method(client, HTTP_METHOD_GET);
    esp_http_client_set_header(client, "User-Agent", "CrossPoint-ESP32-" CROSSPOINT_VERSION);

    err = esp_http_client_open(client, 0);
    if (err != ESP_OK) {
      LOG_ERR("OTA", "HTTP open failed at hop %d: %s", hop, esp_err_to_name(err));
      esp_http_client_cleanup(client);
      break;
    }

    const int contentLen = esp_http_client_fetch_headers(client);
    const int status = esp_http_client_get_status_code(client);
    LOG_DBG("OTA", "Hop %d: status=%d content_len=%d url=%.60s...", hop, status, contentLen, currentUrl.c_str());

    /* Handle redirect */
    if (status >= 300 && status < 400 && ctx.location[0] != '\0') {
      LOG_DBG("OTA", "Redirect → %.80s...", ctx.location);
      currentUrl = ctx.location;
      esp_http_client_close(client);
      esp_http_client_cleanup(client);
      continue;
    }

    if (status != 200) {
      LOG_ERR("OTA", "Unexpected HTTP %d at hop %d", status, hop);
      esp_http_client_close(client);
      esp_http_client_cleanup(client);
      break;
    }

    /* Status 200 — stream firmware data to OTA partition */
    if (contentLen > 0) {
      totalSize = static_cast<size_t>(contentLen);
    }

    char* buf = static_cast<char*>(malloc(kBufSize));
    if (!buf) {
      LOG_ERR("OTA", "Download buffer malloc failed");
      esp_http_client_close(client);
      esp_http_client_cleanup(client);
      break;
    }

    processedSize = 0;
    bool writeError = false;

    while (true) {
      const int readLen = esp_http_client_read(client, buf, kBufSize);
      if (readLen < 0) {
        LOG_ERR("OTA", "Read error: %d", readLen);
        writeError = true;
        break;
      }
      if (readLen == 0) {
        break; /* EOF */
      }

      err = esp_ota_write(ota_handle, buf, static_cast<size_t>(readLen));
      if (err != ESP_OK) {
        LOG_ERR("OTA", "OTA write failed: %s", esp_err_to_name(err));
        writeError = true;
        break;
      }

      processedSize += static_cast<size_t>(readLen);
      render = true;
      delay(10);
    }

    free(buf);
    esp_http_client_close(client);
    esp_http_client_cleanup(client);
    downloadOk = !writeError;
  }

  esp_wifi_set_ps(WIFI_PS_MIN_MODEM);

  if (!downloadOk) {
    LOG_ERR("OTA", "Download failed, aborting OTA");
    esp_ota_abort(ota_handle);
    return HTTP_ERROR;
  }

  err = esp_ota_end(ota_handle);
  if (err != ESP_OK) {
    LOG_ERR("OTA", "esp_ota_end failed: %s", esp_err_to_name(err));
    return INTERNAL_UPDATE_ERROR;
  }

  err = esp_ota_set_boot_partition(partition);
  if (err != ESP_OK) {
    LOG_ERR("OTA", "Set boot partition failed: %s", esp_err_to_name(err));
    return INTERNAL_UPDATE_ERROR;
  }

  LOG_INF("OTA", "Update completed, restarting...");
  return OK;
}

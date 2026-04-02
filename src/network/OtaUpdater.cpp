#include "OtaUpdater.h"

#include <ArduinoJson.h>
#include <Logging.h>

#include "esp_http_client.h"
#include "esp_ota_ops.h"
#include "esp_wifi.h"

namespace {
constexpr char latestReleaseUrl[] = "https://api.github.com/repos/kocha01/crosspoint-halo2-custom/releases/latest";

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

struct OtaWriteCtx {
  esp_ota_handle_t handle;
  esp_err_t write_err;
  size_t* processedSize;
  bool* render;
};

esp_err_t ota_write_event_handler(esp_http_client_event_t* evt) {
  if (evt->event_id != HTTP_EVENT_ON_DATA) return ESP_OK;
  auto* ctx = static_cast<OtaWriteCtx*>(evt->user_data);
  if (ctx->write_err == ESP_OK) {
    ctx->write_err = esp_ota_write(ctx->handle, evt->data, evt->data_len);
    *ctx->processedSize += evt->data_len;
    *ctx->render = true;
  }
  return ESP_OK;
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

  LOG_DBG("OTA", "Found update: %s", latestVersion.c_str());
  return OK;
}

bool OtaUpdater::isUpdateNewer() const {
  if (!updateAvailable || latestVersion.empty() || latestVersion == CROSSPOINT_VERSION) {
    return false;
  }

  int currentMajor, currentMinor, currentPatch;
  int latestMajor, latestMinor, latestPatch;

  const auto currentVersion = CROSSPOINT_VERSION;

  // semantic version check (only match on 3 segments)
  sscanf(latestVersion.c_str(), "%d.%d.%d", &latestMajor, &latestMinor, &latestPatch);
  sscanf(currentVersion, "%d.%d.%d", &currentMajor, &currentMinor, &currentPatch);

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
  // One final check, if we're on an RC build (contains "-rc"), we should consider the latest version as newer even if
  // the segments are equal, since RC builds are pre-release versions.
  if (strstr(currentVersion, "-rc") != nullptr) {
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
  processedSize = 0;

  const esp_partition_t* ota_partition = esp_ota_get_next_update_partition(nullptr);
  if (!ota_partition) {
    LOG_ERR("OTA", "No OTA partition found");
    return INTERNAL_UPDATE_ERROR;
  }

  esp_ota_handle_t ota_handle = 0;
  esp_err_t esp_err = esp_ota_begin(ota_partition, OTA_WITH_SEQUENTIAL_WRITES, &ota_handle);
  if (esp_err != ESP_OK) {
    LOG_ERR("OTA", "esp_ota_begin failed: %s", esp_err_to_name(esp_err));
    return INTERNAL_UPDATE_ERROR;
  }

  OtaWriteCtx ctx = {ota_handle, ESP_OK, &processedSize, &render};

  /* esp_http_client_perform follows HTTP redirects (unlike esp_https_ota_begin
   * which uses esp_http_client_open internally and does not follow redirects).
   * GitHub browser_download_url redirects to CDN, so this is required. */
  esp_http_client_config_t client_config = {
      .url = otaUrl.c_str(),
      .timeout_ms = 60000,
      .max_redirection_count = 5,
      .event_handler = ota_write_event_handler,
      .buffer_size = 8192,
      .buffer_size_tx = 8192,
      .user_data = &ctx,
      .skip_cert_common_name_check = true,
      .crt_bundle_attach = esp_crt_bundle_attach,
      .keep_alive_enable = true,
  };

  /* For better timing and connectivity, we disable power saving for WiFi */
  esp_wifi_set_ps(WIFI_PS_NONE);

  esp_http_client_handle_t client = esp_http_client_init(&client_config);
  if (!client) {
    LOG_ERR("OTA", "HTTP client init failed");
    esp_ota_abort(ota_handle);
    esp_wifi_set_ps(WIFI_PS_MIN_MODEM);
    return INTERNAL_UPDATE_ERROR;
  }

  esp_http_client_set_header(client, "User-Agent", "CrossPoint-ESP32-" CROSSPOINT_VERSION);

  esp_err = esp_http_client_perform(client);
  esp_http_client_cleanup(client);

  /* Return back to default power saving for WiFi */
  esp_wifi_set_ps(WIFI_PS_MIN_MODEM);

  if (esp_err != ESP_OK) {
    LOG_ERR("OTA", "HTTP download failed: %s", esp_err_to_name(esp_err));
    esp_ota_abort(ota_handle);
    return HTTP_ERROR;
  }

  if (ctx.write_err != ESP_OK) {
    LOG_ERR("OTA", "OTA write failed: %s", esp_err_to_name(ctx.write_err));
    esp_ota_abort(ota_handle);
    return INTERNAL_UPDATE_ERROR;
  }

  esp_err = esp_ota_end(ota_handle);
  if (esp_err != ESP_OK) {
    LOG_ERR("OTA", "esp_ota_end failed: %s", esp_err_to_name(esp_err));
    return INTERNAL_UPDATE_ERROR;
  }

  esp_err = esp_ota_set_boot_partition(ota_partition);
  if (esp_err != ESP_OK) {
    LOG_ERR("OTA", "esp_ota_set_boot_partition failed: %s", esp_err_to_name(esp_err));
    return INTERNAL_UPDATE_ERROR;
  }

  LOG_INF("OTA", "Update completed");
  return OK;
}

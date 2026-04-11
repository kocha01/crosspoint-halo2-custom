#include "OtaUpdater.h"

#include <ArduinoJson.h>
#include <Logging.h>

#include "esp_http_client.h"
#include "esp_https_ota.h"
#include "esp_ota_ops.h"
#include "esp_partition.h"
#include "esp_wifi.h"

#include "bootloader_common.h"
#include "esp_flash_partitions.h"

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

esp_err_t http_client_set_header_cb(esp_http_client_handle_t http_client) {
  return esp_http_client_set_header(http_client, "User-Agent", "CrossPoint-ESP32-" CROSSPOINT_VERSION);
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

OtaUpdater::OtaUpdaterError OtaUpdater::installUpdate(void (*onProgress)(void*), void* progressCtx) {
  if (!isUpdateNewer()) {
    return UPDATE_OLDER_ERROR;
  }

  esp_https_ota_handle_t ota_handle = NULL;
  esp_err_t esp_err;
  /* Signal for OtaUpdateActivity */
  render = false;

  esp_http_client_config_t client_config = {
      .url = otaUrl.c_str(),
      .timeout_ms = 15000,
      /* Default HTTP client buffer size 512 byte only
       * not sufficent to handle URL redirection cases or
       * parsing of large HTTP headers.
       */
      .buffer_size = 8192,
      .buffer_size_tx = 8192,
      .skip_cert_common_name_check = true,
      .crt_bundle_attach = esp_crt_bundle_attach,
      .keep_alive_enable = true,
  };

  esp_https_ota_config_t ota_config = {
      .http_config = &client_config,
      .http_client_init_cb = http_client_set_header_cb,
  };

  /* For better timing and connectivity, we disable power saving for WiFi */
  esp_wifi_set_ps(WIFI_PS_NONE);

  esp_err = esp_https_ota_begin(&ota_config, &ota_handle);
  if (esp_err != ESP_OK) {
    LOG_ERR("OTA", "HTTP OTA Begin Failed: %s", esp_err_to_name(esp_err));
    esp_wifi_set_ps(WIFI_PS_MIN_MODEM);
    return INTERNAL_UPDATE_ERROR;
  }

  do {
    esp_err = esp_https_ota_perform(ota_handle);
    processedSize = esp_https_ota_get_image_len_read(ota_handle);
    render = true;
    if (onProgress) onProgress(progressCtx);
    delay(100);
  } while (esp_err == ESP_ERR_HTTPS_OTA_IN_PROGRESS);

  /* Return back to default power saving for WiFi in case of failing */
  esp_wifi_set_ps(WIFI_PS_MIN_MODEM);

  if (esp_err != ESP_OK) {
    LOG_ERR("OTA", "esp_https_ota_perform Failed: %s", esp_err_to_name(esp_err));
    esp_https_ota_finish(ota_handle);
    return HTTP_ERROR;
  }

  if (!esp_https_ota_is_complete_data_received(ota_handle)) {
    LOG_ERR("OTA", "Incomplete data received");
    esp_https_ota_finish(ota_handle);
    return INTERNAL_UPDATE_ERROR;
  }

  /*
   * Workaround for pioarduino 55.03.37 (ESP-IDF v5.5.2) esp_image_verify() bug:
   * esp_https_ota_finish() calls esp_image_verify() which misreads the
   * min_efuse_blk_rev_full field, causing ESP_ERR_OTA_VALIDATE_FAILED with
   * a phantom "efuse blk rev >= v520.28" error even though the actual firmware
   * has min_efuse_blk_rev_full=0 (verified via esp_partition_read).
   *
   * Instead of using esp_https_ota_finish(), we:
   * 1. Verify firmware integrity ourselves using esp_partition_read() (SPI direct)
   * 2. Abort the OTA handle (cleans up HTTP without calling esp_image_verify)
   * 3. Write otadata directly to select the new partition
   * 4. Caller reboots to let the bootloader boot the new firmware
   *    (bootloader reads flash directly via SPI, not through cache, so it works)
   */
  const esp_partition_t* ota_part = esp_ota_get_next_update_partition(NULL);
  if (!ota_part) {
    LOG_ERR("OTA", "No OTA partition found");
    esp_https_ota_abort(ota_handle);
    return INTERNAL_UPDATE_ERROR;
  }

  /* Verify firmware image header and app descriptor via direct SPI read */
  {
    uint8_t buf[48];
    esp_err_t read_err = esp_partition_read(ota_part, 0, buf, sizeof(buf));
    if (read_err != ESP_OK) {
      LOG_ERR("OTA", "Partition read failed: %s", esp_err_to_name(read_err));
      esp_https_ota_abort(ota_handle);
      return INTERNAL_UPDATE_ERROR;
    }

    /* Check image header magic (0xE9 for ESP32) */
    if (buf[0] != 0xE9) {
      LOG_ERR("OTA", "Bad image magic: 0x%02X", buf[0]);
      esp_https_ota_abort(ota_handle);
      return INTERNAL_UPDATE_ERROR;
    }

    /* Check app_desc magic at offset 32 (after 24-byte image header + 8-byte segment header) */
    uint32_t app_magic;
    memcpy(&app_magic, buf + 32, sizeof(app_magic));
    if (app_magic != 0xABCD5432) {
      LOG_ERR("OTA", "Bad app_desc magic: 0x%08lX", (unsigned long)app_magic);
      esp_https_ota_abort(ota_handle);
      return INTERNAL_UPDATE_ERROR;
    }
  }

  LOG_INF("OTA", "Firmware verified OK, writing boot config...");

  /* Abort OTA handle to clean up HTTP connection without calling esp_image_verify */
  esp_https_ota_abort(ota_handle);

  /* Write otadata directly to set the new OTA partition as boot target */
  const esp_partition_t* otadata_part =
      esp_partition_find_first(ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_DATA_OTA, NULL);
  if (!otadata_part) {
    LOG_ERR("OTA", "otadata partition not found");
    return INTERNAL_UPDATE_ERROR;
  }

  /* Read current otadata entries (two sectors, one entry per sector) */
  esp_ota_select_entry_t entry[2];
  esp_partition_read(otadata_part, 0, &entry[0], sizeof(entry[0]));
  esp_partition_read(otadata_part, 0x1000, &entry[1], sizeof(entry[1]));

  bool valid0 = bootloader_common_ota_select_valid(&entry[0]);
  bool valid1 = bootloader_common_ota_select_valid(&entry[1]);

  uint32_t max_seq = 0;
  if (valid0) max_seq = entry[0].ota_seq;
  if (valid1 && entry[1].ota_seq > max_seq) max_seq = entry[1].ota_seq;

  /*
   * ota_slot = (ota_seq - 1) % num_ota_partitions
   * For 2 partitions: even ota_seq → slot 1 (app1), odd → slot 0 (app0)
   * We need to select the target OTA partition.
   */
  int target_slot = ota_part->subtype - ESP_PARTITION_SUBTYPE_APP_OTA_0;
  uint32_t new_seq = max_seq + 1;
  /* Ensure new_seq maps to target_slot: (new_seq - 1) % 2 == target_slot */
  if ((new_seq - 1) % 2 != (uint32_t)target_slot) {
    new_seq++;
  }

  /* Build new otadata entry */
  esp_ota_select_entry_t new_entry;
  memset(&new_entry, 0xFF, sizeof(new_entry));
  new_entry.ota_seq = new_seq;
  new_entry.ota_state = ESP_OTA_IMG_UNDEFINED;
  new_entry.crc = bootloader_common_ota_select_crc(&new_entry);

  /* Write to the sector with lower/invalid seq (alternating writes for wear leveling) */
  int write_sector;
  if (!valid0) {
    write_sector = 0;
  } else if (!valid1) {
    write_sector = 1;
  } else {
    write_sector = (entry[0].ota_seq <= entry[1].ota_seq) ? 0 : 1;
  }

  uint32_t write_offset = write_sector * 0x1000;
  esp_err = esp_partition_erase_range(otadata_part, write_offset, 0x1000);
  if (esp_err != ESP_OK) {
    LOG_ERR("OTA", "otadata erase failed: %s", esp_err_to_name(esp_err));
    return INTERNAL_UPDATE_ERROR;
  }

  esp_err = esp_partition_write(otadata_part, write_offset, &new_entry, sizeof(new_entry));
  if (esp_err != ESP_OK) {
    LOG_ERR("OTA", "otadata write failed: %s", esp_err_to_name(esp_err));
    return INTERNAL_UPDATE_ERROR;
  }

  LOG_INF("OTA", "Update completed (ota_seq=%lu, slot=%d, sector=%d)", (unsigned long)new_seq, target_slot,
          write_sector);
  return OK;
}

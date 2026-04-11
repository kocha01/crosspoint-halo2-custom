#include "BleTimeSync.h"

#include <NimBLEDevice.h>
#include <sys/time.h>

#include <Logging.h>

// Custom service & characteristic UUIDs
static constexpr const char* TIME_SERVICE_UUID = "cfa8a100-7b98-4044-a656-580f213d8cf0";
static constexpr const char* TIME_CHAR_UUID = "cfa8a101-7b98-4044-a656-580f213d8cf0";

static volatile bool sTimeReceived = false;
static bool sSynced = false;

// GATT callback: phone writes a 4-byte Unix timestamp
class TimeWriteCallback : public NimBLECharacteristicCallbacks {
  void onWrite(NimBLECharacteristic* pChar) override {
    const auto& val = pChar->getValue();
    if (val.size() >= 4) {
      uint32_t timestamp;
      memcpy(&timestamp, val.data(), sizeof(timestamp));

      struct timeval tv = {};
      tv.tv_sec = static_cast<time_t>(timestamp);
      settimeofday(&tv, nullptr);

      sTimeReceived = true;
      sSynced = true;
      LOG_INF("BLE", "Time synced: %lu", static_cast<unsigned long>(timestamp));
    }
  }
};

static TimeWriteCallback sCallback;

bool BleTimeSync::sync(const uint32_t timeoutMs) {
  sTimeReceived = false;

  LOG_DBG("BLE", "Starting BLE time sync (timeout %lu ms)", static_cast<unsigned long>(timeoutMs));

  NimBLEDevice::init("CrossPoint");

  // Reduce power to minimum needed for nearby phone
  NimBLEDevice::setPower(ESP_PWR_LVL_P3);

  NimBLEServer* pServer = NimBLEDevice::createServer();
  NimBLEService* pService = pServer->createService(TIME_SERVICE_UUID);

  NimBLECharacteristic* pChar =
      pService->createCharacteristic(TIME_CHAR_UUID, NIMBLE_PROPERTY::WRITE | NIMBLE_PROPERTY::WRITE_NR);
  pChar->setCallbacks(&sCallback);

  pService->start();

  NimBLEAdvertising* pAdv = NimBLEDevice::getAdvertising();
  pAdv->addServiceUUID(TIME_SERVICE_UUID);
  pAdv->setScanResponse(true);
  pAdv->start();

  LOG_DBG("BLE", "Advertising as 'CrossPoint', waiting for time...");

  const unsigned long start = millis();
  while (!sTimeReceived && (millis() - start) < timeoutMs) {
    delay(50);
  }

  // Fully shut down BLE to free RAM
  pAdv->stop();
  NimBLEDevice::deinit(true);

  if (sTimeReceived) {
    LOG_INF("BLE", "Time sync successful");
  } else {
    LOG_DBG("BLE", "Time sync timed out");
  }

  return sTimeReceived;
}

bool BleTimeSync::isSynced() { return sSynced; }

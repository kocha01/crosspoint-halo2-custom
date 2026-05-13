#include <HalGPIO.h>
#include <SPI.h>
#include <esp_ota_ops.h>

void HalGPIO::begin() {
  inputMgr.begin();
  SPI.begin(EPD_SCLK, SPI_MISO, EPD_MOSI, EPD_CS);
  pinMode(UART0_RXD, INPUT);
}

void HalGPIO::update() { inputMgr.update(); }

bool HalGPIO::isPressed(uint8_t buttonIndex) const { return inputMgr.isPressed(buttonIndex); }

bool HalGPIO::wasPressed(uint8_t buttonIndex) const { return inputMgr.wasPressed(buttonIndex); }

bool HalGPIO::wasAnyPressed() const { return inputMgr.wasAnyPressed(); }

bool HalGPIO::wasReleased(uint8_t buttonIndex) const { return inputMgr.wasReleased(buttonIndex); }

bool HalGPIO::wasAnyReleased() const { return inputMgr.wasAnyReleased(); }

unsigned long HalGPIO::getHeldTime() const { return inputMgr.getHeldTime(); }

bool HalGPIO::isUsbConnected() const {
  // U0RXD/GPIO20 reads HIGH when USB is connected
  return digitalRead(UART0_RXD) == HIGH;
}

HalGPIO::WakeupReason HalGPIO::getWakeupReason() const {
  const bool usbConnected = isUsbConnected();
  const auto wakeupCause = esp_sleep_get_wakeup_cause();
  const auto resetReason = esp_reset_reason();

  // First-boot-after-flash detection.
  //
  // The crosspointreader.com web flasher (and the esptool-js OTA path it uses)
  // writes the new firmware to the inactive OTA slot, updates otadata to point
  // at it with state=NEW, then triggers a hard reset over DTR/RTS.  On
  // ESP32-C3 with USB-CDC, that hard reset comes back as ESP_RST_POWERON with
  // USB still connected — which, under the original logic below, looked
  // identical to "USB power was just applied to a fully-discharged device"
  // and routed into AfterUSBPower → deep sleep.  Result: every web-flash
  // bricked the device into a deep-sleep loop until the user power-cycled.
  //
  // OTA state NEW or PENDING_VERIFY is unambiguous evidence that the running
  // image was just written by a flasher and has never been booted before.
  // Treat it as AfterFlash and let setup() proceed normally.  The first time
  // through setup() will additionally call esp_ota_mark_app_valid... so a
  // subsequent USB-power-applied cold boot routes correctly through
  // AfterUSBPower → sleep.
  const esp_partition_t* running = esp_ota_get_running_partition();
  if (running) {
    esp_ota_img_states_t otaState = ESP_OTA_IMG_UNDEFINED;
    if (esp_ota_get_state_partition(running, &otaState) == ESP_OK) {
      if (otaState == ESP_OTA_IMG_NEW || otaState == ESP_OTA_IMG_PENDING_VERIFY) {
        return WakeupReason::AfterFlash;
      }
    }
  }

  if ((wakeupCause == ESP_SLEEP_WAKEUP_UNDEFINED && resetReason == ESP_RST_POWERON && !usbConnected) ||
      (wakeupCause == ESP_SLEEP_WAKEUP_GPIO && resetReason == ESP_RST_DEEPSLEEP && usbConnected)) {
    return WakeupReason::PowerButton;
  }
  if (wakeupCause == ESP_SLEEP_WAKEUP_UNDEFINED && resetReason == ESP_RST_UNKNOWN && usbConnected) {
    return WakeupReason::AfterFlash;
  }
  if (wakeupCause == ESP_SLEEP_WAKEUP_UNDEFINED && resetReason == ESP_RST_POWERON && usbConnected) {
    return WakeupReason::AfterUSBPower;
  }
  return WakeupReason::Other;
}
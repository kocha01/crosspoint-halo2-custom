#pragma once
#include <cstdint>

/**
 * BLE Time Sync — one-shot time synchronization at boot.
 *
 * The device advertises a BLE GATT service and waits for a phone
 * (via Web Bluetooth or companion app) to write the current Unix
 * timestamp. After receiving the time, BLE is fully shut down to
 * free RAM and save power. The system clock (gettimeofday/time)
 * then keeps time using the ESP32's crystal oscillator (~1-2s drift/day).
 */
class BleTimeSync {
 public:
  /// Start BLE, advertise, and wait for time sync.
  /// Returns true if time was successfully received within the timeout.
  /// After return, BLE is fully de-initialized regardless of result.
  static bool sync(uint32_t timeoutMs = 10000);

  /// Returns true if time was synced during this boot session.
  static bool isSynced();
};

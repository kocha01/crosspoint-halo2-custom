// Desktop HalPowerManager stubs
#include <HalPowerManager.h>

HalPowerManager powerManager;

void HalPowerManager::begin() {}
void HalPowerManager::setPowerSaving(bool) {}
void HalPowerManager::startDeepSleep(HalGPIO&) const {}
uint16_t HalPowerManager::getBatteryPercentage() const { return 88; }

HalPowerManager::Lock::Lock() : valid(true) {}
HalPowerManager::Lock::~Lock() {}

#pragma once

#include <Arduino.h>
#include <PubSubClient.h>

namespace NetworkDriver {

void initWifiSta();
void initMqtt(PubSubClient& mqtt, void (*callback)(char*, uint8_t*, unsigned int));
void tryConnectWifi(uint32_t nowMs, uint32_t& nextRetryMs, uint32_t retryMs);
bool tryConnectMqtt(PubSubClient& mqtt, uint32_t nowMs, uint32_t& nextRetryMs, uint32_t retryMs);

} // namespace NetworkDriver

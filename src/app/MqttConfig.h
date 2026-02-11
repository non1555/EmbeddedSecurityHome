#pragma once

#ifndef WIFI_SSID
#define WIFI_SSID ""
#endif

#ifndef WIFI_PASSWORD
#define WIFI_PASSWORD ""
#endif

#ifndef MQTT_BROKER
#define MQTT_BROKER "192.168.1.10"
#endif

#ifndef MQTT_PORT
#define MQTT_PORT 1883
#endif

#ifndef MQTT_USERNAME
#define MQTT_USERNAME ""
#endif

#ifndef MQTT_PASSWORD
#define MQTT_PASSWORD ""
#endif

#ifndef MQTT_CLIENT_ID
#define MQTT_CLIENT_ID "embedded-security-esp32"
#endif

#ifndef MQTT_KEEPALIVE_S
#define MQTT_KEEPALIVE_S 15
#endif

#ifndef MQTT_SOCKET_TIMEOUT_S
#define MQTT_SOCKET_TIMEOUT_S 1
#endif

#ifndef WIFI_RECONNECT_MS
#define WIFI_RECONNECT_MS 5000
#endif

#ifndef MQTT_RECONNECT_MS
#define MQTT_RECONNECT_MS 3000
#endif

#ifndef MQTT_TOPIC_CMD
#define MQTT_TOPIC_CMD "esh/cmd"
#endif

#ifndef MQTT_TOPIC_EVENT
#define MQTT_TOPIC_EVENT "esh/event"
#endif

#ifndef MQTT_TOPIC_STATUS
#define MQTT_TOPIC_STATUS "esh/status"
#endif

#ifndef MQTT_TOPIC_ACK
#define MQTT_TOPIC_ACK "esh/ack"
#endif

#ifndef MQTT_TOPIC_METRICS
#define MQTT_TOPIC_METRICS "esh/metrics"
#endif

#ifndef MQTT_METRICS_PERIOD_MS
#define MQTT_METRICS_PERIOD_MS 10000
#endif

#ifndef MQTT_STORE_CAP
#define MQTT_STORE_CAP 64
#endif

#ifndef MQTT_STORE_FLUSH_BURST
#define MQTT_STORE_FLUSH_BURST 8
#endif

#ifndef MQTT_PUB_DRAIN_BURST
#define MQTT_PUB_DRAIN_BURST 8
#endif

#ifndef APP_VERBOSE_LOG
#define APP_VERBOSE_LOG 0
#endif

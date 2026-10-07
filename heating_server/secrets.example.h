#pragma once

// Dummy configuration for compilation only. Never upload this B0 build.
// A real secrets.h stays local and ignored; B0 never reads or copies it.
#undef _EXTERNAL_WIFI_SID
#undef _EXTERNAL_WIFI_PASS
#undef _INTERNAL_WIFI_SID
#undef _INTERNAL_WIFI_PASS
#undef _MQTT_SERVER
#undef _MQTT_LOGIN
#undef _MQTT_PASSWORD

#define _EXTERNAL_WIFI_SID "dummy-network"
#define _EXTERNAL_WIFI_PASS "dummy-password"
#define _INTERNAL_WIFI_SID "dummy-ap"
#define _INTERNAL_WIFI_PASS "dummy-ap-password"
#define _MQTT_SERVER "192.0.2.1"
#define _MQTT_LOGIN "dummy-user"
#define _MQTT_PASSWORD "dummy-password"

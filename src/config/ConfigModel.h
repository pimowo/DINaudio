#pragma once

#include <Arduino.h>

namespace ConfigSchema {
static constexpr uint16_t CURRENT_VERSION = 1;
}

struct AudioConfig {
    int volume = 25;
    int maxVolume = 100;
};

struct BluetoothConfig {
    bool autoReconnect = false;
    uint32_t reconnectDelayMs = 2500;
};

struct NetworkConfig {
    String wifiSsid;
    String wifiPassword;
};

struct RuntimeConfig {
    uint16_t schemaVersion = ConfigSchema::CURRENT_VERSION;
    AudioConfig audio;
    BluetoothConfig bluetooth;
    NetworkConfig network;
};

#pragma once

#include <Arduino.h>

namespace ConfigSchema {
static constexpr uint16_t CURRENT_VERSION = 4;
}

struct AudioConfig {
    int volume = 25;
    int maxVolume = 100;
};

struct BluetoothConfig {
    bool autoReconnect = true;
    uint32_t reconnectDelayMs = 10000;
};

struct NetworkConfig {
    String wifiSsid;
    String wifiPassword;
};

struct FeaturesConfig {
    bool bluetoothEnabled = true;
    bool radioEnabled = true;
    bool playMediaEnabled = true;
    bool displayEnabled = true;
    bool encoderEnabled = true;
    bool buttonsEnabled = false;
    bool mqttEnabled = false;
    bool yoRadioWsEnabled = true;
    bool haDiscoveryEnabled = true;
};

struct RuntimeConfig {
    uint16_t schemaVersion = ConfigSchema::CURRENT_VERSION;
    AudioConfig audio;
    BluetoothConfig bluetooth;
    NetworkConfig network;
    FeaturesConfig features;
};


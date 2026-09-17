#pragma once
#include <Arduino.h>
#include <WiFi.h>
#include <ESPmDNS.h>

class ConfigManager;

class WiFiService {
public:
    bool begin(ConfigManager& config);
    void loop();
    void reconnect();
    void startConfigAp();

private:
    ConfigManager* _config = nullptr;
    uint32_t _connectStarted = 0;
    uint32_t _lastRetry = 0;
    String _hostname;
    bool _mdnsStarted = false;

    void connectStored();
    String makeDeviceSuffix() const;
    void refreshState();
};

#pragma once
#include <Arduino.h>
#include <WebServer.h>

class ConfigManager;
class WiFiService;

class WebService {
public:
    WebService();
    void begin(ConfigManager& config, WiFiService& wifi);
    void loop();

private:
    WebServer _server;
    ConfigManager* _config = nullptr;
    WiFiService* _wifi = nullptr;

    void routes();
    void handleRoot();
    void handleStatus();
    void handleSaveWifi();
    void handleClearWifi();
    void handlePlay();
    void handleStop();
    void handleVolume();
    void handleReboot();
};

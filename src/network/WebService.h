#pragma once
#include <Arduino.h>
#include <WebServer.h>

class SettingsStore;
class WiFiService;

class WebService {
public:
    WebService();
    void begin(SettingsStore& settings, WiFiService& wifi);
    void loop();

private:
    WebServer _server;
    SettingsStore* _settings = nullptr;
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
    void handleOtaPage();
    void handleOtaUpload();
    void handleOtaDone();
};

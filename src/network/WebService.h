#pragma once
#include <Arduino.h>
#include <WebServer.h>

class ConfigManager;
class WiFiService;
class AudioOutputManager;
class DisplayService;
struct RuntimeConfig;

class WebService {
public:
    WebService();
    void begin(ConfigManager& config, WiFiService& wifi, AudioOutputManager& audioOutput, DisplayService* display);
    void loop();
    bool restartPending() const { return _restartPending; }

private:
    WebServer _server;
    ConfigManager* _config = nullptr;
    WiFiService* _wifi = nullptr;
    AudioOutputManager* _audioOutput = nullptr;
    DisplayService* _display = nullptr;
    String _token;
    bool _restartPending = false;
    uint32_t _restartDeadline = 0;

    void routes();
    void handleRoot();
    void handleStatus();
    void handleConfigGet();
    void handleConfigSave();
    void handleResetDefaults();
    void handleSaveWifi();
    void handleClearWifi();
    void handlePlay();
    void handleStop();
    void handleVolume();
    void handleReboot();
    void sendJson(int status, const String& body);
    bool authorizeAction();
    void scheduleRestart();
    void prepareDisplayDisable(const RuntimeConfig& candidate);
};

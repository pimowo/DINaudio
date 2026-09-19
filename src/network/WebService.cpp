#include "WebService.h"

#include <cerrno>
#include <cstdlib>
#include <esp_system.h>

#include "AppConfig.h"
#include "../audio/AudioOutputManager.h"
#include "../ui/DisplayService.h"
#include "../config/ConfigManager.h"
#include "../core/CommandQueue.h"
#include "../diagnostics/Logger.h"
#include "WebConfigPage.h"
#include "WiFiService.h"

WebService::WebService() : _server(AppConfig::HTTP_PORT) {}

void WebService::begin(ConfigManager& config, WiFiService& wifi,
                       AudioOutputManager& audioOutput,
                       DisplayService* display) {
    _config = &config;
    _wifi = &wifi;
    _audioOutput = &audioOutput;
    _display = display;
    char token[25];
    snprintf(token, sizeof(token), "%08lx%08lx%08lx",
             static_cast<unsigned long>(esp_random()),
             static_cast<unsigned long>(esp_random()),
             static_cast<unsigned long>(esp_random()));
    _token = token;
    routes();
    _server.begin();
    Logger::info("WEB", "HTTP server started");
}

void WebService::prepareDisplayDisable(const RuntimeConfig& candidate) {
    const RuntimeConfig& current = _config->config();
    if (!current.features.displayEnabled || candidate.features.displayEnabled ||
        current.display.type != DisplayType::ST7789 || _display == nullptr ||
        !_display->isInitialized()) {
        return;
    }
    if (!_display->clearToBlack()) {
        Logger::warn("DISPLAY", "Could not clear ST7789 before disable");
    }
}

void WebService::routes() {
    _server.on("/", HTTP_GET, [this]() { handleRoot(); });
    _server.on("/assets/voxone.css", HTTP_GET, [this]() {
        _server.send_P(200, "text/css; charset=utf-8", WebConfigPage::css());
    });
    _server.on("/api/v1/status", HTTP_GET, [this]() { handleStatus(); });
    _server.on("/api/v1/config", HTTP_GET, [this]() { handleConfigGet(); });
    _server.on("/api/v1/config", HTTP_POST, [this]() { handleConfigSave(); });
    _server.on("/api/v1/config/reset", HTTP_POST,
               [this]() { handleResetDefaults(); });

    _server.on("/wifi/save", HTTP_POST, [this]() { handleSaveWifi(); });
    _server.on("/wifi/clear", HTTP_POST, [this]() { handleClearWifi(); });
    _server.on("/reboot", HTTP_POST, [this]() { handleReboot(); });

    _server.on("/play", HTTP_POST, [this]() { handlePlay(); });
    _server.on("/pause", HTTP_POST, [this]() {
        if (!authorizeAction()) return;
        CommandQueue::instance().push({CommandType::Pause, CommandSource::Web, 0});
        sendJson(200, "{\"ok\":true}");
    });
    _server.on("/next", HTTP_POST, [this]() {
        if (!authorizeAction()) return;
        CommandQueue::instance().push({CommandType::Next, CommandSource::Web, 0});
        sendJson(200, "{\"ok\":true}");
    });
    _server.on("/previous", HTTP_POST, [this]() {
        if (!authorizeAction()) return;
        CommandQueue::instance().push({CommandType::Previous, CommandSource::Web, 0});
        sendJson(200, "{\"ok\":true}");
    });
    _server.on("/stop", HTTP_POST, [this]() { handleStop(); });
    _server.on("/volume", HTTP_POST, [this]() { handleVolume(); });

    _server.onNotFound([this]() {
        _server.send(404, "text/plain; charset=utf-8", "Not found");
    });
}

void WebService::handleRoot() {
    _server.send_P(200, "text/html; charset=utf-8", WebConfigPage::html());
}

void WebService::sendJson(int status, const String& body) {
    _server.sendHeader("Cache-Control", "no-store");
    _server.send(status, "application/json; charset=utf-8", body);
}

bool WebService::authorizeAction() {
    if (_restartPending) {
        sendJson(409, "{\"ok\":false,\"error\":\"Restart został już zaplanowany.\"}");
        return false;
    }
    if (_token.isEmpty() || !_server.hasArg("_token") ||
        _server.arg("_token") != _token) {
        sendJson(403, "{\"ok\":false,\"error\":\"Brak ważnego tokena formularza.\"}");
        return false;
    }
    return true;
}

void WebService::scheduleRestart() {
    _restartPending = true;
    _restartDeadline = millis() + 1000;
}

void WebService::handleSaveWifi() {
    if (!authorizeAction()) return;
    RuntimeConfig candidate = _config->config();
    if (!_server.hasArg("ssid") || !_server.hasArg("password")) {
        sendJson(400, "{\"ok\":false,\"error\":\"Brak pól Wi-Fi.\"}");
        return;
    }
    candidate.network.wifiSsid = _server.arg("ssid");
    candidate.network.wifiSsid.trim();
    if (candidate.network.wifiSsid.isEmpty()) {
        sendJson(400, "{\"ok\":false,\"error\":\"SSID jest wymagane.\"}");
        return;
    }
    if (!_server.arg("password").isEmpty())
        candidate.network.wifiPassword = _server.arg("password");
    if (!_config->validate(candidate)) {
        sendJson(400, "{\"ok\":false,\"error\":\"Niepoprawna konfiguracja Wi-Fi.\"}");
        return;
    }
    if (!_config->save(candidate)) {
        sendJson(500, "{\"ok\":false,\"error\":\"Zapis NVS nie powiódł się.\"}");
        return;
    }
    sendJson(200, "{\"ok\":true,\"message\":\"Wi-Fi zapisane. VoxOne uruchomi się ponownie.\"}");
    scheduleRestart();
}

void WebService::handleClearWifi() {
    if (!authorizeAction()) return;
    if (_server.arg("confirm") != "YES") {
        sendJson(400, "{\"ok\":false,\"error\":\"Potwierdzenie jest wymagane.\"}");
        return;
    }
    RuntimeConfig candidate = _config->config();
    candidate.network.wifiSsid = "";
    candidate.network.wifiPassword = "";
    if (!_config->validate(candidate)) {
        sendJson(400, "{\"ok\":false,\"error\":\"Niepoprawna konfiguracja.\"}");
        return;
    }
    if (!_config->save(candidate)) {
        sendJson(500, "{\"ok\":false,\"error\":\"Zapis NVS nie powiódł się.\"}");
        return;
    }
    sendJson(200, "{\"ok\":true,\"message\":\"Wi-Fi usunięte. VoxOne uruchomi się ponownie.\"}");
    scheduleRestart();
}

void WebService::handlePlay() {
    if (!authorizeAction()) return;
    CommandQueue::instance().push({CommandType::SetPlay, CommandSource::Web, 0});
    sendJson(200, "{\"ok\":true}");
}

void WebService::handleStop() {
    if (!authorizeAction()) return;
    CommandQueue::instance().push({CommandType::SetStop, CommandSource::Web, 0});
    sendJson(200, "{\"ok\":true}");
}

void WebService::handleVolume() {
    if (!authorizeAction()) return;
    const String raw = _server.arg("v");
    if (raw.isEmpty() || raw.length() > 3) {
        sendJson(400, "{\"ok\":false,\"error\":\"Niepoprawna głośność.\"}");
        return;
    }
    errno = 0;
    char* end = nullptr;
    const long value = strtol(raw.c_str(), &end, 10);
    if (errno == ERANGE || end == raw.c_str() || *end != '\0' ||
        value < 0 || value > _config->maxVolume()) {
        sendJson(400, "{\"ok\":false,\"error\":\"Niepoprawna głośność.\"}");
        return;
    }
    CommandQueue::instance().push({
        CommandType::SetVolumeAbsolute, CommandSource::Web,
        static_cast<int>(value)
    });
    sendJson(200, "{\"ok\":true}");
}

void WebService::handleReboot() {
    if (!authorizeAction()) return;
    if (_server.arg("confirm") != "YES") {
        sendJson(400, "{\"ok\":false,\"error\":\"Potwierdzenie jest wymagane.\"}");
        return;
    }
    sendJson(200, "{\"ok\":true,\"message\":\"VoxOne uruchomi się ponownie.\"}");
    scheduleRestart();
}

void WebService::loop() {
    _server.handleClient();
    if (_restartPending &&
        static_cast<int32_t>(millis() - _restartDeadline) >= 0) {
        ESP.restart();
    }
}

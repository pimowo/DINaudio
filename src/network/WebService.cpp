#include "WebService.h"
#include <Update.h>
#include "AppConfig.h"
#include "BuildInfo.h"
#include "../storage/SettingsStore.h"
#include "../core/StateStore.h"
#include "../core/CommandQueue.h"
#include "../diagnostics/Logger.h"
#include "WiFiService.h"

static String htmlHeader(const String& title) {
    return String(
        "<!doctype html><html><head><meta charset='utf-8'>"
        "<meta name='viewport' content='width=device-width,initial-scale=1'>"
        "<title>") + title + "</title>"
        "<style>"
        "body{font-family:Arial,sans-serif;max-width:760px;margin:30px auto;padding:0 16px;background:#101215;color:#eee}"
        ".card{background:#1a1d22;padding:18px;border-radius:12px;margin:12px 0}"
        "a,button{color:#fff;background:#2979ff;border:0;border-radius:8px;padding:10px 14px;text-decoration:none;display:inline-block;margin:4px}"
        "input{padding:10px;border-radius:8px;border:1px solid #555;background:#0f1114;color:#fff;width:95%;margin:5px 0}"
        ".warn{background:#b26a00}.danger{background:#c62828}.ok{color:#7ee787}"
        "small{color:#aaa}"
        "</style></head><body>";
}

WebService::WebService() : _server(AppConfig::HTTP_PORT) {}

void WebService::begin(SettingsStore& settings, WiFiService& wifi) {
    _settings = &settings;
    _wifi = &wifi;
    routes();
    _server.begin();
    Logger::info("WEB", "HTTP server started");
}

void WebService::routes() {
    _server.on("/", HTTP_GET, [this]() { handleRoot(); });
    _server.on("/api/v1/status", HTTP_GET, [this]() { handleStatus(); });
    _server.on("/wifi/save", HTTP_POST, [this]() { handleSaveWifi(); });
    _server.on("/wifi/clear", HTTP_POST, [this]() { handleClearWifi(); });

    _server.on("/play", HTTP_POST, [this]() { handlePlay(); });
    _server.on("/stop", HTTP_POST, [this]() { handleStop(); });
    _server.on("/volume", HTTP_POST, [this]() { handleVolume(); });
    _server.on("/reboot", HTTP_POST, [this]() { handleReboot(); });

    _server.on("/update", HTTP_GET, [this]() { handleOtaPage(); });
    _server.on(
        "/update",
        HTTP_POST,
        [this]() { handleOtaDone(); },
        [this]() { handleOtaUpload(); }
    );

    _server.onNotFound([this]() {
        _server.send(404, "text/plain", "Not found");
    });
}

void WebService::handleRoot() {
    auto s = StateStore::instance().snapshot();

    String h = htmlHeader("DINaudio");
    h += "<h1>DINaudio</h1><div class='card'><h2 class='ok'>OTA TEST OK</h2><p>Firmware 0.1.1-m1 uruchomiony po aktualizacji.</p></div>";
    h += "<div class='card'><b>Firmware:</b> " + String(AppConfig::FW_VERSION);
    h += "<br><b>API:</b> " + String(AppConfig::API_VERSION);
    h += "<br><b>Host:</b> " + s.hostname + ".local";
    h += "<br><b>IP:</b> " + s.ip;
    h += "<br><b>Wi-Fi:</b> " + (s.wifiConnected ? s.wifiSsid : String("offline"));
    h += "<br><b>RSSI:</b> " + String(s.wifiRssi) + " dBm";
    h += "</div>";

    h += "<div class='card'><h3>Audio test</h3>";
    h += "<p>VOL: <b>" + String(s.volume) + "</b> / ";
    h += (s.playback == PlaybackState::Playing ? "<span class='ok'>PLAY</span>" : "STOP");
    h += "</p>";
    h += "<form method='post' action='/play' style='display:inline'><button>PLAY</button></form>";
    h += "<form method='post' action='/stop' style='display:inline'><button>STOP</button></form>";
    h += "<form method='post' action='/volume'><input type='number' name='v' min='0' max='100' value='" + String(s.volume) + "'><button>Zapisz głośność</button></form>";
    h += "</div>";

    h += "<div class='card'><h3>Wi-Fi</h3>";
    h += "<form method='post' action='/wifi/save'>";
    h += "<input name='ssid' placeholder='SSID' value='" + _settings->wifiSsid() + "'>";
    h += "<input name='password' type='password' placeholder='Hasło (zostaw puste tylko dla sieci otwartej)'>";
    h += "<button>Zapisz Wi-Fi</button></form>";
    h += "<form method='post' action='/wifi/clear'><button class='danger'>Usuń zapisane Wi-Fi</button></form>";
    h += "</div>";

    h += "<div class='card'><h3>Firmware</h3>";
    h += "<a href='/update'>Aktualizacja OTA</a>";
    h += "<form method='post' action='/reboot' style='display:inline'><button class='warn'>Restart</button></form>";
    h += "</div>";

    h += "<div class='card'><small>Build: " + String(DINAUDIO_BUILD_DATE) + " / " + String(DINAUDIO_BUILD_GIT) + "</small></div>";
    h += "</body></html>";

    _server.send(200, "text/html; charset=utf-8", h);
}

void WebService::handleStatus() {
    auto s = StateStore::instance().snapshot();

    String json = "{";
    json += "\"fw\":\"" + String(AppConfig::FW_VERSION) + "\",";
    json += "\"api\":\"" + String(AppConfig::API_VERSION) + "\",";
    json += "\"hostname\":\"" + s.hostname + "\",";
    json += "\"wifi_connected\":" + String(s.wifiConnected ? "true" : "false") + ",";
    json += "\"ssid\":\"" + s.wifiSsid + "\",";
    json += "\"ip\":\"" + s.ip + "\",";
    json += "\"rssi\":" + String(s.wifiRssi) + ",";
    json += "\"ap_mode\":" + String(s.apMode ? "true" : "false") + ",";
    json += "\"volume\":" + String(s.volume) + ",";
    json += "\"playing\":" + String(s.playback == PlaybackState::Playing ? "true" : "false");
    json += "}";

    _server.send(200, "application/json", json);
}

void WebService::handleSaveWifi() {
    String ssid = _server.arg("ssid");
    String password = _server.arg("password");

    ssid.trim();
    if (ssid.isEmpty()) {
        _server.send(400, "text/plain", "SSID required");
        return;
    }

    if (!_settings->saveWifi(ssid, password)) {
        _server.send(500, "text/plain", "Save failed");
        return;
    }

    _server.send(200, "text/html; charset=utf-8",
        htmlHeader("Wi-Fi") +
        "<h2>Zapisano Wi-Fi</h2><p>DINaudio spróbuje połączyć się z nową siecią.</p>"
        "<a href='/'>Wróć</a></body></html>");

    delay(150);
    _wifi->reconnect();
}

void WebService::handleClearWifi() {
    _settings->clearWifi();
    _server.send(200, "text/plain", "Wi-Fi cleared. Restarting...");
    delay(250);
    ESP.restart();
}

void WebService::handlePlay() {
    CommandQueue::instance().push({CommandType::SetPlay, CommandSource::Web, 0});
    _server.sendHeader("Location", "/");
    _server.send(303);
}

void WebService::handleStop() {
    CommandQueue::instance().push({CommandType::SetStop, CommandSource::Web, 0});
    _server.sendHeader("Location", "/");
    _server.send(303);
}

void WebService::handleVolume() {
    int v = constrain(_server.arg("v").toInt(), 0, 100);
    auto s = StateStore::instance().snapshot();
    int delta = v - s.volume;
    CommandQueue::instance().push({CommandType::VolumeDelta, CommandSource::Web, delta});
    _server.sendHeader("Location", "/");
    _server.send(303);
}

void WebService::handleReboot() {
    _server.send(200, "text/plain", "Restarting...");
    delay(200);
    ESP.restart();
}

void WebService::handleOtaPage() {
    String h = htmlHeader("DINaudio OTA");
    h += "<h1>Aktualizacja firmware</h1>";
    h += "<div class='card'>";
    h += "<p>Aktualny firmware: <b>" + String(AppConfig::FW_VERSION) + "</b></p>";
    h += "<p>Wybierz <code>firmware.bin</code> wygenerowany przez PlatformIO.</p>";
    h += "<form method='POST' action='/update' enctype='multipart/form-data'>";
    h += "<input type='file' name='firmware' accept='.bin' required>";
    h += "<button>Wgraj firmware</button></form>";
    h += "<p><small>Nie odłączaj zasilania podczas aktualizacji.</small></p>";
    h += "</div><a href='/'>Wróć</a></body></html>";
    _server.send(200, "text/html; charset=utf-8", h);
}

void WebService::handleOtaUpload() {
    HTTPUpload& upload = _server.upload();

    if (upload.status == UPLOAD_FILE_START) {
        Logger::info("OTA", "Start: " + upload.filename);

        auto s = StateStore::instance().snapshot();
        s.otaInProgress = true;
        StateStore::instance().update(s);

        if (!Update.begin(UPDATE_SIZE_UNKNOWN, U_FLASH)) {
            Update.printError(Serial);
        }
    } else if (upload.status == UPLOAD_FILE_WRITE) {
        if (Update.write(upload.buf, upload.currentSize) != upload.currentSize) {
            Update.printError(Serial);
        }
    } else if (upload.status == UPLOAD_FILE_END) {
        if (Update.end(true)) {
            Logger::info("OTA", "Success, bytes=" + String(upload.totalSize));
        } else {
            Update.printError(Serial);
            Logger::error("OTA", "Update.end failed");
        }
    } else if (upload.status == UPLOAD_FILE_ABORTED) {
        Update.abort();
        Logger::warn("OTA", "Upload aborted");
    }
}

void WebService::handleOtaDone() {
    bool ok = !Update.hasError();

    auto s = StateStore::instance().snapshot();
    s.otaInProgress = false;
    StateStore::instance().update(s);

    if (!ok) {
        _server.send(500, "text/html; charset=utf-8",
            htmlHeader("OTA error") +
            "<h2>Aktualizacja nieudana</h2><a href='/update'>Wróć</a></body></html>");
        return;
    }

    _server.send(200, "text/html; charset=utf-8",
        htmlHeader("OTA OK") +
        "<h2>Firmware zapisany poprawnie</h2><p>DINaudio uruchomi się ponownie.</p></body></html>");
    delay(500);
    ESP.restart();
}

void WebService::loop() {
    _server.handleClient();
}

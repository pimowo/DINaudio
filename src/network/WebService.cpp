#include "WebService.h"
#include "AppConfig.h"
#include "BuildInfo.h"
#include "../config/ConfigManager.h"
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
        ".warn{background:#b26a00}.danger{background:#c62828}.ok{color:#7ee787}.info{color:#79c0ff}"
        "small{color:#aaa}"
        "</style></head><body>";
}

static const char* sourceName(AudioSource source) {
    switch (source) {
        case AudioSource::Bluetooth: return "BLUETOOTH";
        case AudioSource::Test: return "TEST";
        case AudioSource::Stop:
        default: return "STOP";
    }
}

WebService::WebService() : _server(AppConfig::HTTP_PORT) {}

void WebService::begin(ConfigManager& config, WiFiService& wifi) {
    _config = &config;
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
    _server.on("/pause", HTTP_POST, [this]() {
        CommandQueue::instance().push({
            CommandType::Pause,
            CommandSource::Web,
            0
        });
        _server.sendHeader("Location", "/");
        _server.send(303);
    });
    _server.on("/next", HTTP_POST, [this]() {
        CommandQueue::instance().push({
            CommandType::Next,
            CommandSource::Web,
            0
        });
        _server.sendHeader("Location", "/");
        _server.send(303);
    });
    _server.on("/previous", HTTP_POST, [this]() {
        CommandQueue::instance().push({
            CommandType::Previous,
            CommandSource::Web,
            0
        });
        _server.sendHeader("Location", "/");
        _server.send(303);
    });
    _server.on("/stop", HTTP_POST, [this]() { handleStop(); });
    _server.on("/volume", HTTP_POST, [this]() { handleVolume(); });
    _server.on("/reboot", HTTP_POST, [this]() { handleReboot(); });

    _server.onNotFound([this]() {
        _server.send(404, "text/plain", "Not found");
    });
}

void WebService::handleRoot() {
    const auto s = StateStore::instance().snapshot();

    String h = htmlHeader("DINaudio");
    h += "<h1>DINaudio</h1>";

    h += "<div class='card'><h3>Bluetooth / AVRCP</h3>";
    h += "<b>Firmware:</b> " + String(AppConfig::FW_VERSION);
    h += "<br><b>DINaudio BT:</b> " + s.bluetoothDeviceName;
    h += "<br><b>Telefon:</b> " +
         (s.bluetoothPeerName.isEmpty() ? String("-") : s.bluetoothPeerName);
    h += "<br><b>Połączenie:</b> ";
    h += s.bluetoothConnected
        ? "<span class='ok'>CONNECTED</span>"
        : "DISCONNECTED";
    h += "<br><b>Audio:</b> ";
    h += s.bluetoothPlaying
        ? "<span class='ok'>PLAYING</span>"
        : "IDLE";
    h += "<br><b>Artysta:</b> " +
         (s.bluetoothArtist.isEmpty() ? String("-") : s.bluetoothArtist);
    h += "<br><b>Utwór:</b> " +
         (s.bluetoothTitle.isEmpty() ? String("-") : s.bluetoothTitle);
    h += "<br><b>Źródło:</b> " + String(sourceName(s.audioSource));
    h += "<p>";
    h += "<form method='post' action='/previous' style='display:inline'><button>&lt;&lt;</button></form>";
    h += "<form method='post' action='/play' style='display:inline'><button>PLAY</button></form>";
    h += "<form method='post' action='/pause' style='display:inline'><button>PAUSE</button></form>";
    h += "<form method='post' action='/next' style='display:inline'><button>&gt;&gt;</button></form>";
    h += "</p></div>";

    h += "<div class='card'><h3>Głośność</h3>";
    h += "<p>VOL: <b>" + String(s.volume) +
         "</b> / " + String(_config->maxVolume()) + "</p>";
    h += "<form method='post' action='/volume'>";
    h += "<input type='number' name='v' min='0' max='" +
         String(_config->maxVolume()) +
         "' value='" + String(s.volume) + "'>";
    h += "<button>Zapisz głośność</button></form>";
    h += "<p><small>Telefon, enkoder i WWW powinny synchronizować tę samą wartość.</small></p>";
    h += "</div>";

    h += "<div class='card'><h3>System</h3>";
    h += "<b>Host:</b> " + s.hostname + ".local";
    h += "<br><b>IP:</b> " + s.ip;
    h += "<br><b>Wi-Fi:</b> " +
         (s.wifiConnected ? s.wifiSsid : String("offline"));
    h += "<br><b>RSSI:</b> " + String(s.wifiRssi) + " dBm";
    h += "<br><b>Config schema:</b> " +
         String(_config->schemaVersion());
    h += "</div>";

    h += "<div class='card'><h3>Wi-Fi</h3>";
    h += "<form method='post' action='/wifi/save'>";
    h += "<input name='ssid' placeholder='SSID' value='" +
         _config->wifiSsid() + "'>";
    h += "<input name='password' type='password' placeholder='Hasło'>";
    h += "<button>Zapisz Wi-Fi</button></form>";
    h += "<form method='post' action='/wifi/clear'><button class='danger'>Usuń zapisane Wi-Fi</button></form>";
    h += "</div>";

    h += "<div class='card'><h3>Firmware</h3>";
    h += "<form method='post' action='/reboot' style='display:inline'><button class='warn'>Restart</button></form>";
    h += "</div>";

    h += "<div class='card'><small>Build: " +
         String(DINAUDIO_BUILD_DATE) + " / " +
         String(DINAUDIO_BUILD_GIT) + "</small></div>";

    h += "</body></html>";

    _server.send(
        200,
        "text/html; charset=utf-8",
        h
    );
}

void WebService::handleStatus() {
    const auto s = StateStore::instance().snapshot();

    String json = "{";
    json += "\"fw\":\"" + String(AppConfig::FW_VERSION) + "\",";
    json += "\"config_schema\":" + String(_config->schemaVersion()) + ",";
    json += "\"volume\":" + String(s.volume) + ",";
    json += "\"max_volume\":" + String(_config->maxVolume()) + ",";
    json += "\"audio_source\":\"" + String(sourceName(s.audioSource)) + "\",";
    json += "\"bluetooth_connected\":" +
            String(s.bluetoothConnected ? "true" : "false") + ",";
    json += "\"bluetooth_playing\":" +
            String(s.bluetoothPlaying ? "true" : "false") + ",";
    json += "\"bluetooth_peer\":\"" + s.bluetoothPeerName + "\",";
    json += "\"bluetooth_artist\":\"" + s.bluetoothArtist + "\",";
    json += "\"bluetooth_title\":\"" + s.bluetoothTitle + "\",";
    json += "\"wifi_connected\":" +
            String(s.wifiConnected ? "true" : "false") + ",";
    json += "\"ip\":\"" + s.ip + "\"";
    json += "}";

    _server.send(200, "application/json", json);
}

void WebService::handleSaveWifi() {
    String ssid = _server.arg("ssid");
    String password = _server.arg("password");
    ssid.trim();

    if (ssid.isEmpty()) {
        _server.send(
            400,
            "text/plain",
            "SSID required"
        );
        return;
    }

    if (!_config->saveWifi(ssid, password)) {
        _server.send(
            500,
            "text/plain",
            "Save failed"
        );
        return;
    }

    _server.send(
        200,
        "text/html; charset=utf-8",
        htmlHeader("Wi-Fi") +
        "<h2>Zapisano Wi-Fi</h2><a href='/'>Wróć</a></body></html>"
    );

    delay(150);
    _wifi->reconnect();
}

void WebService::handleClearWifi() {
    _config->clearWifi();
    _server.send(
        200,
        "text/plain",
        "Wi-Fi cleared. Restarting..."
    );
    delay(250);
    ESP.restart();
}

void WebService::handlePlay() {
    CommandQueue::instance().push({
        CommandType::SetPlay,
        CommandSource::Web,
        0
    });
    _server.sendHeader("Location", "/");
    _server.send(303);
}

void WebService::handleStop() {
    CommandQueue::instance().push({
        CommandType::SetStop,
        CommandSource::Web,
        0
    });
    _server.sendHeader("Location", "/");
    _server.send(303);
}

void WebService::handleVolume() {
    const int v = constrain(
        _server.arg("v").toInt(),
        0,
        _config->maxVolume()
    );

    CommandQueue::instance().push({
        CommandType::SetVolumeAbsolute,
        CommandSource::Web,
        v
    });

    _server.sendHeader("Location", "/");
    _server.send(303);
}

void WebService::handleReboot() {
    _server.send(
        200,
        "text/plain",
        "Restarting..."
    );
    delay(200);
    ESP.restart();
}

void WebService::loop() {
    _server.handleClient();
}

#include "WebService.h"

#include <cerrno>
#include <cstdlib>
#include <esp_system.h>

#include "AppConfig.h"
#include "BuildInfo.h"
#include "../audio/AudioOutputManager.h"
#include "../config/ConfigManager.h"
#include "../core/StateStore.h"

namespace {
String jsonQuote(const String& input) {
    String out = "\"";
    for (size_t i = 0; i < input.length(); ++i) {
        const unsigned char ch = static_cast<unsigned char>(input[i]);
        if (ch == '"' || ch == '\\') {
            out += '\\';
            out += static_cast<char>(ch);
        } else if (ch < 0x20) {
            char escaped[7];
            snprintf(escaped, sizeof(escaped), "\\u%04x", ch);
            out += escaped;
        } else {
            out += static_cast<char>(ch);
        }
    }
    out += '"';
    return out;
}

const char* sourceName(AudioSource source) {
    switch (source) {
        case AudioSource::Bluetooth: return "BLUETOOTH";
        case AudioSource::Radio: return "RADIO";
        case AudioSource::Test: return "TEST";
        default: return "STOP";
    }
}

const char* ownerName(AudioOutputOwner owner) {
    switch (owner) {
        case AudioOutputOwner::Bluetooth: return "BLUETOOTH";
        case AudioOutputOwner::Radio: return "RADIO";
        case AudioOutputOwner::PlayMedia: return "PLAY_MEDIA";
        default: return "NONE";
    }
}

bool readNumber(WebServer& server, const char* key, long lo, long hi,
                long& result, String& error) {
    if (!server.hasArg(key)) {
        error = String("Brak pola: ") + key;
        return false;
    }
    String raw = server.arg(key);
    if (raw.isEmpty() || raw.length() > 12) {
        error = String("Niepoprawna liczba: ") + key;
        return false;
    }
    size_t index = raw[0] == '-' ? 1 : 0;
    if (index == raw.length()) {
        error = String("Niepoprawna liczba: ") + key;
        return false;
    }
    for (; index < raw.length(); ++index) {
        if (raw[index] < '0' || raw[index] > '9') {
            error = String("Niepoprawna liczba: ") + key;
            return false;
        }
    }
    errno = 0;
    char* end = nullptr;
    const long value = strtol(raw.c_str(), &end, 10);
    if (errno == ERANGE || end == raw.c_str() || *end != '\0' ||
        value < lo || value > hi) {
        error = String("Wartość poza zakresem: ") + key;
        return false;
    }
    result = value;
    return true;
}

bool readText(WebServer& server, const char* key, size_t maxLength,
              String& result, String& error, bool secret = false) {
    if (!server.hasArg(key)) {
        error = String("Brak pola: ") + key;
        return false;
    }
    const String value = server.arg(key);
    if (value.length() > maxLength) {
        error = String("Tekst jest za długi: ") + key;
        return false;
    }
    if (!secret || !value.isEmpty()) result = value;
    return true;
}

bool parseCandidate(WebServer& server, RuntimeConfig& c, String& error) {
#define READ_NUM(key, field, low, high) do { \
    long value; \
    if (!readNumber(server, key, low, high, value, error)) return false; \
    field = static_cast<decltype(field)>(value); \
} while (false)
#define READ_ENUM(key, field, high, kind) do { \
    long value; \
    if (!readNumber(server, key, 0, high, value, error)) return false; \
    field = static_cast<kind>(value); \
} while (false)
#define READ_TEXT(key, field, maxlen) \
    do { if (!readText(server, key, maxlen, field, error)) return false; } while (false)
#define READ_SECRET(key, field, maxlen) \
    do { if (!readText(server, key, maxlen, field, error, true)) return false; } while (false)
    READ_TEXT("device.name", c.device.name, 48);
    READ_NUM("audio.volume", c.audio.volume, 0, 100);
    READ_NUM("audio.maxVolume", c.audio.maxVolume, 1, 100);
    READ_NUM("audio.startVolume", c.audio.startVolume, 0, 100);
    READ_NUM("audio.maxOutputVolume", c.audio.maxOutputVolume, 0, 100);
    READ_ENUM("audio.defaultSource", c.audio.defaultSource, 2, DefaultSource);
    READ_ENUM("audio.outputType", c.audio.outputType, 1, OutputType);
    READ_NUM("audio.i2sBclk", c.audio.i2sBclk, 0, 39);
    READ_NUM("audio.i2sLrclk", c.audio.i2sLrclk, 0, 39);
    READ_NUM("audio.i2sDout", c.audio.i2sDout, 0, 39);
    READ_ENUM("playMedia.volumeMode", c.playMedia.volumeMode, 1, PlayMediaVolumeMode);
    READ_NUM("playMedia.fixedVolume", c.playMedia.fixedVolume, 0, 100);
    READ_NUM("bluetooth.autoReconnect", c.bluetooth.autoReconnect, 0, 1);
    READ_NUM("bluetooth.reconnectDelayMs", c.bluetooth.reconnectDelayMs, 0, 60000);
    READ_TEXT("bluetooth.deviceName", c.bluetooth.deviceName, 48);
    READ_NUM("bluetooth.discoverableEnabled", c.bluetooth.discoverableEnabled, 0, 1);
    READ_NUM("bluetooth.discoverableSec", c.bluetooth.discoverableSec, 1, 3600);
    READ_NUM("bluetooth.rememberLastPeer", c.bluetooth.rememberLastPeer, 0, 1);
    READ_NUM("radio.defaultStation", c.radio.defaultStation, 0, 9999);
    READ_NUM("radio.autostart", c.radio.autostart, 0, 1);
    READ_NUM("radio.reconnectEnabled", c.radio.reconnectEnabled, 0, 1);
    READ_NUM("radio.streamTimeoutMs", c.radio.streamTimeoutMs, 1000, 60000);
    READ_NUM("radio.icyMetadataEnabled", c.radio.icyMetadataEnabled, 0, 1);
    READ_ENUM("display.type", c.display.type, 1, DisplayType);
    READ_NUM("display.brightness", c.display.brightness, 0, 100);
    READ_NUM("display.screensaverEnabled", c.display.screensaverEnabled, 0, 1);
    READ_NUM("display.screensaverTimeoutSec", c.display.screensaverTimeoutSec, 10, 86400);
    READ_NUM("display.st7789.sck", c.display.st7789.sck, 0, 39);
    READ_NUM("display.st7789.mosi", c.display.st7789.mosi, 0, 39);
    READ_NUM("display.st7789.cs", c.display.st7789.cs, 0, 39);
    READ_NUM("display.st7789.dc", c.display.st7789.dc, 0, 39);
    READ_NUM("display.st7789.rst", c.display.st7789.rst, -1, 39);
    READ_NUM("display.ssd1306.sda", c.display.ssd1306.sda, 0, 39);
    READ_NUM("display.ssd1306.scl", c.display.ssd1306.scl, 0, 39);
    READ_NUM("display.ssd1306.address", c.display.ssd1306.address, 8, 119);
    READ_NUM("encoder.pinA", c.encoder.pinA, 0, 39);
    READ_NUM("encoder.pinB", c.encoder.pinB, 0, 39);
    READ_NUM("encoder.pinButton", c.encoder.pinButton, 0, 39);
    READ_ENUM("encoder.direction", c.encoder.direction, 1, EncoderDirection);
    READ_NUM("encoder.volumeStep", c.encoder.volumeStep, 1, 10);
    READ_NUM("encoder.accelerationEnabled", c.encoder.accelerationEnabled, 0, 1);
    READ_TEXT("mqtt.host", c.mqtt.host, 128);
    READ_NUM("mqtt.port", c.mqtt.port, 1, 65535);
    READ_TEXT("mqtt.username", c.mqtt.username, 128);
    READ_SECRET("mqtt.password", c.mqtt.password, 128);
    READ_TEXT("mqtt.rootTopic", c.mqtt.rootTopic, 128);
    READ_NUM("yoRadio.playlistCompat", c.yoRadio.playlistCompat, 0, 1);
    READ_NUM("yoRadio.extensionsEnabled", c.yoRadio.extensionsEnabled, 0, 1);
    READ_TEXT("network.wifiSsid", c.network.wifiSsid, 32);
    READ_SECRET("network.wifiPassword", c.network.wifiPassword, 64);
    READ_TEXT("network.hostname", c.network.hostname, 63);
    READ_NUM("network.mdnsEnabled", c.network.mdnsEnabled, 0, 1);
    READ_NUM("network.dhcpEnabled", c.network.dhcpEnabled, 0, 1);
    READ_NUM("network.ntpEnabled", c.network.ntpEnabled, 0, 1);
    READ_TEXT("network.timezone", c.network.timezone, 64);
    READ_NUM("features.bluetoothEnabled", c.features.bluetoothEnabled, 0, 1);
    READ_NUM("features.radioEnabled", c.features.radioEnabled, 0, 1);
    READ_NUM("features.playMediaEnabled", c.features.playMediaEnabled, 0, 1);
    READ_NUM("features.displayEnabled", c.features.displayEnabled, 0, 1);
    READ_NUM("features.encoderEnabled", c.features.encoderEnabled, 0, 1);
    READ_NUM("features.buttonsEnabled", c.features.buttonsEnabled, 0, 1);
    READ_NUM("features.mqttEnabled", c.features.mqttEnabled, 0, 1);
    READ_NUM("features.yoRadioWsEnabled", c.features.yoRadioWsEnabled, 0, 1);
    READ_NUM("features.haDiscoveryEnabled", c.features.haDiscoveryEnabled, 0, 1);
#undef READ_SECRET
#undef READ_TEXT
#undef READ_ENUM
#undef READ_NUM
    return true;
}
}

void WebService::handleStatus() {
    const auto s = StateStore::instance().snapshot();
    String json;
    json.reserve(900);
    json = "{";
    auto addString = [&json](const char* key, const String& value) {
        if (json.length() > 1) json += ',';
        json += jsonQuote(key); json += ':'; json += jsonQuote(value);
    };
    auto addNumber = [&json](const char* key, long value) {
        if (json.length() > 1) json += ',';
        json += jsonQuote(key); json += ':'; json += String(value);
    };
    auto addBool = [&json](const char* key, bool value) {
        if (json.length() > 1) json += ',';
        json += jsonQuote(key); json += ':'; json += value ? "true" : "false";
    };
    addString("fw", AppConfig::FW_VERSION);
    addString("build", String(VOXONE_BUILD_DATE) + " / " + VOXONE_BUILD_GIT);
    addNumber("config_schema", _config->schemaVersion());
    addNumber("uptime_s", millis() / 1000);
    addNumber("free_heap", ESP.getFreeHeap());
    addNumber("reset_reason", static_cast<uint32_t>(esp_reset_reason()));
    addNumber("volume", s.volume);
    addNumber("max_volume", _config->maxVolume());
    addString("audio_source", sourceName(s.audioSource));
    addString("audio_owner", ownerName(_audioOutput->owner()));
    addString("radio_state", _audioOutput->isOwnedBy(AudioOutputOwner::Radio) ? "RADIO_OWNER" : "STOP");
    addBool("bluetooth_connected", s.bluetoothConnected);
    addBool("bluetooth_playing", s.bluetoothPlaying);
    addString("bluetooth_peer", s.bluetoothPeerName);
    addString("bluetooth_artist", s.bluetoothArtist);
    addString("bluetooth_title", s.bluetoothTitle);
    addBool("wifi_connected", s.wifiConnected);
    addString("ip", s.ip);
    addString("hostname", s.hostname);
    addNumber("wifi_rssi", s.wifiRssi);
    json += '}';
    sendJson(200, json);
}

void WebService::handleConfigGet() {
    const RuntimeConfig& c = _config->config();
    String json;
    json.reserve(4600);
    json = "{";
    auto addString = [&json](const char* key, const String& value) {
        if (json.length() > 1) json += ',';
        json += jsonQuote(key); json += ':'; json += jsonQuote(value);
    };
    auto addNumber = [&json](const char* key, long value) {
        if (json.length() > 1) json += ',';
        json += jsonQuote(key); json += ':'; json += String(value);
    };
#define PUT_S(key, field) addString(key, field)
#define PUT_N(key, field) addNumber(key, static_cast<long>(field))
    PUT_S("csrf", _token);
    PUT_N("schemaVersion", c.schemaVersion);
    PUT_S("device.name", c.device.name);
    PUT_N("audio.volume", c.audio.volume);
    PUT_N("audio.maxVolume", c.audio.maxVolume);
    PUT_N("audio.startVolume", c.audio.startVolume);
    PUT_N("audio.maxOutputVolume", c.audio.maxOutputVolume);
    PUT_N("audio.defaultSource", c.audio.defaultSource);
    PUT_N("audio.outputType", c.audio.outputType);
    PUT_N("audio.i2sBclk", c.audio.i2sBclk);
    PUT_N("audio.i2sLrclk", c.audio.i2sLrclk);
    PUT_N("audio.i2sDout", c.audio.i2sDout);
    PUT_N("playMedia.volumeMode", c.playMedia.volumeMode);
    PUT_N("playMedia.fixedVolume", c.playMedia.fixedVolume);
    PUT_N("bluetooth.autoReconnect", c.bluetooth.autoReconnect);
    PUT_N("bluetooth.reconnectDelayMs", c.bluetooth.reconnectDelayMs);
    PUT_S("bluetooth.deviceName", c.bluetooth.deviceName);
    PUT_N("bluetooth.discoverableEnabled", c.bluetooth.discoverableEnabled);
    PUT_N("bluetooth.discoverableSec", c.bluetooth.discoverableSec);
    PUT_N("bluetooth.rememberLastPeer", c.bluetooth.rememberLastPeer);
    PUT_N("radio.defaultStation", c.radio.defaultStation);
    PUT_N("radio.autostart", c.radio.autostart);
    PUT_N("radio.reconnectEnabled", c.radio.reconnectEnabled);
    PUT_N("radio.streamTimeoutMs", c.radio.streamTimeoutMs);
    PUT_N("radio.icyMetadataEnabled", c.radio.icyMetadataEnabled);
    PUT_N("display.type", c.display.type);
    PUT_N("display.brightness", c.display.brightness);
    PUT_N("display.screensaverEnabled", c.display.screensaverEnabled);
    PUT_N("display.screensaverTimeoutSec", c.display.screensaverTimeoutSec);
    PUT_N("display.st7789.sck", c.display.st7789.sck);
    PUT_N("display.st7789.mosi", c.display.st7789.mosi);
    PUT_N("display.st7789.cs", c.display.st7789.cs);
    PUT_N("display.st7789.dc", c.display.st7789.dc);
    PUT_N("display.st7789.rst", c.display.st7789.rst);
    PUT_N("display.ssd1306.sda", c.display.ssd1306.sda);
    PUT_N("display.ssd1306.scl", c.display.ssd1306.scl);
    PUT_N("display.ssd1306.address", c.display.ssd1306.address);
    PUT_N("encoder.pinA", c.encoder.pinA);
    PUT_N("encoder.pinB", c.encoder.pinB);
    PUT_N("encoder.pinButton", c.encoder.pinButton);
    PUT_N("encoder.direction", c.encoder.direction);
    PUT_N("encoder.volumeStep", c.encoder.volumeStep);
    PUT_N("encoder.accelerationEnabled", c.encoder.accelerationEnabled);
    PUT_S("mqtt.host", c.mqtt.host);
    PUT_N("mqtt.port", c.mqtt.port);
    PUT_S("mqtt.username", c.mqtt.username);
    PUT_N("mqtt.password_set", !c.mqtt.password.isEmpty());
    PUT_S("mqtt.rootTopic", c.mqtt.rootTopic);
    PUT_N("yoRadio.playlistCompat", c.yoRadio.playlistCompat);
    PUT_N("yoRadio.extensionsEnabled", c.yoRadio.extensionsEnabled);
    PUT_S("network.wifiSsid", c.network.wifiSsid);
    PUT_N("network.wifiPassword_set", !c.network.wifiPassword.isEmpty());
    PUT_S("network.hostname", c.network.hostname);
    PUT_N("network.mdnsEnabled", c.network.mdnsEnabled);
    PUT_N("network.dhcpEnabled", c.network.dhcpEnabled);
    PUT_N("network.ntpEnabled", c.network.ntpEnabled);
    PUT_S("network.timezone", c.network.timezone);
    PUT_N("features.bluetoothEnabled", c.features.bluetoothEnabled);
    PUT_N("features.radioEnabled", c.features.radioEnabled);
    PUT_N("features.playMediaEnabled", c.features.playMediaEnabled);
    PUT_N("features.displayEnabled", c.features.displayEnabled);
    PUT_N("features.encoderEnabled", c.features.encoderEnabled);
    PUT_N("features.buttonsEnabled", c.features.buttonsEnabled);
    PUT_N("features.mqttEnabled", c.features.mqttEnabled);
    PUT_N("features.yoRadioWsEnabled", c.features.yoRadioWsEnabled);
    PUT_N("features.haDiscoveryEnabled", c.features.haDiscoveryEnabled);
#undef PUT_N
#undef PUT_S
    json += '}';
    sendJson(200, json);
}

void WebService::handleConfigSave() {
    if (!authorizeAction()) return;
    RuntimeConfig candidate = _config->config();
    String error;
    if (!parseCandidate(_server, candidate, error)) {
        sendJson(400, String("{\"ok\":false,\"error\":") + jsonQuote(error) + "}");
        return;
    }
    if (!_config->validate(candidate)) {
        sendJson(400, "{\"ok\":false,\"error\":\"Niepoprawna konfiguracja: sprawdź zakresy, wymagane pola MQTT i kolizje GPIO.\"}");
        return;
    }
    if (!_config->save(candidate)) {
        sendJson(500, "{\"ok\":false,\"error\":\"Zapis NVS nie powiódł się.\"}");
        return;
    }
    prepareDisplayDisable(candidate);
    sendJson(200, "{\"ok\":true,\"message\":\"Ustawienia zapisane. VoxOne zostanie uruchomiony ponownie.\"}");
    scheduleRestart();
}

void WebService::handleResetDefaults() {
    if (!authorizeAction()) return;
    if (_server.arg("confirm") != "RESET") {
        sendJson(400, "{\"ok\":false,\"error\":\"Potwierdzenie resetu jest wymagane.\"}");
        return;
    }
    if (!_config->resetToDefaults()) {
        sendJson(500, "{\"ok\":false,\"error\":\"Zapis ustawień domyślnych nie powiódł się.\"}");
        return;
    }
    sendJson(200, "{\"ok\":true,\"message\":\"Przywrócono ustawienia domyślne. VoxOne uruchomi się ponownie.\"}");
    scheduleRestart();
}

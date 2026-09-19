#include "WiFiService.h"
#include "AppConfig.h"
#include "../config/ConfigManager.h"
#include "../core/StateStore.h"
#include "../diagnostics/Logger.h"

String WiFiService::makeDeviceSuffix() const {
    uint64_t mac = ESP.getEfuseMac();
    char buf[7];

    snprintf(
        buf,
        sizeof(buf),
        "%06llX",
        (unsigned long long)(mac & 0xFFFFFFULL)
    );

    return String(buf);
}

bool WiFiService::begin(ConfigManager& config) {
    _config = &config;
    _hostname = String("voxone-") + makeDeviceSuffix();
    _hostname.toLowerCase();

    WiFi.persistent(false);
    WiFi.setSleep(false);

    auto s = StateStore::instance().snapshot();
    s.hostname = _hostname;
    StateStore::instance().update(s);

    if (_config->wifiSsid().isEmpty()) {
        startConfigAp();
        return true;
    }

    connectStored();
    return true;
}

void WiFiService::connectStored() {
    if (_mdnsStarted) {
        MDNS.end();
        _mdnsStarted = false;
    }

    const String ssid = _config->wifiSsid();
    const String pass = _config->wifiPassword();

    WiFi.mode(WIFI_STA);
    WiFi.setHostname(_hostname.c_str());
    WiFi.begin(ssid.c_str(), pass.c_str());

    _connectStarted = millis();
    Logger::info("WIFI", "Connecting to " + ssid);
}

void WiFiService::startConfigAp() {
    if (_mdnsStarted) {
        MDNS.end();
        _mdnsStarted = false;
    }

    WiFi.disconnect(true, false);
    delay(20);
    WiFi.mode(WIFI_AP_STA);

    String ap =
        String(AppConfig::DEVICE_PREFIX) + "-" + makeDeviceSuffix();

    bool ok;

    if (String(AppConfig::AP_PASSWORD).isEmpty()) {
        ok = WiFi.softAP(
            ap.c_str(),
            nullptr,
            AppConfig::AP_CHANNEL
        );
    } else {
        ok = WiFi.softAP(
            ap.c_str(),
            AppConfig::AP_PASSWORD,
            AppConfig::AP_CHANNEL
        );
    }

    auto s = StateStore::instance().snapshot();
    s.apMode = ok;
    s.apSsid = ap;
    s.ip = WiFi.softAPIP().toString();
    s.wifiConnected = false;
    StateStore::instance().update(s);

    Logger::warn(
        "WIFI",
        String("Config AP: ") + ap +
            " @ " + WiFi.softAPIP().toString()
    );
}

void WiFiService::reconnect() {
    if (!_config) return;

    if (_config->wifiSsid().isEmpty()) {
        startConfigAp();
    } else {
        WiFi.disconnect(false, false);
        connectStored();
    }
}

void WiFiService::refreshState() {
    auto s = StateStore::instance().snapshot();

    if (WiFi.status() == WL_CONNECTED) {
        s.wifiConnected = true;
        s.wifiSsid = WiFi.SSID();
        s.ip = WiFi.localIP().toString();
        s.wifiRssi = WiFi.RSSI();

        if (!_mdnsStarted) {
            if (MDNS.begin(_hostname.c_str())) {
                MDNS.addService("http", "tcp", 80);
                _mdnsStarted = true;
                Logger::info(
                    "MDNS",
                    _hostname + ".local"
                );
            } else {
                Logger::warn(
                    "MDNS",
                    "MDNS.begin failed"
                );
            }
        }
    } else {
        s.wifiConnected = false;
    }

    StateStore::instance().update(s);
}

void WiFiService::loop() {
    refreshState();

    if (WiFi.status() == WL_CONNECTED) return;

    if (!_config || _config->wifiSsid().isEmpty()) return;

    if (!StateStore::instance().snapshot().apMode &&
        millis() - _connectStarted >
            AppConfig::WIFI_CONNECT_TIMEOUT_MS) {

        Logger::warn(
            "WIFI",
            "STA timeout -> config AP"
        );

        startConfigAp();
        _lastRetry = millis();
        return;
    }

    if (StateStore::instance().snapshot().apMode &&
        millis() - _lastRetry >
            AppConfig::WIFI_RETRY_INTERVAL_MS) {

        _lastRetry = millis();

        WiFi.begin(
            _config->wifiSsid().c_str(),
            _config->wifiPassword().c_str()
        );
    }
}

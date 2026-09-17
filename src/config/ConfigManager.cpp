#include "ConfigManager.h"

namespace {
static constexpr const char* NVS_NAMESPACE = "dinaudio";

// Existing legacy keys are intentionally preserved.
static constexpr const char* KEY_SCHEMA_VERSION = "cfg_ver";
static constexpr const char* KEY_WIFI_SSID = "wifi_ssid";
static constexpr const char* KEY_WIFI_PASS = "wifi_pass";
static constexpr const char* KEY_VOLUME = "volume";

// New schema-v1 keys.
static constexpr const char* KEY_MAX_VOLUME = "max_volume";
static constexpr const char* KEY_BT_AUTO_RECONNECT = "bt_reconn";
static constexpr const char* KEY_BT_RECONNECT_DELAY = "bt_reconn_ms";

static constexpr int DEFAULT_VOLUME = 25;
static constexpr int DEFAULT_MAX_VOLUME = 100;
static constexpr bool DEFAULT_BT_AUTO_RECONNECT = false;
static constexpr uint32_t DEFAULT_BT_RECONNECT_DELAY_MS = 2500;
}

bool ConfigManager::begin() {
    if (!_prefs.begin(NVS_NAMESPACE, false)) {
        return false;
    }

    const uint16_t storedVersion =
        _prefs.getUShort(KEY_SCHEMA_VERSION, 0);

    if (!migrateIfNeeded(storedVersion)) {
        return false;
    }

    return load();
}

bool ConfigManager::migrateIfNeeded(uint16_t storedVersion) {
    if (storedVersion == ConfigSchema::CURRENT_VERSION) {
        return true;
    }

    if (storedVersion == 0) {
        // Legacy DINaudio had no schema version. Existing wifi_ssid,
        // wifi_pass and volume keys remain untouched.
        return initializeSchemaV1();
    }

    if (storedVersion < ConfigSchema::CURRENT_VERSION) {
        // Future migrations go here, one schema at a time.
        return false;
    }

    // Configuration written by a newer firmware version.
    // Do not overwrite it. Load only the keys this firmware understands.
    return true;
}

bool ConfigManager::initializeSchemaV1() {
    // Add defaults only for keys that did not exist in the legacy store.
    if (!_prefs.isKey(KEY_MAX_VOLUME)) {
        _prefs.putInt(KEY_MAX_VOLUME, DEFAULT_MAX_VOLUME);
    }

    if (!_prefs.isKey(KEY_BT_AUTO_RECONNECT)) {
        _prefs.putBool(KEY_BT_AUTO_RECONNECT, DEFAULT_BT_AUTO_RECONNECT);
    }

    if (!_prefs.isKey(KEY_BT_RECONNECT_DELAY)) {
        _prefs.putUInt(
            KEY_BT_RECONNECT_DELAY,
            DEFAULT_BT_RECONNECT_DELAY_MS
        );
    }

    // Version is written last so an interrupted migration can be retried.
    return _prefs.putUShort(
        KEY_SCHEMA_VERSION,
        ConfigSchema::CURRENT_VERSION
    ) > 0;
}

bool ConfigManager::load() {
    _config.schemaVersion =
        _prefs.getUShort(
            KEY_SCHEMA_VERSION,
            ConfigSchema::CURRENT_VERSION
        );

    _config.network.wifiSsid =
        _prefs.getString(KEY_WIFI_SSID, "");

    _config.network.wifiPassword =
        _prefs.getString(KEY_WIFI_PASS, "");

    _config.audio.maxVolume =
        constrain(
            _prefs.getInt(KEY_MAX_VOLUME, DEFAULT_MAX_VOLUME),
            1,
            100
        );

    _config.audio.volume =
        constrain(
            _prefs.getInt(KEY_VOLUME, DEFAULT_VOLUME),
            0,
            _config.audio.maxVolume
        );

    _config.bluetooth.autoReconnect =
        _prefs.getBool(
            KEY_BT_AUTO_RECONNECT,
            DEFAULT_BT_AUTO_RECONNECT
        );

    _config.bluetooth.reconnectDelayMs =
        constrain(
            _prefs.getUInt(
                KEY_BT_RECONNECT_DELAY,
                DEFAULT_BT_RECONNECT_DELAY_MS
            ),
            static_cast<uint32_t>(0),
            static_cast<uint32_t>(60000)
        );

    return true;
}

bool ConfigManager::saveWifi(
    const String& ssid,
    const String& password
) {
    String normalizedSsid = ssid;
    normalizedSsid.trim();

    if (normalizedSsid.isEmpty()) {
        return false;
    }

    const size_t saved =
        _prefs.putString(KEY_WIFI_SSID, normalizedSsid);

    _prefs.putString(KEY_WIFI_PASS, password);

    if (saved == 0) {
        return false;
    }

    _config.network.wifiSsid = normalizedSsid;
    _config.network.wifiPassword = password;
    return true;
}

void ConfigManager::clearWifi() {
    _prefs.remove(KEY_WIFI_SSID);
    _prefs.remove(KEY_WIFI_PASS);

    _config.network.wifiSsid = "";
    _config.network.wifiPassword = "";
}

void ConfigManager::saveVolume(int value) {
    const int safeValue =
        constrain(value, 0, _config.audio.maxVolume);

    if (_prefs.putInt(KEY_VOLUME, safeValue) > 0) {
        _config.audio.volume = safeValue;
    }
}

bool ConfigManager::saveMaxVolume(int value) {
    const int safeValue = constrain(value, 1, 100);

    if (_prefs.putInt(KEY_MAX_VOLUME, safeValue) == 0) {
        return false;
    }

    _config.audio.maxVolume = safeValue;

    if (_config.audio.volume > safeValue) {
        saveVolume(safeValue);
    }

    return true;
}

bool ConfigManager::saveBluetoothAutoReconnect(bool enabled) {
    if (_prefs.putBool(KEY_BT_AUTO_RECONNECT, enabled) == 0) {
        return false;
    }

    _config.bluetooth.autoReconnect = enabled;
    return true;
}

bool ConfigManager::saveBluetoothReconnectDelayMs(uint32_t value) {
    const uint32_t safeValue =
        constrain(
            value,
            static_cast<uint32_t>(0),
            static_cast<uint32_t>(60000)
        );

    if (_prefs.putUInt(KEY_BT_RECONNECT_DELAY, safeValue) == 0) {
        return false;
    }

    _config.bluetooth.reconnectDelayMs = safeValue;
    return true;
}

#include "ConfigManager.h"

namespace {
// LEGACY NVS COMPATIBILITY: retain the existing namespace so user settings
// survive the product rename. TODO(ConfigManager): migrate all keys to
// "voxone" with verified writes and a power-loss-safe completion marker;
// keep the legacy namespace intact for rollback.
static constexpr const char* NVS_NAMESPACE = "dinaudio";

static constexpr const char* KEY_SCHEMA_VERSION = "cfg_ver";
static constexpr const char* KEY_WIFI_SSID = "wifi_ssid";
static constexpr const char* KEY_WIFI_PASS = "wifi_pass";
static constexpr const char* KEY_VOLUME = "volume";

static constexpr const char* KEY_MAX_VOLUME = "max_volume";
static constexpr const char* KEY_BT_AUTO_RECONNECT = "bt_reconn";
static constexpr const char* KEY_BT_RECONNECT_DELAY = "bt_reconn_ms";

static constexpr int DEFAULT_VOLUME = 25;
static constexpr int DEFAULT_MAX_VOLUME = 100;
static constexpr bool DEFAULT_BT_AUTO_RECONNECT = true;
static constexpr uint32_t DEFAULT_BT_RECONNECT_DELAY_MS = 10000;
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
    if (storedVersion > ConfigSchema::CURRENT_VERSION) {
        // Newer firmware wrote the configuration.
        // Do not overwrite unknown/newer data.
        return true;
    }

    uint16_t version = storedVersion;

    if (version == 0) {
        if (!initializeSchemaV1()) {
            return false;
        }
        version = 1;
    }

    if (version == 1) {
        if (!migrateV1ToV2()) {
            return false;
        }
        version = 2;
    }

    if (version == 2) {
        if (!migrateV2ToV3()) {
            return false;
        }
        version = 3;
    }

    return version == ConfigSchema::CURRENT_VERSION;
}

bool ConfigManager::initializeSchemaV1() {
    if (!_prefs.isKey(KEY_MAX_VOLUME)) {
        _prefs.putInt(KEY_MAX_VOLUME, DEFAULT_MAX_VOLUME);
    }

    if (!_prefs.isKey(KEY_BT_AUTO_RECONNECT)) {
        // Historical schema-v1 default.
        _prefs.putBool(KEY_BT_AUTO_RECONNECT, false);
    }

    if (!_prefs.isKey(KEY_BT_RECONNECT_DELAY)) {
        _prefs.putUInt(
            KEY_BT_RECONNECT_DELAY,
            DEFAULT_BT_RECONNECT_DELAY_MS
        );
    }

    // Write schema marker last.
    return _prefs.putUShort(KEY_SCHEMA_VERSION, 1) > 0;
}

bool ConfigManager::migrateV1ToV2() {
    // v0.4.0 activates the BT reconnect/ownership feature.
    // Prior firmware exposed no user-facing control for this setting,
    // so schema-v2 enables it by default for existing installations.
    if (_prefs.putBool(
            KEY_BT_AUTO_RECONNECT,
            DEFAULT_BT_AUTO_RECONNECT
        ) == 0) {
        return false;
    }

    if (!_prefs.isKey(KEY_BT_RECONNECT_DELAY)) {
        if (_prefs.putUInt(
                KEY_BT_RECONNECT_DELAY,
                DEFAULT_BT_RECONNECT_DELAY_MS
            ) == 0) {
            return false;
        }
    }

    return _prefs.putUShort(
        KEY_SCHEMA_VERSION,
        2
    ) > 0;
}

bool ConfigManager::migrateV2ToV3() {
    // VoxOne 0.4.0: wydluzony grace period dla realnego
    // wylaczenia i ponownego wlaczenia Bluetooth w telefonie.
    if (_prefs.putUInt(
            KEY_BT_RECONNECT_DELAY,
            DEFAULT_BT_RECONNECT_DELAY_MS
        ) == 0) {
        return false;
    }

    return _prefs.putUShort(
        KEY_SCHEMA_VERSION,
        3
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


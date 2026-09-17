#include "SettingsStore.h"

bool SettingsStore::begin() {
    return _prefs.begin("dinaudio", false);
}

String SettingsStore::wifiSsid() {
    return _prefs.getString("wifi_ssid", "");
}

String SettingsStore::wifiPassword() {
    return _prefs.getString("wifi_pass", "");
}

bool SettingsStore::saveWifi(const String& ssid, const String& password) {
    if (ssid.isEmpty()) return false;
    size_t a = _prefs.putString("wifi_ssid", ssid);
    _prefs.putString("wifi_pass", password);
    return a > 0;
}

void SettingsStore::clearWifi() {
    _prefs.remove("wifi_ssid");
    _prefs.remove("wifi_pass");
}

int SettingsStore::volume() {
    return constrain(_prefs.getInt("volume", 25), 0, 100);
}

void SettingsStore::saveVolume(int value) {
    _prefs.putInt("volume", constrain(value, 0, 100));
}

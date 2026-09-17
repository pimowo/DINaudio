#pragma once
#include <Arduino.h>
#include <Preferences.h>

class SettingsStore {
public:
    bool begin();

    String wifiSsid();
    String wifiPassword();
    bool saveWifi(const String& ssid, const String& password);
    void clearWifi();

    int volume();
    void saveVolume(int value);

private:
    Preferences _prefs;
};

#pragma once

#include <Arduino.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ST7789.h>

class DisplayService {
public:
    DisplayService();

    void begin();
    void loop();
    void redraw();

private:
    Adafruit_ST7789 _tft;

    bool _layoutDrawn = false;

    int _lastVolume = -1;
    bool _lastBtConnected = false;
    bool _lastBtPlaying = false;
    String _lastIp;
    String _lastApSsid;
    bool _lastWifiConnected = false;
    bool _lastApMode = false;

    void drawStaticLayout();
    void updateDynamicFields(bool force = false);

    void drawVolume(int volume);
    void drawBtState(bool connected, bool playing);
    void drawNetworkLine(
        bool wifiConnected,
        const String& ip,
        bool apMode,
        const String& apSsid
    );
};

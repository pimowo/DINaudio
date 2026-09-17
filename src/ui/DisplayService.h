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
    bool _volumeScreenActive = false;
    uint32_t _volumeScreenUntil = 0;

    int _lastVolume = -1;
    bool _lastBtConnected = false;
    bool _lastBtPlaying = false;
    String _lastBtPeer;
    String _lastBtTitle;
    String _lastBtArtist;

    int _lastWifiLevel = -1;
    bool _lastWifiConnected = false;
    bool _lastApMode = false;
    uint32_t _lastWifiDrawMs = 0;

    bool _lastTimeValid = false;
    String _lastClockText;

    void drawStaticLayout();
    void updatePlayerFields(bool force = false);

    void drawHeader(
        bool btConnected,
        const String& peerName
    );

    void drawMetadata(
        bool btConnected,
        const String& artist,
        const String& title
    );

    void drawSourceInfo(
        bool btConnected,
        bool btPlaying
    );

    void drawWifiIndicator(
        bool wifiConnected,
        int wifiRssi,
        bool apMode
    );

    void drawClock(
        bool valid,
        const String& clockText
    );

    void drawVolumeBar(int volume);

    void showVolumeScreen(
        int volume,
        const String& ip
    );

    void drawVolumeValue(int volume);
    void drawVolumeIp(const String& ip);

    static int wifiLevel(int rssi);

    static String tftText(
        const String& value,
        bool uppercase = true
    );

    static String fitText(
        const String& value,
        size_t maxChars
    );
};

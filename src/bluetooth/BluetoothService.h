#pragma once

#include <Arduino.h>
#include <BluetoothA2DPSink.h>

#include "../audio/AudioOutput.h"

class BluetoothService {
public:
    bool begin(AudioOutput& output, const String& deviceName, int volume);
    void loop();
    void setVolume(int volume0to100);

    void play();
    void pause();
    void stop();
    void next();
    void previous();

    bool reconnect();

    bool started() const { return _started; }

private:
    BluetoothA2DPSink _sink;

    bool _started = false;
    String _deviceName;
    uint32_t _lastPoll = 0;

    esp_a2d_connection_state_t _lastConnectionState =
        ESP_A2D_CONNECTION_STATE_DISCONNECTED;
    esp_a2d_audio_state_t _lastAudioState =
        ESP_A2D_AUDIO_STATE_STOPPED;

    volatile bool _remoteVolumePending = false;
    volatile int _remoteVolume127 = 0;

    volatile bool _metadataPending = false;
    volatile bool _peerNamePending = false;

    char _pendingTitle[128] = {0};
    char _pendingArtist[128] = {0};
    char _pendingPeerName[64] = {0};

    void publishState(
        esp_a2d_connection_state_t connectionState,
        esp_a2d_audio_state_t audioState
    );
    void flushPendingEvents();

    static uint8_t mapVolume(int volume0to100);
    static int mapRemoteVolume(int volume0to127);

    static BluetoothService* _instance;
    static void onRemoteVolume(int volume);
    static void onMetadata(uint8_t id, const uint8_t* text);
    static void onPeerName(char* peerName);

    void copyMetadata(uint8_t id, const uint8_t* text);
};

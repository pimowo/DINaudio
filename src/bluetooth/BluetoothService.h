#pragma once

#include <Arduino.h>
#include <BluetoothA2DPSink.h>

#include "../audio/AudioOutput.h"

class BluetoothService {
public:
    bool begin(AudioOutput& output, const String& deviceName, int volume);
    void loop();
    void setVolume(int volume0to100);

    bool started() const { return _started; }

private:
    BluetoothA2DPSink _sink;

    bool _started = false;
    String _deviceName;
    uint32_t _lastPoll = 0;

    esp_a2d_connection_state_t _lastConnectionState = ESP_A2D_CONNECTION_STATE_DISCONNECTED;
    esp_a2d_audio_state_t _lastAudioState = ESP_A2D_AUDIO_STATE_STOPPED;

    void publishState(
        esp_a2d_connection_state_t connectionState,
        esp_a2d_audio_state_t audioState
    );

    static uint8_t mapVolume(int volume0to100);
};

#pragma once
#include <Arduino.h>

enum class PlaybackState : uint8_t {
    Stop,
    Playing
};

enum class AudioSource : uint8_t {
    Stop,
    Test,
    Bluetooth
};

struct DeviceState {
    int volume = 25;
    PlaybackState playback = PlaybackState::Stop;
    AudioSource audioSource = AudioSource::Stop;

    bool bluetoothStarted = false;
    bool bluetoothConnected = false;
    bool bluetoothPlaying = false;
    String bluetoothDeviceName;

    bool wifiConnected = false;
    String wifiSsid;
    String ip;
    int wifiRssi = 0;

    bool apMode = false;
    String apSsid;
    String hostname;

    bool otaInProgress = false;
    String lastMessage = "BOOT";
};

class StateStore {
public:
    static StateStore& instance();
    DeviceState snapshot() const;
    void update(const DeviceState& s);

private:
    mutable portMUX_TYPE _mux = portMUX_INITIALIZER_UNLOCKED;
    DeviceState _state;
};

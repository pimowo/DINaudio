#pragma once

#include <Arduino.h>
#include <BluetoothA2DPSink.h>
#include "BluetoothLifecycle.h"

#include "../audio/AudioOutputManager.h"

class BluetoothService {
public:
    // Shared section keeps both prepared switch APIs in the actual size report
    // even though normal App operation does not call them yet.
    bool begin(AudioOutputManager& output, const String& deviceName, int volume)
        __attribute__((section(".text.bt_lifecycle")));
    void loop();
    void setVolume(int volume0to100);

    void play();
    void pause();
    void stop();
    void next();
    void previous();

    bool reconnect();

    // App task only; never called automatically by disconnect/grace handling.
    // Retain the currently unused APIs so build sizes include their real cost.
    bool suspendForSourceSwitch() __attribute__((section(".text.bt_lifecycle")));
    bool resumeAfterSourceSwitch() __attribute__((section(".text.bt_lifecycle")));
    // App consumes this once, also when suspend+resume happen in one loop.
    bool consumeSourceSwitchEvent() {
        if (!onAppTask()) return false;
        const bool pending = _sourceSwitchPending;
        _sourceSwitchPending = false;
        return pending;
    }
    bool sourceSwitchSuspended() const {
        return _lifecycle != Lifecycle::Stopped &&
            _lifecycle != Lifecycle::Running;
    }

    bool started() const { return _started; }

private:
    enum class Lifecycle : uint8_t {
        Stopped, Running, Suspending, Suspended, Resuming, Fault
    };
    BluetoothLifecycleSink _sink;
    BluetoothOutputGate _outputGate;
    Lifecycle _lifecycle = Lifecycle::Stopped;
    bool _sourceSwitchPending = false;
    TaskHandle_t _appTask = nullptr;
    std::atomic<bool> _acceptCallbacks{false};
    int _volume = 25;
    // Recent local AVRCP values identify delayed echoes from the phone.
    struct LocalVolumeMark {
        uint8_t raw = 0;
        uint32_t atMs = 0;
    };
    LocalVolumeMark _recentLocalVolume[16];
    uint8_t _recentLocalCount = 0;
    uint8_t _recentLocalNext = 0;
    std::atomic<uint32_t> _rawPcmCallbacks{0};
    std::atomic<uint32_t> _rawPcmBytes{0};
    std::atomic<uint32_t> _rawNonzeroCallbacks{0};
    std::atomic<uint32_t> _pcmCallbacks{0};
    std::atomic<uint32_t> _pcmBytes{0};
    std::atomic<uint32_t> _pcmNonzeroCallbacks{0};
    std::atomic<uint16_t> _pendingSampleRate{0};
    std::atomic<int> _avrcPlayback{-1};
    uint16_t _configuredSampleRate = 0;
    int _lastAvrcPlayback = -1;
    uint32_t _lastDiagnosticsMs = 0;
    esp_bd_addr_t _lastPeerAddress = {0};
    bool _hasLastPeer = false;
    char _lastPeerName[64] = {0};
    portMUX_TYPE _peerMux = portMUX_INITIALIZER_UNLOCKED;
    bool configureAndStart(bool resume);
    bool finishSuspend();
    bool onAppTask() const;
    void publishStoppedTransport();
    static void onConnectionState(esp_a2d_connection_state_t state, void* context);
    static void onRawPcm(const uint8_t* data, uint32_t len);
    static void onPcm(const uint8_t* data, uint32_t len);
    static void onSampleRate(uint16_t rate);
    static void onAvrcPlayback(esp_avrc_playback_stat_t status);
    void applySampleRate();
    void logAudioDiagnostics(uint32_t now);
    // The lease stays pinned for the entire sink lifetime, including idle and
    // reconnect grace. AVRCP stop() does not stop the asynchronous producer.
    AudioOutputManager* _outputManager = nullptr;
    Print* _outputStream = nullptr;

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

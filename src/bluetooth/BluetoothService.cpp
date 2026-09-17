#include "BluetoothService.h"

#include "AppConfig.h"
#include "../core/StateStore.h"
#include "../diagnostics/Logger.h"

uint8_t BluetoothService::mapVolume(int volume0to100) {
    const int v = constrain(volume0to100, 0, 100);
    return static_cast<uint8_t>((v * 127 + 50) / 100);
}

bool BluetoothService::begin(AudioOutput& output, const String& deviceName, int volume) {
    if (_started) return true;

    if (!output.ready()) {
        Logger::error("BT", "AudioOutput is not ready");
        return false;
    }

    _deviceName = deviceName;

    // M2.1: nie odtwarzamy automatycznie poprzedniego połączenia.
    // Finalny pairing/reconnect manager powstanie później.
    _sink.set_auto_reconnect(false);

    // ESP32-A2DP może pisać bezpośrednio do Arduino I2SClass (Print).
    _sink.set_output(output.stream());

    // Lokalna głośność DINaudio 0..100 -> A2DP 0..127.
    _sink.set_volume(mapVolume(volume));

    _sink.start(_deviceName.c_str());
    _started = true;

    auto s = StateStore::instance().snapshot();
    s.bluetoothStarted = true;
    s.bluetoothDeviceName = _deviceName;
    s.bluetoothConnected = false;
    s.bluetoothPlaying = false;
    s.audioSource = AudioSource::Stop;
    StateStore::instance().update(s);

    Logger::info("BT", "A2DP Sink started as " + _deviceName);
    return true;
}

void BluetoothService::setVolume(int volume0to100) {
    if (!_started) return;
    _sink.set_volume(mapVolume(volume0to100));
}

void BluetoothService::publishState(
    esp_a2d_connection_state_t connectionState,
    esp_a2d_audio_state_t audioState
) {
    const bool connected =
        connectionState == ESP_A2D_CONNECTION_STATE_CONNECTED;

    const bool playing =
        connected && audioState == ESP_A2D_AUDIO_STATE_STARTED;

    auto s = StateStore::instance().snapshot();

    const bool changed =
        s.bluetoothConnected != connected ||
        s.bluetoothPlaying != playing ||
        s.audioSource != (connected ? AudioSource::Bluetooth : AudioSource::Stop);

    if (!changed) return;

    s.bluetoothConnected = connected;
    s.bluetoothPlaying = playing;

    // Ważne: BT posiada urządzenie przez cały czas połączenia,
    // nie tylko wtedy, gdy telefon aktualnie wysyła próbki.
    s.audioSource = connected ? AudioSource::Bluetooth : AudioSource::Stop;
    s.playback = playing ? PlaybackState::Playing : PlaybackState::Stop;

    if (connected) {
        s.lastMessage = playing ? "BT_PLAYING" : "BT_CONNECTED";
    } else {
        s.lastMessage = "BT_DISCONNECTED";
    }

    StateStore::instance().update(s);

    Logger::info(
        "BT",
        String("connection=") + _sink.to_str(connectionState) +
        " audio=" + _sink.to_str(audioState)
    );
}

void BluetoothService::loop() {
    if (!_started) return;

    const uint32_t now = millis();
    if (now - _lastPoll < AppConfig::BT_STATE_POLL_MS) return;
    _lastPoll = now;

    const auto connectionState = _sink.get_connection_state();
    const auto audioState = _sink.get_audio_state();

    if (connectionState == _lastConnectionState &&
        audioState == _lastAudioState) {
        return;
    }

    _lastConnectionState = connectionState;
    _lastAudioState = audioState;
    publishState(connectionState, audioState);
}

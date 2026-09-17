#include "BluetoothService.h"

#include <cstring>

#include "AppConfig.h"
#include "../core/StateStore.h"
#include "../core/CommandQueue.h"
#include "../diagnostics/Logger.h"

BluetoothService* BluetoothService::_instance = nullptr;

uint8_t BluetoothService::mapVolume(int volume0to100) {
    const int v = constrain(volume0to100, 0, 100);
    return static_cast<uint8_t>((v * 127 + 50) / 100);
}

int BluetoothService::mapRemoteVolume(int volume0to127) {
    const int v = constrain(volume0to127, 0, 127);
    return (v * 100 + 63) / 127;
}

void BluetoothService::onRemoteVolume(int volume) {
    if (!_instance) return;
    _instance->_remoteVolume127 = constrain(volume, 0, 127);
    _instance->_remoteVolumePending = true;
}

void BluetoothService::onMetadata(uint8_t id, const uint8_t* text) {
    if (!_instance || !text) return;
    _instance->copyMetadata(id, text);
}

void BluetoothService::onPeerName(char* peerName) {
    if (!_instance || !peerName) return;

    strncpy(
        _instance->_pendingPeerName,
        peerName,
        sizeof(_instance->_pendingPeerName) - 1
    );
    _instance->_pendingPeerName[
        sizeof(_instance->_pendingPeerName) - 1
    ] = '\0';

    _instance->_peerNamePending = true;
}

void BluetoothService::copyMetadata(
    uint8_t id,
    const uint8_t* text
) {
    const char* src =
        reinterpret_cast<const char*>(text);

    if (id == ESP_AVRC_MD_ATTR_TITLE) {
        strncpy(
            _pendingTitle,
            src,
            sizeof(_pendingTitle) - 1
        );
        _pendingTitle[
            sizeof(_pendingTitle) - 1
        ] = '\0';
        _metadataPending = true;
    } else if (id == ESP_AVRC_MD_ATTR_ARTIST) {
        strncpy(
            _pendingArtist,
            src,
            sizeof(_pendingArtist) - 1
        );
        _pendingArtist[
            sizeof(_pendingArtist) - 1
        ] = '\0';
        _metadataPending = true;
    }
}

bool BluetoothService::begin(
    AudioOutput& output,
    const String& deviceName,
    int volume
) {
    if (_started) return true;

    if (!output.ready()) {
        Logger::error(
            "BT",
            "AudioOutput is not ready"
        );
        return false;
    }

    _instance = this;
    _deviceName = deviceName;

    // DINaudio owns reconnect policy.
    _sink.set_auto_reconnect(false);
    _sink.set_output(output.stream());

    _sink.set_avrc_metadata_attribute_mask(
        ESP_AVRC_MD_ATTR_TITLE |
        ESP_AVRC_MD_ATTR_ARTIST
    );
    _sink.set_avrc_metadata_callback(
        &BluetoothService::onMetadata
    );
    _sink.set_avrc_rn_volumechange(
        &BluetoothService::onRemoteVolume
    );
    _sink.set_peer_name_callback(
        &BluetoothService::onPeerName
    );

    _sink.set_volume(mapVolume(volume));

    _sink.start(_deviceName.c_str());
    _started = true;

    auto s = StateStore::instance().snapshot();
    s.bluetoothStarted = true;
    s.bluetoothDeviceName = _deviceName;
    s.bluetoothConnected = false;
    s.bluetoothPlaying = false;
    s.bluetoothPeerName = "";
    s.bluetoothTitle = "";
    s.bluetoothArtist = "";
    s.bluetoothOwnership =
        BluetoothOwnershipState::Disconnected;
    s.bluetoothReconnectGrace = false;
    StateStore::instance().update(s);

    Logger::info(
        "BT",
        "A2DP/AVRCP started as " + _deviceName
    );
    return true;
}

void BluetoothService::setVolume(int volume0to100) {
    if (!_started) return;
    _sink.set_volume(mapVolume(volume0to100));
}

void BluetoothService::play() {
    if (_started) _sink.play();
}

void BluetoothService::pause() {
    if (_started) _sink.pause();
}

void BluetoothService::stop() {
    if (_started) _sink.stop();
}

void BluetoothService::next() {
    if (_started) _sink.next();
}

void BluetoothService::previous() {
    if (_started) _sink.previous();
}

bool BluetoothService::reconnect() {
    if (!_started) return false;

    const bool started = _sink.reconnect();

    Logger::info(
        "BT",
        started
            ? "Reconnect requested"
            : "Reconnect request unavailable"
    );

    return started;
}

void BluetoothService::flushPendingEvents() {
    if (_remoteVolumePending) {
        const int raw = _remoteVolume127;
        _remoteVolumePending = false;

        CommandQueue::instance().push({
            CommandType::SetVolumeAbsolute,
            CommandSource::Bluetooth,
            mapRemoteVolume(raw)
        });
    }

    if (_metadataPending) {
        _metadataPending = false;

        auto s = StateStore::instance().snapshot();
        s.bluetoothTitle = _pendingTitle;
        s.bluetoothArtist = _pendingArtist;
        StateStore::instance().update(s);
    }

    if (_peerNamePending) {
        _peerNamePending = false;

        auto s = StateStore::instance().snapshot();
        s.bluetoothPeerName = _pendingPeerName;
        StateStore::instance().update(s);
    }
}

void BluetoothService::publishState(
    esp_a2d_connection_state_t connectionState,
    esp_a2d_audio_state_t audioState
) {
    const bool connected =
        connectionState ==
        ESP_A2D_CONNECTION_STATE_CONNECTED;

    const bool playing =
        connected &&
        audioState ==
        ESP_A2D_AUDIO_STATE_STARTED;

    auto s = StateStore::instance().snapshot();

    const bool changed =
        s.bluetoothConnected != connected ||
        s.bluetoothPlaying != playing;

    if (!changed) return;

    // BluetoothService reports transport state only.
    // App owns source arbitration / ownership policy.
    s.bluetoothConnected = connected;
    s.bluetoothPlaying = playing;

    StateStore::instance().update(s);

    Logger::info(
        "BT",
        String("connection=") +
        _sink.to_str(connectionState) +
        " audio=" +
        _sink.to_str(audioState)
    );
}

void BluetoothService::loop() {
    if (!_started) return;

    flushPendingEvents();

    const uint32_t now = millis();

    if (
        now - _lastPoll <
        AppConfig::BT_STATE_POLL_MS
    ) {
        return;
    }

    _lastPoll = now;

    const auto connectionState =
        _sink.get_connection_state();

    const auto audioState =
        _sink.get_audio_state();

    if (
        connectionState == _lastConnectionState &&
        audioState == _lastAudioState
    ) {
        return;
    }

    _lastConnectionState = connectionState;
    _lastAudioState = audioState;

    publishState(
        connectionState,
        audioState
    );
}

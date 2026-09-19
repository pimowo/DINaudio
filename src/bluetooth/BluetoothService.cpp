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
    portENTER_CRITICAL(&_instance->_peerMux);
    if (_instance->_acceptCallbacks.load()) {
        _instance->_remoteVolume127 = constrain(volume, 0, 127);
        _instance->_remoteVolumePending = true;
    }
    portEXIT_CRITICAL(&_instance->_peerMux);
}

void BluetoothService::onMetadata(uint8_t id, const uint8_t* text) {
    if (!_instance || !text) return;
    portENTER_CRITICAL(&_instance->_peerMux);
    if (_instance->_acceptCallbacks.load()) _instance->copyMetadata(id, text);
    portEXIT_CRITICAL(&_instance->_peerMux);
}

void BluetoothService::onPeerName(char* peerName) {
    if (!_instance || !peerName) return;
    portENTER_CRITICAL(&_instance->_peerMux);
    if (!_instance->_acceptCallbacks.load()) {
        portEXIT_CRITICAL(&_instance->_peerMux);
        return;
    }
    strncpy(_instance->_lastPeerName, peerName, sizeof(_instance->_lastPeerName) - 1);
    _instance->_lastPeerName[sizeof(_instance->_lastPeerName) - 1] = '\0';

    strncpy(
        _instance->_pendingPeerName,
        peerName,
        sizeof(_instance->_pendingPeerName) - 1
    );
    _instance->_pendingPeerName[
        sizeof(_instance->_pendingPeerName) - 1
    ] = '\0';

    _instance->_peerNamePending = true;
    portEXIT_CRITICAL(&_instance->_peerMux);
}


void BluetoothService::onConnectionState(
    esp_a2d_connection_state_t state, void* context
) {
    auto* service = static_cast<BluetoothService*>(context);
    if (!service ||
        state != ESP_A2D_CONNECTION_STATE_CONNECTED) return;
    // The library invokes this after updating peer_bd_addr.
    portENTER_CRITICAL(&service->_peerMux);
    if (!service->_acceptCallbacks.load()) {
        portEXIT_CRITICAL(&service->_peerMux);
        return;
    }
    memcpy(service->_lastPeerAddress, *service->_sink.get_current_peer_address(),
        ESP_BD_ADDR_LEN);
    service->_hasLastPeer = true;
    portEXIT_CRITICAL(&service->_peerMux);
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
    AudioOutputManager& output,
    const String& deviceName,
    int volume
) {
    if (_started) {
        return _outputManager == &output &&
            output.isOwnedBy(AudioOutputOwner::Bluetooth);
    }
    if (_lifecycle != Lifecycle::Stopped) return false;
    _instance = this;
    _outputManager = &output;
    _deviceName = deviceName;
    _volume = constrain(volume, 0, 100);
    _appTask = xTaskGetCurrentTaskHandle();
    if (!configureAndStart(false)) return false;

    auto s = StateStore::instance().snapshot();
    s.bluetoothStarted = true;
    s.bluetoothDeviceName = _deviceName;
    s.bluetoothConnected = false;
    s.bluetoothPlaying = false;
    s.bluetoothPeerName = "";
    s.bluetoothTitle = "";
    s.bluetoothArtist = "";
    s.bluetoothOwnership = BluetoothOwnershipState::Disconnected;
    s.bluetoothReconnectGrace = false;
    StateStore::instance().update(s);
    Logger::info("BT", "A2DP/AVRCP started as " + _deviceName);
    return true;
}

bool BluetoothService::configureAndStart(bool resume) {
    Print* stream = _outputManager->attach(AudioOutputOwner::Bluetooth);
    if (stream == nullptr) {
        Logger::error("BT", "Bluetooth output lease unavailable");
        return false;
    }
    if (resume) _sink.resetTransportForResume();
    _outputGate.open(*stream);
    _acceptCallbacks.store(true);
    // start must not auto-connect on resume: policy belongs to VoxOne.
    _sink.set_auto_reconnect(false);
    _sink.set_output(_outputGate);
    _sink.set_on_connection_state_changed(&BluetoothService::onConnectionState, this);
    // Audio state continues to be polled, as in 0.4.0.
    _sink.set_on_audio_state_changed(nullptr);
    _sink.set_on_audio_state_changed_post(nullptr);
    // Initial sink uses the library's GENERAL_DISCOVERABLE default. On resume
    // the BT stack is already enabled and discovery can safely be restored.
    if (resume) _sink.set_discoverability(ESP_BT_GENERAL_DISCOVERABLE);
    _sink.set_avrc_metadata_attribute_mask(
        ESP_AVRC_MD_ATTR_TITLE | ESP_AVRC_MD_ATTR_ARTIST
    );
    _sink.set_avrc_metadata_callback(&BluetoothService::onMetadata);
    _sink.set_avrc_rn_volumechange(&BluetoothService::onRemoteVolume);
    _sink.set_peer_name_callback(&BluetoothService::onPeerName);
    _sink.set_volume(mapVolume(_volume));
    _sink.expectProfileEvent();
    _sink.start(_deviceName.c_str());
    if (!_sink.waitForProfile(ESP_A2D_INIT_SUCCESS, 3000)) {
        Logger::error("BT", "Start failed: A2DP init not confirmed");
        _lifecycle = Lifecycle::Suspending;
        finishSuspend();
        return false;
    }
    _started = true;
    _lifecycle = Lifecycle::Running;
    return true;
}

bool BluetoothService::onAppTask() const {
    return _appTask != nullptr && xTaskGetCurrentTaskHandle() == _appTask;
}

void BluetoothService::publishStoppedTransport() {
    auto s = StateStore::instance().snapshot();
    s.bluetoothStarted = false;
    s.bluetoothConnected = false;
    s.bluetoothPlaying = false;
    // Keep peer/metadata and source ownership. App handles intentional switches.
    StateStore::instance().update(s);
}

bool BluetoothService::suspendForSourceSwitch() {
    if (!onAppTask()) {
        Logger::error("BT", "Suspend requires App task");
        return false;
    }
    if (_lifecycle == Lifecycle::Suspended) return true;
    if (_lifecycle != Lifecycle::Running) {
        Logger::warn("BT", "Suspend rejected: invalid lifecycle");
        return false;
    }
    _lifecycle = Lifecycle::Suspending;
    _started = false;
    _sourceSwitchPending = true;
    return finishSuspend();
}

bool BluetoothService::finishSuspend() {
    portENTER_CRITICAL(&_peerMux);
    _acceptCallbacks.store(false);
    portEXIT_CRITICAL(&_peerMux);
    // All VoxOne callbacks that accepted events have now completed.
    flushPendingEvents();
    if (!_outputGate.closeAndDrain(2000)) {
        _lifecycle = Lifecycle::Fault;
        Logger::error("BT", "Suspend failed: PCM write still active; lease retained");
        publishStoppedTransport();
        return false;
    }
    _sink.set_auto_reconnect(false); // end(false) must not erase peer NVS.
    _sink.set_discoverability(ESP_BT_NON_DISCOVERABLE);
    _sink.set_connectable(false);
    // end(false) clears last_connection BEFORE disconnect; disconnect first.
    if (_sink.get_connection_state() != ESP_A2D_CONNECTION_STATE_DISCONNECTED) {
        // Includes an in-progress connection to a peer not yet recorded as last.
        memcpy(*_sink.get_last_peer_address(), *_sink.get_current_peer_address(),
            ESP_BD_ADDR_LEN);
        _sink.disconnect();
        const uint32_t started = millis();
        while (_sink.get_connection_state() != ESP_A2D_CONNECTION_STATE_DISCONNECTED &&
               millis() - started < 3000) delay(1);
        if (_sink.get_connection_state() != ESP_A2D_CONNECTION_STATE_DISCONNECTED) {
            _lifecycle = Lifecycle::Fault;
            Logger::error("BT", "Suspend failed: disconnect timeout; lease retained");
            publishStoppedTransport();
            return false;
        }
    }
    // A disconnect event can restore connectability; disable it again.
    _sink.set_connectable(false);
    _sink.expectProfileEvent();
    _sink.end(false);
    if (!_sink.waitForProfile(ESP_A2D_DEINIT_SUCCESS, 3000)) {
        _lifecycle = Lifecycle::Fault;
        Logger::error("BT", "Suspend failed: deinit not confirmed; lease retained");
        publishStoppedTransport();
        return false;
    }
    _remoteVolumePending = _metadataPending = _peerNamePending = false;
    _lastConnectionState = ESP_A2D_CONNECTION_STATE_DISCONNECTED;
    _lastAudioState = ESP_A2D_AUDIO_STATE_STOPPED;
    if (!_outputManager->detach(AudioOutputOwner::Bluetooth) ||
        !_outputManager->release(AudioOutputOwner::Bluetooth)) {
        _lifecycle = Lifecycle::Fault;
        Logger::error("BT", "Suspend failed: output teardown; lease retained");
        publishStoppedTransport();
        return false;
    }
    _lifecycle = Lifecycle::Suspended;
    publishStoppedTransport();
    Logger::info("BT", "Suspended for source switch; I2S released");
    return true;
}

bool BluetoothService::resumeAfterSourceSwitch() {
    if (!onAppTask()) {
        Logger::error("BT", "Resume requires App task");
        return false;
    }
    if (_lifecycle != Lifecycle::Suspended) {
        Logger::warn("BT", "Resume rejected: suspend required");
        return false;
    }
    if (!_outputManager->acquire(AudioOutputOwner::Bluetooth)) {
        Logger::warn("BT", "Resume denied: output unavailable");
        return false;
    }
    _lifecycle = Lifecycle::Resuming;
    if (!configureAndStart(true)) {
        // If attach failed no producer started: release our new reservation.
        if (_lifecycle == Lifecycle::Resuming) {
            if (_outputManager->release(AudioOutputOwner::Bluetooth)) {
                _lifecycle = Lifecycle::Suspended;
            } else _lifecycle = Lifecycle::Fault;
        }
        return false;
    }
    _lastPoll = 0;
    auto s = StateStore::instance().snapshot();
    s.bluetoothStarted = true;
    s.bluetoothConnected = false;
    s.bluetoothPlaying = false;
    StateStore::instance().update(s);
    Logger::info("BT", "Resumed; explicit VoxOne reconnect available");
    return true;
}

void BluetoothService::setVolume(int volume0to100) {
    _volume = constrain(volume0to100, 0, 100);
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

    // VoxOne's last connected peer is authoritative, including after
    // end(false). Restore RAM only when App explicitly requests reconnect.
    portENTER_CRITICAL(&_peerMux);
    if (_hasLastPeer) memcpy(*_sink.get_last_peer_address(), _lastPeerAddress,
        ESP_BD_ADDR_LEN);
    portEXIT_CRITICAL(&_peerMux);
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
    int raw;
    bool volumePending, metadataPending, peerPending;
    char title[sizeof(_pendingTitle)];
    char artist[sizeof(_pendingArtist)];
    char peer[sizeof(_pendingPeerName)];
    portENTER_CRITICAL(&_peerMux);
    volumePending = _remoteVolumePending;
    metadataPending = _metadataPending;
    peerPending = _peerNamePending;
    raw = _remoteVolume127;
    if (metadataPending) {
        memcpy(title, _pendingTitle, sizeof(title));
        memcpy(artist, _pendingArtist, sizeof(artist));
    }
    if (peerPending) memcpy(peer, _pendingPeerName, sizeof(peer));
    _remoteVolumePending = _metadataPending = _peerNamePending = false;
    portEXIT_CRITICAL(&_peerMux);

    if (volumePending) {
        _volume = mapRemoteVolume(raw);

        CommandQueue::instance().push({
            CommandType::SetVolumeAbsolute,
            CommandSource::Bluetooth,
            mapRemoteVolume(raw)
        });
    }

    if (metadataPending) {
        auto s = StateStore::instance().snapshot();
        s.bluetoothTitle = title;
        s.bluetoothArtist = artist;
        StateStore::instance().update(s);
    }

    if (peerPending) {
        auto s = StateStore::instance().snapshot();
        s.bluetoothPeerName = peer;
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

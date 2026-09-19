#include "BluetoothService.h"

#include <cstring>

#include "AppConfig.h"
#include "../core/StateStore.h"
#include "../core/CommandQueue.h"
#include "../diagnostics/Logger.h"

BluetoothService* BluetoothService::_instance = nullptr;

namespace {
bool containsNonzeroSample(const uint8_t* data, uint32_t len) {
    if (!data) return false;
    const uint32_t inspected = len < 64 ? len : 64;
    for (uint32_t i = 0; i < inspected; ++i) {
        if (data[i] != 0) return true;
    }
    return false;
}
}

void BluetoothService::onRawPcm(const uint8_t* data, uint32_t len) {
    auto* service = _instance;
    if (!service || !service->_acceptCallbacks.load(std::memory_order_relaxed)) return;
    service->_rawPcmCallbacks.fetch_add(1, std::memory_order_relaxed);
    service->_rawPcmBytes.fetch_add(len, std::memory_order_relaxed);
    if (containsNonzeroSample(data, len))
        service->_rawNonzeroCallbacks.fetch_add(1, std::memory_order_relaxed);
}

void BluetoothService::onPcm(const uint8_t* data, uint32_t len) {
    auto* service = _instance;
    if (!service || !service->_acceptCallbacks.load(std::memory_order_relaxed)) return;
    service->_pcmCallbacks.fetch_add(1, std::memory_order_relaxed);
    service->_pcmBytes.fetch_add(len, std::memory_order_relaxed);
    if (containsNonzeroSample(data, len))
        service->_pcmNonzeroCallbacks.fetch_add(1, std::memory_order_relaxed);
}

void BluetoothService::onSampleRate(uint16_t rate) {
    auto* service = _instance;
    if (service && service->_acceptCallbacks.load(std::memory_order_relaxed)) {
        service->_outputGate.pauseWrites();
        service->_pendingSampleRate.store(rate, std::memory_order_relaxed);
    }
}

void BluetoothService::onAvrcPlayback(esp_avrc_playback_stat_t status) {
    auto* service = _instance;
    if (service && service->_acceptCallbacks.load(std::memory_order_relaxed))
        service->_avrcPlayback.store(static_cast<int>(status), std::memory_order_relaxed);
}

uint8_t BluetoothService::mapVolume(int volume0to100) {
    const int v = constrain(volume0to100, 0, 100);
    return static_cast<uint8_t>((v * 127 + 50) / 100);
}

int BluetoothService::mapRemoteVolume(int volume0to127) {
    const int v = constrain(volume0to127, 0, 127);
    return (v * 100 + 63) / 127;
}

void BluetoothService::onRemoteVolume(int volume) {
    auto* service = _instance;
    if (!service) return;
    bool restoreLocal = false;
    portENTER_CRITICAL(&service->_peerMux);
    if (service->_acceptCallbacks.load()) {
        const uint8_t raw = constrain(volume, 0, 127);
        const uint32_t now = millis();
        bool localEcho = false;
        for (uint8_t i = 0; i < service->_recentLocalCount; ++i) {
            const auto& mark = service->_recentLocalVolume[i];
            if (mark.raw == raw && now - mark.atMs <= 1500) {
                localEcho = true;
                break;
            }
        }
        if (!service->_a2dpConnected || !service->_initialVolumeSynced) {
            // The library has already applied the phone's value to PCM.
            restoreLocal = true;
        } else if (!localEcho) {
            service->_remoteVolume127 = raw;
            service->_remoteVolumePending = true;
        }
    }
    portEXIT_CRITICAL(&service->_peerMux);
    if (restoreLocal)
        service->_sink.setLocalAttenuation(
            mapVolume(service->_volume.load(std::memory_order_relaxed)));
}
void BluetoothService::onVolumeNotificationReady(int) {
    auto* service = _instance;
    if (!service) return;
    portENTER_CRITICAL(&service->_peerMux);
    if (service->_acceptCallbacks.load() &&
        service->_sink.get_connection_state() != ESP_A2D_CONNECTION_STATE_DISCONNECTED)
        service->_volumeNotifyReady = true;
    portEXIT_CRITICAL(&service->_peerMux);
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
    if (!service || !service->_acceptCallbacks.load()) return;
    if (state != ESP_A2D_CONNECTION_STATE_CONNECTED) {
        service->_avrcPlayback.store(-1);
        portENTER_CRITICAL(&service->_peerMux);
        if (service->_a2dpConnected) ++service->_volumeSession;
        service->_a2dpConnected = false;
        service->_volumeNotifyReady = false;
        service->_initialVolumeSyncPending = false;
        service->_initialVolumeSynced = false;
        service->_remoteVolumePending = false;
        service->_recentLocalCount = 0;
        portEXIT_CRITICAL(&service->_peerMux);
        return;
    }
    service->_avrcPlayback.store(-1);
    // The library invokes this after updating peer_bd_addr.
    portENTER_CRITICAL(&service->_peerMux);
    if (!service->_acceptCallbacks.load()) {
        portEXIT_CRITICAL(&service->_peerMux);
        return;
    }
    if (!service->_a2dpConnected) {
        service->_a2dpConnected = true;
        // Keep a notification registered during A2DP CONNECTING.
        service->_initialVolumeSyncPending = true;
        service->_initialVolumeSynced = false;
        service->_remoteVolumePending = false;
        service->_recentLocalCount = 0;
        ++service->_volumeSession;
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
    _outputStream = stream;
    _configuredSampleRate = AppConfig::AUDIO_SAMPLE_RATE;
    _pendingSampleRate.store(0);
    _avrcPlayback.store(-1);
    _lastAvrcPlayback = -1;
    _outputGate.open(*stream);
    _outputGate.resumeWrites();
    _acceptCallbacks.store(true);
    // start must not auto-connect on resume: policy belongs to VoxOne.
    _sink.set_auto_reconnect(false);
    _sink.set_output(_outputGate);
    _sink.set_on_connection_state_changed(&BluetoothService::onConnectionState, this);
    // Audio state continues to be polled, as in 0.4.0.
    _sink.set_on_audio_state_changed(nullptr);
    _sink.set_on_audio_state_changed_post(nullptr);
#ifdef VOXONE_DEBUG
    _sink.set_raw_stream_reader(&BluetoothService::onRawPcm);
    _sink.set_stream_reader(&BluetoothService::onPcm, true);
#endif
    _sink.set_sample_rate_callback(&BluetoothService::onSampleRate);
    _sink.set_avrc_rn_playstatus_callback(&BluetoothService::onAvrcPlayback);
    // Initial sink uses the library's GENERAL_DISCOVERABLE default. On resume
    // the BT stack is already enabled and discovery can safely be restored.
    if (resume) _sink.set_discoverability(ESP_BT_GENERAL_DISCOVERABLE);
    _sink.set_avrc_metadata_attribute_mask(
        ESP_AVRC_MD_ATTR_TITLE | ESP_AVRC_MD_ATTR_ARTIST
    );
    _sink.set_avrc_metadata_callback(&BluetoothService::onMetadata);
    _sink.set_avrc_rn_volumechange(&BluetoothService::onRemoteVolume);
    // Called after the phone registers AVRCP volume notifications.
    _sink.set_avrc_rn_volumechange_completed(
        &BluetoothService::onVolumeNotificationReady);
    _sink.set_peer_name_callback(&BluetoothService::onPeerName);
    _sink.resetVolumeNotification();
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
    portENTER_CRITICAL(&_peerMux);
    _a2dpConnected = false;
    _volumeNotifyReady = false;
    _initialVolumeSyncPending = false;
    _initialVolumeSynced = false;
    ++_volumeSession;
    _recentLocalCount = 0;
    portEXIT_CRITICAL(&_peerMux);
    _pendingSampleRate.store(0);
    _avrcPlayback.store(-1);
    _outputStream = nullptr;
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

void BluetoothService::applySampleRate() {
    const uint16_t rate = _pendingSampleRate.exchange(0);
    if (rate == 0) return;
    if (rate == _configuredSampleRate) {
        _outputGate.resumeWrites();
        Logger::info("BT", "Negotiated PCM rate=" + String(rate) + " Hz");
        return;
    }
    if (!_outputStream || !_outputGate.closeAndDrain(2000)) {
        Logger::error("BT", "PCM rate change blocked: output did not drain");
        return;
    }
    if (!_outputManager->configureStereo16(AudioOutputOwner::Bluetooth, rate)) {
        Logger::error("BT", "PCM rate change failed; output remains closed");
        return;
    }
    _configuredSampleRate = rate;
    _outputGate.open(*_outputStream);
    _outputGate.resumeWrites();
    Logger::info("BT", "PCM rate applied=" + String(rate) + " Hz stereo 16-bit");
}

void BluetoothService::logAudioDiagnostics(uint32_t now) {
#ifdef VOXONE_DEBUG
    if (now - _lastDiagnosticsMs < 5000) return;
    _lastDiagnosticsMs = now;
    if (_sink.get_connection_state() != ESP_A2D_CONNECTION_STATE_CONNECTED) return;
    Logger::debug(
        "BT",
        "PCM raw_callbacks=" + String(_rawPcmCallbacks.load()) +
        " raw_bytes=" + String(_rawPcmBytes.load()) +
        " raw_nonzero=" + String(_rawNonzeroCallbacks.load()) +
        " callbacks=" + String(_pcmCallbacks.load()) +
        " bytes=" + String(_pcmBytes.load()) +
        " nonzero=" + String(_pcmNonzeroCallbacks.load()) +
        " rate=" + String(_configuredSampleRate) +
        " channels=" + String(_sink.channels()) +
        " volume=" + String(StateStore::instance().snapshot().volume) +
        " avrc_volume=" + String(_sink.get_volume())
    );
    Logger::debug(
        "AUDIO",
        "I2S writes=" + String(_outputGate.writes()) +
        " bytes=" + String(_outputGate.writtenBytes()) +
        " errors=" + String(_outputGate.writeErrors()) +
        " dropped=" + String(_outputGate.droppedBytes())
    );
#else
    (void)now;
#endif
}

void BluetoothService::setVolume(int volume0to100) {
    _volume = constrain(volume0to100, 0, 100);
    if (!_started) return;
    portENTER_CRITICAL(&_peerMux);
    const bool ready = _a2dpConnected && _initialVolumeSynced;
    portEXIT_CRITICAL(&_peerMux);
    if (!ready) {
        // Keep PCM attenuation responsive without notifying the phone yet.
        _sink.setLocalAttenuation(mapVolume(_volume));
        return;
    }
    const uint8_t raw = mapVolume(_volume);
    portENTER_CRITICAL(&_peerMux);
    _recentLocalVolume[_recentLocalNext] = {raw, millis()};
    _recentLocalNext = (_recentLocalNext + 1) % 16;
    if (_recentLocalCount < 16) ++_recentLocalCount;
    portEXIT_CRITICAL(&_peerMux);
    _sink.set_volume(raw);
}

void BluetoothService::syncInitialVolume() {
    uint32_t session = 0;
    portENTER_CRITICAL(&_peerMux);
    const bool ready = _a2dpConnected && _volumeNotifyReady &&
        _initialVolumeSyncPending && !_initialVolumeSynced;
    if (ready) {
        session = _volumeSession;
        _initialVolumeSyncPending = false;
    }
    portEXIT_CRITICAL(&_peerMux);
    if (!ready) return;

    const int local = constrain(StateStore::instance().snapshot().volume, 0, 100);
    const uint8_t raw = mapVolume(local);
    portENTER_CRITICAL(&_peerMux);
    if (!_a2dpConnected || session != _volumeSession) {
        portEXIT_CRITICAL(&_peerMux);
        return;
    }
    _volume = local;
    _recentLocalVolume[_recentLocalNext] = {raw, millis()};
    _recentLocalNext = (_recentLocalNext + 1) % 16;
    if (_recentLocalCount < 16) ++_recentLocalCount;
    portEXIT_CRITICAL(&_peerMux);

    // The remote callback stays gated until this write has completed.
    _sink.set_volume(raw);

    portENTER_CRITICAL(&_peerMux);
    const bool synced = _a2dpConnected && session == _volumeSession;
    if (synced) _initialVolumeSynced = true;
    portEXIT_CRITICAL(&_peerMux);
    if (synced)
        Logger::info("BT", "Initial volume sync: VoxOne=" +
            String(local) + " AVRCP=" + String(raw));
}

void BluetoothService::play() {
    if (!_started) return;
    if (!_sink.is_avrc_connected()) {
        Logger::warn("BT", "AVRCP PLAY unavailable: controller not connected");
        return;
    }
    Logger::info("BT", "AVRCP PLAY requested");
    _sink.play();
}

void BluetoothService::pause() {
    if (!_started) return;
    if (!_sink.is_avrc_connected()) {
        Logger::warn("BT", "AVRCP PAUSE unavailable: controller not connected");
        return;
    }
    Logger::info("BT", "AVRCP PAUSE requested");
    _sink.pause();
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
    volumePending = _remoteVolumePending && _a2dpConnected && _initialVolumeSynced;
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

    const int avrcStatus = _avrcPlayback.load();
    const bool playing =
        connected &&
        audioState == ESP_A2D_AUDIO_STATE_STARTED &&
        (avrcStatus < 0 || avrcStatus == ESP_AVRC_PLAYBACK_PLAYING);

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
}

void BluetoothService::loop() {
    if (!_started) return;

    flushPendingEvents();
    syncInitialVolume();
    applySampleRate();

    const uint32_t now = millis();
    logAudioDiagnostics(now);

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
    const int avrcStatus = _avrcPlayback.load();

    if (connectionState == _lastConnectionState &&
        audioState == _lastAudioState &&
        avrcStatus == _lastAvrcPlayback) return;

    _lastConnectionState = connectionState;
    _lastAudioState = audioState;
    _lastAvrcPlayback = avrcStatus;

    publishState(connectionState, audioState);
    const bool playing = connectionState == ESP_A2D_CONNECTION_STATE_CONNECTED &&
        audioState == ESP_A2D_AUDIO_STATE_STARTED &&
        (avrcStatus < 0 || avrcStatus == ESP_AVRC_PLAYBACK_PLAYING);
    Logger::info(
        "BT",
        String("connection=") + _sink.to_str(connectionState) +
        " audio=" + _sink.to_str(audioState) +
        " playback=" + (playing ? "PLAY" : "PAUSE") +
        " avrc=" + String(avrcStatus)
    );
}

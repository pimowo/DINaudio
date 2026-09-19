#include "App.h"

#include "AppConfig.h"
#include "BoardConfig.h"
#include "StateStore.h"
#include "CommandQueue.h"
#include "../diagnostics/Logger.h"
#include <cstring>

#ifndef VOXONE_RADIO_TEST_CONTROLS
#define VOXONE_RADIO_TEST_CONTROLS 1
#endif
#ifndef VOXONE_RADIO_TEST_URL
// Verified HTTP 200 audio/mpeg, 128 kb/s, 44.1 kHz stereo; ICY disabled.
#define VOXONE_RADIO_TEST_URL "http://ice1.somafm.com/groovesalad-128-mp3"
#endif

String App::makeBluetoothName() const {
    const uint32_t suffix =
        static_cast<uint32_t>(
            ESP.getEfuseMac() & 0xFFFFFFULL
        );

    char mac6[7];
    snprintf(
        mac6,
        sizeof(mac6),
        "%06X",
        suffix
    );

    return String(AppConfig::DEVICE_PREFIX) +
        "-" + mac6;
}

bool App::begin() {
    Logger::begin(AppConfig::SERIAL_BAUD);

    Logger::info(
        "BOOT",
        String("VoxOne ") +
        AppConfig::FW_VERSION
    );

    Logger::info(
        "BOOT",
        String("Board: ") +
        Board::PROFILE_NAME
    );

    if (!CommandQueue::instance().begin()) {
        Logger::error(
            "CORE",
            "CommandQueue init failed"
        );
        return false;
    }

    if (!_config.begin()) {
        Logger::error(
            "CONFIG",
            "ConfigManager init failed"
        );
        return false;
    }

    Logger::info(
        "CONFIG",
        "Schema v" +
        String(_config.schemaVersion())
    );

    Logger::info(
        "BT",
        String("Auto reconnect=") +
        (_config.bluetoothAutoReconnect()
            ? "ON"
            : "OFF") +
        " grace=" +
        String(
            _config.bluetoothReconnectDelayMs()
        ) +
        "ms"
    );

    auto s =
        StateStore::instance().snapshot();

    s.volume = _config.volume();
    s.playback = PlaybackState::Stop;
    s.audioSource = AudioSource::Stop;
    s.bluetoothOwnership =
        BluetoothOwnershipState::Disconnected;
    s.bluetoothReconnectGrace = false;

    StateStore::instance().update(s);

    if (Board::HAS_ENCODER) {
        _encoder.begin();
    }

    if (Board::HAS_DISPLAY) {
        _display.begin();
    }

    if (_audioOutput.begin() &&
        _audioOutput.acquire(AudioOutputOwner::Bluetooth)) {
        const String btName =
            makeBluetoothName();

        if (!_bluetooth.begin(
                _audioOutput,
                btName,
                s.volume
            )) {
            Logger::warn(
                "BT",
                "Bluetooth unavailable"
            );
            _audioOutput.release(AudioOutputOwner::Bluetooth);
        }
    } else {
        Logger::warn(
            "AUDIO",
            "Bluetooth skipped because "
            "I2S is unavailable"
        );
    }

    _wifi.begin(_config);
    _radio.begin(_audioOutput);
    _radio.setVolume(s.volume);
#if VOXONE_RADIO_TEST_CONTROLS
    Logger::info("RADIO", "Serial test commands: radio start / radio stop");
#endif
    _time.begin();
    _web.begin(_config, _wifi);

    Logger::info(
        "BOOT",
        "VoxOne ready"
    );

    return true;
}

bool App::startRadio(const char* url) {
    if (_radioSession || !url || strncmp(url, "http://", 7) != 0 ||
        !StateStore::instance().snapshot().wifiConnected) return false;
    if (!_bluetooth.suspendForSourceSwitch()) {
        Logger::error("RADIO", "BT suspend failed; radio not started");
        return false;
    }
    _radioSession = true;
    _btWasConnected = false;
    _btReconnectGraceActive = false;
    _btReconnectGraceUntil = 0;
    _sourceBeforeBluetooth = AudioSource::Stop;
    auto s = StateStore::instance().snapshot();
    s.bluetoothOwnership = BluetoothOwnershipState::Disconnected;
    s.bluetoothReconnectGrace = false;
    s.audioSource = AudioSource::Stop;
    s.playback = PlaybackState::Stop;
    s.lastMessage = "RADIO_STARTING";
    StateStore::instance().update(s);
    _radio.setVolume(s.volume);
    if (!_radio.start(url)) {
        stopRadio();
        return false;
    }
    s = StateStore::instance().snapshot();
    s.audioSource = AudioSource::Radio;
    s.playback = PlaybackState::Playing;
    s.lastMessage = "RADIO_RUNNING";
    StateStore::instance().update(s);
    return true;
}

void App::stopRadio() {
    if (!_radioSession) return;
    _radio.stop();
    auto s = StateStore::instance().snapshot();
    s.audioSource = AudioSource::Stop;
    s.playback = PlaybackState::Stop;
    s.lastMessage = _radio.lastError() ? _radio.lastError() : "RADIO_STOPPED";
    StateStore::instance().update(s);
    // A failed release must never be bypassed by resuming A2DP.
    if (_audioOutput.owner() != AudioOutputOwner::None) {
        Logger::error("RADIO", "Output lease retained; BT resume blocked");
        return;
    }
    _radioSession = false;
    if (!_bluetooth.resumeAfterSourceSwitch()) {
        Logger::error("RADIO", "BT resume failed; output remains unavailable");
        return;
    }
    if (_config.bluetoothAutoReconnect()) _bluetooth.reconnect();
}

void App::processRadioTestCommands() {
#if VOXONE_RADIO_TEST_CONTROLS
    // Bounded, newline-terminated serial commands; no automatic radio start.
    for (int budget = 0; budget < 24 && Serial.available(); ++budget) {
        const char character = static_cast<char>(Serial.read());
        if (character == '\r') continue;
        if (character == '\n') {
            _radioCommand[_radioCommandLength] = '\0';
            if (!_radioCommandOverflow) {
                if (strcmp(_radioCommand, "radio start") == 0) {
                    if (!startRadio(VOXONE_RADIO_TEST_URL))
                        Logger::warn("RADIO", "Serial start rejected or failed");
                } else if (strcmp(_radioCommand, "radio stop") == 0) {
                    stopRadio();
                }
            }
            _radioCommandLength = 0;
            _radioCommandOverflow = false;
        } else if (_radioCommandLength < sizeof(_radioCommand) - 1) {
            _radioCommand[_radioCommandLength++] = character;
        } else {
            _radioCommandOverflow = true;
        }
    }
#endif
}

void App::enterBluetoothReconnectGrace() {
    const uint32_t graceMs =
        _config.bluetoothReconnectDelayMs();

    auto s =
        StateStore::instance().snapshot();

    _btReconnectGraceActive = true;
    _btReconnectGraceUntil =
        millis() + graceMs;

    s.audioSource =
        AudioSource::Bluetooth;
    s.playback =
        PlaybackState::Stop;
    s.bluetoothOwnership =
        BluetoothOwnershipState::ReconnectGrace;
    s.bluetoothReconnectGrace = true;
    s.lastMessage = "BT_RECONNECT_GRACE";

    StateStore::instance().update(s);

    Logger::info(
        "BT",
        "Reconnect grace started: " +
        String(graceMs) +
        " ms"
    );

    _bluetooth.reconnect();
}

void App::finishBluetoothReconnectGrace() {
    _btReconnectGraceActive = false;

    auto s =
        StateStore::instance().snapshot();

    s.bluetoothReconnectGrace = false;
    s.bluetoothOwnership =
        BluetoothOwnershipState::Disconnected;
    s.audioSource =
        _sourceBeforeBluetooth;
    s.playback =
        PlaybackState::Stop;

    // Metadata is no longer relevant after the grace
    // window expires. Keep it during the grace period
    // so a fast reconnect can continue cleanly.
    s.bluetoothPeerName = "";
    s.bluetoothTitle = "";
    s.bluetoothArtist = "";
    s.lastMessage = "BT_FALLBACK";

    StateStore::instance().update(s);

    Logger::info(
        "BT",
        "Reconnect grace expired; "
        "restored previous source"
    );
}

void App::updateBluetoothOwnership() {
    // An intentional source switch must not start transport reconnect grace.
    if (_bluetooth.consumeSourceSwitchEvent() ||
        _bluetooth.sourceSwitchSuspended()) {
        _btWasConnected = false;
        _btReconnectGraceActive = false;
        _btReconnectGraceUntil = 0;
        auto s = StateStore::instance().snapshot();
        if (s.bluetoothReconnectGrace ||
            s.bluetoothOwnership != BluetoothOwnershipState::Disconnected) {
            s.bluetoothReconnectGrace = false;
            s.bluetoothOwnership = BluetoothOwnershipState::Disconnected;
            StateStore::instance().update(s);
        }
        if (_bluetooth.sourceSwitchSuspended()) return;
    }
    auto s =
        StateStore::instance().snapshot();

    const bool connected =
        s.bluetoothConnected;

    if (connected) {
        if (!_btWasConnected) {
            if (!_btReconnectGraceActive) {
                _sourceBeforeBluetooth =
                    s.audioSource ==
                    AudioSource::Bluetooth
                        ? AudioSource::Stop
                        : s.audioSource;
            }

            Logger::info(
                "BT",
                _btReconnectGraceActive
                    ? "Fast reconnect succeeded"
                    : "Bluetooth took source ownership"
            );
        }

        _btWasConnected = true;
        _btReconnectGraceActive = false;

        s.bluetoothReconnectGrace = false;
        s.audioSource =
            AudioSource::Bluetooth;

        if (s.bluetoothPlaying) {
            s.playback =
                PlaybackState::Playing;
            s.bluetoothOwnership =
                BluetoothOwnershipState::Playing;
            s.lastMessage = "BT_PLAYING";
        } else {
            s.playback =
                PlaybackState::Stop;
            s.bluetoothOwnership =
                BluetoothOwnershipState::ConnectedIdle;
            s.lastMessage = "BT_CONNECTED";
        }

        StateStore::instance().update(s);
        return;
    }

    if (_btWasConnected) {
        _btWasConnected = false;

        if (
            _config.bluetoothAutoReconnect() &&
            _config.bluetoothReconnectDelayMs() > 0
        ) {
            enterBluetoothReconnectGrace();
        } else {
            _btReconnectGraceActive = true;
            finishBluetoothReconnectGrace();
        }

        return;
    }

    if (_btReconnectGraceActive) {
        if (
            static_cast<int32_t>(
                millis() -
                _btReconnectGraceUntil
            ) >= 0
        ) {
            finishBluetoothReconnectGrace();
        }

        return;
    }

    if (
        s.bluetoothOwnership !=
            BluetoothOwnershipState::Disconnected ||
        s.bluetoothReconnectGrace
    ) {
        s.bluetoothOwnership =
            BluetoothOwnershipState::Disconnected;
        s.bluetoothReconnectGrace = false;
        StateStore::instance().update(s);
    }
}

void App::processCommands() {
    Command cmd;

    while (
        CommandQueue::instance().pop(cmd)
    ) {
        auto s =
            StateStore::instance().snapshot();

        switch (cmd.type) {
            case CommandType::VolumeDelta:
                s.volume = constrain(
                    s.volume + cmd.value,
                    0,
                    _config.maxVolume()
                );

                _bluetooth.setVolume(
                    s.volume
                );

                _volumeDirty = true;
                _volumeSaveDue =
                    millis() + 1500;
                _radio.setVolume(s.volume);
                break;

            case CommandType::SetVolumeAbsolute:
                s.volume = constrain(
                    cmd.value,
                    0,
                    _config.maxVolume()
                );

                if (
                    cmd.source !=
                    CommandSource::Bluetooth
                ) {
                    _bluetooth.setVolume(
                        s.volume
                    );
                }

                _volumeDirty = true;
                _volumeSaveDue =
                    millis() + 1500;
                _radio.setVolume(s.volume);
                break;

            case CommandType::TogglePlayStop:
                if (_radioSession) {
                    stopRadio();
                    continue; // Do not overwrite the fresh BT transport state.
                }
                if (s.bluetoothConnected) {
                    if (s.bluetoothPlaying) {
                        _bluetooth.pause();
                    } else {
                        _bluetooth.play();
                    }
                }
                break;

            case CommandType::SetPlay:
                _bluetooth.play();
                break;

            case CommandType::Pause:
                _bluetooth.pause();
                break;

            case CommandType::SetStop:
                if (_radioSession) {
                    stopRadio();
                    continue;
                }
                _bluetooth.stop();
                break;

            case CommandType::Next:
                _bluetooth.next();
                break;

            case CommandType::Previous:
                _bluetooth.previous();
                break;
        }

        StateStore::instance().update(s);
    }

    if (
        _volumeDirty &&
        static_cast<int32_t>(
            millis() -
            _volumeSaveDue
        ) >= 0
    ) {
        _volumeDirty = false;

        _config.saveVolume(
            StateStore::instance()
                .snapshot()
                .volume
        );

        Logger::debug(
            "CONFIG",
            "Volume saved"
        );
    }
}

void App::loop() {
    processRadioTestCommands();
    _radio.loop();
    if (_radioSession && !_radio.isRunning() &&
        _audioOutput.owner() == AudioOutputOwner::None) stopRadio();
    if (Board::HAS_ENCODER) {
        _encoder.loop();
    }

    _bluetooth.loop();
    updateBluetoothOwnership();
    processCommands();

    _wifi.loop();
    _time.loop();
    _web.loop();

    if (Board::HAS_DISPLAY) {
        _display.loop();
    }

    delay(0);
}

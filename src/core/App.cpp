#include "App.h"

#include "AppConfig.h"
#include "BoardConfig.h"
#include "StateStore.h"
#include "CommandQueue.h"
#include "../diagnostics/Logger.h"
#include <cstring>
#include <new>

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

    const auto& runtime = _config.config();
    const auto& features = runtime.features;
    const auto defaultSource = _config.effectiveDefaultSource();
    Logger::info("BOOT", "VoxOne features:");
    Logger::info("BOOT", String("  Bluetooth: ") + (features.bluetoothEnabled ? "ON" : "OFF"));
    Logger::info("BOOT", String("  Radio: ") + (features.radioEnabled ? "ON" : "OFF"));
    Logger::info("BOOT", String("  Display: ") + (features.displayEnabled ? "ON" : "OFF"));
    Logger::info("BOOT", String("  Encoder: ") + (features.encoderEnabled ? "ON" : "OFF"));
    Logger::info("BOOT", String("  PlayMedia: configured ") +
        (features.playMediaEnabled ? "ON" : "OFF") + " (runtime pending)");
    Logger::info("BOOT", String("  Buttons: configured ") +
        (features.buttonsEnabled ? "ON" : "OFF") + " (runtime pending)");
    Logger::info("BOOT", String("  MQTT: configured ") +
        (features.mqttEnabled ? "ON" : "OFF") + " (runtime pending)");
    Logger::info("BOOT", String("  yoRadio WS: configured ") +
        (features.yoRadioWsEnabled ? "ON" : "OFF") + " (runtime pending)");
    Logger::info("BOOT", String("  HA Discovery: configured ") +
        (features.haDiscoveryEnabled ? "ON" : "OFF") + " (runtime pending)");
    const char* sourceName = defaultSource == DefaultSource::Bluetooth ? "BT" :
        defaultSource == DefaultSource::Radio ? "RADIO" : "STOP";
    Logger::info("BOOT", String("  Default source: ") + sourceName);
    if (defaultSource != runtime.audio.defaultSource)
        Logger::warn("BOOT", "Configured default source disabled; using STOP");
    Logger::info("BOOT", String("  Audio output: ") +
        (runtime.audio.outputType == OutputType::PCM5102A ? "PCM5102A" : "MAX98357A"));
    Logger::info("BOOT", String("  Display type: ") +
        (runtime.display.type == DisplayType::ST7789 ? "ST7789" : "SSD1306"));

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

    if (features.encoderEnabled && Board::HAS_ENCODER) {
        _encoder.begin(runtime.encoder);
    }
    if (features.displayEnabled && Board::HAS_DISPLAY) {
        if (runtime.display.type == DisplayType::ST7789) {
            _display.reset(new (std::nothrow) DisplayService(runtime.display.st7789));
            if (_display) _display->begin();
            else Logger::error("DISPLAY", "ST7789 allocation failed");
        } else {
            Logger::warn("DISPLAY", "SSD1306 not runtime implemented; display skipped");
        }
    }

    if (!_audioOutput.begin(runtime.audio.i2sBclk, runtime.audio.i2sLrclk, runtime.audio.i2sDout)) {
        Logger::error("AUDIO", "AudioOutputManager init failed");
        return false;
    }
    if (runtime.audio.outputType == OutputType::MAX98357A)
        Logger::warn("AUDIO", "MAX98357A not runtime implemented; audio sources skipped");
    if (features.bluetoothEnabled && defaultSource == DefaultSource::Bluetooth &&
        runtime.audio.outputType == OutputType::PCM5102A) {
        if (_audioOutput.acquire(AudioOutputOwner::Bluetooth)) {
            _bluetoothAvailable = _bluetooth.begin(
                _audioOutput, makeBluetoothName(), s.volume
            );
            if (!_bluetoothAvailable) {
                Logger::warn("BT", "Bluetooth unavailable");
                _audioOutput.release(AudioOutputOwner::Bluetooth);
            }
        } else {
            Logger::warn("BT", "Bluetooth skipped because I2S is unavailable");
        }
    }

    if (features.bluetoothEnabled && defaultSource != DefaultSource::Bluetooth)
        Logger::info("BT", "Enabled but not selected; A2DP startup deferred");
    _wifi.begin(_config);
    if (features.radioEnabled) {
        if (defaultSource == DefaultSource::Radio || runtime.radio.autostart)
            Logger::warn("RADIO", "Startup station selection pending; no stream started");
        _radioAvailable = _radio.begin(_audioOutput);
        if (_radioAvailable) _radio.setVolume(s.volume);
        else Logger::warn("RADIO", "Radio unavailable");
    }
#if VOXONE_RADIO_TEST_CONTROLS
    Logger::info("RADIO", "Serial test commands: radio start / radio stop");
#endif
    _time.begin();
    _web.begin(_config, _wifi, _audioOutput, _display.get());

    Logger::info(
        "BOOT",
        "VoxOne ready"
    );

    return true;
}

bool App::startRadio(const char* url) {
    if (_config.config().audio.outputType != OutputType::PCM5102A ||
        !_radioAvailable || _radioSession || !url ||
        strncmp(url, "http://", 7) != 0 ||
        !StateStore::instance().snapshot().wifiConnected) return false;
    if (_bluetoothAvailable) {
        if (!_bluetooth.suspendForSourceSwitch()) {
            Logger::error("RADIO", "BT suspend failed; radio not started");
            return false;
        }
        _btSuspendedForRadio = true;
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
    if (_btSuspendedForRadio) {
        _btSuspendedForRadio = false;
        if (!_bluetooth.resumeAfterSourceSwitch()) {
            Logger::error("RADIO", "BT resume failed; output remains unavailable");
            return;
        }
        if (_config.bluetoothAutoReconnect()) _bluetooth.reconnect();
    }
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
                    if (_radioAvailable) stopRadio();
                    else Logger::warn("RADIO", "Serial stop ignored: radio disabled");
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
        if (cmd.source == CommandSource::Bluetooth &&
            !_bluetoothAvailable) {
            Logger::warn("BT", "Command ignored: Bluetooth disabled");
            continue;
        }
        if (cmd.source == CommandSource::Encoder &&
            !(_config.features().encoderEnabled && Board::HAS_ENCODER)) {
            Logger::warn("ENCODER", "Command ignored: encoder disabled");
            continue;
        }

        auto s =
            StateStore::instance().snapshot();

        switch (cmd.type) {
            case CommandType::VolumeDelta:
                s.volume = constrain(
                    s.volume + cmd.value,
                    0,
                    _config.maxVolume()
                );

                if (_bluetoothAvailable) {
                    _bluetooth.setVolume(s.volume);
                }

                _volumeDirty = true;
                _volumeSaveDue =
                    millis() + 1500;
                if (_radioAvailable) _radio.setVolume(s.volume);
                break;

            case CommandType::SetVolumeAbsolute:
                s.volume = constrain(
                    cmd.value,
                    0,
                    _config.maxVolume()
                );

                if (_bluetoothAvailable &&
                    cmd.source != CommandSource::Bluetooth) {
                    _bluetooth.setVolume(s.volume);
                }

                _volumeDirty = true;
                _volumeSaveDue =
                    millis() + 1500;
                if (_radioAvailable) _radio.setVolume(s.volume);
                break;

            case CommandType::TogglePlayStop:
                if (_radioSession) {
                    stopRadio();
                    continue; // Do not overwrite the fresh BT transport state.
                }
                if (_bluetoothAvailable && s.bluetoothConnected) {
                    if (s.bluetoothPlaying) {
                        _bluetooth.pause();
                    } else {
                        _bluetooth.play();
                    }
                }
                break;

            case CommandType::SetPlay:
                if (_bluetoothAvailable) _bluetooth.play();
                else Logger::warn("BT", "Play ignored: Bluetooth disabled");
                break;

            case CommandType::Pause:
                if (_bluetoothAvailable) _bluetooth.pause();
                else Logger::warn("BT", "Pause ignored: Bluetooth disabled");
                break;

            case CommandType::SetStop:
                if (_radioSession) {
                    stopRadio();
                    continue;
                }
                if (_bluetoothAvailable) _bluetooth.stop();
                else Logger::warn("BT", "Stop ignored: Bluetooth disabled");
                break;

            case CommandType::Next:
                if (_bluetoothAvailable) _bluetooth.next();
                else Logger::warn("BT", "Next ignored: Bluetooth disabled");
                break;

            case CommandType::Previous:
                if (_bluetoothAvailable) _bluetooth.previous();
                else Logger::warn("BT", "Previous ignored: Bluetooth disabled");
                break;
        }

        StateStore::instance().update(s);
    }

    if (
        _volumeDirty &&
        !_web.restartPending() &&
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
    if (_radioAvailable) {
        _radio.loop();
        if (_radioSession && !_radio.isRunning() &&
            _audioOutput.owner() == AudioOutputOwner::None) stopRadio();
    }
    if (_config.features().encoderEnabled && Board::HAS_ENCODER) {
        _encoder.loop();
    }

    if (_bluetoothAvailable) {
        _bluetooth.loop();
        updateBluetoothOwnership();
    }
    processCommands();

    _wifi.loop();
    _time.loop();
    _web.loop();

    if (_display) {
        _display->loop();
    }

    delay(0);
}

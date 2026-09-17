#include "App.h"

#include "AppConfig.h"
#include "BoardConfig.h"
#include "StateStore.h"
#include "CommandQueue.h"
#include "../diagnostics/Logger.h"

String App::makeBluetoothName() const {
    const uint32_t suffix =
        static_cast<uint32_t>(ESP.getEfuseMac() & 0xFFFFFFULL);

    char mac6[7];
    snprintf(mac6, sizeof(mac6), "%06X", suffix);

    return String(AppConfig::DEVICE_PREFIX) + "-" + mac6;
}

bool App::begin() {
    Logger::begin(AppConfig::SERIAL_BAUD);

    Logger::info(
        "BOOT",
        String("DINaudio ") + AppConfig::FW_VERSION
    );
    Logger::info("BOOT", String("Board: ") + Board::PROFILE_NAME);

    if (!CommandQueue::instance().begin()) {
        Logger::error("CORE", "CommandQueue init failed");
        return false;
    }

    if (!_config.begin()) {
        Logger::error("CONFIG", "ConfigManager init failed");
        return false;
    }

    Logger::info(
        "CONFIG",
        "Schema v" + String(_config.schemaVersion())
    );

    auto s = StateStore::instance().snapshot();
    s.volume = _config.volume();
    s.playback = PlaybackState::Stop;
    s.audioSource = AudioSource::Stop;
    StateStore::instance().update(s);

    if (Board::HAS_ENCODER) _encoder.begin();
    if (Board::HAS_DISPLAY) _display.begin();

    if (_audioOutput.begin()) {
        const String btName = makeBluetoothName();

        if (!_bluetooth.begin(
                _audioOutput,
                btName,
                s.volume
            )) {
            Logger::warn("BT", "Bluetooth unavailable");
        }
    } else {
        Logger::warn(
            "AUDIO",
            "Bluetooth skipped because I2S is unavailable"
        );
    }

    _wifi.begin(_config);
    _time.begin();
    _web.begin(_config, _wifi);

    Logger::info("BOOT", "DINaudio ready");
    return true;
}

void App::processCommands() {
    Command cmd;

    while (CommandQueue::instance().pop(cmd)) {
        auto s = StateStore::instance().snapshot();

        switch (cmd.type) {
            case CommandType::VolumeDelta:
                s.volume = constrain(
                    s.volume + cmd.value,
                    0,
                    _config.maxVolume()
                );

                _bluetooth.setVolume(s.volume);
                _volumeDirty = true;
                _volumeSaveDue = millis() + 1500;
                break;

            case CommandType::SetVolumeAbsolute:
                s.volume = constrain(
                    cmd.value,
                    0,
                    _config.maxVolume()
                );

                if (cmd.source != CommandSource::Bluetooth) {
                    _bluetooth.setVolume(s.volume);
                }

                _volumeDirty = true;
                _volumeSaveDue = millis() + 1500;
                break;

            case CommandType::TogglePlayStop:
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

        // DisplayService observes StateStore by itself.
        // Do not force a complete dynamic redraw for every command.
    }

    if (_volumeDirty &&
        (int32_t)(millis() - _volumeSaveDue) >= 0) {

        _volumeDirty = false;

        _config.saveVolume(
            StateStore::instance().snapshot().volume
        );

        Logger::debug("CONFIG", "Volume saved");
    }
}

void App::loop() {
    if (Board::HAS_ENCODER) _encoder.loop();

    _bluetooth.loop();
    processCommands();

    _wifi.loop();
    _time.loop();
    _web.loop();

    if (Board::HAS_DISPLAY) _display.loop();

    delay(0);
}

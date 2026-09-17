#include "App.h"

#include "AppConfig.h"
#include "BoardConfig.h"
#include "StateStore.h"
#include "CommandQueue.h"
#include "../diagnostics/Logger.h"

String App::makeBluetoothName() const {
    const uint32_t suffix = static_cast<uint32_t>(ESP.getEfuseMac() & 0xFFFFFFULL);

    char mac6[7];
    snprintf(mac6, sizeof(mac6), "%06X", suffix);

    return String(AppConfig::DEVICE_PREFIX) + "-" + mac6;
}

bool App::begin() {
    Logger::begin(AppConfig::SERIAL_BAUD);

    Logger::info("BOOT", String("DINaudio ") + AppConfig::FW_VERSION);
    Logger::info("BOOT", String("Board: ") + Board::PROFILE_NAME);

    if (!CommandQueue::instance().begin()) {
        Logger::error("CORE", "CommandQueue init failed");
        return false;
    }

    if (!_settings.begin()) {
        Logger::error("STORE", "Preferences init failed");
        return false;
    }

    auto s = StateStore::instance().snapshot();
    s.volume = _settings.volume();
    s.playback = PlaybackState::Stop;
    s.audioSource = AudioSource::Stop;
    StateStore::instance().update(s);

    if (Board::HAS_ENCODER) _encoder.begin();
    if (Board::HAS_DISPLAY) _display.begin();

    // Docelowa kolejność DINaudio: audio/BT przed Wi-Fi.
    if (_audioOutput.begin()) {
        const String btName = makeBluetoothName();
        if (!_bluetooth.begin(_audioOutput, btName, s.volume)) {
            Logger::warn("BT", "Bluetooth unavailable");
        }
    } else {
        Logger::warn("AUDIO", "Bluetooth skipped because I2S is unavailable");
    }

    _wifi.begin(_settings);
    _web.begin(_settings, _wifi);

    Logger::info("BOOT", "M2.1 ready");
    return true;
}

void App::processCommands() {
    Command cmd;

    while (CommandQueue::instance().pop(cmd)) {
        auto s = StateStore::instance().snapshot();

        switch (cmd.type) {
            case CommandType::VolumeDelta:
                s.volume = constrain(s.volume + cmd.value, 0, 100);
                _bluetooth.setVolume(s.volume);

                _volumeDirty = true;
                _volumeSaveDue = millis() + 1500;
                break;

            case CommandType::TogglePlayStop:
                // M2.1 nie wprowadza jeszcze sterowania AVRCP.
                // Przycisk zostaje w architekturze, ale nie udaje PLAY/PAUSE.
                s.lastMessage = "AVRCP_PENDING";
                break;

            case CommandType::SetStop:
                s.lastMessage = "AVRCP_PENDING";
                break;

            case CommandType::SetPlay:
                s.lastMessage = "AVRCP_PENDING";
                break;
        }

        StateStore::instance().update(s);

        if (Board::HAS_DISPLAY) {
            _display.redraw();
        }
    }

    if (_volumeDirty && (int32_t)(millis() - _volumeSaveDue) >= 0) {
        _volumeDirty = false;
        _settings.saveVolume(StateStore::instance().snapshot().volume);
        Logger::debug("STORE", "Volume saved");
    }
}

void App::loop() {
    if (Board::HAS_ENCODER) {
        _encoder.loop();
    }

    processCommands();

    _bluetooth.loop();
    _wifi.loop();
    _web.loop();

    if (Board::HAS_DISPLAY) {
        _display.loop();
    }

    delay(0);
}

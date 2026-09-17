#include "App.h"
#include "AppConfig.h"
#include "BoardConfig.h"
#include "StateStore.h"
#include "CommandQueue.h"
#include "../diagnostics/Logger.h"

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
    StateStore::instance().update(s);

    if (Board::HAS_ENCODER) _encoder.begin();
    if (Board::HAS_DISPLAY) _display.begin();

    if (!_tone.begin()) {
        Logger::warn("AUDIO", "Tone unavailable");
    }
    _tone.setVolume(s.volume);
    _tone.setPlaying(false);

    _wifi.begin(_settings);
    _web.begin(_settings, _wifi);

    Logger::info("BOOT", "M1 ready");
    return true;
}

void App::processCommands() {
    Command cmd;
    while (CommandQueue::instance().pop(cmd)) {
        auto s = StateStore::instance().snapshot();

        switch (cmd.type) {
            case CommandType::VolumeDelta:
                s.volume = constrain(s.volume + cmd.value, 0, 100);
                _tone.setVolume(s.volume);
                _volumeDirty = true;
                _volumeSaveDue = millis() + 1500;
                break;

            case CommandType::TogglePlayStop:
                s.playback = (s.playback == PlaybackState::Playing)
                    ? PlaybackState::Stop
                    : PlaybackState::Playing;
                _tone.setPlaying(s.playback == PlaybackState::Playing);
                break;

            case CommandType::SetStop:
                s.playback = PlaybackState::Stop;
                _tone.setPlaying(false);
                break;

            case CommandType::SetPlay:
                s.playback = PlaybackState::Playing;
                _tone.setPlaying(true);
                break;
        }

        StateStore::instance().update(s);
        if (Board::HAS_DISPLAY) _display.redraw();
    }

    if (_volumeDirty && (int32_t)(millis() - _volumeSaveDue) >= 0) {
        _volumeDirty = false;
        _settings.saveVolume(StateStore::instance().snapshot().volume);
        Logger::debug("STORE", "Volume saved");
    }
}

void App::loop() {
    if (Board::HAS_ENCODER) _encoder.loop();

    processCommands();

    _tone.loop();
    _wifi.loop();
    _web.loop();

    if (Board::HAS_DISPLAY) _display.loop();

    delay(0); // yield FreeRTOS/Wi-Fi; nie jest blokującym opóźnieniem
}

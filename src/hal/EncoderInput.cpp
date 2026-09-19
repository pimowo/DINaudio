#include "EncoderInput.h"
#include "BoardConfig.h"
#include "../core/CommandQueue.h"

static const int8_t QUAD_TABLE[16] = {
     0, -1, +1,  0,
    +1,  0,  0, -1,
    -1,  0,  0, +1,
     0, +1, -1,  0
};

void EncoderInput::begin(const EncoderConfig& config) {
    _config = config;
    pinMode(_config.pinA, Board::ENC_INTERNAL_PULLUP ? INPUT_PULLUP : INPUT);
    pinMode(_config.pinB, Board::ENC_INTERNAL_PULLUP ? INPUT_PULLUP : INPUT);
    pinMode(_config.pinButton, Board::ENC_INTERNAL_PULLUP ? INPUT_PULLUP : INPUT);
}

void EncoderInput::loop() {
    const uint8_t a = digitalRead(_config.pinA) ? 1 : 0;
    const uint8_t b = digitalRead(_config.pinB) ? 1 : 0;

    _history = ((_history << 2) | (a << 1) | b) & 0x0F;
    const int8_t move = QUAD_TABLE[_history];

    if (move != 0) {
        _accumulator += move;

        if (_accumulator >= 4) {
            _accumulator = 0;
            CommandQueue::instance().push({
                CommandType::VolumeDelta,
                CommandSource::Encoder,
                (_config.direction == EncoderDirection::Reversed ? -1 : 1) * _config.volumeStep
            });
        } else if (_accumulator <= -4) {
            _accumulator = 0;
            CommandQueue::instance().push({
                CommandType::VolumeDelta,
                CommandSource::Encoder,
                (_config.direction == EncoderDirection::Reversed ? 1 : -1) * _config.volumeStep
            });
        }
    }

    const bool raw = digitalRead(_config.pinButton);
    if (raw != _lastButtonRaw) {
        _lastButtonRaw = raw;
        _buttonChangedMs = millis();
    }

    if ((millis() - _buttonChangedMs) >= 30 && raw != _stableButton) {
        _stableButton = raw;
        if (_stableButton == LOW) {
            CommandQueue::instance().push({
                CommandType::TogglePlayStop,
                CommandSource::Encoder,
                0
            });
        }
    }
}

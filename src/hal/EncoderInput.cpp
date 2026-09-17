#include "EncoderInput.h"
#include "BoardConfig.h"
#include "../core/CommandQueue.h"

static const int8_t QUAD_TABLE[16] = {
     0, -1, +1,  0,
    +1,  0,  0, -1,
    -1,  0,  0, +1,
     0, +1, -1,  0
};

void EncoderInput::begin() {
    pinMode(Board::ENC_LEFT, Board::ENC_INTERNAL_PULLUP ? INPUT_PULLUP : INPUT);
    pinMode(Board::ENC_RIGHT, Board::ENC_INTERNAL_PULLUP ? INPUT_PULLUP : INPUT);
    pinMode(Board::ENC_BUTTON, Board::ENC_INTERNAL_PULLUP ? INPUT_PULLUP : INPUT);
}

void EncoderInput::loop() {
    const uint8_t a = digitalRead(Board::ENC_LEFT) ? 1 : 0;
    const uint8_t b = digitalRead(Board::ENC_RIGHT) ? 1 : 0;

    _history = ((_history << 2) | (a << 1) | b) & 0x0F;
    const int8_t move = QUAD_TABLE[_history];

    if (move != 0) {
        _accumulator += move;

        if (_accumulator >= 4) {
            _accumulator = 0;
            CommandQueue::instance().push({
                CommandType::VolumeDelta,
                CommandSource::Encoder,
                +1 * Board::ENC_DIRECTION
            });
        } else if (_accumulator <= -4) {
            _accumulator = 0;
            CommandQueue::instance().push({
                CommandType::VolumeDelta,
                CommandSource::Encoder,
                -1 * Board::ENC_DIRECTION
            });
        }
    }

    const bool raw = digitalRead(Board::ENC_BUTTON);
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

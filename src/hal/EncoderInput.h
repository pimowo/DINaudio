#pragma once
#include <Arduino.h>
#include "../config/ConfigModel.h"

class EncoderInput {
public:
    void begin(const EncoderConfig& config);
    void loop();

private:
    EncoderConfig _config;
    uint8_t _history = 0;
    int _accumulator = 0;
    bool _lastButtonRaw = HIGH;
    bool _stableButton = HIGH;
    uint32_t _buttonChangedMs = 0;
};

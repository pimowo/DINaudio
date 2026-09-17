#pragma once
#include <Arduino.h>

class EncoderInput {
public:
    void begin();
    void loop();

private:
    uint8_t _history = 0;
    int _accumulator = 0;
    bool _lastButtonRaw = HIGH;
    bool _stableButton = HIGH;
    uint32_t _buttonChangedMs = 0;
};

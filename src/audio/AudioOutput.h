#pragma once

#include <Arduino.h>
#include <ESP_I2S.h>

class AudioOutput {
public:
    bool begin();
    bool ready() const { return _ready; }
    I2SClass& stream() { return _i2s; }

private:
    I2SClass _i2s;
    bool _ready = false;
};

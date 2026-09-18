#pragma once

#include <Arduino.h>
#include <ESP_I2S.h>

class AudioOutput {
private:
    friend class AudioOutputManager;
    bool begin();
    bool end();
    bool ready() const { return _ready; }
    I2SClass& stream() { return _i2s; }

    I2SClass _i2s;
    bool _ready = false;
};

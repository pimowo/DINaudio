#pragma once

#include "../config/ConfigManager.h"
#include "../hal/EncoderInput.h"
#include "../ui/DisplayService.h"
#include "../audio/AudioOutput.h"
#include "../bluetooth/BluetoothService.h"
#include "../network/WiFiService.h"
#include "../network/WebService.h"

class App {
public:
    bool begin();
    void loop();

private:
    ConfigManager _config;
    EncoderInput _encoder;
    DisplayService _display;

    AudioOutput _audioOutput;
    BluetoothService _bluetooth;

    WiFiService _wifi;
    WebService _web;

    uint32_t _volumeSaveDue = 0;
    bool _volumeDirty = false;

    void processCommands();
    String makeBluetoothName() const;
};

#pragma once

#include "../storage/SettingsStore.h"
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
    SettingsStore _settings;
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

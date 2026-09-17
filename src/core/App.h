#pragma once

#include "StateStore.h"

#include "StateStore.h"

#include "../config/ConfigManager.h"
#include "../hal/EncoderInput.h"
#include "../ui/DisplayService.h"
#include "../audio/AudioOutput.h"
#include "../bluetooth/BluetoothService.h"
#include "../network/WiFiService.h"
#include "../network/WebService.h"
#include "../time/TimeService.h"

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
    TimeService _time;

    uint32_t _volumeSaveDue = 0;
    bool _volumeDirty = false;

    bool _btWasConnected = false;
    bool _btReconnectGraceActive = false;
    uint32_t _btReconnectGraceUntil = 0;
    AudioSource _sourceBeforeBluetooth = AudioSource::Stop;

    void processCommands();
    void updateBluetoothOwnership();
    void enterBluetoothReconnectGrace();
    void finishBluetoothReconnectGrace();
    String makeBluetoothName() const;
};


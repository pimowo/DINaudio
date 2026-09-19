#pragma once
#include <memory>

#include "StateStore.h"

#include "StateStore.h"

#include "../config/ConfigManager.h"
#include "../hal/EncoderInput.h"
#include "../ui/DisplayService.h"
#include "../audio/AudioOutputManager.h"
#include "../bluetooth/BluetoothService.h"
#include "../radio/RadioService.h"
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
    std::unique_ptr<DisplayService> _display;

    AudioOutputManager _audioOutput;
    BluetoothService _bluetooth;
    RadioService _radio;
    bool _bluetoothAvailable = false;
    bool _radioAvailable = false;
    bool _btSuspendedForRadio = false;
    bool _radioSession = false;
    char _radioCommand[24] = {0};
    uint8_t _radioCommandLength = 0;
    bool _radioCommandOverflow = false;

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
    bool startRadio(const char* url);
    void stopRadio();
    void processRadioTestCommands();
    void updateBluetoothOwnership();
    void enterBluetoothReconnectGrace();
    void finishBluetoothReconnectGrace();
    String makeBluetoothName() const;
};


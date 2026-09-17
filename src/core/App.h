#pragma once
#include "../storage/SettingsStore.h"
#include "../hal/EncoderInput.h"
#include "../ui/DisplayService.h"
#include "../audio/TestTone.h"
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
    TestTone _tone;
    WiFiService _wifi;
    WebService _web;

    uint32_t _volumeSaveDue = 0;
    bool _volumeDirty = false;

    void processCommands();
};

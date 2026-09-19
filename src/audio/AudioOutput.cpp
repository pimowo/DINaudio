#include "AudioOutput.h"

#include "AppConfig.h"
#include "../diagnostics/Logger.h"

bool AudioOutput::begin(int bclk, int lrclk, int dout) {
    if (_ready) return true;

    _i2s.setPins(bclk, lrclk, dout);

    if (!_i2s.begin(
            I2S_MODE_STD,
            AppConfig::AUDIO_SAMPLE_RATE,
            I2S_DATA_BIT_WIDTH_16BIT,
            I2S_SLOT_MODE_STEREO,
            I2S_STD_SLOT_BOTH)) {
        Logger::error("AUDIO", "I2S init failed");
        return false;
    }

    _ready = true;
    Logger::info(
        "AUDIO",
        String("I2S ready BCLK=") + bclk +
        " WS=" + lrclk +
        " DOUT=" + dout +
        " rate=" + AppConfig::AUDIO_SAMPLE_RATE
    );
    return true;
}

bool AudioOutput::end() {
    if (!_ready) return true;
    // Only after the manager has detached a quiescent producer. ESP_I2S end
    // disables/deletes channels and releases DMA resources and pin ownership.
    if (!_i2s.end()) {
        Logger::error("AUDIO", "I2S teardown failed");
        return false;
    }
    _ready = false;
    return true;
}

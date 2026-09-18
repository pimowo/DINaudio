#pragma once

#include <Arduino.h>
#include <HTTPClient.h>
#include <NetworkClient.h>
#include <libhelix-mp3/mp3dec.h>

#include "../audio/AudioOutputManager.h"

// All calls (including stop) run on the App task. No worker or PCM callbacks.
// App suspends Bluetooth before start and resumes it only after lease release.
class RadioService {
public:
    RadioService() = default;
    RadioService(const RadioService&) = delete;
    RadioService& operator=(const RadioService&) = delete;

    bool begin(AudioOutputManager& output);
    bool start(const char* url);
    void stop();
    void loop();
    bool isRunning() const { return _running; }
    void setVolume(int volume) { _volume = constrain(volume, 0, 100); }
    const char* lastError() const { return _error; }

private:
    static constexpr size_t INPUT_BYTES = 8192;
    static constexpr size_t PCM_SAMPLES = 2304;
    static constexpr uint32_t STALL_MS = 10000;

    AudioOutputManager* _manager = nullptr;
    TaskHandle_t _appTask = nullptr;
    // Client must outlive HTTPClient (members are destroyed in reverse order).
    NetworkClient _client;
    HTTPClient _http;
    Print* _output = nullptr;
    HMP3Decoder _decoder = nullptr;
    uint8_t* _input = nullptr;
    int16_t* _pcm = nullptr;
    size_t _used = 0;
    size_t _pcmBytes = 0;
    size_t _pcmOffset = 0;
    size_t _skipped = 0;
    int _bodyRemaining = -1;
    uint32_t _lastData = 0;
    uint32_t _lastFrame = 0;
    uint32_t _sampleRate = 0;
    uint8_t _reservoirMisses = 0;
    int _volume = 25;
    bool _lease = false;
    bool _attached = false;
    bool _running = false;
    bool _fault = false;
    const char* _error = nullptr;

    bool onAppTask() const;
    void fail(const char* error);
    void consume(size_t bytes);
};

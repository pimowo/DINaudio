#include "RadioService.h"

#include <cstdlib>
#include <cstring>
#include <WiFi.h>
#include "../diagnostics/Logger.h"

// Use the decoder's allocator interface directly. Return nullptr on OOM,
// unlike the upstream C++ wrapper allocator, which loops forever on failure.
extern "C" void* helix_malloc(int size) {
    return size > 0 ? malloc(static_cast<size_t>(size)) : nullptr;
}
extern "C" void helix_free(void* pointer) { free(pointer); }

bool RadioService::onAppTask() const {
    return _appTask && _appTask == xTaskGetCurrentTaskHandle();
}

bool RadioService::begin(AudioOutputManager& output) {
    if (_manager) return _manager == &output && onAppTask();
    _manager = &output;
    _appTask = xTaskGetCurrentTaskHandle();
    return true;
}

bool RadioService::start(const char* url) {
    if (!onAppTask() || _running || _lease || _fault) return false;
    _error = nullptr;
    if (!url || strncmp(url, "http://", 7) != 0 || WiFi.status() != WL_CONNECTED) {
        fail("HTTP URL or Wi-Fi unavailable");
        return false;
    }
    // Do not take over an existing Radio instance's idempotent manager lease.
    if (_manager->owner() != AudioOutputOwner::None ||
        !_manager->acquire(AudioOutputOwner::Radio)) {
        fail("I2S acquire failed");
        return false;
    }
    _lease = true;
    _output = _manager->attach(AudioOutputOwner::Radio);
    if (!_output) {
        fail("I2S attach failed");
        return false;
    }
    _attached = true;
    _input = static_cast<uint8_t*>(malloc(INPUT_BYTES));
    _pcm = static_cast<int16_t*>(malloc(PCM_SAMPLES * sizeof(int16_t)));
    _decoder = MP3InitDecoder();
    if (!_input || !_pcm || !_decoder) {
        fail("MP3 allocation failed");
        return false;
    }

    _http.setReuse(false);
    _http.useHTTP10(true);
    _http.setConnectTimeout(5000);
    _http.setTimeout(2000);
    _http.setFollowRedirects(HTTPC_DISABLE_FOLLOW_REDIRECTS);
    if (!_http.begin(_client, String(url))) {
        fail("HTTP begin failed");
        return false;
    }
    const char* headers[] = {
        "Content-Type", "Content-Encoding", "Transfer-Encoding", "icy-metaint"
    };
    _http.collectHeaders(headers, 4);
    _http.addHeader("Icy-MetaData", "0");
    const int status = _http.GET();
    if (status != HTTP_CODE_OK) {
        Logger::warn("RADIO", String("HTTP status: ") + status);
        fail("HTTP GET failed (only direct 200 supported)");
        return false;
    }
    String type = _http.header("Content-Type");
    type.toLowerCase();
    String encoding = _http.header("Content-Encoding");
    String transfer = _http.header("Transfer-Encoding");
    encoding.toLowerCase();
    transfer.toLowerCase();
    if ((!type.startsWith("audio/mpeg") && !type.startsWith("audio/mp3")) ||
        (encoding.length() && encoding != "identity") ||
        (transfer.length() && transfer != "identity") ||
        (_http.hasHeader("icy-metaint") && _http.header("icy-metaint") != "0")) {
        fail("Unsupported HTTP body (MP3 identity without ICY required)");
        return false;
    }
    _bodyRemaining = _http.getSize();
    _used = _pcmBytes = _pcmOffset = _skipped = 0;
    _sampleRate = 0;
    _reservoirMisses = 0;
    _lastData = _lastFrame = millis();
    _running = true;
    Logger::info("RADIO", String("HTTP MP3 started; heap=") + ESP.getFreeHeap());
    return true;
}

void RadioService::stop() {
    if (!onAppTask() || _fault) return; // A teardown fault requires a reboot.
    // Same task as loop: no in-flight producer remains after this point.
    _running = false;
    _http.end();
    _client.stop();
    if (_decoder) MP3FreeDecoder(_decoder);
    _decoder = nullptr;
    free(_input);
    free(_pcm);
    _input = nullptr;
    _pcm = nullptr;
    _used = _pcmBytes = _pcmOffset = 0;
    _output = nullptr;
    if (_attached) {
        if (!_manager->detach(AudioOutputOwner::Radio)) {
            _fault = true;
            _error = "I2S detach failed; lease retained";
            Logger::error("RADIO", _error);
            return;
        }
        _attached = false;
    }
    if (_lease) {
        if (!_manager->release(AudioOutputOwner::Radio)) {
            _fault = true;
            _error = "I2S release failed; lease retained";
            Logger::error("RADIO", _error);
            return;
        }
        _lease = false;
        Logger::info("RADIO", String("Stopped; heap=") + ESP.getFreeHeap());
    }
}

void RadioService::fail(const char* error) {
    _error = error;
    Logger::error("RADIO", error);
    stop();
}

void RadioService::consume(size_t bytes) {
    _used -= bytes;
    memmove(_input, _input + bytes, _used);
}

void RadioService::loop() {
    if (!onAppTask() || !_running) return;
    if (!_manager->isOwnedBy(AudioOutputOwner::Radio)) {
        fail("I2S ownership lost");
        return;
    }
    // Read only bytes already available; never wait for a whole network frame.
    int available = _client.available();
    if (available > 0 && _used < INPUT_BYTES && _bodyRemaining != 0) {
        size_t count = min(static_cast<size_t>(available), INPUT_BYTES - _used);
        count = min(count, static_cast<size_t>(1024));
        if (_bodyRemaining > 0) count = min(count, static_cast<size_t>(_bodyRemaining));
        const int received = _client.read(_input + _used, count);
        if (received > 0) {
            _used += received;
            if (_bodyRemaining > 0) _bodyRemaining -= received;
            _lastData = millis();
        }
    }
    // Bound each App loop write to <= 1024 PCM bytes (about 6 ms at 44.1 kHz).
    if (_pcmOffset < _pcmBytes) {
        const size_t count = min(_pcmBytes - _pcmOffset, static_cast<size_t>(1024));
        const size_t written = _output->write(
            reinterpret_cast<uint8_t*>(_pcm) + _pcmOffset, count);
        if (written != count) {
            fail("I2S PCM write failed");
            return;
        }
        _pcmOffset += written;
        return;
    }
    const bool ended = _bodyRemaining == 0 ||
        (!_client.connected() && _client.available() == 0);
    if (_used < 6) {
        if (ended) fail("HTTP stream ended");
        else if (millis() - _lastData >= STALL_MS) fail("HTTP stream stalled");
        return;
    }
    if (millis() - _lastFrame >= STALL_MS) {
        fail("No decodable MP3 frame");
        return;
    }
    const int offset = MP3FindSyncWord(_input, static_cast<int>(_used));
    if (offset != 0) {
        const size_t skipped = offset < 0 ? _used - 1 : static_cast<size_t>(offset);
        consume(skipped); // Keep the last byte for a split sync word.
        _skipped += skipped;
        if (_skipped > 65536) fail("MP3 sync not found");
        return;
    }
    // Reject reserved versions, non-Layer III and free-bitrate headers before
    // Helix reads tables/sideinfo. Only decode a COMPLETE, bounded frame.
    const int version = (_input[1] >> 3) & 3;
    if (version < 2 || ((_input[1] >> 1) & 3) != 1 ||
        (_input[2] >> 4) == 0 || (_input[2] >> 4) == 15 ||
        ((_input[2] >> 2) & 3) == 3) {
        consume(1);
        ++_skipped;
        return;
    }
    MP3FrameInfo info{};
    if (MP3GetNextFrameInfo(_decoder, &info, _input) != ERR_MP3_NONE ||
        info.samprate < 16000 || info.samprate > 48000 || info.bitrate <= 0) {
        fail("Invalid MP3 header");
        return;
    }
    const size_t frameBytes = (version == 3 ? 144 : 72) * info.bitrate /
        info.samprate + ((_input[2] >> 1) & 1);
    const size_t sideBytes = version == 3 ? (info.nChans == 1 ? 17 : 32) :
        (info.nChans == 1 ? 9 : 17);
    if (frameBytes < 4 + ((_input[1] & 1) ? 0 : 2) + sideBytes || frameBytes > 2048) {
        fail("Unsupported MP3 frame size");
        return;
    }
    if (_used < frameBytes) {
        if (ended) fail("Truncated MP3 frame");
        else if (millis() - _lastData >= STALL_MS) fail("HTTP stream stalled");
        return;
    }
    unsigned char* cursor = _input;
    int bytesLeft = static_cast<int>(frameBytes);
    const int result = MP3Decode(_decoder, &cursor, &bytesLeft, _pcm, 0);
    consume(frameBytes);
    if (result == ERR_MP3_MAINDATA_UNDERFLOW && ++_reservoirMisses <= 8) return;
    if (result != ERR_MP3_NONE) {
        Logger::warn("RADIO", String("Helix error: ") + result);
        fail("MP3 decode failed");
        return;
    }
    _reservoirMisses = 0;
    _skipped = 0;
    MP3GetLastFrameInfo(_decoder, &info);
    if (info.bitsPerSample != 16 || (info.nChans != 1 && info.nChans != 2) ||
        info.outputSamps <= 0 || info.outputSamps > static_cast<int>(PCM_SAMPLES) ||
        (info.nChans == 1 && info.outputSamps > static_cast<int>(PCM_SAMPLES / 2)) ||
        info.outputSamps % info.nChans) {
        fail("Unsupported decoded PCM");
        return;
    }
    if (_sampleRate != static_cast<uint32_t>(info.samprate)) {
        if (!_manager->configureStereo16(AudioOutputOwner::Radio, info.samprate)) {
            fail("I2S PCM configuration failed");
            return;
        }
        _sampleRate = info.samprate;
        Logger::info("RADIO", String("PCM rate=") + _sampleRate +
            " channels=" + info.nChans);
    }
    // Stereo output is fixed. Expand mono backwards in the same PCM buffer.
    if (info.nChans == 1) {
        for (int i = info.outputSamps - 1; i >= 0; --i) {
            const int16_t value = static_cast<int32_t>(_pcm[i]) * _volume / 100;
            _pcm[2 * i] = _pcm[2 * i + 1] = value;
        }
        _pcmBytes = info.outputSamps * 2 * sizeof(int16_t);
    } else {
        for (int i = 0; i < info.outputSamps; ++i)
            _pcm[i] = static_cast<int32_t>(_pcm[i]) * _volume / 100;
        _pcmBytes = info.outputSamps * sizeof(int16_t);
    }
    _pcmOffset = 0;
    _lastFrame = millis();
}

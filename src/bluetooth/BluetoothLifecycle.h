#pragma once

#include <atomic>
#include <BluetoothA2DPSink.h>

// A persistent output adapter: end(false) does not clear the library's Print
// pointer. Closing this gate protects I2S even from a late audio callback.
class BluetoothOutputGate : public Print {
public:
    size_t write(uint8_t byte) override { return write(&byte, 1); }
    size_t write(const uint8_t* data, size_t size) override {
        portENTER_CRITICAL(&_mux);
        Print* target = _target;
        if (_paused.load(std::memory_order_relaxed) || !_open || target == nullptr) {
            portEXIT_CRITICAL(&_mux);
            _droppedBytes.fetch_add(size, std::memory_order_relaxed);
            // A2DP loops until all input is consumed; returning zero hangs it.
            return size;
        }
        ++_inFlight;
        portEXIT_CRITICAL(&_mux);
        const size_t written = target->write(data, size);
        _writes.fetch_add(1, std::memory_order_relaxed);
        _writtenBytes.fetch_add(written, std::memory_order_relaxed);
        if (written != size) {
            _writeErrors.fetch_add(1, std::memory_order_relaxed);
            _droppedBytes.fetch_add(size - written, std::memory_order_relaxed);
        }
        portENTER_CRITICAL(&_mux);
        if (written != size) _open = false;
        --_inFlight;
        portEXIT_CRITICAL(&_mux);
        // Never let the library spin forever on a short I2S write.
        return size;
    }

    uint32_t writes() const { return _writes.load(std::memory_order_relaxed); }
    uint32_t writtenBytes() const { return _writtenBytes.load(std::memory_order_relaxed); }
    uint32_t writeErrors() const { return _writeErrors.load(std::memory_order_relaxed); }
    uint32_t droppedBytes() const { return _droppedBytes.load(std::memory_order_relaxed); }

    void pauseWrites() { _paused.store(true, std::memory_order_relaxed); }
    void resumeWrites() { _paused.store(false, std::memory_order_relaxed); }

    void open(Print& target) {
        portENTER_CRITICAL(&_mux);
        _target = &target;
        _open = true;
        portEXIT_CRITICAL(&_mux);
    }

    bool closeAndDrain(uint32_t timeoutMs) {
        portENTER_CRITICAL(&_mux);
        _open = false;
        portEXIT_CRITICAL(&_mux);
        const uint32_t started = millis();
        do {
            portENTER_CRITICAL(&_mux);
            const bool drained = _inFlight == 0;
            if (drained) _target = nullptr;
            portEXIT_CRITICAL(&_mux);
            if (drained) return true;
            delay(1);
        } while (millis() - started < timeoutMs);
        return false;
    }

private:
    portMUX_TYPE _mux = portMUX_INITIALIZER_UNLOCKED;
    Print* _target = nullptr;
    uint32_t _inFlight = 0;
    bool _open = false;
    std::atomic<bool> _paused{false};
    std::atomic<uint32_t> _writes{0};
    std::atomic<uint32_t> _writtenBytes{0};
    std::atomic<uint32_t> _writeErrors{0};
    std::atomic<uint32_t> _droppedBytes{0};
};

// Observe raw IDF events before the library queues them. Its end(false)
// deletes the event task, so a queued profile-deinit notification can be lost.
class BluetoothLifecycleSink : public BluetoothA2DPSink {
public:
    void expectProfileEvent() { _profile.store(-1); }
    bool waitForProfile(esp_a2d_init_state_t expected, uint32_t timeoutMs) {
        const uint32_t started = millis();
        do {
            if (_profile.load() == static_cast<int>(expected)) return true;
            delay(1);
        } while (millis() - started < timeoutMs);
        return false;
    }

    void resetVolumeNotification() { s_volume_notify = false; }
    void setLocalAttenuation(uint8_t volume) {
        volume_control()->set_volume(volume);
        volume_control()->set_enabled(true);
    }

    void resetTransportForResume() {
        connection_state = ESP_A2D_CONNECTION_STATE_DISCONNECTED;
        audio_state = ESP_A2D_AUDIO_STATE_STOPPED;
        avrc_connection_state = false;
        s_avrc_peer_rn_cap.bits = 0;
        connection_rety_count = 0;
        is_target_status_active = true;
        is_autoreconnect_allowed = false;
    }

protected:
    void app_a2d_callback(esp_a2d_cb_event_t event,
                          esp_a2d_cb_param_t* param) override {
        if (event == ESP_A2D_PROF_STATE_EVT && param != nullptr) {
            _profile.store(static_cast<int>(param->a2d_prof_stat.init_state));
        }
        BluetoothA2DPSink::app_a2d_callback(event, param);
    }

private:
    std::atomic<int> _profile{-1};
};

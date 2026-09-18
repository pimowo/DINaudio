#include "AudioOutputManager.h"

#include "../diagnostics/Logger.h"

namespace {
const char* ownerName(AudioOutputOwner owner) {
    switch (owner) {
        case AudioOutputOwner::None: return "NONE";
        case AudioOutputOwner::Bluetooth: return "BLUETOOTH";
        case AudioOutputOwner::Radio: return "RADIO";
        case AudioOutputOwner::PlayMedia: return "PLAY_MEDIA";
        default: return "INVALID";
    }
}

bool validOwner(AudioOutputOwner owner) {
    return owner == AudioOutputOwner::Bluetooth ||
        owner == AudioOutputOwner::Radio ||
        owner == AudioOutputOwner::PlayMedia;
}
}

bool AudioOutputManager::begin() {
    // I2S is created only when a source successfully acquires the lease.
    _begun = true;
    return true;
}

bool AudioOutputManager::acquire(AudioOutputOwner requested) {
    if (!_begun || !validOwner(requested) || _fault) {
        Logger::warn("AUDIO", String("Acquire rejected: ") + ownerName(requested));
        return false;
    }
    if (_owner == requested) return true;
    if (_owner != AudioOutputOwner::None) {
        Logger::warn("AUDIO", String("Acquire denied: ") + ownerName(requested) +
            "; held by " + ownerName(_owner));
        return false;
    }
    if (!_output.begin()) {
        // ESP_I2S may retain a partially initialized channel on failure.
        // Do not let another source retry against uncertain driver state.
        _fault = true;
        Logger::error("AUDIO", "Acquire failed: I2S init; reboot required");
        return false;
    }
    _owner = requested;
    Logger::info("AUDIO", String("Acquired: ") + ownerName(_owner));
    return true;
}

bool AudioOutputManager::release(AudioOutputOwner requested) {
    if (!validOwner(requested) || !isOwnedBy(requested)) {
        Logger::warn("AUDIO", String("Invalid release: ") + ownerName(requested));
        return false;
    }
    if (_attached) {
        Logger::warn("AUDIO", "Release denied: producer still attached");
        return false;
    }
    if (!_output.end()) {
        // Keep ownership on teardown failure; another source must not start.
        _fault = true;
        Logger::error("AUDIO", "Release failed: I2S teardown; lease retained");
        return false;
    }
    _owner = AudioOutputOwner::None;
    _fault = false;
    Logger::info("AUDIO", String("Released: ") + ownerName(requested));
    return true;
}

Print* AudioOutputManager::attach(AudioOutputOwner requested) {
    if (!isOwnedBy(requested) || _attached || _fault || !_output.ready()) {
        Logger::warn("AUDIO", "Attach rejected: invalid or busy lease");
        return nullptr;
    }
    _attached = true;
    return &_output.stream();
}

bool AudioOutputManager::detach(AudioOutputOwner requested) {
    if (!isOwnedBy(requested) || !_attached) {
        Logger::warn("AUDIO", "Detach rejected: invalid lease");
        return false;
    }
    _attached = false;
    return true;
}

bool AudioOutputManager::configureStereo16(AudioOutputOwner requested, uint32_t sampleRate) {
    if (!isOwnedBy(requested) || !_attached || _fault || !_output.ready() ||
        sampleRate < 8000 || sampleRate > 48000) return false;
    if (!_output.stream().configureTX(sampleRate, I2S_DATA_BIT_WIDTH_16BIT,
            I2S_SLOT_MODE_STEREO, I2S_STD_SLOT_BOTH)) {
        _fault = true;
        Logger::error("AUDIO", "PCM configuration failed; output blocked");
        return false;
    }
    _output.stream().setTimeout(100);
    return true;
}

#pragma once

#include "AudioOutput.h"

enum class AudioOutputOwner : uint8_t {
    None,
    Bluetooth,
    Radio,
    PlayMedia
};

// Lifecycle calls belong to the App task, never to audio callbacks.
// Source priority and user-visible ownership remain App responsibilities.
class AudioOutputManager {
public:
    AudioOutputManager() = default;
    AudioOutputManager(const AudioOutputManager&) = delete;
    AudioOutputManager& operator=(const AudioOutputManager&) = delete;

    bool begin();
    bool acquire(AudioOutputOwner owner);
    bool release(AudioOutputOwner owner);
    AudioOutputOwner owner() const { return _owner; }
    bool isOwnedBy(AudioOutputOwner owner) const {
        return owner != AudioOutputOwner::None && _owner == owner;
    }

    // Pin the lease while an asynchronous producer retains the Print pointer.
    // A producer must stop/join ALL callbacks before detach; then discard its
    // pointer. release refuses to end I2S while a producer is attached.
    Print* attach(AudioOutputOwner owner);
    bool detach(AudioOutputOwner owner);

private:
    AudioOutput _output;
    AudioOutputOwner _owner = AudioOutputOwner::None;
    bool _begun = false;
    bool _attached = false;
    bool _fault = false;
};

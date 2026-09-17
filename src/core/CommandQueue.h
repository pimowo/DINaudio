#pragma once
#include <Arduino.h>

enum class CommandType : uint8_t {
    VolumeDelta,
    TogglePlayStop,
    SetStop,
    SetPlay
};

enum class CommandSource : uint8_t {
    Encoder,
    Web,
    System
};

struct Command {
    CommandType type;
    CommandSource source;
    int value = 0;
};

class CommandQueue {
public:
    static CommandQueue& instance();
    bool begin();
    bool push(const Command& cmd);
    bool pop(Command& cmd);

private:
    QueueHandle_t _queue = nullptr;
};

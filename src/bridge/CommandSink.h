// Injectable boundary for control commands. Implementations make a batch
// all-or-nothing so related note events cannot be partially scheduled.
#pragma once

#include <span>

#include "core/AudioTypes.h"
#include "engine/AudioEngine.h"

namespace drumas::bridge {

class CommandSink {
public:
    virtual ~CommandSink() = default;

    [[nodiscard]] virtual bool pushBatch(
        std::span<const core::Command> commands) noexcept = 0;

    [[nodiscard]] bool push(const core::Command& command) noexcept {
        return pushBatch(std::span<const core::Command>(&command, 1));
    }
};

class AudioEngineCommandSink final : public CommandSink {
public:
    explicit AudioEngineCommandSink(engine::AudioEngine& engine) noexcept : engine_(engine) {}

    [[nodiscard]] bool pushBatch(
        std::span<const core::Command> commands) noexcept override {
        return engine_.pushCommands(commands);
    }

private:
    engine::AudioEngine& engine_;
};

}  // namespace drumas::bridge

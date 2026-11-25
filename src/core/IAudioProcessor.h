// The contract between the audio engine and whatever produces sound.
//
// The engine knows nothing about oscillators, voices or effects: it owns the
// device, the command queue and the statistics, and calls one prepared
// processor. `synth` implements this interface; later stages add effects and
// rhythms as processors or chains placed in front of it.
#pragma once

#include <span>

#include "core/AudioTypes.h"

namespace drumas::core {

class IAudioProcessor {
public:
    IAudioProcessor() = default;
    virtual ~IAudioProcessor() = default;

    IAudioProcessor(const IAudioProcessor&) = delete;
    IAudioProcessor& operator=(const IAudioProcessor&) = delete;
    IAudioProcessor(IAudioProcessor&&) = delete;
    IAudioProcessor& operator=(IAudioProcessor&&) = delete;

    /// Allocates every buffer the processor will ever use and configures it for
    /// the given stream. NOT real-time safe. Control thread only, and never
    /// while the audio thread is running.
    virtual void prepare(const AudioSpec& spec) = 0;

    /// Drops all sounding voices and restarts internal state. NOT real-time
    /// safe in the general case; control thread only.
    virtual void reset() = 0;

    /// Releases resources owned by a command that was accepted by the engine
    /// queue but never applied. Called on the control thread with the stream
    /// stopped. Most commands own no resources.
    virtual void discardCommand(const Command& /*command*/) noexcept {}

    /// Renders `numFrames` frames into planar, non-interleaved channel buffers
    /// (`output[ch][frame]`), consuming the commands queued for this block.
    ///
    /// REAL-TIME SAFE: no allocation, no locks, no I/O, no exceptions, no
    /// blocking. Every channel buffer is `numFrames` long and must be fully
    /// written (silence included).
    virtual void process(std::span<const Command> commands, float* const* output, int numChannels,
                         int numFrames) noexcept = 0;
};

}  // namespace drumas::core

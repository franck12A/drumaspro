// The real-time half of the engine.
//
// AudioGraph owns everything the audio thread touches: the incoming command
// queue, the planar scratch buffers handed to the processor, and the atomic
// statistics the UI reads. It knows nothing about audio devices, which is what
// makes it testable offline in CI without a sound card.
//
// Thread ownership:
//   - prepare()/setProcessor(): control thread, stream stopped.
//   - pushCommand(): producer side of the queue (control thread, or the console
//     app's main loop). RT-SAFE.
//   - render(): consumer side. Audio thread only. RT-SAFE.
//   - popPendingCommand(): control thread only, with the stream stopped and no
//     concurrent producer.
//   - stats(): any thread (atomics).
#pragma once

#include <array>
#include <atomic>
#include <cstdint>
#include <span>
#include <vector>

#include "core/AudioTypes.h"
#include "core/IAudioProcessor.h"
#include "core/SpscQueue.h"

namespace drumas::engine {

static_assert(std::atomic<float>::is_always_lock_free,
              "The output peak meter requires a lock-free atomic float");

class AudioGraph {
public:
    /// Queue size. Power of two; usable capacity is one less. Sized so that a
    /// burst of keyboard events between two callbacks cannot overflow it, and
    /// small enough to drain completely in one block.
    static constexpr std::size_t kCommandCapacity = 512;

    /// Highest channel count the planar scratch buffer supports.
    static constexpr int kMaxChannels = 8;

    /// Counters published to the control thread. Plain values, copied out of
    /// atomics by stats().
    struct Stats {
        std::uint64_t blocks = 0;
        std::uint64_t frames = 0;
        std::uint64_t commandsApplied = 0;
        std::uint64_t commandsDropped = 0;
        /// Stream glitches: the audio timeline fell more than two blocks behind
        /// the wall clock, i.e. the device ran out of audio and the clock had to
        /// be re-anchored. Counted once per event, not once per late callback.
        std::uint64_t lateBlocks = 0;
        /// Callbacks whose processing time exceeded one block period: we missed
        /// the deadline ourselves.
        std::uint64_t overdueBlocks = 0;
        /// Longest callback duration seen, in milliseconds.
        double maxCallbackMs = 0.0;

        /// Frames per callback as requested by the device. WASAPI does not
        /// always honour the requested period and some backends vary it, so the
        /// observed range is reported instead of assumed.
        std::uint32_t lastBlockFrames = 0;
        std::uint32_t minBlockFrames = 0;
        std::uint32_t maxBlockFrames = 0;
        float outputPeak = 0.0F;
    };

    /// Control thread, stream stopped. Allocates the planar scratch buffers.
    void prepare(const core::AudioSpec& spec);

    /// Control thread, stream stopped.
    void setProcessor(core::IAudioProcessor* processor) noexcept { processor_ = processor; }

    /// Control thread. Starts from silence; statistics are kept.
    void reset() noexcept;

    /// Producer side. Returns false when the queue is full (the event is
    /// dropped and counted).
    [[nodiscard]] bool pushCommand(const core::Command& command) noexcept;

    /// Producer side. Enqueues every command or none. A rejected batch counts
    /// each command as dropped.
    [[nodiscard]] bool pushCommands(std::span<const core::Command> commands) noexcept;

    /// Control thread, only while the stream is stopped and the producer is
    /// quiescent. Removes one command that the audio thread did not apply.
    [[nodiscard]] bool popPendingCommand(core::Command& command) noexcept {
        return commands_.pop(command);
    }

    /// Audio thread. Renders one interleaved block, draining the command queue
    /// first. Blocks larger than the prepared maximum are split internally.
    /// RT-SAFE.
    void render(float* interleavedOutput, int channels, int numFrames) noexcept;

    /// Any thread.
    [[nodiscard]] Stats stats() const noexcept;
    [[nodiscard]] core::AudioSpec spec() const noexcept { return spec_; }

private:
    void renderSlice(float* interleavedOutput, int channels, int numFrames) noexcept;

    std::vector<float> planeStorage_;
    std::array<float*, kMaxChannels> planes_{};
    std::array<core::Command, kCommandCapacity> blockCommands_{};

    core::SpscQueue<core::Command, kCommandCapacity> commands_;
    core::IAudioProcessor* processor_ = nullptr;

    core::AudioSpec spec_{};
    int minBlockFramesPrivate_ = 0;
    int maxBlockFramesPrivate_ = 0;

    // Drift window: wall clock since the anchor vs. audio time rendered since
    // the anchor. Audio-thread-private except for the published statistics.
    std::uint64_t driftAnchorTicks_ = 0;
    std::uint64_t driftFrames_ = 0;

    std::atomic<std::uint64_t> blocks_{0};
    std::atomic<std::uint64_t> frames_{0};
    std::atomic<std::uint64_t> commandsApplied_{0};
    std::atomic<std::uint64_t> commandsDropped_{0};
    std::atomic<std::uint64_t> lateBlocks_{0};
    std::atomic<std::uint32_t> lastBlockFrames_{0};
    std::atomic<std::uint32_t> minBlockFrames_{0};
    std::atomic<std::uint32_t> maxBlockFrames_{0};
    std::atomic<std::uint64_t> overdueBlocks_{0};
    std::atomic<std::uint64_t> maxCallbackMicros_{0};
    float outputPeakAudioThread_ = 0.0F;
    std::atomic<float> outputPeak_{0.0F};
};

}  // namespace drumas::engine

#include "engine/AudioGraph.h"

#include <algorithm>
#include <chrono>
#include <cstddef>
#include <span>

namespace drumas::engine {
namespace {

using Clock = std::chrono::steady_clock;

[[nodiscard]] std::uint64_t toTicks(Clock::time_point timePoint) noexcept {
    return static_cast<std::uint64_t>(
        std::chrono::duration_cast<std::chrono::nanoseconds>(timePoint.time_since_epoch()).count());
}

/// How far the audio timeline may fall behind the wall clock before the stream
/// counts as glitched. Expressed in block durations so the threshold follows the
/// block size the device actually uses.
constexpr double kGlitchThresholdBlocks = 2.0;
constexpr std::uint64_t kNsPerMs = 1000000;

}  // namespace

void AudioGraph::prepare(const core::AudioSpec& spec) {
    spec_ = spec;
    spec_.channels = std::clamp(spec.channels, 1, kMaxChannels);
    spec_.maxBlockSize = std::max(spec.maxBlockSize, 1);

    // Single allocation for the whole life of the graph.
    planeStorage_.assign(static_cast<std::size_t>(spec_.maxBlockSize) *
                             static_cast<std::size_t>(spec_.channels),
                         0.0F);
    for (int channel = 0; channel < spec_.channels; ++channel) {
        planes_[static_cast<std::size_t>(channel)] =
            planeStorage_.data() +
            (static_cast<std::size_t>(channel) * static_cast<std::size_t>(spec_.maxBlockSize));
    }

    driftAnchorTicks_ = 0;
    driftFrames_ = 0;
    minBlockFramesPrivate_ = 0;
    maxBlockFramesPrivate_ = 0;
    lastBlockFrames_.store(0, std::memory_order_relaxed);
    minBlockFrames_.store(0, std::memory_order_relaxed);
    maxBlockFrames_.store(0, std::memory_order_relaxed);
    blocks_.store(0, std::memory_order_relaxed);
    frames_.store(0, std::memory_order_relaxed);
    commandsApplied_.store(0, std::memory_order_relaxed);
    commandsDropped_.store(0, std::memory_order_relaxed);
    lateBlocks_.store(0, std::memory_order_relaxed);
    overdueBlocks_.store(0, std::memory_order_relaxed);
    maxCallbackMicros_.store(0, std::memory_order_relaxed);
}

void AudioGraph::reset() noexcept {
    if (processor_ != nullptr) {
        processor_->reset();
    }
    driftAnchorTicks_ = 0;
    driftFrames_ = 0;
}

bool AudioGraph::pushCommand(const core::Command& command) noexcept {
    if (!commands_.push(command)) {
        commandsDropped_.fetch_add(1, std::memory_order_relaxed);
        return false;
    }
    return true;
}

void AudioGraph::render(float* interleavedOutput, int channels, int numFrames) noexcept {
    if (interleavedOutput == nullptr || numFrames <= 0) {
        return;
    }

    const auto start = Clock::now();
    const std::uint64_t startTicks = toTicks(start);

    // Expected wall-clock duration of this block, derived from the frame count
    // the device actually asked for (WASAPI may not give us the exact period we
    // requested).
    const double expectedMs =
        1000.0 * static_cast<double>(numFrames) / std::max(spec_.sampleRate, 1.0);

    // Glitch detection by drift, not by per-callback gaps: WASAPI can vary the
    // block size from one callback to the next, so a gap comparison produces
    // false positives. Instead, check whether the audio timeline (frames
    // rendered) is falling behind the wall clock (time actually elapsed).
    if (driftAnchorTicks_ == 0) {
        driftAnchorTicks_ = startTicks;
        driftFrames_ = 0;
    }
    driftFrames_ += static_cast<std::uint64_t>(numFrames);
    {
        const double audioMs =
            1000.0 * static_cast<double>(driftFrames_) / std::max(spec_.sampleRate, 1.0);
        const double wallMs =
            static_cast<double>(startTicks - driftAnchorTicks_) / static_cast<double>(kNsPerMs);
        const double currentBlockMs =
            1000.0 * static_cast<double>(numFrames) / std::max(spec_.sampleRate, 1.0);
        if (wallMs - audioMs > kGlitchThresholdBlocks * currentBlockMs) {
            lateBlocks_.fetch_add(1, std::memory_order_relaxed);
            // Re-anchor: one event, one count. Without this the counter would
            // keep firing every callback until the backlog cleared.
            driftAnchorTicks_ = startTicks;
            driftFrames_ = 0;
        }
    }

    const int blockFrames = numFrames;
    lastBlockFrames_.store(static_cast<std::uint32_t>(blockFrames), std::memory_order_relaxed);
    if (minBlockFramesPrivate_ == 0 || blockFrames < minBlockFramesPrivate_) {
        minBlockFramesPrivate_ = blockFrames;
        minBlockFrames_.store(static_cast<std::uint32_t>(blockFrames), std::memory_order_relaxed);
    }
    if (blockFrames > maxBlockFramesPrivate_) {
        maxBlockFramesPrivate_ = blockFrames;
        maxBlockFrames_.store(static_cast<std::uint32_t>(blockFrames), std::memory_order_relaxed);
    }

    // Split oversized blocks so that a device period larger than what prepare()
    // sized never allocates and never writes out of bounds.
    const int usableChannels = std::clamp(channels, 1, kMaxChannels);
    int offset = 0;
    while (offset < numFrames) {
        const int slice = std::min(spec_.maxBlockSize, numFrames - offset);
        renderSlice(interleavedOutput +
                        (static_cast<std::size_t>(offset) * static_cast<std::size_t>(channels)),
                    usableChannels, slice);
        offset += slice;
    }

    const auto end = Clock::now();
    const double elapsedMs = std::chrono::duration<double, std::milli>(end - start).count();
    const auto elapsedMicros = static_cast<std::uint64_t>(
        std::chrono::duration_cast<std::chrono::microseconds>(end - start).count());

    if (elapsedMs > expectedMs) {
        overdueBlocks_.fetch_add(1, std::memory_order_relaxed);
    }

    std::uint64_t previousMax = maxCallbackMicros_.load(std::memory_order_relaxed);
    while (elapsedMicros > previousMax &&
           !maxCallbackMicros_.compare_exchange_weak(previousMax, elapsedMicros,
                                                     std::memory_order_relaxed)) {
        // previousMax was refreshed by the failed exchange; retry.
    }

    blocks_.fetch_add(1, std::memory_order_relaxed);
    frames_.fetch_add(static_cast<std::uint64_t>(numFrames), std::memory_order_relaxed);
}

void AudioGraph::renderSlice(float* interleavedOutput, int channels, int numFrames) noexcept {
    // Drain at the start of the block. Anything that does not fit stays queued
    // for the next block; nothing is allocated here.
    std::size_t commandCount = 0;
    core::Command command;
    while (commandCount < blockCommands_.size() && commands_.pop(command)) {
        blockCommands_[commandCount++] = command;
    }
    commandsApplied_.fetch_add(commandCount, std::memory_order_relaxed);

    const auto commands = std::span<const core::Command>(blockCommands_.data(), commandCount);

    // Zero the planes: the processor contract says it writes every frame, but a
    // silent plane is cheaper than a debugging session with a half-written one.
    for (int channel = 0; channel < channels; ++channel) {
        std::fill_n(planes_[static_cast<std::size_t>(channel)], numFrames, 0.0F);
    }

    if (processor_ != nullptr) {
        processor_->process(commands, planes_.data(), channels, numFrames);
    }

    // Planar -> interleaved for the device.
    for (int channel = 0; channel < channels; ++channel) {
        const float* plane = planes_[static_cast<std::size_t>(channel)];
        for (int frame = 0; frame < numFrames; ++frame) {
            interleavedOutput[static_cast<std::size_t>(frame) * static_cast<std::size_t>(channels) +
                              static_cast<std::size_t>(channel)] = plane[frame];
        }
    }
}

AudioGraph::Stats AudioGraph::stats() const noexcept {
    Stats snapshot;
    snapshot.blocks = blocks_.load(std::memory_order_relaxed);
    snapshot.frames = frames_.load(std::memory_order_relaxed);
    snapshot.commandsApplied = commandsApplied_.load(std::memory_order_relaxed);
    snapshot.commandsDropped = commandsDropped_.load(std::memory_order_relaxed);
    snapshot.lateBlocks = lateBlocks_.load(std::memory_order_relaxed);
    snapshot.overdueBlocks = overdueBlocks_.load(std::memory_order_relaxed);
    snapshot.maxCallbackMs =
        static_cast<double>(maxCallbackMicros_.load(std::memory_order_relaxed)) / 1000.0;
    snapshot.lastBlockFrames = lastBlockFrames_.load(std::memory_order_relaxed);
    snapshot.minBlockFrames = minBlockFrames_.load(std::memory_order_relaxed);
    snapshot.maxBlockFrames = maxBlockFrames_.load(std::memory_order_relaxed);
    return snapshot;
}

}  // namespace drumas::engine

// Audio device ownership: opens the default playback device through miniaudio
// (WASAPI on Windows), 32-bit float, and drives one IAudioProcessor through the
// real-time AudioGraph.
//
// Lifecycle: initialize() -> start() -> ... -> stop() -> shutdown(). Every step
// returns a result instead of throwing; the error text is available through
// lastError(). The audio callback never throws, never allocates and never logs.
//
// miniaudio's C API stays out of this header on purpose: consumers do not need
// it, and it keeps compile times and /Wall style diagnostics contained.
#pragma once

#include <atomic>
#include <cstdint>
#include <memory>
#include <string>
#include <string_view>

#include "core/AudioTypes.h"
#include "core/IAudioProcessor.h"
#include "engine/AudioGraph.h"

struct ma_context;
struct ma_device;

namespace drumas::engine {

class AudioEngine {
public:
    struct Config {
        std::uint32_t sampleRate = static_cast<std::uint32_t>(core::kDefaultSampleRate);
        std::uint32_t channels = static_cast<std::uint32_t>(core::kDefaultChannels);
        /// Requested device period. WASAPI may round it; deviceInfo() reports
        /// what the device actually uses.
        std::uint32_t bufferFrames = 256;
        /// Upper bound for one device block. Sizes the planar scratch buffers
        /// once, during initialize().
        std::uint32_t maxBlockFrames = 2048;
    };

    struct DeviceInfo {
        std::string backend;
        std::string deviceName;
        double sampleRate = 0.0;
        int channels = 0;
        int reportedBufferFrames = 0;
        /// Lower bound: driver buffer / sample rate. The callback period and the
        /// WASAPI shared-mode engine add to it, so the real latency is higher;
        /// the app prints the observed callback period next to it.
        double estimatedLatencyMs = 0.0;
        /// Notifications from the backend (device stopped, rerouted,
        /// interrupted) since initialize(). Non-zero means the stream was
        /// disturbed by the system.
        std::uint64_t deviceEvents = 0;
    };

    AudioEngine();
    ~AudioEngine();

    AudioEngine(const AudioEngine&) = delete;
    AudioEngine& operator=(const AudioEngine&) = delete;
    AudioEngine(AudioEngine&&) = delete;
    AudioEngine& operator=(AudioEngine&&) = delete;

    /// Control thread. Prepares the processor (allocations happen here) and
    /// opens the device. Returns false and sets lastError() on failure.
    bool initialize(const Config& config, core::IAudioProcessor& processor);

    /// Control thread. Starts the stream.
    bool start();

    /// Control thread. Stops the stream and silences the processor.
    void stop() noexcept;

    /// Control thread. Stops the stream and releases the device and context.
    /// Safe to call more than once, and safe after a failed initialize().
    void shutdown() noexcept;

    [[nodiscard]] bool isRunning() const noexcept {
        return running_.load(std::memory_order_acquire);
    }

    /// Any thread. Producer side of the lock-free command queue. RT-SAFE.
    [[nodiscard]] bool pushCommand(const core::Command& command) noexcept {
        return graph_.pushCommand(command);
    }

    /// Any thread.
    [[nodiscard]] AudioGraph::Stats stats() const noexcept { return graph_.stats(); }

    /// Control thread, after initialize().
    [[nodiscard]] DeviceInfo deviceInfo() const;

    [[nodiscard]] std::string_view lastError() const noexcept { return lastError_; }

    /// Diagnostics and tests. Control thread.
    [[nodiscard]] AudioGraph& graph() noexcept { return graph_; }

    /// Internal: called by the miniaudio notification callback when the backend
    /// reports a disturbed stream. Any thread, atomic only.
    void noteDeviceEvent() noexcept { deviceEvents_.fetch_add(1, std::memory_order_relaxed); }

private:
    static void dataCallback(struct ma_device* device, void* output, const void* input,
                             unsigned int frameCount);

    void setError(std::string message);

    Config config_{};
    AudioGraph graph_;
    std::unique_ptr<ma_context> context_;
    std::unique_ptr<ma_device> device_;
    std::string lastError_;
    std::atomic<bool> running_{false};
    std::atomic<std::uint64_t> deviceEvents_{0};
};

}  // namespace drumas::engine

#include "engine/AudioEngine.h"

#include <algorithm>
#include <utility>

// miniaudio is third-party C code: keep its diagnostics out of the first-party
// /W4 + /WX policy without weakening our own flags.
#if defined(_MSC_VER)
#pragma warning(push)
#pragma warning(disable : 4201)  // nonstandard extension used: nameless struct/union
#endif
#include "miniaudio.h"
#if defined(_MSC_VER)
#pragma warning(pop)
#endif

#include "core/Realtime.h"

namespace drumas::engine {
namespace {

[[nodiscard]] std::string describe(ma_result result) {
    const char* text = ma_result_description(result);
    return text != nullptr ? std::string(text) : std::string("unknown miniaudio error");
}

/// Bridges miniaudio's C callback to the engine. Defined here, where the
/// miniaudio types are complete, so that the public header does not have to
/// expose them.
void onDeviceNotification(const ma_device_notification* notification) {
    if (notification == nullptr || notification->pDevice == nullptr) {
        return;
    }
    auto* engine = static_cast<AudioEngine*>(notification->pDevice->pUserData);
    if (engine == nullptr) {
        return;
    }
    switch (notification->type) {
        case ma_device_notification_type_stopped:
        case ma_device_notification_type_rerouted:
        case ma_device_notification_type_interruption_began:
            engine->noteDeviceEvent();
            break;
        default:
            break;
    }
}

}  // namespace

AudioEngine::AudioEngine() = default;

AudioEngine::~AudioEngine() {
    shutdown();
}

void AudioEngine::setError(std::string message) {
    lastError_ = std::move(message);
}

bool AudioEngine::initialize(const Config& config, core::IAudioProcessor& processor) {
    if (device_ != nullptr) {
        setError("initialize() called on an engine that already has a device");
        return false;
    }
    if (config.channels < 1 || config.channels > AudioGraph::kMaxChannels) {
        setError("unsupported channel count: " + std::to_string(config.channels));
        return false;
    }
    if (config.sampleRate == 0 || config.bufferFrames == 0) {
        setError("sampleRate and bufferFrames must be greater than zero");
        return false;
    }

    config_ = config;
    lastError_.clear();

    // The processor is prepared before the device exists: this is where every
    // allocation for the audio path happens.
    const core::AudioSpec spec{
        static_cast<double>(config.sampleRate),
        static_cast<int>(std::max(config.maxBlockFrames, config.bufferFrames)),
        static_cast<int>(config.channels)};
    graph_.prepare(spec);
    graph_.setProcessor(&processor);
    processor.prepare(spec);

    context_ = std::make_unique<ma_context>();
    ma_context_config contextConfig = ma_context_config_init();

    // WASAPI is the backend this product targets; try it explicitly first and
    // fall back to miniaudio's priority list only if the system refuses it.
    ma_backend backends[] = {ma_backend_wasapi};
    ma_result result = ma_context_init(backends, 1, &contextConfig, context_.get());
    if (result != MA_SUCCESS) {
        result = ma_context_init(nullptr, 0, &contextConfig, context_.get());
        if (result != MA_SUCCESS) {
            context_.reset();
            setError("audio backend initialisation failed: " + describe(result));
            return false;
        }
    }

    device_ = std::make_unique<ma_device>();
    ma_device_config deviceConfig = ma_device_config_init(ma_device_type_playback);
    deviceConfig.playback.format = ma_format_f32;
    deviceConfig.playback.channels = config.channels;
    deviceConfig.sampleRate = config.sampleRate;
    deviceConfig.periodSizeInFrames = config.bufferFrames;
    deviceConfig.dataCallback = &AudioEngine::dataCallback;
    deviceConfig.notificationCallback = &onDeviceNotification;
    deviceConfig.pUserData = this;

    result = ma_device_init(context_.get(), &deviceConfig, device_.get());
    if (result != MA_SUCCESS) {
        device_.reset();
        ma_context_uninit(context_.get());
        context_.reset();
        setError("audio device initialisation failed: " + describe(result));
        return false;
    }

    deviceEvents_.store(0, std::memory_order_relaxed);
    return true;
}

bool AudioEngine::start() {
    if (device_ == nullptr) {
        setError("start() called before initialize()");
        return false;
    }
    if (isRunning()) {
        return true;
    }
    const ma_result result = ma_device_start(device_.get());
    if (result != MA_SUCCESS) {
        setError("audio device start failed: " + describe(result));
        return false;
    }
    running_.store(true, std::memory_order_release);
    return true;
}

void AudioEngine::stop() noexcept {
    if (device_ == nullptr) {
        return;
    }
    if (isRunning()) {
        ma_device_stop(device_.get());
        running_.store(false, std::memory_order_release);
    }
    // The stream is stopped, so the audio thread is no longer in the callback:
    // resetting the processor here is safe and kills any release tail.
    graph_.reset();
}

void AudioEngine::shutdown() noexcept {
    stop();
    if (device_ != nullptr) {
        ma_device_uninit(device_.get());
        device_.reset();
    }
    if (context_ != nullptr) {
        ma_context_uninit(context_.get());
        context_.reset();
    }
}

AudioEngine::DeviceInfo AudioEngine::deviceInfo() const {
    DeviceInfo info;
    if (device_ == nullptr) {
        return info;
    }

    info.backend =
        device_->pContext != nullptr ? ma_get_backend_name(device_->pContext->backend) : "unknown";

    char name[MA_MAX_DEVICE_NAME_LENGTH + 1] = {};
    if (ma_device_get_name(device_.get(), ma_device_type_playback, name, sizeof(name), nullptr) ==
        MA_SUCCESS) {
        info.deviceName = name;
    }

    info.sampleRate = static_cast<double>(device_->sampleRate);
    info.channels = static_cast<int>(device_->playback.channels);
    info.reportedBufferFrames = static_cast<int>(device_->playback.internalPeriodSizeInFrames);
    if (info.sampleRate > 0.0) {
        info.estimatedLatencyMs =
            1000.0 * static_cast<double>(info.reportedBufferFrames) / info.sampleRate;
    }
    info.deviceEvents = deviceEvents_.load(std::memory_order_relaxed);
    return info;
}

void AudioEngine::dataCallback(struct ma_device* device, void* output, const void* /*input*/,
                               unsigned int frameCount) {
    auto* engine = device != nullptr ? static_cast<AudioEngine*>(device->pUserData) : nullptr;
    if (engine == nullptr || output == nullptr) {
        return;
    }

    // Denormals cost hundreds of cycles per sample once a tail decays towards
    // zero; flush them on this thread for the duration of the callback.
    // [[nodiscard]] on purpose: this is a named object, not a temporary.
    const core::ScopedDenormalGuard denormalGuard;

    engine->graph_.render(static_cast<float*>(output), static_cast<int>(device->playback.channels),
                          static_cast<int>(frameCount));
}

}  // namespace drumas::engine

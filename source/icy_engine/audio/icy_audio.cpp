#include <icy_engine/audio/icy_audio.hpp>
#include <icy_engine/core/icy_map.hpp>
#include "../../libs/portaudio/include/portaudio.h"
#if _DEBUG
#pragma comment(lib, "portaudiod.lib")
#else
#pragma comment(lib, "portaudio.lib")
#endif
using namespace icy;

static error_type make_audio_error(const PaError code) noexcept
{
    return error_type(code, icy::error_source_audio);
}

error_type icy::create_audio_stream(audio_stream& stream, const audio_device& device, const audio_buffer buffer) noexcept
{
    if (stream.m_buffer || device.channels == 0 || !buffer)
        return make_stdlib_error(std::errc::invalid_argument);

    ICY_ERROR(buffer.resample(device.sample, stream.m_buffer));

    PaStreamParameters params = {};
    params.device = device.uid;
    params.sampleFormat = paFloat32;
    params.suggestedLatency = std::chrono::duration_cast<std::chrono::nanoseconds>(
        device.sound_latency).count() / 1e9;
    params.channelCount = std::min(device.channels, buffer.channels());

    static auto input_proc = [](const void* input, void*, unsigned long frameCount,
        const PaStreamCallbackTimeInfo* timeInfo, PaStreamCallbackFlags flags, void* userData) -> int
    {
        const auto self = static_cast<audio_stream*>(userData);
        const auto ibuffer = static_cast<const float*>(input);
        self->m_time.store(timeInfo->currentTime, std::memory_order_release);
        self->m_buffer.append(const_array_view<float>(ibuffer, ibuffer + frameCount));
        return paContinue;
    };
    static auto output_proc = [](const void*, void* output, unsigned long frameCount,
        const PaStreamCallbackTimeInfo* timeInfo, PaStreamCallbackFlags, void* userData) -> int
    {
        const auto self = static_cast<audio_stream*>(userData);
        const auto obuffer = static_cast<float*>(output);
        self->m_time.store(timeInfo->currentTime, std::memory_order_release);
        self->m_offset += self->m_buffer.read(self->m_offset, obuffer, frameCount, self->m_channels);
        const auto result = self->m_offset == self->m_buffer.size() ? paComplete : paContinue;
        return result;
    };

    if (const auto error = Pa_Initialize())
        return make_audio_error(error);
    
    auto success = false;
    ICY_SCOPE_EXIT{ if (!success) Pa_Terminate(); };

    if (device.type == audio_device_type::input)
    {
        if (const auto error = Pa_OpenStream(&stream.m_stream, &params, nullptr, 
            device.sample, paFramesPerBufferUnspecified, paNoFlag, input_proc, &stream))
        {
            return make_audio_error(error);
        }
    }
    else
    {
        if (const auto error = Pa_OpenStream(&stream.m_stream, nullptr, &params, 
            device.sample, paFramesPerBufferUnspecified, paNoFlag, output_proc, &stream))
        {
            return make_audio_error(error);
        }
    }
    stream.m_channels = params.channelCount;
    success = true;
    return error_type();
}

audio_stream::~audio_stream() noexcept
{
    if (m_stream)
    {
        Pa_AbortStream(m_stream);
        Pa_Terminate();
    }
}
error_type audio_stream::enable(const bool value) noexcept
{
    if (!m_stream)
        return make_stdlib_error(std::errc::invalid_argument);

    const auto error = value ? Pa_StartStream(m_stream) : Pa_StopStream(m_stream);
    return error ? make_audio_error(error) : error_type();
}
error_type audio_stream::reset(const uint64_t offset) noexcept
{
    if (!m_stream || offset > m_buffer.size())
        return make_stdlib_error(std::errc::invalid_argument);

    const auto error = Pa_AbortStream(m_stream);
    m_offset = offset;
    return error ? make_audio_error(error) : error_type();
}

error_type icy::enum_audio_input_devices(array<audio_device>& vec) noexcept
{
    if (const auto error = Pa_Initialize())
        return make_audio_error(error);

    ICY_SCOPE_EXIT{ Pa_Terminate(); };

    const auto host_index = Pa_GetDefaultHostApi();
    auto host = Pa_GetHostApiInfo(host_index);
    if (!host)
        return make_audio_error(PaErrorCode::paHostApiNotFound);

    for (auto k = 0; k < host->deviceCount; ++k)
    {
        const auto device_index = Pa_HostApiDeviceIndexToDeviceIndex(host_index, k);
        const auto device_info = Pa_GetDeviceInfo(device_index);

        if (device_info->maxInputChannels == 0)
            continue;

        audio_device new_device;
        new_device.type = audio_device_type::input;
        new_device.uid = device_index;
        ICY_ERROR(copy(string_view(device_info->name, strlen(device_info->name)), new_device.name));
        new_device.sample = lround(device_info->defaultSampleRate);
        new_device.channels = device_info->maxInputChannels;
        new_device.rtime_latency = std::chrono::nanoseconds(lround(device_info->defaultLowInputLatency * 1e9));
        new_device.sound_latency = std::chrono::nanoseconds(lround(device_info->defaultHighInputLatency * 1e9));
        ICY_ERROR(vec.push_back(std::move(new_device)));
    }
    return error_type();
}
error_type icy::enum_audio_output_devices(array<audio_device>& vec) noexcept
{
    if (const auto error = Pa_Initialize())
        return make_audio_error(error);

    ICY_SCOPE_EXIT{ Pa_Terminate(); };

    const auto host_index = Pa_GetDefaultHostApi();
    auto host = Pa_GetHostApiInfo(host_index);
    if (!host)
        return make_audio_error(PaErrorCode::paHostApiNotFound);

    for (auto k = 0; k < host->deviceCount; ++k)
    {
        const auto device_index = Pa_HostApiDeviceIndexToDeviceIndex(host_index, k);
        const auto device_info = Pa_GetDeviceInfo(device_index);

        if (device_info->maxOutputChannels == 0)
            continue;

        audio_device new_device;
        new_device.type = audio_device_type::output;
        new_device.uid = device_index;
        ICY_ERROR(copy(string_view(device_info->name, strlen(device_info->name)), new_device.name));
        new_device.sample = lround(device_info->defaultSampleRate);
        new_device.channels = device_info->maxOutputChannels;
        new_device.rtime_latency = std::chrono::nanoseconds(lround(device_info->defaultLowOutputLatency * 1e9));
        new_device.sound_latency = std::chrono::nanoseconds(lround(device_info->defaultHighOutputLatency * 1e9));
        ICY_ERROR(vec.push_back(std::move(new_device)));
    }
    return error_type();
}

static error_type audio_error_to_string(const unsigned code, const string_view, string& str) noexcept
{
    return to_string(Pa_GetErrorText(code), str);
}
const error_source icy::error_source_audio = register_error_source("audio"_s, audio_error_to_string);

extern "C"
{
    void* PaUtil_ReallocateMemory(void* ptr, long size)
    {
        return icy::realloc(ptr, size);
    }
    void* PaUtil_AllocateMemory(long size)
    {
        if (auto ptr = icy::realloc(nullptr, size))
        {
            memset(ptr, 0, size);
            return ptr;
        }
        return nullptr;
    }
    void PaUtil_FreeMemory(void* block)
    {
        icy::realloc(block, 0);
    }
}
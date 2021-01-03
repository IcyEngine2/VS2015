#pragma once

#include <icy_engine/core/icy_array.hpp>
#include <icy_engine/core/icy_string.hpp>
#include <icy_engine/core/icy_string_view.hpp>

namespace icy
{
    extern const error_source error_source_audio;
    extern const error_source error_source_dsp;

    enum class audio_type : uint32_t
    {
        unknown,
        wav,
    };
    enum class audio_device_type
    {
        output,
        input,
    };
    struct audio_device
    {
        audio_device_type type = audio_device_type::output;
        uint32_t uid = 0;
        string name;
        uint32_t sample = 0;
        uint32_t channels = 0;
        duration_type rtime_latency;    //  Default latency value for interactive performance.
        duration_type sound_latency;    //  Default latency values for robust non-interactive applications (eg. playing sound files)
    };

    class audio_buffer
    {
    public:
        friend error_type copy(const audio_buffer& src, audio_buffer& dst) noexcept;
        struct data_type;
        static audio_type type_from_string(const string_view str) noexcept;
        static audio_type type_from_buffer(const const_array_view<uint8_t> buffer) noexcept;
        audio_buffer() noexcept = default;
        ~audio_buffer() noexcept;
        audio_buffer(const audio_buffer& rhs) noexcept;
        ICY_DEFAULT_COPY_ASSIGN(audio_buffer);
        error_type load(const const_array_view<uint8_t> buffer, const audio_type type = audio_type::unknown) noexcept
        {
            return load(&global_realloc, nullptr, buffer, type);
        }
        error_type load(const string_view file_name, const audio_type type = audio_type::unknown) noexcept
        {
            return load(&global_realloc, nullptr, file_name, type);
        }
        error_type load(heap& heap, const const_array_view<uint8_t> buffer, const audio_type type = audio_type::unknown) noexcept
        {
            return load(&heap_realloc, &heap, buffer, type);
        }
        error_type load(heap& heap, const string_view file_name, const audio_type type = audio_type::unknown) noexcept
        {
            return load(&heap_realloc, &heap, file_name, type);
        }
        error_type load(const realloc_func realloc, void* const user, const const_array_view<uint8_t> buffer, const audio_type type = audio_type::unknown) noexcept;
        error_type load(const realloc_func realloc, void* const user, const string_view file_name, const audio_type type = audio_type::unknown) noexcept;
        error_type save(const audio_type type, array<uint8_t>& buffer) const noexcept;
        error_type save(const string_view file, const audio_type type = audio_type::unknown) const noexcept;
        audio_type type() const noexcept
        {
            return m_type;
        }
        uint32_t channels() const noexcept;
        uint32_t sample() const noexcept;
        uint64_t size() const noexcept;
        uint64_t read(const uint64_t offset, float* const buffer, const uint64_t count, const uint32_t channels = 0) const noexcept;
        error_type append(const const_array_view<float> buffer) noexcept;
        error_type resample(const uint32_t sample, audio_buffer& output) const noexcept;
        explicit operator bool() const noexcept
        {
            return !!m_data;
        }
    private:
        audio_type m_type = audio_type::unknown;
        data_type* m_data = nullptr;
    };

    class audio_stream
    {
    public:
        friend error_type create_audio_stream(audio_stream& stream, const audio_device& input, const audio_buffer buffer) noexcept;
        audio_stream() noexcept = default;
        audio_stream(const audio_stream&) noexcept = delete;
        ~audio_stream() noexcept;
    public:
        duration_type time() const noexcept
        {
            return std::chrono::nanoseconds(lround(m_time.load() * 1e9));
        }
        error_type enable(const bool value) noexcept;
        error_type reset(const uint64_t offset = 0) noexcept;
        audio_buffer buffer() const noexcept
        {
            return m_buffer;
        }
    private:
        void* m_stream = nullptr;
        audio_buffer m_buffer;
        std::atomic<size_t> m_offset = 0;
        std::atomic<double> m_time = 0;
        uint32_t m_channels = 0;
    };
    error_type enum_audio_input_devices(array<audio_device>& vec) noexcept;
    error_type enum_audio_output_devices(array<audio_device>& vec) noexcept;
}
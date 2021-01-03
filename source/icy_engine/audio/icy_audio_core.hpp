#pragma once

#include <icy_engine/audio/icy_audio.hpp>

struct SpeexResamplerState_;
struct icy::audio_buffer::data_type
{
    struct write_stream
    {
        virtual error_type append(const uint8_t* const data, const size_t size) noexcept = 0;
    };
    data_type(const realloc_func realloc, void* const user) noexcept : realloc(realloc), user(user)
    {

    }
    virtual icy::error_type load(const icy::const_array_view<uint8_t> bytes) noexcept = 0;
    virtual icy::error_type save(const audio_buffer& src, icy::array<uint8_t>& buffer) const noexcept = 0;
    virtual uint64_t read(const uint64_t offset, float* const buffer, const uint64_t count) const noexcept = 0;
    std::atomic<uint32_t> ref = 1;
    icy::realloc_func realloc = nullptr;
    void* user = nullptr;
    uint32_t channels = 0;
    uint32_t sample = 0;
    uint64_t frames = 0;
    array_view<uint8_t> source;
    bool decoded = false;
};

extern icy::audio_buffer::data_type* make_audio_wav(const icy::realloc_func realloc, void* const user) noexcept;
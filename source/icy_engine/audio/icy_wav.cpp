#include "icy_audio_core.hpp"
#define DRWAV_IMPLEMENTATION
#include "drwav.h"

using namespace icy;

ICY_STATIC_NAMESPACE_BEG
class audio_wav : public audio_buffer::data_type
{
public:
    using data_type::data_type;
    error_type load(const const_array_view<uint8_t> bytes) noexcept override;
    error_type save(const audio_buffer& src, array<uint8_t>& buffer) const noexcept override;
    uint64_t read(const uint64_t offset, float* const buffer, const uint64_t count) const noexcept override;
    ~audio_wav() noexcept
    {
        drwav_uninit(&data);
        realloc(source.data(), 0, user);
    }
public:
    drwav data = {};
};
ICY_STATIC_NAMESPACE_END

static drwav_allocation_callbacks drwav_alloc(const audio_wav& self)
{
    drwav_allocation_callbacks callbacks = {};
    callbacks.onFree = [](void* ptr, void* user)
    {
        const auto self = static_cast<audio_wav*>(user);
        self->realloc(ptr, 0, self->user);
    };
    callbacks.onMalloc = [](size_t size, void* user)
    {
        const auto self = static_cast<audio_wav*>(user);
        return self->realloc(nullptr, size, self->user);
    };
    callbacks.onRealloc = [](void* ptr, size_t size, void* user)
    {
        const auto self = static_cast<audio_wav*>(user);
        return self->realloc(ptr, size, self->user);
    };
    callbacks.pUserData = const_cast<audio_wav*>(&self);
    return callbacks;
}

error_type audio_wav::load(const const_array_view<uint8_t> bytes) noexcept
{
    source = { static_cast<uint8_t*>(realloc(nullptr, bytes.size(), user)), bytes.size() };
    if (!source.data())
        return make_stdlib_error(std::errc::not_enough_memory);
    memcpy(source.data(), bytes.data(), source.size());

    auto alloc = drwav_alloc(*this);
    if (!drwav_init_memory(&data, source.data(), source.size(), &alloc))
        return make_stdlib_error(std::errc::not_enough_memory);

    channels = data.channels;
    sample = data.sampleRate;
    frames = data.totalPCMFrameCount;
    return error_type();
}
error_type audio_wav::save(const audio_buffer& src, array<uint8_t>& buffer) const noexcept
{
    if (data.channels || !src.channels())
        return make_stdlib_error(std::errc::invalid_argument);

    drwav_data_format fmt = {};
    fmt.bitsPerSample = CHAR_BIT * sizeof(int16_t);
    fmt.channels = src.channels();
    fmt.container = drwav_container_riff;
    fmt.format = DR_WAVE_FORMAT_PCM;
    fmt.sampleRate = src.sample();

    const auto len = drwav_target_write_size_bytes(&fmt, src.size());
    ICY_ERROR(buffer.reserve(len));

    const auto proc = [](void* user, const void* data, size_t size) -> size_t
    {
        const auto ptr = static_cast<const uint8_t*>(data);
        if (static_cast<array<uint8_t>*>(user)->append(ptr, ptr + size))
            return 0;
        return size;
    };
    auto alloc = drwav_alloc(*this);

    drwav wav;
    if (!drwav_init_write_sequential_pcm_frames(&wav, &fmt, src.size(), proc, &buffer, &alloc))
        return make_stdlib_error(std::errc::not_enough_memory);
    ICY_SCOPE_EXIT{ drwav_uninit(&wav); };

    float fbuf[0x4000];
    for (auto n = 0_z; n < src.size();)
    {
        const auto len = src.read(n, fbuf, std::size(fbuf) / src.channels(), src.channels());
        drwav_int16 ubuf[std::size(fbuf)];
        drwav_f32_to_s16(ubuf, fbuf, len * src.channels());
        drwav_write_pcm_frames(&wav, len, ubuf);
        n += len;
    }
    return error_type();
}

uint64_t audio_wav::read(const uint64_t offset, float* const buffer, uint64_t count) const noexcept
{
    drwav copy = data;
    drwav_seek_to_pcm_frame(&copy, offset);
    return drwav_read_pcm_frames_f32(&copy, count, buffer);
}

extern audio_buffer::data_type* make_audio_wav(const realloc_func realloc, void* const user) noexcept
{
    if (const auto ptr = realloc(nullptr, sizeof(audio_wav), user))
        return new (ptr) audio_wav(realloc, user);
    return nullptr;
}
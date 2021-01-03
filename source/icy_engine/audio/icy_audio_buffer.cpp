#include "icy_audio_core.hpp"
#include <icy_engine/core/icy_string.hpp>
#include <icy_engine/core/icy_file.hpp>
#include "../../libs/speexdsp/include/speex_resampler.h"
#if _DEBUG
#pragma comment(lib, "speexdspd.lib")
#else
#pragma comment(lib, "speexdsp.lib")
#endif

using namespace icy;

ICY_STATIC_NAMESPACE_BEG
static error_type make_dsp_error(const int code) noexcept
{
    return error_type(code, icy::error_source_dsp);
}
ICY_STATIC_NAMESPACE_END

static void release(audio_buffer::data_type*& data) noexcept
{
    if (data)
    {        
        const auto realloc = data->realloc;
        const auto user = data->user;
        allocator_type::destroy(data);
        realloc(data, 0, user);
        data = nullptr;
    }
}
static error_type make_audio_data(const realloc_func realloc, void* const user, const audio_type type, audio_buffer::data_type*& data) noexcept
{
    switch (type)
    {
    case audio_type::wav:
        data = make_audio_wav(realloc, user);
        break;
    default:
        return make_stdlib_error(std::errc::invalid_argument);
    }
    if (!data)
        return make_stdlib_error(std::errc::not_enough_memory);
    return error_type();
}

audio_buffer::audio_buffer(const audio_buffer& rhs) noexcept : m_data(rhs.m_data), m_type(rhs.m_type)
{
    if (m_data)
        m_data->ref.fetch_add(1, std::memory_order_release);
}
audio_buffer::~audio_buffer() noexcept
{
    if (m_data && m_data->ref.fetch_sub(1, std::memory_order_acq_rel) == 1)
        release(m_data);
}
audio_type audio_buffer::type_from_string(const string_view str) noexcept
{
    switch (hash(file_name(str).extension))
    {
    case ".wav"_hash:
        return audio_type::wav;
    default:
        return audio_type::unknown;
    }
}
audio_type audio_buffer::type_from_buffer(const_array_view<uint8_t> buffer) noexcept
{
    return audio_type::unknown;
}
error_type audio_buffer::load(const realloc_func realloc, void* const user, const const_array_view<uint8_t> buffer, audio_type type) noexcept
{
    if (type == audio_type::unknown)
        type = type_from_buffer(buffer);

    data_type* new_data = nullptr;
    ICY_ERROR(make_audio_data(realloc, user, type, new_data));
    ICY_SCOPE_EXIT{ release(new_data); };
    ICY_ERROR(new_data->load(buffer));

    m_type = type;
    std::swap(m_data, new_data);
    return {};
}
error_type audio_buffer::load(const realloc_func realloc, void* const user, const string_view file_name, audio_type type) noexcept
{
    if (type == audio_type::unknown)
        type = type_from_string(file_name);
    
    file file;
    ICY_ERROR(file.open(file_name, file_access::read, file_open::open_existing, file_share::read));
    const auto info = file.info();

    array<uint8_t> bytes;
    ICY_ERROR(bytes.resize(info.size));
    auto count = bytes.size();
    ICY_ERROR(file.read(bytes.data(), count));
    ICY_ERROR(bytes.resize(count));

    return load(realloc, user, bytes, type);
}
error_type audio_buffer::save(const audio_type type, array<uint8_t>& buffer) const noexcept
{
    if (!m_data || type == audio_type::unknown)
        return make_stdlib_error(std::errc::invalid_argument);

    audio_buffer tmp;
    ICY_ERROR(make_audio_data(m_data->realloc, m_data->user, tmp.m_type = type, tmp.m_data));
    ICY_ERROR(tmp.m_data->save(*this, buffer));
    return error_type();
}
error_type audio_buffer::save(const string_view filename, audio_type type) const noexcept
{
    if (!m_data)
        return make_stdlib_error(std::errc::invalid_argument);

    if (type == audio_type::unknown)
        type = type_from_string(filename);
    
    array<uint8_t> buffer(m_data->realloc, m_data->user);    
    ICY_ERROR(save(type, buffer));

    file file;
    ICY_ERROR(file.open(filename, file_access::write, file_open::create_always, file_share::none));
    ICY_ERROR(file.append(buffer.data(), buffer.size()));
    return error_type();
}

uint64_t audio_buffer::read(const uint64_t offset, float* const buffer, uint64_t count, uint32_t channels) const noexcept
{
    if (!m_data || offset > size())
        return 0;

    if (!channels)
        channels = m_data->channels;

    if (m_data->decoded)
    {
        const auto ptr = reinterpret_cast<const float*>(m_data->source.data());
        count = std::min(count, size() - offset);
        memcpy(buffer, ptr + offset * channels, count * sizeof(float) * channels);
        return count;
    }
    return m_data->read(offset, buffer, count);
}
uint32_t audio_buffer::channels() const noexcept
{
    return m_data ? m_data->channels : 0;
}
uint32_t audio_buffer::sample() const noexcept
{
    return m_data ? m_data->sample : 0;
}
uint64_t audio_buffer::size() const noexcept
{
    return m_data ? m_data->frames : 0;
}
error_type audio_buffer::append(const_array_view<float> buffer) noexcept
{
    return error_type();
}
error_type audio_buffer::resample(const uint32_t sample, audio_buffer& output) const noexcept
{
    if (!sample || !m_data || output.m_data)
        return make_stdlib_error(std::errc::invalid_argument);

    if (sample == m_data->sample)
    {
        output = *this;
        return error_type();
    }

    array<float> ibuffer(m_data->realloc, m_data->user);
    ICY_ERROR(ibuffer.resize(m_data->frames * m_data->channels));

    auto olen = uint32_t(m_data->frames * m_data->channels * sample / m_data->sample);
    auto obuffer = static_cast<float*>(m_data->realloc(nullptr, olen * sizeof(float), m_data->user));
    ICY_SCOPE_EXIT{ if (obuffer) m_data->realloc(obuffer, 0, m_data->user); };

    auto speex_realloc = [](void* ptr, spx_size_t len, void* user) -> void*
    {
        auto mem0 = !ptr;
        auto data = static_cast<audio_buffer::data_type*>(user);
        ptr = data->realloc(ptr, len, data->user);
        if (mem0 && ptr)
            memset(ptr, 0, len);
        return ptr;
    };

    auto sampler = speex_resampler_init(m_data->channels, m_data->sample,
        sample, SPEEX_RESAMPLER_QUALITY_MAX, nullptr, speex_realloc, m_data);
    if (!sampler)
        return make_stdlib_error(std::errc::not_enough_memory);
    ICY_SCOPE_EXIT{ speex_resampler_destroy(sampler); };

    auto ilen = uint32_t(m_data->read(0, ibuffer.data(), m_data->frames));
    speex_resampler_process_interleaved_float(sampler, ibuffer.data(), &ilen, obuffer, &olen);

    ICY_ERROR(make_audio_data(m_data->realloc, m_data->user, m_type, output.m_data));
    output.m_data->channels = channels();
    output.m_data->frames = olen / channels();
    output.m_data->sample = sample;
    output.m_data->source = array_view<uint8_t>(reinterpret_cast<uint8_t*>(obuffer), olen / sizeof(float));
    output.m_data->decoded = true;
    obuffer = nullptr;

    return error_type();
}

error_type icy::copy(const audio_buffer& src, audio_buffer& dst) noexcept
{
    auto data = src.m_data;
    if (!data)
    {
        dst = audio_buffer();
        return error_type();
    }
    audio_buffer buf;
    ICY_ERROR(buf.load(data->realloc, data->user, data->source, src.m_type));
    dst = std::move(buf);
    return error_type();
}

static error_type dsp_error_to_string(const unsigned code, const string_view, string& str) noexcept
{
    const auto ptr = speex_resampler_strerror(code);
    return to_string(string_view(ptr, ptr ? strlen(ptr) : 0), str);
}
const error_source icy::error_source_dsp = register_error_source("dsp"_s, dsp_error_to_string);
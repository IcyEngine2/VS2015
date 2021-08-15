#include "icy_image_core.hpp"
#include <icy_engine/core/icy_file.hpp>

using namespace icy;

static void release(image::data_type*& data) noexcept
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
static error_type make_image_data(const realloc_func realloc, void* const user, const image_type type, icy::image::data_type*& data) noexcept
{
    switch (type)
    {
    case image_type::png:
        data = make_image_png(realloc, user);
        break;
    case image_type::jpg:
        data = make_image_jpg(realloc, user);
        break;

    default:
        return make_stdlib_error(std::errc::invalid_argument);
    }
    if (!data)
        return make_stdlib_error(std::errc::not_enough_memory);

    return {};
}
image::~image() noexcept
{
    release(m_data);
}
image_type image::type_from_string(const string_view str) noexcept
{
    file_name file;
    file.initialize(str);
    switch (hash(file.extension))
    {
    case ".png"_hash:
        return image_type::png;
    case ".jpg"_hash:
        return image_type::jpg;
    default:
        return image_type::unknown;
    }
}
image_type image::type_from_buffer(const_array_view<uint8_t> buffer) noexcept
{
    return image_type::unknown;
}
image_size image::size() const noexcept
{
    return m_data ? m_data->size : image_size();
}
error_type image::save(const realloc_func realloc, void* const user, const const_matrix_view<color> colors, const image_type type, array<uint8_t>& buffer) noexcept
{
    data_type* new_data = nullptr;
    ICY_ERROR(make_image_data(realloc, user, type, new_data));
    ICY_SCOPE_EXIT{ release(new_data); };
    
    struct buffer_write : data_type::write_stream
    {
        error_type append(const uint8_t* const data, const size_t size) noexcept
        {
            return buffer->append(data, data + size);
        }
        array<uint8_t>* buffer = nullptr;
    } write;
    write.buffer = &buffer;
    ICY_ERROR(new_data->save(colors, write));
    return {};
}
error_type image::save(const realloc_func realloc, void* const user, const const_matrix_view<color> colors, const string_view file_name, image_type type)
{
    if (type == image_type::unknown)
        type = type_from_string(file_name);
    
    data_type* new_data = nullptr;
    ICY_ERROR(make_image_data(realloc, user, type, new_data));
    ICY_SCOPE_EXIT{ release(new_data); };

    struct file_write : data_type::write_stream
    {
        error_type append(const uint8_t* const data, const size_t size) noexcept
        {
            return output.append(data, size);
        }
        icy::file output;
    } write;
    ICY_ERROR(write.output.open(file_name, file_access::app, file_open::create_always, file_share::read));
    ICY_ERROR(new_data->save(colors, write));
    return {};
}
error_type image::load(const realloc_func realloc, void* const user, const const_array_view<uint8_t> buffer, image_type type) noexcept
{
    if (type == image_type::unknown)
        type = type_from_buffer(buffer);

    data_type* new_data = nullptr;
    ICY_ERROR(make_image_data(realloc, user, type, new_data));
    ICY_SCOPE_EXIT{ release(new_data); };
    ICY_ERROR(new_data->load(buffer));

    m_type = type;
    std::swap(m_data, new_data);
    return {};
}
error_type image::load(const realloc_func realloc, void* const user, const string_view file_name, image_type type) noexcept
{
    if (type == image_type::unknown)
    {
        ;
    }

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
error_type image::view(const image_size offset, const matrix_view<color> colors) noexcept
{
    if (!m_data ||
        offset.x + colors.cols() > size().x ||
        offset.y + colors.rows() > size().y)
        return make_stdlib_error(std::errc::invalid_argument);

    return m_data->view(offset, colors);
}

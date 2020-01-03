#include "icy_image_core.hpp"
#include <jpeg/jpeglib.h>
#if _DEBUG
#pragma comment(lib, "libjpegd")
#else
#pragma comment(lib, "libjpeg")
#endif

using namespace icy;

ICY_STATIC_NAMESPACE_BEG
class image_jpg : public image::data_type
{
public:
    using data_type::data_type;
    ~image_jpg() noexcept
    {
        heap.realloc(data, 0, heap.user);
    }
    error_type save(const const_matrix_view<color> colors, write_stream& output) noexcept override;
    error_type load(const const_array_view<uint8_t> bytes) noexcept override;
    error_type view(const image_size offset, matrix_view<color> colors) const noexcept override;
public:
    uint8_t* data = nullptr;
};
ICY_STATIC_NAMESPACE_END

void* jpeg_user_realloc(j_common_ptr ptr, void* object, size_t size)
{
    return static_cast<heap*>(ptr->client_data)->realloc(object, size);
}

static error_type jpg_make_error() noexcept
{
    if (const auto code = errno)
        return make_stdlib_error(std::errc(code));
    else
        return make_unexpected_error();
}

error_type image_jpg::load(const_array_view<uint8_t> bytes) noexcept
{
    jpeg_decompress_struct info = {};
    ICY_SCOPE_EXIT{ jpeg_destroy_decompress(&info); };
   
    jmp_buf jmp;
    if (setjmp(jmp))
        return jpg_make_error();

    jpeg_error_mgr err;    
    info.client_data = &heap;
    info.err = jpeg_std_error(&err);
    jpeg_create_decompress(&info);

    jpeg_source_mgr src;
    src.bytes_in_buffer = bytes.size();
    src.next_input_byte = bytes.data();
    src.init_source = [](j_decompress_ptr) {};
    src.fill_input_buffer = [](j_decompress_ptr) { return TRUE; };
    src.skip_input_data = [](j_decompress_ptr cinfo, long bytes)
    {
        auto src = static_cast<jpeg_source_mgr*>(cinfo->src);
        if (src && bytes > 0) 
        {
            src->next_input_byte += size_t(bytes);
            src->bytes_in_buffer -= size_t(bytes);
        }
    };
    src.resync_to_restart = jpeg_resync_to_restart;
    src.term_source = [](j_decompress_ptr) {};
    
    info.src = &src;
    if (!jpeg_read_header(&info, true))
        return make_stdlib_error(std::errc::illegal_byte_sequence);
    info.out_color_space = JCS_RGB;
    jpeg_start_decompress(&info);

    size = { info.output_width, info.output_height };
    data = static_cast<uint8_t*>(heap.realloc(data, size.x * size.y * 3, heap.user));
    if (!data)
        return make_stdlib_error(std::errc::not_enough_memory);
    
    for (auto k = 0u; k < size.y;)
    {
        auto row = data;
        row += info.output_scanline * size.x * 3;
        uint8_t* rows[8] = {};
        for (auto n = 0u; n < _countof(rows); ++n)
        {
            rows[n] = row;
            row += size.x * 3;
        }
        k += jpeg_read_scanlines(&info, rows, _countof(rows));
    }
    jpeg_finish_decompress(&info);
    return {};
}
error_type image_jpg::view(const image_size offset, matrix_view<color> colors) const noexcept
{
    const auto dx = size.x;
    for (auto row = offset.y; row < offset.y + colors.rows(); ++row)
    {
        const auto src = data + (row * dx + offset.x) * 3;
        const auto dst = colors.row(row - offset.y).data();
        for (auto k = 0u; k < colors.cols(); ++k)
        {
            dst[k].b = src[3 * k + 2];
            dst[k].g = src[3 * k + 1];
            dst[k].r = src[3 * k + 0];
            dst[k].a = 0xFF;
        }
    }
    return {};
}
error_type image_jpg::save(const const_matrix_view<color> colors, write_stream& output)  noexcept
{
    return {};
}

extern image::data_type* make_image_jpg(global_heap_type heap) noexcept
{
    if (const auto ptr = heap.realloc(nullptr, sizeof(image_jpg), heap.user))
        return new (ptr) image_jpg(heap);
    return nullptr;
}
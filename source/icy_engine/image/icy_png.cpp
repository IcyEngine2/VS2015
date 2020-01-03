#include "icy_image_core.hpp"
#include <png/png.h>
#if _DEBUG
#pragma comment(lib, "libpngd")
#pragma comment(lib, "zlibd")
#else
#pragma comment(lib, "libpng")
#pragma comment(lib, "zlib")
#endif

using namespace icy;

ICY_STATIC_NAMESPACE_BEG
class image_png : public image::data_type
{
public:
    using data_type::data_type;
    error_type save(const const_matrix_view<color> colors, write_stream& output) noexcept override;
    error_type load(const const_array_view<uint8_t> bytes) noexcept override;
    error_type view(const image_size offset, matrix_view<color> colors) const noexcept override;
public:
    array<uint8_t> data;
};
ICY_STATIC_NAMESPACE_END

static const auto png_sig_size = 0x08;
static void* png_malloc(png_structp png, const size_t size) noexcept
{
    return static_cast<heap*>(png_get_mem_ptr(png))->realloc(nullptr, size);
}
static void png_free(png_structp png, png_voidp ptr) noexcept
{
    static_cast<heap*>(png_get_mem_ptr(png))->realloc(ptr, 0);
}
static void png_error(png_structp png, const char* text)
{
    png_longjmp(png, 0);
}
static error_type png_make_error() noexcept
{
    if (const auto code = errno)
        return make_stdlib_error(std::errc(code));
    else
        return make_unexpected_error();
}
error_type image_png::load(const_array_view<uint8_t> bytes) noexcept
{
    png_struct_def* png = nullptr;
    png_info_def* info = nullptr;
    ICY_SCOPE_EXIT{ png_destroy_read_struct(&png, &info, nullptr); };

    if (bytes.size() < png_sig_size || png_sig_cmp(bytes.data(), 0, png_sig_size))
        return make_stdlib_error(std::errc::illegal_byte_sequence);
    
    errno = 0;
    png = png_create_read_struct_2(PNG_LIBPNG_VER_STRING, this, png_error, png_error, &heap, png_malloc, png_free);
    if (!png) 
        return png_make_error();
    
    info = png_create_info_struct(png);
    if (!info) 
        return png_make_error();

    array<uint8_t*> rows;
    if (setjmp(png_jmpbuf(png)))
        return png_make_error();

    const auto png_read = [](const png_structp png, const png_bytep data, const size_t size)
    {
        auto& bytes= *static_cast<const_array_view<uint8_t>*>(png_get_io_ptr(png));
        if (size > bytes.size())
            return;

        memcpy(data, bytes.data(), size);
        bytes = const_array_view<uint8_t>(bytes.data() + size, bytes.size() - size);
    };
    png_set_read_fn(png, &bytes, png_read);
    png_read_info(png, info);

    auto dx = 0u;
    auto dy = 0u;
    auto bits = 0;
    auto type = 0;
    png_get_IHDR(png, info, &dx, &dy, &bits, &type, nullptr, nullptr, nullptr);    
    png_set_bgr(png);
    png_set_strip_16(png);
    png_set_packing(png);
    png_set_gray_to_rgb(png);
    png_set_expand_gray_1_2_4_to_8(png);
    png_set_palette_to_rgb(png);
    png_set_filler(png, 0xFF, PNG_FILLER_AFTER);
    png_set_tRNS_to_alpha(png);
    png_read_update_info(png, info);

    ICY_ERROR(rows.resize(dy));
    ICY_ERROR(data.resize(dx * dy * sizeof(color)));    
    auto pointer = data.data();
    for (auto k = 0u; k < dy; ++k, pointer += dx * sizeof(color))
        rows[k] = pointer;

    png_read_image(png, rows.data());
    png_read_end(png, info);
    size = { dx, dy };
    return {};
}
error_type image_png::view(const image_size offset, matrix_view<color> colors) const noexcept
{
    const auto dx = size.x;
    for (auto row = offset.y; row < offset.y + colors.rows(); ++row)
    {
        const auto src = data.data() + (row * dx + offset.x) * sizeof(color);
        const auto dst = colors.row(row - offset.y).data();
        memcpy(dst, src, colors.cols() * sizeof(color));
    }
    return {};
}
error_type image_png::save(const const_matrix_view<color> colors, write_stream& output)  noexcept
{
    png_struct_def* png = nullptr;
    png_info_def* info = nullptr;
    ICY_SCOPE_EXIT{ png_destroy_write_struct(&png, &info); };

    errno = 0;
    png = png_create_write_struct_2(PNG_LIBPNG_VER_STRING, this, png_error, png_error, &heap, png_malloc, png_free);
    if (!png)
        return png_make_error();

    info = png_create_info_struct(png);
    if (!info)
        return png_make_error();

    using pair_type = std::pair<error_type, write_stream*>;
    pair_type pair;
    pair.second = &output;

    array<uint8_t*> rows;
    if (setjmp(png_jmpbuf(png)))
    {
        if (pair.first)
            return pair.first;
        else 
            return png_make_error();
    }

    const auto png_write = [](const png_structp png, const png_bytep data, const size_t size)
    {
        auto& pair = *static_cast<pair_type*>(png_get_io_ptr(png));
        if (const auto error = pair.second->append(data, size))
        {
            pair.first = error;
            png_longjmp(png, 0);
        }
    };
    png_set_write_fn(png, &pair, png_write, nullptr);
    png_set_bgr(png);
    png_set_IHDR(png, info, png_uint_32(colors.cols()), png_uint_32(colors.rows()), 8, PNG_COLOR_TYPE_RGBA,
        PNG_INTERLACE_NONE, PNG_COMPRESSION_TYPE_DEFAULT, PNG_NO_FILTERS);
    png_write_info(png, info);

    ICY_ERROR(rows.resize(colors.rows()));
    for (auto k = 0u; k < rows.size(); ++k)
        rows[k] = const_cast<uint8_t*>(reinterpret_cast<const uint8_t*>(colors.row(k).data()));

    png_write_image(png, rows.data());
    png_write_end(png, info);

    return {};
}

extern image::data_type* make_image_png(global_heap_type heap) noexcept
{
    if (const auto ptr = heap.realloc(nullptr, sizeof(image_png), heap.user))
        return new (ptr) image_png(heap);
    return nullptr;
}
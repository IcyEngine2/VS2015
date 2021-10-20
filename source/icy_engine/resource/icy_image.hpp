#pragma once

#include <icy_engine/core/icy_color.hpp>
#include <icy_engine/core/icy_matrix.hpp>

namespace icy
{
    enum class image_type : uint32_t
    {
        unknown,
        png,
        jpg,
    };
    struct image_size
    {
        image_size() noexcept = default;
        image_size(const uint32_t x, const uint32_t y) noexcept : x(x), y(y)
        {

        }
        uint32_t x = 0;
        uint32_t y = 0;
    };

    class image
    {
    public:
        struct data_type;
        static image_type type_from_string(const string_view str) noexcept;
        static image_type type_from_buffer(const const_array_view<uint8_t> buffer) noexcept;
        static error_type save(const realloc_func realloc, void* const user, const const_matrix_view<color> data, const image_type type, array<uint8_t>& buffer) noexcept;
        static error_type save(const realloc_func realloc, void* const user, const const_matrix_view<color> data, const string_view file, const image_type type = image_type::unknown);
        image() noexcept = default;
        ~image() noexcept;
        image(image&& rhs) noexcept : m_type(rhs.m_type), m_data(rhs.m_data)
        {

        }
        ICY_DEFAULT_MOVE_ASSIGN(image);        
        error_type load(const realloc_func realloc, void* const user, const const_array_view<uint8_t> buffer, const image_type type = image_type::unknown) noexcept;
        error_type load(const realloc_func realloc, void* const user, const string_view file_name, const image_type type = image_type::unknown) noexcept;
        error_type view(const image_size offset, matrix_view<color> data) noexcept;
        image_type type() const noexcept
        {
            return m_type;
        }
        image_size size() const noexcept;
    private:
        image_type m_type = image_type::unknown;
        data_type* m_data = nullptr;
    };
}
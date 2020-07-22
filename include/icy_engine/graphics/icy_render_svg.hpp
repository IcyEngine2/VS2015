#pragma once

#include <icy_engine/core/icy_array.hpp>
#include <icy_engine/core/icy_string.hpp>

namespace icy
{
    class render_svg_text_flag
    {
    public:
        enum _enum : uint32_t
        {
            none = 0,

            font_min_stretch = 0,
            font_max_stretch = 9,
            font_min_weight = 0,
            font_max_weight = 999,

            //  applied at string level (can be inline)

            font_size = 0,
            font_stretch,
            font_normal,
            font_italic,
            font_oblique,
            font_weight,
            strikethrough,
            underline,

            //  applied at paragraph level
            line_spacing,   //  0   (default)   -   non uniform
            par_align_near,
            par_align_far,
            par_align_center,
            trimming_none,
            trimming_word,
            trimming_char,
            align_leading,
            align_trailing,
            align_center,

            _total,
        };
        render_svg_text_flag(const _enum type = none, const uint32_t value = 0) noexcept : type(type), value(value)
        {

        }
        const _enum type;
        const uint32_t value;
    };
    class render_svg_font_data
    {
    public:
        array<render_svg_text_flag> flags;
        string name;
    };
    class render_svg_font
    {
    public:
        render_svg_font() noexcept = default;
        render_svg_font(const render_svg_font& rhs) noexcept;
        ICY_DEFAULT_COPY_ASSIGN(render_svg_font);
        ~render_svg_font() noexcept;
    public:
        class data_type;
        data_type* data = nullptr;
    };
    class render_svg_text
    {
    public:
        render_svg_text() noexcept = default;
        render_svg_text(const render_svg_text& rhs) noexcept;
        ICY_DEFAULT_COPY_ASSIGN(render_svg_text);
        ~render_svg_text() noexcept;
    public:
        class data_type;
        data_type* data = nullptr;
    };
    class render_svg_geometry
    {
    public:
        render_svg_geometry() noexcept = default;
        render_svg_geometry(const render_svg_geometry& rhs) noexcept;
        ICY_DEFAULT_COPY_ASSIGN(render_svg_geometry);
        ~render_svg_geometry() noexcept;
    public:
        //error_type tesselate(array<render_d2d_vertex>& vertices) noexcept;
    public:
        class data_type;
        data_type* data = nullptr;
    };
}
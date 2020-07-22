#pragma once

#include <icy_engine/core/icy_event.hpp>
#include <icy_engine/core/icy_array.hpp>
#include <icy_engine/core/icy_string.hpp>
#include <icy_engine/core/icy_color.hpp>
#include <icy_engine/core/icy_matrix.hpp>
#include "icy_render_math.hpp"

struct HWND__;

namespace icy
{
    class render_svg_geometry;
    class adapter;

    enum class render_flags : uint32_t
    {
        none = 0x00,
        msaa_x2 = 0x01,
        msaa_x4 = 0x02,
        msaa_x8 = 0x04,
        msaa_x16 = 0x08,
        msaa_hardware = 0x10,
        sRGB = 0x20,
    };
    inline constexpr bool operator&(const render_flags lhs, const render_flags rhs) noexcept
    {
        return !!(uint32_t(lhs) & uint32_t(rhs));
    }
    inline constexpr render_flags operator|(const render_flags lhs, const render_flags rhs) noexcept
    {
        return render_flags(uint32_t(lhs) | uint32_t(rhs));
    }
    inline uint32_t sample_count(const render_flags flag) noexcept
    {
        if (flag & render_flags::msaa_x16)
            return 16;
        else if (flag & render_flags::msaa_x8)
            return 8;
        else if (flag & render_flags::msaa_x4)
            return 4;
        else if (flag & render_flags::msaa_x2)
            return 2;
        else
            return 1;
    }

    struct render_camera
    {
        render_d3d_vector pos;
        render_d3d_vector dir;
        float min_z = 0;
        float max_z = 0;
        render_d2d_rectangle view;
    };
    struct render_frame
    {
        enum
        {
            time_render_cpu,
            time_render_gpu,
            time_display,
            _time_count,
        };
        size_t index = 0;
        window_size size;
        duration_type time[_time_count];
    };

    class render_texture;
    class render_svg_font_data;
    class render_svg_font;
    class render_svg_text;
    class render_svg_geometry;
    class render_commands_2d;
    class render_commands_3d;

    class render_factory
    {
    public:
        virtual ~render_factory() noexcept = 0 { }
        virtual render_flags flags() const noexcept = 0;
        virtual error_type enum_font_names(array<string>& fonts) const noexcept = 0;
        virtual error_type enum_font_sizes(array<uint32_t>& sizes) const noexcept = 0;
        virtual error_type create_svg_font(const render_svg_font_data& data, render_svg_font& font) const noexcept = 0;
        virtual error_type create_svg_text(const string_view str, const color color, const window_size size, const render_svg_font& font, icy::render_svg_text& text) const noexcept = 0;
        virtual error_type create_svg_geometry(const const_array_view<char> xml, render_svg_geometry& geometry) const noexcept = 0;
        virtual error_type open_commands_2d(render_commands_2d& commands) noexcept = 0;
        virtual error_type open_commands_3d(render_commands_3d& commands) noexcept = 0;
        virtual error_type create_texture(const const_array_view<uint8_t> bytes, render_texture& texture) const noexcept = 0;
        virtual error_type create_texture(const const_matrix_view<color> bytes, render_texture& texture) const noexcept = 0;
    };
    class render_texture
    {
    public:
        render_texture() noexcept = default;
        render_texture(const render_texture& rhs) noexcept;
        ICY_DEFAULT_COPY_ASSIGN(render_texture);
        ~render_texture() noexcept;
    public:
        window_size size() const noexcept;
        error_type save(const render_d2d_rectangle_u& rect, matrix_view<color> colors) const noexcept;
    public:
        class data_type;
        data_type* data = nullptr;
    };

    class render_commands_2d
    {
    public:
        render_commands_2d() noexcept = default;
        render_commands_2d(const render_commands_2d& rhs) noexcept;
        ICY_DEFAULT_COPY_ASSIGN(render_commands_2d);
        ~render_commands_2d() noexcept;
    public:
        error_type draw(const render_svg_geometry& svg, const render_d2d_matrix& transform = render_d2d_matrix(), const float layer = 0, const render_texture texture = render_texture()) noexcept;
        error_type draw(const render_svg_text& text, const render_d2d_matrix& transform = render_d2d_matrix(), const float layer = 0) noexcept;
        error_type close(const window_size size, render_texture& texture) noexcept;
    public:
        class data_type;
        data_type* data = nullptr;
    };
    class render_commands_3d
    {
    public:
        render_commands_3d() noexcept = default;
        render_commands_3d(const render_commands_3d& rhs) noexcept;
        ICY_DEFAULT_COPY_ASSIGN(render_commands_3d);
        ~render_commands_3d() noexcept;
    public:
        error_type clear(const color clear) noexcept;
        error_type camera(const render_camera& camera) noexcept;
        error_type draw(const render_d2d_rectangle rect, const render_texture texture) noexcept;
        error_type close(const window_size size, render_texture& texture) noexcept;
    public:
        class data_type;
        data_type* data = nullptr;
    };

    error_type create_render_factory(shared_ptr<render_factory>& system, const adapter& adapter, const render_flags flags) noexcept;
}

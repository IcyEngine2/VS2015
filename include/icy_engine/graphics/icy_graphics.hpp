#pragma once

#include <cmath>
#include <icy_engine/core/icy_event.hpp>
#include <icy_engine/core/icy_array.hpp>
#include <icy_engine/core/icy_string.hpp>
#include <icy_engine/core/icy_color.hpp>
#include <icy_engine/core/icy_matrix.hpp>

struct HWND__;

#pragma warning(push)
#pragma warning(disable:4201)

namespace icy
{
    struct render_d2d_vector
    {
        render_d2d_vector() noexcept : x(0), y(0)
        {

        }
        render_d2d_vector(const float x, const float y) noexcept : x(x), y(y)
        {

        }
        float x;
        float y;
    };
    struct render_d3d_vector
    {
        render_d3d_vector() noexcept : x(0), y(0), z(0)
        {

        }
        render_d3d_vector(const float x, const float y, const float z) noexcept : x(x), y(y), z(z)
        {

        }
        float x;
        float y;
        float z;
    };
    struct render_d4d_vector
    {
        render_d4d_vector() noexcept : x(0), y(0), z(0), w(0)
        {

        }
        render_d4d_vector(const float x, const float y, const float z, const float w) noexcept : x(x), y(y), z(z), w(w)
        {

        }
        float x;
        float y;
        float z;
        float w;
    };
    struct render_d2d_matrix
    {
        render_d2d_matrix() noexcept : array{ 1, 0, 0, 1, 0, 0 }
        {

        }
        render_d2d_matrix(const float x, const float y) noexcept : array{ 1, 0, 0, 1, x, y }
        {

        }
        render_d2d_matrix(const render_d2d_vector vec) noexcept : array{ 1, 0, 0, 1, vec.x, vec.y }
        {

        }
        static render_d2d_matrix rotate(const float angle) noexcept
        {
            const auto s = sin(angle);
            const auto c = cos(angle);
            render_d2d_matrix value;
            value._11 = c;
            value._22 = c;
            value._12 = +s;
            value._21 = -s;
            return value;
        }
        static render_d2d_matrix scale(const float sx, const float sy) noexcept
        {
            render_d2d_matrix value;
            value._11 = sx;
            value._22 = sy;
            return value;
        }
        render_d2d_matrix operator*(const render_d2d_matrix& rhs) const noexcept
        {
            const auto _33 = 1.0F;
            auto lhs = *this;
            lhs._11 = _11 * rhs._11 + _12 * rhs._21;
            lhs._12 = _11 * rhs._12 + _12 * rhs._22;
            lhs._21 = _21 * rhs._11 + _22 * rhs._21;
            lhs._22 = _21 * rhs._12 + _22 * rhs._22;
            lhs._31 = _31 * rhs._11 + _32 * rhs._21 + _33 * rhs._31;
            lhs._32 = _31 * rhs._12 + _32 * rhs._22 + _33 * rhs._32;
            return lhs;
        }
        render_d2d_matrix& operator*=(const render_d2d_matrix& rhs) noexcept
        {
            return *this = *this * rhs;
        }
        render_d2d_vector operator*(const render_d2d_vector& rhs) const noexcept
        {
            const auto rhs_z = 1.0F;
            return
            {
                _11 * rhs.x + _21 * rhs.y + _31 * rhs_z,
                _12 * rhs.x + _22 * rhs.y + _32 * rhs_z,
            };
        }
        union
        {
            struct
            {
                float _11;
                float _12;
                float _21;
                float _22;
                float _31;
                float _32;
            };
            float array[6];
        };
    };
    struct render_d2d_rectangle_u;
    struct render_d2d_rectangle
    {
        render_d2d_rectangle() noexcept : x(0), y(0), w(0), h(0)
        {

        }
        render_d2d_rectangle(const float x, const float y, const float w, const float h) noexcept : x(x), y(y), w(w), h(h)
        {

        }
        render_d2d_rectangle(const render_d2d_rectangle_u&) noexcept;
        float x;
        float y;
        float w;
        float h;
    };
    struct render_d2d_rectangle_u
    {
        render_d2d_rectangle_u() noexcept : x(0), y(0), w(0), h(0)
        {

        }
        render_d2d_rectangle_u(const uint32_t x, const uint32_t y, const uint32_t w, const uint32_t h) noexcept : x(x), y(y), w(w), h(h)
        {

        }
        uint32_t x;
        uint32_t y;
        uint32_t w;
        uint32_t h;
    };
    inline render_d2d_rectangle::render_d2d_rectangle(const render_d2d_rectangle_u& rhs) noexcept :
        x(float(rhs.x)), y(float(rhs.y)), w(float(rhs.w)), h(float(rhs.h))
    {

    }
    struct render_d2d_line
    {
        render_d2d_line() noexcept = default;
        render_d2d_line(const render_d2d_vector v0, const render_d2d_vector v1) noexcept : v0(v0), v1(v1)
        {

        }
        render_d2d_vector v0;
        render_d2d_vector v1;
    };
    struct render_d2d_triangle
    {
        render_d2d_triangle() noexcept : vec{}
        {

        }
        render_d2d_triangle(const render_d2d_vector v0, const render_d2d_vector v1, const render_d2d_vector v2) noexcept : v0(v0), v1(v1), v2(v2)
        {

        }
        union
        {
            struct
            {
                render_d2d_vector v0;
                render_d2d_vector v1;
                render_d2d_vector v2;
            };
            render_d2d_vector vec[3];
        };
    };
    struct render_box
    {
        render_d3d_vector center;
        render_d3d_vector axis[2];
        render_d3d_vector dimensions;
    };
    struct render_d2d_vertex
    {
        render_d3d_vector coord;	//	x,y (screen), z(layer 0..1)
        unsigned color;			    //	bgra
    };
}

#pragma warning(pop)

namespace icy
{
    class render_svg_geometry;
    class adapter;

    enum class adapter_flags : uint32_t
    {
        none        =   0x00,
        hardware    =   0x01,
        d3d10       =   0x02,
        d3d11       =   0x04,
        d3d12       =   0x08,
        debug       =   0x10,
    };
    enum class window_style : uint32_t
    {
        windowed    =   0x00,
        popup       =   0x01,
        maximized   =   0x02,
    };
    enum class window_flags : uint32_t
    {
        none            =   0x00,
        quit_on_close   =   0x01,
    };
    enum class display_flags : uint32_t
    {
        none            =   0x00,
        triple_buffer   =   0x01,
    };
    enum class render_flags : uint32_t
    {
        none            =   0x00,
        msaa_x2         =   0x01,
        msaa_x4         =   0x02,
        msaa_x8         =   0x04,
        msaa_x16        =   0x08,
        msaa_hardware   =   0x10,
        sRGB            =   0x20,
    };


    static constexpr auto default_window_flags = window_flags::quit_on_close;
    inline constexpr bool operator&(const adapter_flags lhs, const adapter_flags rhs) noexcept
    {
        return !!(uint32_t(lhs) & uint32_t(rhs));
    }
    inline constexpr adapter_flags operator|(const adapter_flags lhs, const adapter_flags rhs) noexcept
    {
        return adapter_flags(uint32_t(lhs) | uint32_t(rhs));
    }
    inline constexpr bool operator&(const window_flags lhs, const window_flags rhs) noexcept
    {
        return !!(uint32_t(lhs) & uint32_t(rhs));
    }
    inline constexpr window_flags operator|(const window_flags lhs, const window_flags rhs) noexcept
    {
        return window_flags(uint32_t(lhs) | uint32_t(rhs));
    }
    inline constexpr bool operator&(const window_style lhs, const window_style rhs) noexcept
    {
        return !!(uint32_t(lhs) & uint32_t(rhs));
    }
    inline constexpr window_style operator|(const window_style lhs, const window_style rhs) noexcept
    {
        return window_style(uint32_t(lhs) | uint32_t(rhs));
    }
    inline constexpr bool operator&(const render_flags lhs, const render_flags rhs) noexcept
    {
        return !!(uint32_t(lhs) & uint32_t(rhs));
    }
    inline constexpr render_flags operator|(const render_flags lhs, const render_flags rhs) noexcept
    {
        return render_flags(uint32_t(lhs) | uint32_t(rhs));
    }
    inline constexpr bool operator&(const display_flags lhs, const display_flags rhs) noexcept
    {
        return !!(uint32_t(lhs) & uint32_t(rhs));
    }
    inline constexpr display_flags operator|(const display_flags lhs, const display_flags rhs) noexcept
    {
        return display_flags(uint32_t(lhs) | uint32_t(rhs));
    }
    inline constexpr uint32_t buffer_count(const display_flags flag) noexcept
    {
        return (flag & display_flags::triple_buffer) ? 3 : 2;
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

    struct window_size
    {
        window_size() noexcept : x(0), y(0)
        {

        }
        window_size(const uint32_t x, const uint32_t y) noexcept : x(x), y(y)
        {

        }
        uint32_t x;
        uint32_t y;
    };
    struct window_info
    {
        string name;
        uint32_t pid = 0;
        HWND__* handle = nullptr;
    };
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
    struct display_options
    {
        duration_type vsync = std::chrono::seconds(-1);
        window_size size;
    };

    static const auto display_frame_vsync = max_timeout;
    static const auto display_frame_unlim = std::chrono::seconds(0);
    inline auto display_frame_fps(const size_t fps) noexcept
    {
        return fps ? std::chrono::nanoseconds(std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::seconds(1)).count() / fps) : max_timeout;
    }

    class adapter
    {
    public:
        static error_type enumerate(const adapter_flags filter, array<adapter>& array) noexcept;
        adapter() noexcept = default;
        adapter(const adapter& rhs) noexcept;
        ICY_DEFAULT_COPY_ASSIGN(adapter);
        ~adapter() noexcept;
    public:
        string_view name() const noexcept;
        error_type msaa(render_flags& flags) const noexcept;
        explicit operator bool() const noexcept
        {
            return !!data;
        }
        void* handle() const noexcept;
        size_t vid_mem() const noexcept;
        size_t sys_mem() const noexcept;
        size_t shr_mem() const noexcept;
        uint64_t luid() const noexcept;
        adapter_flags flags() const noexcept;
    public:
        class data_type;
        data_type* data = nullptr;
    };
    class render_texture;
    class window_system : public event_system
    {
    public:
        virtual error_type initialize() noexcept = 0;
        virtual error_type restyle(const window_style style) noexcept = 0;
        virtual error_type rename(const string_view name) noexcept = 0;
        virtual HWND__* handle() const noexcept = 0;
        virtual window_flags flags() const noexcept = 0;
    };
    class display_system : public event_system
    {
    public:
        virtual ~display_system() noexcept = 0 { }
        virtual error_type options(display_options data) noexcept = 0;
        virtual error_type render(render_texture& frame) noexcept = 0;
    };

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
        error_type tesselate(array<render_d2d_vertex>& vertices) noexcept;
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
    
    error_type create_event_system(shared_ptr<window_system>& window, const window_flags flags = default_window_flags) noexcept;
    error_type create_event_system(shared_ptr<display_system>& display, const adapter& adapter, HWND__* const hwnd, const display_flags flags) noexcept;
    error_type enum_windows(array<window_info>& vec) noexcept;
    error_type create_render_factory(shared_ptr<render_factory>& system, const adapter& adapter, const render_flags flags) noexcept;
}
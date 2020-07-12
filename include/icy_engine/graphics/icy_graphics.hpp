#pragma once

#include <icy_engine/core/icy_event.hpp>
#include <icy_engine/core/icy_array.hpp>
#include <icy_engine/core/icy_string.hpp>
#include <icy_engine/core/icy_color.hpp>
#include "icy_render_math.hpp"

struct HWND__;

namespace icy
{
    class render_svg_geometry;
    class adapter;

    enum class adapter_flags : uint32_t
    {
        none        =   0x00,
        hardware    =   0x01,
        d3d11       =   0x02,
        d3d12       =   0x04,
        debug       =   0x08,
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
        sRGB            =   0x02,
    };
    enum class render_flags : uint32_t
    {
        none            =   0x00,
        msaa_x2         =   0x01,
        msaa_x4         =   0x02,
        msaa_x8         =   0x04,
        msaa_x16        =   0x08,
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
        render_vector pos;
        render_vector dir;
        float min_z = 0;
        float max_z = 0;
        render_d2d_rectangle view;
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
        size_t vid_mem = 0;
        size_t sys_mem = 0;
        size_t shr_mem = 0;
        uint64_t luid = 0;
        adapter_flags flags = adapter_flags::none;
    public:
        class data_type;
        data_type* data = nullptr;
    };
    class render_frame
    {
    public:
        render_frame() noexcept = default;
        render_frame(const render_frame& rhs) noexcept;
        ICY_DEFAULT_COPY_ASSIGN(render_frame);
        ~render_frame() noexcept;
    public:
        size_t index() const noexcept;
        duration_type time_cpu() const noexcept;
        duration_type time_gpu() const noexcept;
    public:
        class data_type;
        data_type* data = nullptr;
    };
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
        virtual error_type vsync(const duration_type delta) noexcept = 0;
        virtual error_type resize(const window_size size) noexcept = 0;
        virtual error_type render(const render_frame frame) noexcept = 0;
    };
    class render_list
    {
    public:
        virtual ~render_list() noexcept = 0 { };
        virtual error_type clear(const color color) noexcept = 0;
        virtual error_type draw(const render_svg_geometry geometry, const render_d2d_matrix& offset, const float layer = 0) noexcept = 0;
    };
    class render_factory
    {
    public:
        virtual ~render_factory() noexcept = 0 { }
        virtual render_flags flags() const noexcept = 0;
        virtual error_type resize(const window_size size) noexcept = 0;
        virtual error_type exec(render_list& list, render_frame& frame) noexcept = 0;
    };

    error_type create_render_list(shared_ptr<render_list>& list) noexcept;
    error_type create_render_factory(shared_ptr<render_factory>& system, const adapter& adapter, const render_flags flags) noexcept;
    error_type create_event_system(shared_ptr<window_system>& window, const window_flags flags = default_window_flags) noexcept;
    error_type create_event_system(shared_ptr<display_system>& display, const adapter& adapter, HWND__* const hwnd, const display_flags flags) noexcept;
    error_type enum_windows(array<window_info>& vec) noexcept;
}
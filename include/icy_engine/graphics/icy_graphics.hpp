#pragma once

#include <icy_engine/core/icy_event.hpp>
#include <icy_engine/core/icy_array.hpp>
#include <icy_engine/core/icy_string.hpp>

struct HWND__;

namespace icy
{
    class render_list;
    class adapter;

    enum class adapter_flag : uint32_t
    {
        none = 0x00,
        hardware = 0x01,
        d3d11 = 0x02,
        d3d12 = 0x04,
    };
    enum class window_style : uint32_t
    {
        windowed    =   0x00,
        popup       =   0x01,
        maximized   =   0x02,
    };
    enum class window_flags : uint32_t
    {
        none,
        debug_layer     =   0x01,
        triple_buffer   =   0x02,
        sRGB            =   0x04,
        msaa_x2         =   0x08,
        msaa_x4         =   0x10,
        msaa_x8         =   0x20,
        msaa_x16        =   0x40,
        quit_on_close   =   0x80,
    };
    inline constexpr bool operator&(const window_flags lhs, const window_flags rhs) noexcept
    {
        return !!(uint32_t(lhs) & uint32_t(rhs));
    }
    inline constexpr window_flags operator|(const window_flags lhs, const window_flags rhs) noexcept
    {
        return window_flags(uint32_t(lhs) | uint32_t(rhs));
    }

    static constexpr auto default_window_flags =
        window_flags::quit_on_close |
#if _DEBUG
        window_flags::debug_layer;
#else
        window_flags::none;
#endif

    inline constexpr bool operator&(const adapter_flag lhs, const adapter_flag rhs) noexcept
    {
        return !!(uint32_t(lhs) & uint32_t(rhs));
    }
    inline constexpr adapter_flag operator|(const adapter_flag lhs, const adapter_flag rhs) noexcept
    {
        return adapter_flag(uint32_t(lhs) | uint32_t(rhs));
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
   
    class window_system : public event_system
    {
    public:
        virtual error_type initialize() noexcept = 0;
        virtual error_type restyle(const window_style style) noexcept = 0;
        virtual error_type rename(const string_view name) noexcept = 0;
        virtual HWND__* handle() const noexcept = 0;
        virtual size_t frame() const noexcept = 0;
        virtual window_flags flags() const noexcept = 0;
    };
    class adapter
    {
    public:
        static error_type enumerate(const adapter_flag filter, array<adapter>& array) noexcept;
        adapter() noexcept = default;
        adapter(const adapter& rhs) noexcept;
        ICY_DEFAULT_COPY_ASSIGN(adapter);
        ~adapter() noexcept;
    public:
        string_view name() const noexcept;
        error_type msaa(window_flags& flags) const noexcept;
        explicit operator bool() const noexcept
        {
            return !!data;
        }
        void* handle() const noexcept;
        size_t vid_mem = 0;
        size_t sys_mem = 0;
        size_t shr_mem = 0;
        uint64_t luid = 0;
        adapter_flag flags = adapter_flag::none;
    public:
        class data_type;
        data_type* data = nullptr;
    };
    error_type create_event_system(shared_ptr<window_system>& window, const adapter& adapter, const window_flags flags = default_window_flags) noexcept;
    

    inline bool operator&(const window_style lhs, const window_style rhs) noexcept
    {
        return !!(uint32_t(lhs) & uint32_t(rhs));
    }
    inline window_style operator|(const window_style lhs, const window_style rhs) noexcept
    {
        return window_style(uint32_t(lhs) | uint32_t(rhs));
    }

    struct window_info
    {
        icy::string name;
        uint32_t pid = 0;
        HWND__* handle = nullptr;
    };
    error_type enum_windows(array<window_info>& vec) noexcept;

    inline constexpr uint32_t buffer_count(const window_flags flag) noexcept
    {
        return (flag & window_flags::triple_buffer) ? 3 : 2;
    }
    inline uint32_t sample_count(const window_flags flag) noexcept
    {
        if (flag & window_flags::msaa_x16)
            return 16;
        else if (flag & window_flags::msaa_x8)
            return 8;
        else if (flag & window_flags::msaa_x4)
            return 4;
        else if (flag & window_flags::msaa_x2)
            return 2;
        else
            return 1;
    }
}
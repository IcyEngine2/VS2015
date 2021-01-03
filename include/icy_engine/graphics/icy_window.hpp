#pragma once

#include <icy_engine/core/icy_event.hpp>
#include <icy_engine/core/icy_input.hpp>
#include <icy_engine/core/icy_array.hpp>

struct HWND__;

namespace icy
{
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
        layered         =   0x02,
    };

    static constexpr auto default_window_flags = window_flags::quit_on_close;

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
    class window
    {
    public:
        window() noexcept = default;
        window(const window&) noexcept;
        ICY_DEFAULT_COPY_ASSIGN(window);
        ~window() noexcept;
    public:
        error_type restyle(const window_style style) noexcept;
        error_type rename(const string_view name) noexcept;
        error_type show(const bool value) noexcept;
        HWND__* handle() const noexcept;
        window_flags flags() const noexcept;
    public:
        class data_type;
        data_type* data = nullptr;
    };

    class thread;

    enum class window_state : uint32_t
    {
        none        =   0x00,
        closing     =   0x01,
        minimized   =   0x02,
        inactive    =   0x04,
    };
    inline window_state operator|(const window_state lhs, const window_state rhs) noexcept
    {
        return window_state(uint32_t(lhs) | uint32_t(rhs));
    }

    struct window_message
    {
        uint32_t index = 0u;
        void* handle = nullptr;
        window_state state = window_state::none;
        window_size size;
        struct
        {
            uint64_t user;
            array<uint8_t> bytes;
        } data = {};
        input_message input;
    };
    class window_system : public event_system
    {
    public:
        virtual const icy::thread& thread() const noexcept = 0;
        virtual icy::thread& thread() noexcept
        {
            return const_cast<icy::thread&>(static_cast<const window_system*>(this)->thread());
        }
        virtual error_type create(icy::window& window, const window_flags flags = default_window_flags) const noexcept = 0;        
    };

    error_type create_window_system(shared_ptr<window_system>& system) noexcept;
}
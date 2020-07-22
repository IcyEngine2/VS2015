#pragma once

#include <icy_engine/core/icy_event.hpp>

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

    class window_system : public event_system
    {
    public:
        virtual error_type initialize() noexcept = 0;
        virtual error_type restyle(const window_style style) noexcept = 0;
        virtual error_type rename(const string_view name) noexcept = 0;
        virtual HWND__* handle() const noexcept = 0;
        virtual window_flags flags() const noexcept = 0;
    };

    error_type create_event_system(shared_ptr<window_system>& window, const window_flags flags = default_window_flags) noexcept;

}
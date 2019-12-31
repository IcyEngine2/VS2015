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
        none,
        quit_on_close,
    };
    class window : public event_queue
    {
    public:
        static error_type create(shared_ptr<window>& window, const window_flags flags = window_flags::none) noexcept;
        virtual error_type loop(const duration_type timeout = max_timeout) noexcept = 0;
        virtual error_type restyle(const window_style style) noexcept = 0;
        virtual error_type rename(const string_view name) noexcept = 0;
        virtual error_type _close() noexcept = 0;
        virtual HWND__* handle() const noexcept = 0;
    };

    inline bool operator&(const window_style lhs, const window_style rhs) noexcept
    {
        return !!(uint32_t(lhs) & uint32_t(rhs));
    }
    inline window_style operator|(const window_style lhs, const window_style rhs) noexcept
    {
        return window_style(uint32_t(lhs) | uint32_t(rhs));
    }
    
    error_type find_window(const string_view name, HWND__*& handle) noexcept;
}
#pragma once

#include <icy_engine/graphics/icy_window.hpp>
#include <icy_engine/graphics/icy_graphics.hpp>

class window_data : public icy::window
{
public:
    icy::error_type loop(const icy::duration_type timeout) noexcept override;
    icy::error_type restyle(const icy::window_style style) noexcept override;
    icy::error_type rename(const icy::string_view name) noexcept override;
    icy::error_type _close() noexcept override
    {
        return post(this, icy::event_type::window_close, true);
    }
    HWND__* handle() const noexcept
    {
        return m_hwnd;
    }
    icy::error_type signal(const icy::event_data& event) noexcept override;
    icy::display* display = nullptr;
public:
    static long long __stdcall proc(HWND__*, uint32_t, unsigned long long, long long) noexcept;
    icy::error_type initialize(const icy::window_flags flags) noexcept;
    ~window_data() noexcept override;
private:
    icy::library m_library = icy::library("user32");
    HWND__* m_hwnd = nullptr;
    icy::error_type m_error;
    uint32_t m_win32_flags = 0;
    icy::window_flags m_flags = icy::window_flags::none;
    wchar_t m_cname[64] = {};
};
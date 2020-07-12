#pragma once

#include <icy_engine/graphics/icy_graphics.hpp>
#include <icy_engine/core/icy_queue.hpp>

class window_data : public icy::window_system
{
    friend icy::error_type icy::create_event_system(icy::shared_ptr<icy::window_system>& window, const icy::window_flags flags) noexcept;
public:
    window_data(const icy::window_flags flags, void* const event) noexcept : m_flags(flags), m_event(event)
    {
        
    }
    ~window_data() noexcept override;
    void shutdown() noexcept;
private:
    icy::error_type initialize() noexcept override;
    icy::error_type restyle(const icy::window_style style) noexcept override;
    icy::error_type rename(const icy::string_view name) noexcept override;
    icy::error_type exec() noexcept override;
    icy::error_type exec(icy::event& event, const icy::duration_type timeout) noexcept;
    HWND__* handle() const noexcept
    {
        return m_hwnd;
    }
    icy::error_type signal(const icy::event_data& event) noexcept override;
    icy::window_flags flags() const noexcept override
    {
        return m_flags;
    }
    static long long __stdcall proc(HWND__*, uint32_t, unsigned long long, long long) noexcept;
private:
    const icy::window_flags m_flags;
    void* const m_event = nullptr;
    icy::library m_library = icy::library("user32");
    HWND__* m_hwnd = nullptr;
    icy::error_type m_error;
    uint32_t m_win32_flags = 0;
    wchar_t m_cname[64] = {};
};
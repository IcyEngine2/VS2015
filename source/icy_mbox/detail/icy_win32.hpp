#pragma once

#include <icy_engine/core/icy_array.hpp>
#include <icy_engine/core/icy_input.hpp>

struct HWND__;

class window_hook
{
public:
    ~window_hook() noexcept
    {
        shutdown();
    }
    void shutdown() noexcept;
    icy::error_type initialize(HWND__* handle) noexcept;
    icy::error_type send(const icy::const_array_view<icy::input_message> msg) noexcept;
    icy::error_type rename(const icy::string_view name) noexcept;
    virtual icy::error_type callback(const icy::input_message& msg, bool& cancel) noexcept = 0;
private:
    static long long __stdcall proc(HWND__*, uint32_t, unsigned long long, long long) noexcept;    
private:
    HWND__* m_handle = nullptr;
    icy::detail::rw_spin_lock m_lock;
    icy::array<icy::input_message> m_queue;
    icy::array<wchar_t> m_name;
    icy::error_type m_error;
};
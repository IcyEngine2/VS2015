#include "icy_win32.hpp"
#define WIN32_LEAN_AND_MEAN 1
#include <Windows.h>
#pragma comment(lib, "user32.lib")
using namespace icy;

static window_hook* g_instance = nullptr;
static WNDPROC g_wndproc = nullptr;

void window_hook::shutdown() noexcept
{
    g_instance = nullptr;
    if (m_handle)
    {
        SetWindowLongPtrW(m_handle, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(g_wndproc));
        m_handle = nullptr;
    }
}

error_type window_hook::initialize(HWND__* hwnd) noexcept
{
    shutdown();
    g_wndproc = reinterpret_cast<WNDPROC>(SetWindowLongPtrW(hwnd, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(&proc)));
    m_handle = hwnd;
    uint8_t keys[256];
    GetKeyboardState(keys);
    for (auto k = 0u; k < m_keyboard.size(); ++k)
        m_keyboard[k] = (keys[k] & 0b1000'0000) != 0;
    g_instance = this;
    return {};
}

LRESULT __stdcall window_hook::proc(HWND__* hwnd, uint32_t msg, WPARAM wparam, LPARAM lparam) noexcept
{
    if (!g_wndproc)
        return DefWindowProcW(hwnd, msg, wparam, lparam);
    else if (!g_instance)
        return g_wndproc(hwnd, msg, wparam, lparam);

    switch (msg)
    {
    case WM_NULL:
    {
        ICY_LOCK_GUARD(g_instance->m_lock);
        if (!g_instance->m_queue.empty())
        {
            auto instance = g_instance;
            g_instance = nullptr;
            for (auto&& input : g_instance->m_queue)
            {
                auto next = detail::to_winapi(input);
                SendMessageW(hwnd, next.message, next.wParam, next.lParam);
            }
            g_instance = instance;
            g_instance->m_queue.clear();
        }
        return 0;
    }

    case WM_KEYDOWN:
    case WM_KEYUP:
    case WM_SYSKEYDOWN:
    case WM_SYSKEYUP:
    {
        key_message key_input;
        auto count = detail::from_winapi(key_input, wparam, lparam, g_instance->m_keyboard);
        g_instance->m_keyboard[uint32_t(key_input.key)] = key_input.event == key_event::press;
        auto cancel = false;
        if (const auto error = g_instance->callback(key_input, cancel))
            g_instance->m_error = error;
        else if (cancel)
            return 0;
        break;
    }

    case WM_LBUTTONDOWN:
    case WM_RBUTTONDOWN:
    case WM_MBUTTONDOWN:
    case WM_XBUTTONDOWN:
    case WM_LBUTTONUP:
    case WM_RBUTTONUP:
    case WM_MBUTTONUP:
    case WM_XBUTTONUP:
    case WM_LBUTTONDBLCLK:
    case WM_RBUTTONDBLCLK:
    case WM_MBUTTONDBLCLK:
    case WM_XBUTTONDBLCLK:
    {
        mouse_message mouse_input;
        detail::from_winapi(mouse_input, 0, 0, msg, wparam, lparam, g_instance->m_keyboard);
        auto cancel = false;
        if (const auto error = g_instance->callback(mouse_input, cancel))
            g_instance->m_error = error;
        else if (cancel)
            return 0;
        break;
    }
    }
    return g_wndproc(hwnd, msg, wparam, lparam);
}
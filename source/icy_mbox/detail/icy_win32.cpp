#include "icy_win32.hpp"
#include <icy_engine/core/icy_string.hpp>
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

static std::bitset<256> key_state() noexcept
{
    uint8_t keyboard[0x100];
    GetKeyboardState(keyboard);
    std::bitset<256> keys;
    const auto mask = 0x80;
    keys[uint32_t(key::left_ctrl)] = (keyboard[VK_LCONTROL] & mask) != 0;
    keys[uint32_t(key::left_alt)] = (keyboard[VK_LMENU] & mask) != 0;
    keys[uint32_t(key::left_shift)] = (keyboard[VK_LSHIFT] & mask) != 0;
    keys[uint32_t(key::right_ctrl)] = (keyboard[VK_RCONTROL] & mask) != 0;
    keys[uint32_t(key::right_alt)] = (keyboard[VK_RMENU] & mask) != 0;
    keys[uint32_t(key::right_shift)] = (keyboard[VK_RSHIFT] & mask) != 0;
    return keys;
}

error_type window_hook::initialize(HWND__* hwnd) noexcept
{
    shutdown();

    if (!AllowSetForegroundWindow(GetCurrentProcessId()))
        return last_system_error();

    g_wndproc = reinterpret_cast<WNDPROC>(SetWindowLongPtrW(hwnd, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(&proc)));
    m_handle = hwnd;
    g_instance = this;
    return {};
}
error_type window_hook::send(const const_array_view<input_message> vec) noexcept
{
    ICY_LOCK_GUARD_WRITE(m_lock);
    for (auto&& msg : vec)
    {
        ICY_ERROR(m_queue.push_back(msg));
    }
    if (!PostMessageW(m_handle, WM_NULL, 0, 0))
        return last_system_error();
    return {};
}
error_type window_hook::rename(const icy::string_view name) noexcept
{
    ICY_ERROR(to_utf16(name, m_name));
    PostMessageW(m_handle, WM_NULL, 0, 0);
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
        ICY_LOCK_GUARD_WRITE(g_instance->m_lock);
        auto instance = g_instance;
        g_instance = nullptr;
        if (!instance->m_queue.empty())
        {
            auto has_mouse = false;
            for (auto&& input : instance->m_queue)
                has_mouse |= input.type == input_type::mouse;

            /*if (has_mouse)
            {
                SetForegroundWindow(hwnd);
                SetFocus(hwnd);
                SetActiveWindow(hwnd);
            }*/
            
            for (auto&& input : instance->m_queue)
            {
                if (input.type == input_type::key)
                {
                    auto next = detail::to_winapi(input);
                    SendMessageW(hwnd, next.message, next.wParam, next.lParam);
                }
                else if (input.type == input_type::mouse)
                {
                    auto mouse = input.mouse;
                    //if (mouse.event != mouse_event::btn_press)
                    //    continue;
                    
                    auto move = mouse;
                    auto press = mouse;
                    auto release = mouse;
                    move.delta = {};
                    move.event = mouse_event::move;
                    press.event = mouse_event::btn_press;
                    release.event = mouse_event::btn_release;
                    //SetCursorPos(mouse.point.x, mouse.point.y);
                    auto msg_move = detail::to_winapi(move);
                    auto msg_press = detail::to_winapi(press);
                    auto msg_release = detail::to_winapi(release);
                    SendMessageW(hwnd, msg_move.message, msg_move.wParam, msg_move.lParam);
                    SendMessageW(hwnd, msg_press.message, msg_press.wParam, msg_press.lParam);
                    SendMessageW(hwnd, msg_release.message, msg_release.wParam, msg_release.lParam);                    
                }
                else if (input.type == input_type::active && input.active)
                {
                    SetForegroundWindow(hwnd);
                    SetFocus(hwnd);
                    SetActiveWindow(hwnd);
                }
            }

            //if (has_mouse)
             //   SetFocus(focus);

            instance->m_queue.clear();
        }
        if (!instance->m_name.empty())
        {
            SetWindowTextW(hwnd, instance->m_name.data());
            instance->m_name.clear();
        }
        g_instance = instance;
        return 0;
    }

    case WM_CHAR:
    case WM_UNICHAR:
    {
        auto cancel = false;
        if (const auto error = g_instance->callback(input_message(L""), cancel))
            g_instance->m_error = error;
        if (cancel)
            return 0;
        break;
    }

    case WM_ACTIVATE:
    {
        const auto activate = LOWORD(wparam) != WA_INACTIVE;
        auto cancel = false;
        if (const auto error = g_instance->callback(input_message(activate), cancel))
            g_instance->m_error = error;
        break;
    }


    case WM_KEYDOWN:
    case WM_KEYUP:
    case WM_SYSKEYDOWN:
    case WM_SYSKEYUP:
    {
        key_message key_input;
        auto count = detail::from_winapi(key_input, wparam, lparam, key_state());
        auto cancel = false;
        if (const auto error = g_instance->callback(key_input, cancel))
            g_instance->m_error = error;
        if (cancel)
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
        detail::from_winapi(mouse_input, 0, 0, msg, wparam, lparam, key_state());
        auto cancel = false;
        if (const auto error = g_instance->callback(mouse_input, cancel))
            g_instance->m_error = error;
        if (cancel)
            return 0;
        break;
    }
    }
    return g_wndproc(hwnd, msg, wparam, lparam);
}
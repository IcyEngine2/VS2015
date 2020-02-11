#include "icy_win32.hpp"
#include <icy_engine/core/icy_string.hpp>
#include <icy_engine/utility/icy_minhook.hpp>
#define WIN32_LEAN_AND_MEAN 1
#include <Windows.h>
#pragma comment(lib, "user32.lib")
using namespace icy;

using get_message = int(__stdcall*)(tagMSG*, HWND__*, uint32_t, uint32_t);

static icy::mutex g_lock;
static window_callback* g_instance = nullptr;
static WNDPROC g_wndproc = nullptr;
static void(*g_callback)() = nullptr;
static hook<get_message> g_hook;

void window_callback::shutdown() noexcept
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

HWND__* window_callback::find() noexcept
{
    const auto proc = [](HWND hwnd, LPARAM param)
    {
        auto process = 0ul;
        GetWindowThreadProcessId(hwnd, &process);
        RECT rect;
        if (process == GetCurrentProcessId() && IsWindowVisible(hwnd) && GetWindow(hwnd, GW_OWNER) == 0)
        {
            *reinterpret_cast<HWND__**>(param) = hwnd;
            return FALSE;
        }
        return TRUE;
    };
    HWND handle = nullptr;
    EnumWindows(proc, reinterpret_cast<LPARAM>(&handle));
    return handle;
}
error_type window_callback::initialize(HWND__* hwnd) noexcept
{
    shutdown();

    if (!AllowSetForegroundWindow(GetCurrentProcessId()))
        return last_system_error();

    ICY_ERROR(g_lock.initialize());
    g_wndproc = reinterpret_cast<WNDPROC>(SetWindowLongPtrW(hwnd, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(&proc)));
    m_handle = hwnd;
    g_instance = this;
    return {};
}
error_type window_callback::send(const const_array_view<input_message> vec) noexcept
{
    ICY_LOCK_GUARD(g_lock);
    for (auto&& msg : vec)
    {
        ICY_ERROR(m_queue.push_back(msg));
    }
    if (!PostMessageW(m_handle, WM_NULL, 0, 0))
        return last_system_error();
    return {};
}
error_type window_callback::rename(const icy::string_view name) noexcept
{
    ICY_LOCK_GUARD(g_lock);
    ICY_ERROR(to_utf16(name, m_name));
    PostMessageW(m_handle, WM_NULL, 0, 0);
    return {};
}
error_type window_callback::hook(void(*callback)()) noexcept
{
    const auto func = [](tagMSG* msg, HWND__* hwnd, unsigned lower, unsigned upper)
    {
        auto value = g_hook(msg, hwnd, lower, upper);
        if (msg && msg->hwnd == hwnd && g_instance)
        {
            if (!g_instance->m_queue.empty())
            {
                auto has_mouse = false;
                for (auto&& input : g_instance->m_queue)
                    has_mouse |= input.type == input_type::mouse;
                
                for (auto&& input : g_instance->m_queue)
                {
                    if (input.type == input_type::key)
                    {
                        auto next = detail::to_winapi(input);
                        SendMessageW(hwnd, next.message, next.wParam, next.lParam);
                    }
                    else if (input.type == input_type::mouse)
                    {
                        if (input.mouse.event == mouse_event::btn_press ||
                            input.mouse.event == mouse_event::btn_release)
                            ;
                        else
                            continue;

                        RECT rect;
                        if (!GetClientRect(hwnd, &rect))
                            continue;

                        if (input.mouse.point.x < 0 || input.mouse.point.y < 0 ||
                            input.mouse.point.x >= 1 || input.mouse.point.y >= 1)
                        {
                            POINT point;
                            if (!GetCursorPos(&point) || !ScreenToClient(hwnd, &point))
                                continue;
                            input.mouse.point.x = point.x;
                            input.mouse.point.y = point.y;
                        }
                        else
                        {
                            input.mouse.point.x = (rect.right - rect.left) * input.mouse.point.x;
                            input.mouse.point.y = (rect.bottom - rect.top) * input.mouse.point.y;
                        }

                        auto move = input.mouse;
                        move.event = mouse_event::move;
                        move.delta = {};
                        auto msg_move = detail::to_winapi(move);
                        SendMessageW(hwnd, msg_move.message, msg_move.wParam, msg_move.lParam);

                        auto msg_mouse = detail::to_winapi(input.mouse);
                        SendMessageW(hwnd, msg_mouse.message, msg_mouse.wParam, msg_mouse.lParam);
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

                g_instance->m_queue.clear();
            }
            if (!g_instance->m_name.empty())
            {
                SetWindowTextW(hwnd, g_instance->m_name.data());
                g_instance->m_name.clear();
            }
        }
        return value;
    };
    return g_hook.initialize(&GetMessageW, func);
}

LRESULT __stdcall window_callback::proc(HWND__* hwnd, uint32_t msg, WPARAM wparam, LPARAM lparam) noexcept
{
    if (!g_wndproc)
        return DefWindowProcW(hwnd, msg, wparam, lparam);
    
    if (!g_instance)
        return g_wndproc(hwnd, msg, wparam, lparam);

    if (g_callback)
    {
        g_callback();
        g_callback = nullptr;
    }

    switch (msg)
    {
    case WM_NULL:
    {
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
                    if (input.mouse.event == mouse_event::btn_press ||
                        input.mouse.event == mouse_event::btn_release)
                        ;
                    else
                        continue;

                    RECT rect;
                    if (!GetClientRect(hwnd, &rect))
                        continue;

                    if (input.mouse.point.x < 0 || input.mouse.point.y < 0 || 
                        input.mouse.point.x >= 1 || input.mouse.point.y >= 1)
                    {
                        POINT point;
                        if (!GetCursorPos(&point) || !ScreenToClient(hwnd, &point))
                            continue;
                        input.mouse.point.x = point.x;
                        input.mouse.point.y = point.y;
                    }
                    else
                    {
                        input.mouse.point.x = (rect.right - rect.left) * input.mouse.point.x;
                        input.mouse.point.y = (rect.bottom - rect.top) * input.mouse.point.y;
                    }

                    auto move = input.mouse;
                    move.event = mouse_event::move;
                    move.delta = {};
                    auto msg_move = detail::to_winapi(move);
                    SendMessageW(hwnd, msg_move.message, msg_move.wParam, msg_move.lParam);
                    
                    auto msg_mouse = detail::to_winapi(input.mouse);
                    SendMessageW(hwnd, msg_mouse.message, msg_mouse.wParam, msg_mouse.lParam);
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
        RECT rect;
        GetClientRect(hwnd, &rect);
        mouse_input.point.x /= (rect.right - rect.left);
        mouse_input.point.y /= (rect.bottom - rect.top);

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
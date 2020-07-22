#include "icy_window.hpp"
#include <icy_engine/core/icy_string.hpp>
#include <icy_engine/core/icy_array.hpp>
#include <icy_engine/core/icy_input.hpp>
#include <Windows.h>

using namespace icy;

#define ICY_WIN32_FUNC(X, Y) X = ICY_FIND_FUNC(m_library, Y); if (!X) return make_stdlib_error(std::errc::function_not_supported) 

ICY_STATIC_NAMESPACE_BEG
enum class window_message_type : uint32_t
{
    none,
    restyle,
    rename,
    close,
};
struct window_message
{
    window_message_type type = window_message_type::none;
    wchar_t name[64] = {};
    input_message input;
    window_style style = window_style::windowed;
    duration_type frame;
};
static decltype(&::TranslateMessage) win32_translate_message;
static decltype(&::DispatchMessageW) win32_dispatch_message;
static decltype(&::GetKeyboardState) win32_get_keyboard_state;
static decltype(&::PeekMessageW) win32_peek_message;
static decltype(&::PostMessageW) win32_post_message;
static decltype(&::DefWindowProcW) win32_def_window_proc;
static decltype(&::PostQuitMessage) win32_post_quit_message;
static decltype(&::DestroyWindow) win32_destroy_window;
static decltype(&::GetWindowPlacement) win32_get_window_placement;
static decltype(&::SetWindowPlacement) win32_set_window_placement;
static decltype(&::MsgWaitForMultipleObjectsEx) win32_msg_wait_for_multiple_objects_ex;
static decltype(&::SetWindowTextW) win32_set_window_text;
static decltype(&::GetWindowTextW) win32_get_window_text;
static decltype(&::GetWindowTextLengthW) win32_get_window_text_length;
static decltype(&::GetClientRect) win32_get_client_rect;
static decltype(&::GetWindowLongPtrW) win32_get_window_longptr;
static decltype(&::SetWindowLongPtrW) win32_set_window_longptr;
static decltype(&::UnregisterClassW) win32_unregister_class;
static decltype(&::MonitorFromWindow) win32_monitor_from_window;
static decltype(&::GetMonitorInfoW) win32_get_monitor_info;
struct win32_flag_enum
{
    enum : uint32_t
    {
        none        =   0x00,
        resizing1   =   0x01,
        resizing2   =   0x02,
        moving      =   0x04,
        //fullscreen  =   0x02,
        minimized   =   0x08,
        initialized =   0x10,
        //closing     =   0x10,
        //resized     =   0x10,
    };
};
using win32_flag = decltype(win32_flag_enum::none);
static const auto win32_normal_style = WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX | WS_MAXIMIZEBOX | WS_THICKFRAME;
static const auto win32_popup_style = WS_POPUP;
static std::bitset<256> key_state() noexcept
{
    uint8_t keyboard[0x100];
    win32_get_keyboard_state(keyboard);
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
ICY_STATIC_NAMESPACE_END
static error_type win32_name(const HWND hwnd, string& str) noexcept
{
    const auto length = win32_get_window_text_length(hwnd);
    if (length < 0)
    {
        return last_system_error();
    }
    else if (length == 0)
    {
        str.clear();
        return error_type();
    }

    array<wchar_t> buffer;
    ICY_ERROR(buffer.resize(size_t(length) + 1));
    win32_get_window_text(hwnd, buffer.data(), length + 1);
    ICY_ERROR(to_string(buffer, str));
    return error_type();
}

window_data::~window_data() noexcept
{
    filter(0);
    shutdown();
    if (m_event)
        CloseHandle(m_event);
}
icy::error_type window_data::signal(const icy::event_data& event) noexcept
{
    if (!SetEvent(m_event))
        return last_system_error();
    return error_type();
}
void window_data::shutdown() noexcept
{
    if (m_hwnd && win32_set_window_longptr && win32_destroy_window)
    {
        win32_set_window_longptr(m_hwnd, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(win32_def_window_proc));
        win32_destroy_window(m_hwnd);    
        m_hwnd = nullptr;
    }
    if (*m_cname)
    {
        win32_unregister_class(m_cname, win32_instance());
        memset(m_cname, 0, sizeof(m_cname));
    }
    m_win32_flags = m_win32_flags & ~win32_flag::initialized;
}
error_type window_data::initialize() noexcept
{
    shutdown();
    ICY_ERROR(m_library.initialize());
    
    if (const auto func = ICY_FIND_FUNC(m_library, SetThreadDpiAwarenessContext))
        func(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);

    decltype(&::RegisterClassExW) win32_register_class = nullptr;
    decltype(&::CreateWindowExW) win32_create_window = nullptr;
    decltype(&::LoadCursorW) win32_load_cursor = nullptr;

    ICY_WIN32_FUNC(win32_translate_message, TranslateMessage);
    ICY_WIN32_FUNC(win32_dispatch_message, DispatchMessageW);
    ICY_WIN32_FUNC(win32_get_keyboard_state, GetKeyboardState);
    ICY_WIN32_FUNC(win32_peek_message, PeekMessageW);
    ICY_WIN32_FUNC(win32_post_message, PostMessageW);
    ICY_WIN32_FUNC(win32_def_window_proc, DefWindowProcW);
    ICY_WIN32_FUNC(win32_post_quit_message, PostQuitMessage);
    ICY_WIN32_FUNC(win32_unregister_class, UnregisterClassW);
    ICY_WIN32_FUNC(win32_register_class, RegisterClassExW);
    ICY_WIN32_FUNC(win32_create_window, CreateWindowExW);
    ICY_WIN32_FUNC(win32_destroy_window, DestroyWindow);
    ICY_WIN32_FUNC(win32_get_window_placement, GetWindowPlacement);
    ICY_WIN32_FUNC(win32_set_window_placement, SetWindowPlacement);
    ICY_WIN32_FUNC(win32_msg_wait_for_multiple_objects_ex, MsgWaitForMultipleObjectsEx);
    ICY_WIN32_FUNC(win32_set_window_text, SetWindowTextW);
    ICY_WIN32_FUNC(win32_get_window_text, GetWindowTextW);
    ICY_WIN32_FUNC(win32_get_window_text_length, GetWindowTextLengthW);
    ICY_WIN32_FUNC(win32_get_client_rect, GetClientRect);
    ICY_WIN32_FUNC(win32_get_window_longptr, GetWindowLongPtrW);
    ICY_WIN32_FUNC(win32_set_window_longptr, SetWindowLongPtrW);
    ICY_WIN32_FUNC(win32_monitor_from_window, MonitorFromWindow);
    ICY_WIN32_FUNC(win32_get_monitor_info, GetMonitorInfoW);
    ICY_WIN32_FUNC(win32_load_cursor, LoadCursorW);

    swprintf_s(m_cname, L"Icy Window %u.%u", GetCurrentProcessId(), GetCurrentThreadId());
    
    WNDCLASSEXW cls = { sizeof(WNDCLASSEXW) };
	cls.lpszClassName = m_cname;
	cls.style = CS_HREDRAW | CS_VREDRAW | CS_DBLCLKS;
	cls.lpfnWndProc = win32_def_window_proc;
	cls.hInstance = win32_instance();
    cls.hCursor = win32_load_cursor(nullptr, IDC_ARROW);
	
	if (win32_register_class(&cls) == 0)
	    return last_system_error();
	
    m_hwnd = win32_create_window(0, cls.lpszClassName, nullptr, win32_normal_style,
		CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT,
		nullptr, nullptr, cls.hInstance, nullptr);	
	if (!m_hwnd)
		return last_system_error();
    
    win32_set_window_longptr(m_hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(this));
    win32_set_window_longptr(m_hwnd, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(proc));
    
    filter(event_type::system_internal);
    m_win32_flags |= win32_flag::initialized;
    return error_type();
}
LRESULT WINAPI window_data::proc(const HWND hwnd, const UINT msg, const WPARAM wparam, const LPARAM lparam) noexcept
{
    const auto window = reinterpret_cast<window_data*>(win32_get_window_longptr(hwnd, GWLP_USERDATA));
    const auto on_resize = [window, hwnd]
    {
        RECT rect = {};
        if (!win32_get_client_rect(hwnd, &rect))
            return last_system_error();

        auto size = window_size{ uint32_t(rect.right - rect.left), uint32_t(rect.bottom - rect.top) };
        return event::post(window, event_type::window_resize, size);
    };
    
    switch (msg)
    {
    case WM_PAINT:
        break;

    case WM_MENUCHAR:
        return MAKELRESULT(0, MNC_CLOSE);

    case WM_SETCURSOR:
        //window->g_cursor.wnd_proc({ hwnd, msg, wparam, lparam });
        //if (LOWORD(lparam) == HTCLIENT)
        //    return TRUE;
        break;

    case WM_SYSCOMMAND:
        if ((wparam & 0xFFF0) == SC_KEYMENU)
            return 0;
        break;

    case WM_CLOSE:
        //  internal
        window->m_error = event::post(window, event_type::window_close, false);
        if (window->m_flags & window_flags::quit_on_close)
        {
            event::post(nullptr, event_type::global_quit);
            //win32_post_quit_message(0);
        }
        return 0;

    case WM_ENTERSIZEMOVE:
        window->m_win32_flags |= win32_flag::resizing1;
        break;

    case WM_SIZING:
        window->m_win32_flags |= win32_flag::resizing2;
        break;

    case WM_EXITSIZEMOVE:
        if ((window->m_win32_flags & win32_flag::resizing1) && (window->m_win32_flags & win32_flag::resizing2))
            window->m_error = on_resize();
        window->m_win32_flags &= ~(win32_flag::resizing1 | win32_flag::resizing2);
        break;

    case WM_SIZE:
    {
        if (wparam == SIZE_MINIMIZED)
        {
            window->m_win32_flags |= win32_flag::minimized;
            window->m_error = event::post(window, event_type::window_minimized, true);
            break;
        }
        else if (window->m_win32_flags & win32_flag::minimized)
        {
            window->m_win32_flags &= ~win32_flag::minimized;
            window->m_error = event::post(window, event_type::window_minimized, false);
        }
        else if (wparam == SIZE_MAXIMIZED || (wparam == SIZE_RESTORED &&
            (window->m_win32_flags & win32_flag::resizing1) == 0 && (window->m_win32_flags & win32_flag::resizing2) == 0))
        {
            window->m_error = on_resize();
        }
        break;
    }
    case WM_ACTIVATE:
    {
        const auto active = LOWORD(wparam);
        window->m_error = event::post(window, event_type::window_active, active != WA_INACTIVE);
        break;
    }

    case WM_KEYDOWN:
    case WM_KEYUP:
    case WM_SYSKEYDOWN:
    case WM_SYSKEYUP:
    {
        input_message key_input;
        auto count = detail::from_winapi(key_input, wparam, lparam, key_state());
        for (auto k = 0u; k < count; ++k)
        {
            if (const auto error = event::post(window, event_type::window_input, key_input))
            {
                window->m_error = error;
                break;
            }
        }
        break;
    }
    case WM_MOUSEMOVE:
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
        input_message mouse_input;
        detail::from_winapi(mouse_input, 0, 0, msg, wparam, lparam, key_state());
        window->m_error = event::post(window, event_type::window_input, mouse_input);
        break;
    }
    }
    return win32_def_window_proc(hwnd, msg, wparam, lparam);
};
error_type window_data::exec() noexcept
{
    ICY_ERROR(restyle(window_style::windowed));

    while (true)
    {
        event event;
        ICY_ERROR(exec(event, max_timeout));
        if (event->type == event_type::global_quit)
            break;

        if (event->type == event_type::window_close &&
            shared_ptr<event_system>(event->source).get() == this)
            break;
    }
    return error_type();
}
error_type window_data::exec(event& event, const duration_type timeout) noexcept
{
    if (timeout.count() < 0)
        return make_stdlib_error(std::errc::invalid_argument);

    if (!(m_win32_flags & win32_flag::initialized))
        return make_stdlib_error(std::errc::invalid_argument);

    const auto ms = ms_timeout(timeout);
    while (true)
    {
        if (event = pop())
        {
            if (event->type != event_type::system_internal)
                break;

            auto& event_data = event->data<window_message>();
            if (shared_ptr<event_system>(event->source).get() == this)
            {
                if (event_data.type == window_message_type::rename)
                {
                    if (!win32_set_window_text(m_hwnd, event_data.name))
                        return last_system_error();
                }
                else if (event_data.type == window_message_type::restyle)
                {
                    auto win32_style = (event_data.style & window_style::popup) ? win32_popup_style : win32_normal_style;
                    WINDOWPLACEMENT place{ sizeof(place) };
                    if (!win32_get_window_placement(m_hwnd, &place))
                        return last_system_error();

                    place.showCmd = SW_SHOWNORMAL;
                    if (event_data.style & window_style::maximized)
                    {
                        if (event_data.style & window_style::popup)
                        {
                            const auto hmon = win32_monitor_from_window(m_hwnd, MONITOR_DEFAULTTONEAREST);
                            MONITORINFO info{ sizeof(info) };
                            if (!hmon || !win32_get_monitor_info(hmon, &info))
                                return last_system_error();
                            place.rcNormalPosition = info.rcMonitor;
                        }
                        else
                            place.showCmd = SW_MAXIMIZE;
                    }
                    else if (event_data.style & window_style::popup)
                        win32_style |= WS_VISIBLE;

                    win32_set_window_longptr(m_hwnd, GWL_STYLE, win32_style);
                    if (!win32_set_window_placement(m_hwnd, &place))
                        return last_system_error();
                }
            }
            continue;
        }

        const auto index = win32_msg_wait_for_multiple_objects_ex(1, &m_event, ms, QS_ALLINPUT, MWMO_ALERTABLE);

        void* handle = index == WAIT_OBJECT_0 ? m_event : nullptr;

        if (handle == m_event)
        {
            continue;
        }
        else if (index == WAIT_OBJECT_0 + 1)
        {
            MSG win32_msg = {};
            while (win32_peek_message(&win32_msg, m_hwnd, 0, 0, PM_REMOVE))
            {
                win32_translate_message(&win32_msg);
                win32_dispatch_message(&win32_msg);
            }
            ICY_ERROR(m_error);
        }
        else if (index == WAIT_TIMEOUT)
        {
            return make_stdlib_error(std::errc::timed_out);
        }
        else //if (index == WAIT_FAILED)
        {
            return last_system_error();
        }
	}
	return{};
}
error_type window_data::restyle(const window_style style) noexcept
{
    window_message msg;
    msg.type = window_message_type::restyle;
    new (&msg.style) window_style(style);
    return event_system::post(this, event_type::system_internal, std::move(msg));
}
error_type window_data::rename(const string_view name) noexcept
{
    window_message msg;
    msg.type = window_message_type::rename;
    auto size = _countof(msg.name);
    ICY_ERROR(name.to_utf16(msg.name, &size));
    return event_system::post(this, event_type::system_internal, std::move(msg));
}
error_type icy::create_event_system(shared_ptr<window_system>& window, const window_flags flags) noexcept
{
    auto event = CreateEventW(nullptr, FALSE, FALSE, nullptr);
    if (!event)
        return last_system_error();
    ICY_SCOPE_EXIT{ if (event) CloseHandle(event); };

    shared_ptr<window_data> new_ptr;
    ICY_ERROR(make_shared(new_ptr, flags, event));
    event = nullptr;
    ICY_ERROR(new_ptr->initialize());
    window = new_ptr;
    return error_type();
}
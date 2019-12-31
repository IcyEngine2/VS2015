#define ICY_WINDOW_INTERNAL 1
#include <icy_engine/graphics/icy_window.hpp>
#include <icy_engine/graphics/icy_graphics.hpp>
#include <icy_engine/core/icy_string.hpp>
#include <icy_engine/core/icy_array.hpp>
#include <Windows.h>

using namespace icy;

#define ICY_WIN32_FUNC(X, Y) X = ICY_FIND_FUNC(m_library, Y); if (!X) return make_stdlib_error(std::errc::function_not_supported) 

ICY_STATIC_NAMESPACE_BEG
static decltype(&::TranslateMessage) win32_translate_message;
static decltype(&::DispatchMessageW) win32_dispatch_message;
static decltype(&::PeekMessageW) win32_peek_message;
static decltype(&::PostMessageW) win32_post_message;
static decltype(&::DefWindowProcW) win32_def_window_proc;
static decltype(&::PostQuitMessage) win32_post_quit_message;
static decltype(&::DestroyWindow) win32_destroy_window;
static decltype(&::GetWindowPlacement) win32_get_window_placement;
static decltype(&::SetWindowPlacement) win32_set_window_placement;
static decltype(&::MsgWaitForMultipleObjects) win32_msg_wait_for_multiple_objects_ex;
static decltype(&::SetWindowTextW) win32_set_window_text;
static decltype(&::GetWindowTextW) win32_get_window_text;
static decltype(&::GetWindowTextLengthW) win32_get_window_text_length;
static decltype(&::GetClientRect) win32_get_client_rect;
static decltype(&::GetWindowLongPtrW) win32_get_window_longptr;
static decltype(&::SetWindowLongPtrW) win32_set_window_longptr;
//static decltype(&::EnumWindows) win32_enum_windows;
static decltype(&::UnregisterClassW) win32_unregister_class;
static decltype(&::MonitorFromWindow) win32_monitor_from_window;
static decltype(&::GetMonitorInfoW) win32_get_monitor_info;


struct win32_find_window
{
    string_view name;
    HWND hwnd = nullptr;
    error_type error;
};
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
class window_data : public window
{
public:
    error_type loop(const duration_type timeout) noexcept override;
    error_type restyle(const window_style style) noexcept override;
    error_type rename(const string_view name) noexcept override;
    error_type _close() noexcept override
    {
        return post(this, event_type::window_close, true);
    }
    HWND__* handle() const noexcept
    {
        return m_hwnd;
    }
    error_type signal(const event_data& event) noexcept override
    {
        if (!win32_post_message(m_hwnd, WM_USER, event.type, 0))
            return last_system_error();
        return {};
    }
public:
    static LRESULT WINAPI proc(const HWND hwnd, const UINT msg, const WPARAM wparam, const LPARAM lparam) noexcept;
    error_type initialize(const window_flags flags) noexcept;
    ~window_data() noexcept;
private:
    library m_library = "user32"_lib;
    HWND m_hwnd = nullptr;
    error_type m_error;
    uint32_t m_win32_flags = 0;
    window_flags m_flags = window_flags::none;
    wchar_t m_cname[64] = {};
};
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
        return {};
    }

    array<wchar_t> buffer;
    ICY_ERROR(buffer.resize(size_t(length) + 1));
    win32_get_window_text(hwnd, buffer.data(), length + 1);
    ICY_ERROR(to_string(buffer, str));
    return {};
}
window_data::~window_data() noexcept
{
    if (m_hwnd && (m_win32_flags & win32_flag::initialized))
    {
        win32_set_window_longptr(m_hwnd, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(win32_def_window_proc));
        win32_destroy_window(m_hwnd);    
    }
    if (*m_cname)
        win32_unregister_class(m_cname, win32_instance());
}
error_type window_data::initialize(const window_flags flags) noexcept
{
    ICY_ERROR(m_library.initialize());
    m_flags = flags;

    if (const auto func = ICY_FIND_FUNC(m_library, SetThreadDpiAwarenessContext))
        func(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);

    decltype(&::RegisterClassExW) win32_register_class = nullptr;
    decltype(&::CreateWindowExW) win32_create_window = nullptr;
    decltype(&::LoadCursorW) win32_load_cursor = nullptr;

    ICY_WIN32_FUNC(win32_translate_message, TranslateMessage);
    ICY_WIN32_FUNC(win32_dispatch_message, DispatchMessageW);
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
    ICY_WIN32_FUNC(win32_msg_wait_for_multiple_objects_ex, MsgWaitForMultipleObjects);
    ICY_WIN32_FUNC(win32_set_window_text, SetWindowTextW);
    ICY_WIN32_FUNC(win32_get_window_text, GetWindowTextW);
    ICY_WIN32_FUNC(win32_get_window_text_length, GetWindowTextLengthW);
    ICY_WIN32_FUNC(win32_get_client_rect, GetClientRect);
    ICY_WIN32_FUNC(win32_get_window_longptr, GetWindowLongPtrW);
    ICY_WIN32_FUNC(win32_set_window_longptr, SetWindowLongPtrW);
    ICY_WIN32_FUNC(win32_monitor_from_window, MonitorFromWindow);
    ICY_WIN32_FUNC(win32_get_monitor_info, GetMonitorInfoW);
    ICY_WIN32_FUNC(win32_load_cursor, LoadCursorW);

	swprintf_s(m_cname, L"IcyWindow %lld", clock_type::now().time_since_epoch().count());

	auto cls = WNDCLASSEXW{ sizeof(WNDCLASSEXW) };
	cls.lpszClassName = m_cname;
	cls.style = CS_HREDRAW | CS_VREDRAW | CS_DBLCLKS;
	cls.lpfnWndProc = win32_def_window_proc;
	cls.hInstance = win32_instance();
    cls.hCursor = win32_load_cursor(nullptr, IDC_ARROW);
	
	if (win32_register_class(&cls) == 0)
	{
		*m_cname = 0;
		return last_system_error();
	}

    m_hwnd = win32_create_window(0, cls.lpszClassName, nullptr, win32_normal_style,
		CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT,
		nullptr, nullptr, cls.hInstance, nullptr);	
	if (!m_hwnd)
		return last_system_error();
    m_win32_flags |= win32_flag::initialized;
    
    win32_set_window_longptr(m_hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(this));
    win32_set_window_longptr(m_hwnd, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(proc));
    filter(event_type::window_close | event_type::window_rename);
    return {};
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
        if (window->m_flags == window_flags::quit_on_close)
            win32_post_quit_message(0);
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
	}
	return win32_def_window_proc(hwnd, msg, wparam, lparam);
};
error_type window_data::loop(const duration_type timeout) noexcept
{
	if (timeout.count() < 0)
		return make_stdlib_error(std::errc::invalid_argument);
	
    const auto ms = ms_timeout(timeout);	
	while (true)
	{
		MSG win32_msg = {};
		while (win32_peek_message(&win32_msg, nullptr, 0, 0, PM_REMOVE))
		{
			ICY_ERROR(m_error);
			win32_translate_message(&win32_msg);
			win32_dispatch_message(&win32_msg);
            if (win32_msg.message == WM_QUIT)
            {
                return event::post(this, event_type::window_close, true);
            }
            else if (win32_msg.message == WM_USER)
            {
                while (auto event = pop())
                {
                    if (event->type == event_type::global_quit)
                    {
                        return {};
                    }
                    else if (event->type == event_type::window_close)
                    {
                        //  external
                        if (event->data<bool>())
                            return {};
                    }
                    else if (event->type == event_type::window_rename)
                    {
                        auto& str = event->data<array<wchar_t>>();
                        if (!win32_set_window_text(m_hwnd, str.data()))
                            return last_system_error();
                    }
                }
            }
			//if (win32_msg.hwnd != data->hwnd)
			//	continue;
			//g_input.wnd_proc(msg);
		}

        const auto count = win32_msg_wait_for_multiple_objects_ex(0, nullptr, ms_timeout(timeout), QS_ALLEVENTS, MWMO_INPUTAVAILABLE);
        if (count == WAIT_OBJECT_0)
        {
            ;
        }
        else if (count == WAIT_TIMEOUT)
        {
            ICY_ERROR(event::post(this, event_type::global_timeout));
        }
        else if (count == WAIT_FAILED)
        {
            return last_system_error();
        }
	}
	return{};
}
error_type window_data::restyle(const window_style style) noexcept
{
    auto win32_style = (style & window_style::popup) ? win32_popup_style : win32_normal_style;
    WINDOWPLACEMENT place{ sizeof(place) };
    if (!win32_get_window_placement(m_hwnd, &place))
        return last_system_error();

    place.showCmd = SW_SHOWNORMAL;
    if (style & window_style::maximized)
    {
        if (style & window_style::popup)
        {
            const auto hmon = win32_monitor_from_window(m_hwnd, MONITOR_DEFAULTTONEAREST);
            MONITORINFO info{ sizeof(info) };

            const auto func = ICY_FIND_FUNC(m_library, GetMonitorInfoW);
            if (!func)
                return make_stdlib_error(std::errc::function_not_supported);

            if (!hmon || !func(hmon, &info))
                return last_system_error();

            place.rcNormalPosition = info.rcMonitor;
        }
        else
            place.showCmd = SW_MAXIMIZE;
    }
    else if (style & window_style::popup)
        win32_style |= WS_VISIBLE;

    win32_set_window_longptr(m_hwnd, GWL_STYLE, win32_style);
    if (!win32_set_window_placement(m_hwnd, &place))
        return last_system_error();

    return {};
}
error_type window_data::rename(const string_view name) noexcept
{
    array<wchar_t> wide;
    ICY_ERROR(to_utf16(name, wide));
    return event::post(this, event_type::window_rename, std::move(wide));
}
error_type window::create(shared_ptr<window>& window, const window_flags flags) noexcept
{
    shared_ptr<window_data> new_ptr;
    ICY_ERROR(make_shared(new_ptr));
    ICY_ERROR(new_ptr->initialize(flags));
    window = new_ptr;
    return {};
}

error_type icy::find_window(const string_view name, HWND__*& window) noexcept
{
    auto m_library = "user32"_lib;
    ICY_ERROR(m_library.initialize());
        
    decltype(&EnumWindows) win32_enum_windows = nullptr;
    ICY_WIN32_FUNC(win32_enum_windows, EnumWindows);
    ICY_WIN32_FUNC(win32_get_window_text_length, GetWindowTextLengthW);
    ICY_WIN32_FUNC(win32_get_window_text, GetWindowTextW);

    win32_find_window data;
    data.name = name;
    const auto proc = [](HWND hwnd, LPARAM ptr)
    {
        const auto self = reinterpret_cast<win32_find_window*>(ptr);
        string str;
        self->error = win32_name(hwnd, str);
        if (self->error)
            return 0;
        if (str != self->name)
            return 1;

        self->hwnd = hwnd;
        return 1;
    };

    if (!win32_enum_windows(proc, reinterpret_cast<LPARAM>(&data)))
        return last_system_error();

    ICY_ERROR(data.error);
    if (data.hwnd)
        window = data.hwnd;
    else
        window = nullptr;
    return {};
}


/*error_type window::timer(const duration_type frequency) noexcept
{
    const auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(frequency).count();
    if (!data || ms >= USER_TIMER_MAXIMUM)
        return make_stdlib_error(std::errc::invalid_argument);

    if (ms == 0)
    {
        KillTimer(data->hwnd, 0);
    }
    else
    {
        if (!SetTimer(data->hwnd, 0, uint32_t(ms), nullptr))
            return last_system_error();
    }
    return {};
}*/
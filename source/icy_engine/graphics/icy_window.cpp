#include "icy_window.hpp"
#include <icy_engine/core/icy_string.hpp>
#include <icy_engine/core/icy_array.hpp>
#include <icy_engine/core/icy_input.hpp>
#include <dwrite_3.h>
#include <d2d1_3.h>
#include <Windows.h>

using namespace icy;

#define ICY_USER32_FUNC(X, Y) X = ICY_FIND_FUNC(m_lib_user32, Y); if (!X) return make_stdlib_error(std::errc::function_not_supported) 
#define ICY_GDI32_FUNC(X, Y) X = ICY_FIND_FUNC(m_lib_gdi32, Y); if (!X) return make_stdlib_error(std::errc::function_not_supported) 

ICY_STATIC_NAMESPACE_BEG
enum class internal_message_type : uint32_t
{
    none,
    create,
    destroy,
    restyle,
    rename,
    close,
    show,
    repaint,
};
struct internal_message
{
    internal_message_type type = internal_message_type::none;
    uint32_t index = 0;
    string name;
    input_message input;
    window_style style = window_style::windowed;
    bool show = false;
    window_repaint_type repaint;
};
struct win32_flag_enum
{
    enum : uint32_t
    {
        none = 0x00,
        resizing1 = 0x01,
        resizing2 = 0x02,
        moving = 0x04,
        minimized = 0x08,
        inactive = 0x10,
    };
};
using win32_flag = decltype(win32_flag_enum::none);
static const auto win32_normal_style = WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX | WS_MAXIMIZEBOX | WS_THICKFRAME;
static const auto win32_popup_style = WS_POPUP;
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
static decltype(&::GetWindowLongW) win32_get_window_long;
static decltype(&::GetWindowLongPtrW) win32_get_window_longptr;
static decltype(&::SetWindowLongPtrW) win32_set_window_longptr;
static decltype(&::SetWindowLongW) win32_set_window_long;
static decltype(&::UnregisterClassW) win32_unregister_class;
static decltype(&::MonitorFromWindow) win32_monitor_from_window;
static decltype(&::GetMonitorInfoW) win32_get_monitor_info;
static decltype(&::BeginPaint) win32_begin_paint;
static decltype(&::EndPaint) win32_end_paint;
static decltype(&::InvalidateRect) win32_invalidate_rect;
static decltype(&::PostThreadMessageW) win32_post_thread_message;
static decltype(&::CreateWindowExW) win32_create_window = nullptr;
static decltype(&::SetLayeredWindowAttributes) win32_set_layered_window_attributes = nullptr;
static decltype(&::SetWindowPos) win32_set_window_pos = nullptr;
class window_data
{
public:
    window_data(window_system_data* const system = nullptr, const uint32_t index = 0, const window_flags flags = window_flags::none) noexcept : 
        m_system(system), m_index(index), m_flags(flags)
    {

    }
    window_data(window_data&& rhs) noexcept = default;
    ICY_DEFAULT_MOVE_ASSIGN(window_data);
    ~window_data() noexcept;
    error_type initialize(const wchar_t* cname) noexcept;
    error_type repaint(window_repaint_type&& data) noexcept;
    error_type restyle(const window_style style) noexcept;
    error_type rename(const string_view name) noexcept;
    error_type show(const bool value) noexcept;
    HWND__* handle() const noexcept;
    window_flags flags() const noexcept
    {
        return m_flags;
    }
private:
    LRESULT proc(uint32_t msg, WPARAM wparam, LPARAM lparam) noexcept;
private:
    window_system_data* m_system = nullptr;
    uint32_t m_index = 0;
    window_flags m_flags;
    HWND__* m_hwnd = nullptr;
    uint32_t m_win32_flags = 0;
    window_repaint_type m_repaint;
};
class window_thread_data : public thread
{
public:
    void cancel() noexcept
    {
        system->post(nullptr, event_type::global_quit);
    }
    error_type run() noexcept override
    {
        if (auto error = system->exec())
            return event::post(system, event_type::system_error, std::move(error));
        return error_type();
    }
public:
    window_system* system = nullptr;
};
ICY_STATIC_NAMESPACE_END
class window_system_data : public window_system
{
public:
    ~window_system_data() noexcept override;
    error_type initialize() noexcept;
    error_type exec() noexcept override;
    void exit(const error_type error) noexcept;
    error_type post(internal_message&& msg) noexcept;
    window_flags flags(const uint32_t index) const noexcept;
    HWND handle(const uint32_t index) const noexcept;
private:
    const icy::thread& thread() const noexcept override
    {
        return *m_thread;
    }
    error_type create(icy::window& window, const window_flags flags) const noexcept override;
    error_type signal(const event_data& event) noexcept override
    {
        win32_post_thread_message(m_thread->index(), WM_NULL, 0, 0);
        SetLastError(0);
        return error_type();
    }
private:
    mutable mutex m_lock;
    library m_lib_user32 = library("user32");
    library m_lib_gdi32 = library("gdi32");
    mutable map<uint32_t, unique_ptr<window_data>> m_data;
    mutable uint32_t m_next = 1;
    wchar_t m_cname[64] = {};
    shared_ptr<window_thread_data> m_thread;
    error_type m_error;
};

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
    if (m_hwnd)
    {
        win32_set_window_longptr(m_hwnd, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(win32_def_window_proc));
        win32_destroy_window(m_hwnd);
    }
}
error_type window_data::initialize(const wchar_t* cname) noexcept
{
    const auto style = m_flags & window_flags::layered ? win32_popup_style : win32_normal_style;
    const auto ex_style = m_flags & window_flags::layered ? (WS_EX_TOPMOST | WS_EX_LAYERED | WS_EX_TOOLWINDOW | WS_EX_TRANSPARENT) : WS_EX_APPWINDOW;
    m_hwnd = win32_create_window(ex_style, cname, nullptr, style, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT,
        nullptr, nullptr, win32_instance(), nullptr);
    if (!m_hwnd)
        return last_system_error();

    if (m_flags & window_flags::layered)
    {
        if (!win32_set_layered_window_attributes(m_hwnd, 0, 0, LWA_COLORKEY))
            return last_system_error();
    }

    const auto proc = [](HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) -> LRESULT
    {
        const auto window = reinterpret_cast<window_data*>(win32_get_window_longptr(hwnd, GWLP_USERDATA));
        if (window)
            return window->proc(msg, wparam, lparam);
        else
            return win32_def_window_proc(hwnd, msg, wparam, lparam);
    };

    win32_set_window_longptr(m_hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(this));
    win32_set_window_longptr(m_hwnd, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(WNDPROC(proc)));
    return error_type();
}
error_type window_data::repaint(window_repaint_type&& data) noexcept
{
    m_repaint = std::move(data);
    if (!win32_invalidate_rect(m_hwnd, nullptr, FALSE))
        return last_system_error();
    return error_type();
}
error_type window_data::restyle(const window_style style) noexcept 
{
    const auto was_visible = win32_get_window_longptr(m_hwnd, GWL_STYLE) & WS_VISIBLE;
    const auto win32_style = (style & window_style::popup) ? win32_popup_style : win32_normal_style;

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
            if (!hmon || !win32_get_monitor_info(hmon, &info))
                return last_system_error();
            place.rcNormalPosition = info.rcMonitor;
        }
        else
            place.showCmd = SW_MAXIMIZE;
    }
    win32_set_window_longptr(m_hwnd, GWL_STYLE, win32_style | was_visible);
    if (!win32_set_window_placement(m_hwnd, &place))
        return last_system_error();

    return last_system_error();
}
error_type window_data::rename(const string_view name) noexcept
{
    array<wchar_t> wname;
    ICY_ERROR(to_utf16(name, wname));
    if (!win32_set_window_text(m_hwnd, wname.data()))
        return last_system_error();
    return error_type();
}
error_type window_data::show(const bool value) noexcept
{
    auto style = win32_get_window_long(m_hwnd, GWL_STYLE);
    style &= ~WS_VISIBLE;
    if (value) 
        style |= WS_VISIBLE;
    win32_set_window_longptr(m_hwnd, GWL_STYLE, style);
    return error_type();
}
HWND__* window_data::handle() const noexcept
{
    return m_hwnd;
}
LRESULT window_data::proc(uint32_t msg, WPARAM wparam, LPARAM lparam) noexcept
{
    const auto make_message = [this](window_message& message) noexcept
    {
        message.index = m_index;
        message.handle = m_hwnd;

        if (m_win32_flags & win32_flag::inactive) message.state = message.state | window_state::inactive;
        if (m_win32_flags & win32_flag::minimized) message.state = message.state | window_state::minimized;

        RECT rect;
        if (!win32_get_client_rect(m_hwnd, &rect))
        {
            m_system->exit(last_system_error());
            return false;
        }
        message.size.x = rect.right - rect.left;
        message.size.y = rect.bottom - rect.top;
        return true;
    };
   /* const auto on_resize = [this]
    {
        RECT rect = {};
        if (!win32_get_client_rect(m_hwnd, &rect))
            return last_system_error();

        window_message msg = {};
        msg.index = m_index;
        msg.size = window_size{ uint32_t(rect.right - rect.left), uint32_t(rect.bottom - rect.top) };
        msg.state = m_state;
        return event::post(m_system, event_type::window_resize, msg);
    };*/
    const auto on_repaint = [this]
    {
       /* auto render = shared_ptr<render_factory>(m_repaint.render);
        if (!render || !m_repaint.device)
            return error_type();

        RECT rect;
        if (!win32_get_client_rect(m_hwnd, &rect))
            return last_system_error();

        com_ptr<ID2D1Factory> factory;
        m_repaint.device->GetFactory(&factory);

        const auto pixel = D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED);

        com_ptr<ID2D1DCRenderTarget> target;
        ICY_COM_ERROR(factory->CreateDCRenderTarget(&D2D1::RenderTargetProperties(
            D2D1_RENDER_TARGET_TYPE::D2D1_RENDER_TARGET_TYPE_DEFAULT, pixel), &target));

        PAINTSTRUCT ps;
        auto hdc = win32_begin_paint(m_hwnd, &ps);
        if (!hdc)
            return last_system_error();

        auto end_paint_done = false;
        ICY_SCOPE_EXIT{ if (!end_paint_done) win32_end_paint(m_hwnd, &ps); };

        ICY_COM_ERROR(target->BindDC(hdc, &rect));
        target->BeginDraw();

        auto end_draw_done = false;
        ICY_SCOPE_EXIT{ if (!end_draw_done) target->EndDraw(); };

        target->Clear(D2D1::ColorF(0, 0, 0, 0));

        for (const auto& layer : m_repaint.map)
        {
            for (auto&& cmd : layer.value)
            {
                target->SetTransform(reinterpret_cast<const D2D1_MATRIX_3X2_F*>(&cmd.transform));
                if (cmd.geometry.data)
                {
                    ICY_ERROR(cmd.geometry.data->render(*target));
                }
                if (cmd.text)
                {
                    com_ptr<ID2D1SolidColorBrush> brush;
                    ICY_COM_ERROR(target->CreateSolidColorBrush(D2D1::ColorF(cmd.color.to_rgb(), cmd.color.a / 255.0f), &brush));
                    target->DrawTextLayout(D2D1::Point2F(), cmd.text, brush, D2D1_DRAW_TEXT_OPTIONS_ENABLE_COLOR_FONT);
                }
            }
        }
        end_draw_done = true;
        ICY_COM_ERROR(target->EndDraw());

        end_paint_done = true;
        if (!win32_end_paint(m_hwnd, &ps))
            return last_system_error();
            */
        return error_type();
    };


    switch (msg)
    {
    case WM_PAINT:
        if (const auto error = on_repaint())
            m_system->exit(error);
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
    {
        window_message message;
        if (!make_message(message))
            return 0;

        message.state = message.state | window_state::closing;
        if (const auto error = event::post(m_system, event_type::window_state, std::move(message)))
            m_system->exit(error);

        if (m_flags & window_flags::quit_on_close)
            event::post(nullptr, event_type::global_quit);
        return 0;
    }

    case WM_COPYDATA:
    {
        window_message message;
        if (!make_message(message))
            break;

        const auto cdata = reinterpret_cast<COPYDATASTRUCT*>(lparam);
        const auto ptr = static_cast<const uint8_t*>(cdata->lpData);
        message.data.user = cdata->dwData;
        if (const auto error = message.data.bytes.append(const_array_view<uint8_t>(ptr, cdata->cbData)))
        {
            m_system->exit(error);
            break;
        }

        if (const auto error = event::post(m_system, event_type::window_data, std::move(message)))
            m_system->exit(error);
        break;
    }

    case WM_ENTERSIZEMOVE:
        m_win32_flags |= win32_flag::resizing1;
        break;

    case WM_SIZING:
        m_win32_flags |= win32_flag::resizing2;
        break;

    case WM_EXITSIZEMOVE:
        if ((m_win32_flags & win32_flag::resizing1) && (m_win32_flags & win32_flag::resizing2))
        {
            window_message message;
            if (!make_message(message))
                break;
            if (const auto error = event::post(m_system, event_type::window_resize, std::move(message)))
                m_system->exit(error);
        }
        m_win32_flags &= ~(win32_flag::resizing1 | win32_flag::resizing2);
        break;

    case WM_SIZE:
    {
        if (wparam == SIZE_MINIMIZED)
        {
            m_win32_flags |= win32_flag::minimized;
            window_message message;
            if (!make_message(message))
                break;
            
            if (const auto error = event::post(m_system, event_type::window_state, std::move(message)))
                m_system->exit(error);
            break;
        }
        else if (m_win32_flags & win32_flag::minimized)
        {
            m_win32_flags &= ~win32_flag::minimized; 
            window_message message;
            if (!make_message(message))
                break;

            if (const auto error = event::post(m_system, event_type::window_state, std::move(message)))
                m_system->exit(error);
            break;
        }
        else if (wparam == SIZE_MAXIMIZED || (wparam == SIZE_RESTORED && (m_win32_flags & win32_flag::resizing1) == 0 && (m_win32_flags & win32_flag::resizing2) == 0))
        {
            window_message message;
            if (!make_message(message))
                break;

            if (const auto error = event::post(m_system, event_type::window_resize, std::move(message)))
                m_system->exit(error);
            break;
        }
        break;
    }
    case WM_ACTIVATE:
    {
        const auto active = LOWORD(wparam);

        m_win32_flags &= ~win32_flag::inactive;
        if (active == WA_INACTIVE)
            m_win32_flags |= win32_flag::inactive;

        window_message message;
        if (!make_message(message))
            break;

        if (const auto error = event::post(m_system, event_type::window_state, std::move(message)))
            m_system->exit(error);
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
            window_message message;
            if (!make_message(message))
                break;

            message.input = key_input;
            if (const auto error = event::post(m_system, event_type::window_input, std::move(message)))
            {
                m_system->exit(error);
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
        window_message message;
        if (!make_message(message))
            break;

        detail::from_winapi(message.input, 0, 0, msg, wparam, lparam, key_state());

        if (const auto error = event::post(m_system, event_type::window_input, std::move(message)))
            m_system->exit(error);
        break;
    }
    }
    return win32_def_window_proc(m_hwnd, msg, wparam, lparam);
}

window_system_data::~window_system_data() noexcept
{
    if (m_thread)
        m_thread->wait();
    if (*m_cname)
        win32_unregister_class(m_cname, win32_instance());
    filter(0);
}
error_type window_system_data::initialize() noexcept
{
    ICY_ERROR(m_lock.initialize());

    ICY_ERROR(m_lib_user32.initialize());
    ICY_ERROR(m_lib_gdi32.initialize());

    if (const auto func = ICY_FIND_FUNC(m_lib_user32, SetThreadDpiAwarenessContext))
        func(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);

    decltype(&::RegisterClassExW) win32_register_class = nullptr;
    decltype(&::LoadCursorW) win32_load_cursor = nullptr;
    
    ICY_USER32_FUNC(win32_translate_message, TranslateMessage);
    ICY_USER32_FUNC(win32_dispatch_message, DispatchMessageW);
    ICY_USER32_FUNC(win32_get_keyboard_state, GetKeyboardState);
    ICY_USER32_FUNC(win32_peek_message, PeekMessageW);
    ICY_USER32_FUNC(win32_post_message, PostMessageW);
    ICY_USER32_FUNC(win32_def_window_proc, DefWindowProcW);
    ICY_USER32_FUNC(win32_post_quit_message, PostQuitMessage);
    ICY_USER32_FUNC(win32_unregister_class, UnregisterClassW);
    ICY_USER32_FUNC(win32_register_class, RegisterClassExW);
    ICY_USER32_FUNC(win32_create_window, CreateWindowExW);
    ICY_USER32_FUNC(win32_destroy_window, DestroyWindow);
    ICY_USER32_FUNC(win32_get_window_placement, GetWindowPlacement);
    ICY_USER32_FUNC(win32_set_window_placement, SetWindowPlacement);
    ICY_USER32_FUNC(win32_msg_wait_for_multiple_objects_ex, MsgWaitForMultipleObjectsEx);
    ICY_USER32_FUNC(win32_set_window_text, SetWindowTextW);
    ICY_USER32_FUNC(win32_get_window_text, GetWindowTextW);
    ICY_USER32_FUNC(win32_get_window_text_length, GetWindowTextLengthW);
    ICY_USER32_FUNC(win32_get_client_rect, GetClientRect);
    ICY_USER32_FUNC(win32_get_window_long, GetWindowLongW);
    ICY_USER32_FUNC(win32_get_window_longptr, GetWindowLongPtrW);
    ICY_USER32_FUNC(win32_set_window_longptr, SetWindowLongPtrW);
    ICY_USER32_FUNC(win32_monitor_from_window, MonitorFromWindow);
    ICY_USER32_FUNC(win32_get_monitor_info, GetMonitorInfoW);
    ICY_USER32_FUNC(win32_begin_paint, BeginPaint);
    ICY_USER32_FUNC(win32_end_paint, EndPaint);
    ICY_USER32_FUNC(win32_invalidate_rect, InvalidateRect);
    ICY_USER32_FUNC(win32_post_thread_message, PostThreadMessageW);
    ICY_USER32_FUNC(win32_load_cursor, LoadCursorW);
    ICY_USER32_FUNC(win32_set_layered_window_attributes, SetLayeredWindowAttributes);
    ICY_USER32_FUNC(win32_set_window_pos, SetWindowPos);

    swprintf_s(m_cname, L"Icy Window %u.%u", GetCurrentProcessId(), GetCurrentThreadId());

    WNDCLASSEXW cls = { sizeof(WNDCLASSEXW) };
    cls.lpszClassName = m_cname;
    cls.style = CS_HREDRAW | CS_VREDRAW | CS_DBLCLKS;
    cls.lpfnWndProc = win32_def_window_proc;
    cls.hInstance = win32_instance();
    cls.hCursor = win32_load_cursor(nullptr, IDC_ARROW);
    if (win32_register_class(&cls) == 0)
        return last_system_error();

    ICY_ERROR(make_shared(m_thread));
    m_thread->system = this;

    filter(event_type::global_quit);
    return error_type();
}
error_type window_system_data::exec() noexcept
{
    ICY_SCOPE_EXIT{ m_data.clear(); };
    const auto timeout = max_timeout;
    const auto ms = ms_timeout(timeout);
    while (true)
    {
        while (auto event = pop())
        {
            if (event->type == event_type::global_quit)
                return error_type();

            if (event->type != event_type::system_internal)
                continue;

            auto& msg_data = event->data<internal_message>();

            auto it = m_data.find(msg_data.index);
            if (it == m_data.end())
                return make_stdlib_error(std::errc::invalid_argument);
            
            auto& wnd = it->value;
            if (msg_data.type == internal_message_type::create)
            {
                if (!wnd->handle())
                {
                    if (const auto error = wnd->initialize(m_cname))
                    {
                        m_data.erase(it);
                        return error;
                    }
                }
            }
            else if (msg_data.type == internal_message_type::destroy)
            {
                m_data.erase(it);
            }
            else if (msg_data.type == internal_message_type::show)
            {
                ICY_ERROR(wnd->show(msg_data.show));
            }
            else if (msg_data.type == internal_message_type::rename)
            {
                ICY_ERROR(wnd->rename(msg_data.name));
            }
            else if (msg_data.type == internal_message_type::restyle)
            {
                ICY_ERROR(wnd->restyle(msg_data.style));
            }
            else if (msg_data.type == internal_message_type::repaint)
            {
                ICY_ERROR(wnd->repaint(std::move(msg_data.repaint)));
            }
        }

        const auto index = win32_msg_wait_for_multiple_objects_ex(0, nullptr, ms, QS_ALLINPUT, MWMO_ALERTABLE);
        if (index == WAIT_OBJECT_0)
        {
            MSG win32_msg = {};
            while (win32_peek_message(&win32_msg, nullptr, 0, 0, PM_REMOVE))
            {
                win32_translate_message(&win32_msg);
                win32_dispatch_message(&win32_msg);
            }
            ICY_ERROR(m_error);
        }
        else if (index == WAIT_IO_COMPLETION)
        {
            return error_type();
        }
        else if (index == WAIT_TIMEOUT)
        {
            return make_stdlib_error(std::errc::timed_out);
        }
        else if (index == WAIT_FAILED)
        {
            return last_system_error();
        }
    }
    return error_type();
}
error_type window_system_data::post(internal_message&& msg) noexcept
{
    return event_system::post(this, event_type::system_internal, std::move(msg));
}
window_flags window_system_data::flags(const uint32_t index) const noexcept
{
    ICY_LOCK_GUARD(m_lock);
    const auto it = m_data.find(index);
    if (it == m_data.end())
        return window_flags::none;

    return it->value->flags();
}
HWND window_system_data::handle(const uint32_t index) const noexcept
{
    while (true)
    {
        ICY_LOCK_GUARD(m_lock);
        const auto it = m_data.find(index);
        if (it == m_data.end())
            return nullptr;

        if (const auto hwnd = it->value->handle())
            return hwnd;
        else
            sleep(std::chrono::milliseconds(0));
    }
    return nullptr;
}
error_type window_system_data::create(icy::window& new_window, const window_flags flags) const noexcept
{
    auto new_data = allocator_type::allocate<window::data_type>(1);
    if (!new_data)
        return make_stdlib_error(std::errc::not_enough_memory);
    allocator_type::construct(new_data);
    ICY_SCOPE_EXIT{ if (new_data) { allocator_type::destroy(new_data); allocator_type::deallocate(new_data); } };
    new_data->system = make_shared_from_this(this);
    {
        const auto self = const_cast<window_system_data*>(this);
        auto new_ptr = make_unique<window_data>(self, m_next, flags);
        if (!new_ptr)
            return make_stdlib_error(std::errc::not_enough_memory);
        
        ICY_LOCK_GUARD(m_lock);
        new_data->index = m_next;
        ICY_ERROR(m_data.insert(m_next, std::move(new_ptr)));
        m_next++;

        internal_message msg;
        msg.type = internal_message_type::create;
        msg.index = new_data->index;
        ICY_ERROR(self->post(std::move(msg)));
    }
    new_window = window();
    new_window.data = new_data;
    new_data = nullptr;
    return error_type();
}
void window_system_data::exit(const error_type error) noexcept
{
    m_error = error;
}

window::window(const window& rhs) noexcept : data(rhs.data)
{
    if (data)
        data->ref.fetch_add(1, std::memory_order_release);
}
window::~window() noexcept 
{
    if (data && data->ref.fetch_sub(1, std::memory_order_acq_rel) == 1)
    {
        if (auto sys = shared_ptr<window_system_data>(data->system))
        {
            internal_message msg;
            msg.type = internal_message_type::destroy;
            msg.index = data->index;
            sys->post(std::move(msg));
        }
        allocator_type::destroy(data);
        allocator_type::deallocate(data);
    }
}
error_type window::restyle(const window_style style) noexcept
{
    if (!data)
        return make_stdlib_error(std::errc::invalid_argument);

    auto sys = shared_ptr<window_system_data>(data->system);
    if (!sys)
        return make_stdlib_error(std::errc::invalid_argument);

    internal_message msg;
    msg.type = internal_message_type::restyle;
    msg.index = data->index;
    msg.style = style;
    return sys->post(std::move(msg));
}
error_type window::rename(const string_view name) noexcept
{
    if (!data)
        return make_stdlib_error(std::errc::invalid_argument);

    auto sys = shared_ptr<window_system_data>(data->system);
    if (!sys)
        return make_stdlib_error(std::errc::invalid_argument);

    internal_message msg;
    msg.type = internal_message_type::rename;
    msg.index = data->index;
    ICY_ERROR(copy(name, msg.name));
    return sys->post(std::move(msg));
}
error_type window::show(const bool value) noexcept
{
    if (!data)
        return make_stdlib_error(std::errc::invalid_argument);

    auto sys = shared_ptr<window_system_data>(data->system);
    if (!sys)
        return make_stdlib_error(std::errc::invalid_argument);

    internal_message msg;
    msg.type = internal_message_type::show;
    msg.index = data->index;
    msg.show = value;
    return sys->post(std::move(msg));
}
HWND__* window::handle() const noexcept
{
    if (!data)
        return nullptr;

    auto sys = shared_ptr<window_system_data>(data->system);
    if (!sys)
        return nullptr;

    return sys->handle(data->index);
}
window_flags window::flags() const noexcept
{
    if (!data)
        return window_flags::none;

    auto sys = shared_ptr<window_system_data>(data->system);
    if (!sys)
        return window_flags::none;

    return sys->flags(data->index);
}

error_type window::data_type::repaint(window_repaint_type&& repaint) noexcept 
{
    if (auto sys = shared_ptr<window_system_data>(system))
    {
        internal_message msg;
        msg.type = internal_message_type::repaint;
        msg.index = index;
        msg.repaint = std::move(repaint);
        return sys->post(std::move(msg));
    }
    return error_type();
}

error_type icy::create_window_system(shared_ptr<window_system>& system) noexcept
{
    shared_ptr<window_system_data> new_ptr;
    ICY_ERROR(make_shared(new_ptr));
    ICY_ERROR(new_ptr->initialize());
    system = std::move(new_ptr);
    return error_type();
}
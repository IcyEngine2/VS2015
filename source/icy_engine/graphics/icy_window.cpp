#include <icy_engine/graphics/icy_window.hpp>
#include <icy_engine/core/icy_string.hpp>
#include <icy_engine/core/icy_array.hpp>
#include <icy_engine/core/icy_input.hpp>
#include <icy_engine/core/icy_thread.hpp>
#include <icy_engine/core/icy_set.hpp>
#include <icy_engine/core/icy_map.hpp>
#include <icy_engine/core/icy_blob.hpp>
#include <icy_engine/utility/icy_com.hpp>
#include <icy_engine/graphics/icy_image.hpp>
#include <icy_engine/graphics/icy_render.hpp>

#if _WIN32
#include <Windows.h>
#include <windowsx.h>
#include <d2d1.h>
#endif

static const uint32_t default_window_w = 800;
static const uint32_t default_window_h = 640;

#if X11_GUI
#include <setjmp.h>  
struct HWND__;
struct HICON__;
#if _WIN32
#include <WinSock2.h>
#if _DEBUG
#pragma comment(lib, "xlibd")
#else
#pragma comment(lib, "xlib")
#endif
#endif
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#undef index
#if _WIN32
static inline error_type last_network_error()
{
    return make_system_error(WSAGetLastError());
}
#else
static inline error_type last_network_error()
{
    return make_system_error(errno);
}
#endif
#endif

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
    repaint,
    //close,
    show,
    cursor,
    query,
};
struct internal_message
{
    struct cursor_type
    {
        shared_ptr<window_cursor> value;
        uint32_t priority = 0;
    };
    struct query_type
    {
        sync_handle sync;
        std::atomic<HWND> hwnd = nullptr;
    };
    internal_message_type type = internal_message_type::none;
    uint32_t index = 0;
    string name;
    input_message input;
    window_style style = window_style::windowed;
    window_flags flags = window_flags::none;
    bool show = false;
    cursor_type cursor;
    shared_ptr<query_type> query;
    array<window_render_item> repaint;
    shared_ptr<render_surface> surface;
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
#if X11_GUI
static error_type x11_error_to_string(unsigned int code, string_view locale, string& str)
{
    return make_stdlib_error(std::errc::function_not_supported);
}
static const error_source error_source_x11 = register_error_source("x11"_s, x11_error_to_string);
static error_type make_x11_error(int code) noexcept
{
    return error_type(code, error_source_x11);
}
#elif _WIN32
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
static decltype(&::CreateWindowExW) win32_create_window;
static decltype(&::SetLayeredWindowAttributes) win32_set_layered_window_attributes;
static decltype(&::SetWindowPos) win32_set_window_pos;
static decltype(&::DestroyCursor) win32_destroy_cursor;
static decltype(&::LoadCursorW) win32_load_cursor;
static decltype(&::SetCursor) win32_set_cursor;
static decltype(&::GetDpiForWindow) win32_get_dpi_for_window;
static decltype(&::SetCapture) win32_set_capture;
static decltype(&::ReleaseCapture) win32_release_capture;
static decltype(&::OpenClipboard) win32_open_clipboard;
static decltype(&::CloseClipboard) win32_close_clipboard;
static decltype(&::GetClipboardData) win32_get_clipboard_data;
static decltype(&::ScreenToClient) win32_screen_to_client;
#endif
class window_system_data;
class window_data_usr : public window
{
public:
    window_data_usr(shared_ptr<window_system_data> system, const window_flags flags, const uint32_t index) noexcept :
        m_system(system), m_flags(flags), m_index(index)
    {

    }
    ~window_data_usr() noexcept;
    shared_ptr<window_system> system() noexcept override
    {
        return shared_ptr<window_system_data>(m_system);
    }
    uint32_t index() const noexcept override
    {
        return m_index;
    }
    error_type restyle(const window_style style) noexcept override;
    error_type rename(const string_view name) noexcept override;
    error_type show(const bool value) noexcept override;
    error_type cursor(const uint32_t priority, const shared_ptr<window_cursor> cursor) noexcept override;
    window_flags flags() const noexcept override
    {
        return m_flags;
    }
    window_size size() const noexcept
    {
#if X11_GUI
        return window_size(default_window_w, default_window_h);
#elif _WIN32
        auto system = shared_ptr<window_system_data>(m_system);
        void* handle = nullptr;
        win_handle(handle);
        if (handle && system)
        {
            RECT rect;
            if (win32_get_client_rect(static_cast<HWND__*>(handle), &rect))
                return window_size(rect.right - rect.left, rect.bottom - rect.top);
        }
        return window_size();
#endif
    }
    uint32_t dpi() const noexcept
    {
        if (auto system = shared_ptr<window_system_data>(m_system))
        {
#if X11_GUI

#elif _WIN32
            void* handle = nullptr;
            win_handle(handle);
            if (win32_get_dpi_for_window && handle)
            {
                if (auto value = win32_get_dpi_for_window(static_cast<HWND__*>(handle)))
                    return value;
            }
#endif
        }
        return 96;
    }
    error_type repaint(const string_view tag, array<window_render_item>& items) noexcept override;
    error_type win_handle(void*& handle) const noexcept override;
private:
    weak_ptr<window_system_data> m_system;
    window_flags m_flags = window_flags::none;
    const uint32_t m_index = 0;
    mutable std::atomic<HWND__*> m_handle = nullptr;
};
class window_data_sys
{
public:
    window_data_sys() noexcept : 
        m_system(*static_cast<window_system_data*>(nullptr)), m_index(0)
    {

    }
    window_data_sys(window_system_data& system, const uint32_t index) noexcept : 
        m_system(system), m_index(index)
    {

    }
    window_data_sys(window_data_sys&& rhs) noexcept;
    ICY_DEFAULT_MOVE_ASSIGN(window_data_sys);
    ~window_data_sys() noexcept;
    error_type initialize(const wchar_t* cname, const window_flags flags) noexcept;
    error_type restyle(const window_style style) noexcept;
    error_type rename(const string_view name) noexcept;
    error_type show(const bool value) noexcept;
    error_type cursor(const uint32_t priority, shared_ptr<window_cursor> cursor) noexcept;
    error_type repaint(string&& tag, array<window_render_item>&& items) noexcept;
    error_type repaint(string&& tag, shared_ptr<render_surface> surface) noexcept;
    error_type resize(bool& changed) noexcept;
#if X11_GUI
    error_type start_timer(const duration_type timeout) noexcept
    {
        if (!m_timer)
        {
            m_timer = make_unique<timer>();
            if (!m_timer)
                return make_stdlib_error(std::errc::not_enough_memory);
        }
        ICY_ERROR(m_timer->initialize(1, timeout));
        return error_type();
    }
    const timer* get_timer() const noexcept
    {
        return m_timer.get();
    }
    window_size get_size() const noexcept
    {
        return m_size;
    }
#elif _WIN32
    LRESULT proc(uint32_t msg, WPARAM wparam, LPARAM lparam) noexcept;
#endif
    auto handle() const noexcept
    {
        return m_hwnd;
    }
private:
    window_system_data& m_system;
    const uint32_t m_index;
#if X11_GUI
    Window m_hwnd = 0;
    GC m_gc = 0;
    XImage* m_image = nullptr;
    unique_ptr<timer> m_timer;
    window_size m_size;
#elif _WIN32
    HWND m_hwnd = nullptr;
#endif
    window_flags m_flags = window_flags::none;
    uint32_t m_win32_flags = 0;
    window_style m_style = window_style::windowed;
    struct cursor_type
    {
        map<uint32_t, shared_ptr<window_cursor>> refs;
        void* handle = nullptr;
        void* arrow = nullptr;
    } m_cursor;
    wchar_t m_chr = 0;
    uint32_t m_capture = 0;
    map<string, array<window_render_item>> m_repaint;
};
class window_thread_data : public thread
{
public:
    error_type run() noexcept override
    {
        if (auto error = system->exec())
        {
            icy::post_quit_event();
            return error;
        }
        return error_type();
    }
public:
    window_system* system = nullptr;
};
class window_cursor_data : public window_cursor
{
public:
    window_cursor_data(const type type) noexcept : m_type(type)
    {

    }
    ~window_cursor_data() noexcept
    {
        //if (m_handle && m_type == type::none)
        //    win32_destroy_cursor(m_handle);
    }
    error_type initialize() noexcept;
    HICON__* handle() const noexcept
    {
        return m_handle;
    }
private:
#if X11_GUI
#elif _WIN32
    library m_library = "user32"_lib;
#endif
    const type m_type = type::none;
    HICON__* m_handle = nullptr;
};
class window_system_data : public window_system
{
public:
    ~window_system_data() noexcept override;
    error_type initialize() noexcept;
    error_type exec() noexcept override;
    void exit(const error_type error) noexcept;
    error_type post(internal_message&& msg) noexcept;
#if X11_GUI
    Display& display() const noexcept
    {
        return *m_display;
    }
    error_type error() const noexcept
    {
        return m_error;
    }
#else
    ID2D1DCRenderTarget& render() const noexcept
    {
        return *m_render;
    }
    com_ptr<ID2D1Bitmap> image(const blob blob) const noexcept 
    {
        auto it = m_images.find(blob.index);
        if (it != m_images.end())
            return it->value;
        return nullptr;
    }
#endif
private:
    const icy::thread& thread() const noexcept override
    {
        return *m_thread;
    }
    error_type create(shared_ptr<window>& window, const window_flags flags) noexcept override;
    error_type signal(const event_data* event) noexcept override
    {
#if X11_GUI
        //char buf[64] = {};
        //if (!sendto(m_socket, buf, sizeof(buf), 0))
        //    return last_network_error();
#elif _WIN32
        win32_post_thread_message(m_thread->index(), WM_NULL, 0, 0);
        SetLastError(0);
#endif
        return error_type();
    }
private:
    mutable mutex m_lock;
#if X11_GUI
    Display* m_display = nullptr;
    struct jmp_type { jmp_buf buf; };
    icy::unique_ptr<jmp_type> m_jmp;
#elif _WIN32
    library m_lib_d2d1 = library("d2d1");
    library m_lib_dwrite = library("dwrite");
    library m_lib_user32 = library("user32");
    library m_lib_gdi32 = library("gdi32");
    HCURSOR m_cursor = nullptr;
    com_ptr<ID2D1Factory> m_factory;
    com_ptr<ID2D1DCRenderTarget> m_render;
    map<uint32_t, com_ptr<ID2D1Bitmap>> m_images;
#endif
    wchar_t m_cname[64] = {};
    shared_ptr<window_thread_data> m_thread;
    error_type m_error;
    std::atomic<uint32_t> m_index = 0;
};
ICY_STATIC_NAMESPACE_END

#if X11_GUI

#elif _WIN32
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
#endif

window_data_usr::~window_data_usr() noexcept
{
    if (auto system = shared_ptr<window_system_data>(m_system))
    {
        internal_message msg;
        msg.type = internal_message_type::destroy;
        msg.index = m_index;
        system->post(std::move(msg));
    }
}

window_data_sys::~window_data_sys() noexcept
{
    
#if X11_GUI
    if (m_gc)
        XFreeGC(&m_system.display(), m_gc);
    if (m_image)
        XDestroyImage(m_image);
    if (m_hwnd)
        XDestroyWindow(&m_system.display(), m_hwnd);
#elif _WIN32
    if (m_hwnd)
    {
        //win32_remove_clipboard_format_listener(m_hwnd);
        win32_set_window_longptr(m_hwnd, GWLP_WNDPROC, LONG_PTR(win32_def_window_proc));
        win32_destroy_window(m_hwnd);
    }
#endif
}
window_data_sys::window_data_sys(window_data_sys&& rhs) noexcept : m_index(rhs.m_index)
    , m_system(rhs.m_system)
    , m_flags(rhs.m_flags)
#if X11_GUI
    , m_hwnd(rhs.m_hwnd)
    , m_gc(rhs.m_gc)
    , m_image(rhs.m_image)
#elif _WIN32
    , m_hwnd(rhs.m_hwnd)
#endif
    , m_win32_flags(rhs.m_win32_flags)
    , m_style(rhs.m_style)
    , m_cursor(std::move(rhs.m_cursor))
    , m_chr(rhs.m_chr)

{
#if X11_GUI
    rhs.m_hwnd = 0;
    rhs.m_gc = nullptr;
    rhs.m_image = nullptr;
#elif _WIN32
    rhs.m_hwnd = nullptr;
#endif
}
error_type window_data_sys::initialize(const wchar_t* cname, const window_flags flags) noexcept
{
    m_flags = flags;
#if X11_GUI
    const auto display = &m_system.display();
    auto screen = DefaultScreen(display);
    auto w = default_window_w;
    auto h = default_window_h;
    auto x = 0;
    auto y = 0;
    auto border_width = 5u;
    auto black_color = BlackPixel(display, screen);
    auto white_color = WhitePixel(display, screen);
    m_hwnd = XCreateSimpleWindow(display, DefaultRootWindow(display), x, y, w, h, border_width, white_color, black_color);
    if (!m_hwnd)
        return system.error();

    if (!XSetStandardProperties(display, m_hwnd, "X11 Window", "X11 Icon", 0, __argv, __argc, nullptr))
        return system.error();

    auto mask = 0
        | ExposureMask
        | StructureNotifyMask
        | ButtonPressMask
        | ButtonReleaseMask
        | KeyPressMask
        | KeyReleaseMask
        | PointerMotionMask
        | ButtonMotionMask
        ;
    if (!XSelectInput(display, m_hwnd, mask))
        return system.error();

    m_gc = XCreateGC(display, m_hwnd, 0, nullptr);
    if (!m_gc)
        return system.error();

    XSetBackground(display, m_gc, white_color);
    XSetForeground(display, m_gc, black_color);

    if (!XClearWindow(display, m_hwnd))
        return system.error();

    if (!XMapRaised(display, m_hwnd))
        return system.error();
        
    auto change = false;
    ICY_ERROR(resize(change));
#elif _WIN32

    const auto style = m_flags & window_flags::layered ? win32_popup_style : win32_normal_style;
    const auto ex_style = m_flags & window_flags::layered ? (WS_EX_TOPMOST | WS_EX_LAYERED | WS_EX_TOOLWINDOW | WS_EX_TRANSPARENT) : WS_EX_APPWINDOW;
    m_hwnd = win32_create_window(ex_style, cname, nullptr, style, 
        CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT,
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
        const auto window = reinterpret_cast<window_data_sys*>(win32_get_window_longptr(hwnd, GWLP_USERDATA));
        if (window)
            return window->proc(msg, wparam, lparam);
        else
            return win32_def_window_proc(hwnd, msg, wparam, lparam);
    };
    
    m_cursor.arrow = m_cursor.handle = win32_load_cursor(nullptr, IDC_ARROW);
    if (!m_cursor.arrow)
        return last_system_error();

    win32_set_window_longptr(m_hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(this));
    win32_set_window_longptr(m_hwnd, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(WNDPROC(proc)));

    //if (!win32_add_clipboard_format_listener(m_hwnd))
    //    return last_system_error();

#endif

    return error_type();
}
error_type window_data_sys::restyle(const window_style style) noexcept
{
#if X11_GUI

#elif _WIN32
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
#endif

    m_style = style;
    return error_type();
}
error_type window_data_sys::rename(const string_view name) noexcept
{
#if X11_GUI

#elif _WIN32
    array<wchar_t> wname;
    ICY_ERROR(to_utf16(name, wname));
    if (!win32_set_window_text(m_hwnd, wname.data()))
        return last_system_error();
#endif
    return error_type();
}
error_type window_data_sys::show(const bool value) noexcept
{
#if X11_GUI

#elif _WIN32
    auto style = win32_get_window_long(m_hwnd, GWL_STYLE);
    style &= ~WS_VISIBLE;
    if (value)
        style |= WS_VISIBLE;
    win32_set_window_longptr(m_hwnd, GWL_STYLE, style);
#endif
    return error_type();
}
error_type window_data_sys::cursor(const uint32_t priority, shared_ptr<window_cursor> cursor) noexcept
{
    auto it = m_cursor.refs.find(priority);
    if (it == m_cursor.refs.end())
    {
        if (cursor)
            ICY_ERROR(m_cursor.refs.insert(priority, std::move(cursor)));
    }
    else
    {
        if (cursor)
            it->value = std::move(cursor);
        else
            m_cursor.refs.erase(it);
    }

    if (!m_cursor.refs.empty())
    {
        auto cursor_data = static_cast<window_cursor_data*>(m_cursor.refs.back().value.get());
        m_cursor.handle = cursor_data->handle();
    }
    else
    {
        m_cursor.handle = m_cursor.arrow;
    }
    return error_type();
}
error_type window_data_sys::repaint(string&& tag, array<window_render_item>&& items) noexcept
{
    auto it = m_repaint.find(tag);
    if (it == m_repaint.end())
    {
        ICY_ERROR(m_repaint.insert(std::move(tag), array<window_render_item>(), &it));
    }
    it->value = std::move(items);
    if (!win32_invalidate_rect(m_hwnd, nullptr, TRUE))
        return last_system_error();
    return error_type();
}
error_type window_data_sys::resize(bool& change) noexcept
{
#if X11_GUI
    auto display = &system.display();

    XWindowAttributes attr;
    if (!XGetWindowAttributes(display, m_hwnd, &attr))
        return system.error();

    if (m_image && m_image->width == attr.width && m_image->height == attr.height)
    {
        change = false;
        return error_type();
    }

    auto data = static_cast<char*>(malloc(attr.width * attr.height * sizeof(color)));
    if (!data)
        return make_stdlib_error(std::errc::not_enough_memory);

    if (m_image)
    {
        for (auto row = 0; row < std::min(m_image->height, attr.height); ++row)
        {
            auto dst = data + row * attr.width * sizeof(color);
            auto src = m_image->data + row * m_image->width * sizeof(color);
            auto len = std::min(m_image->width, attr.width) * sizeof(color);
            memcpy(dst, src, len);
            if (attr.width > m_image->width)
                memset(dst + len, 0, attr.width * sizeof(color) - len);
        }
        for (auto row = std::min(m_image->height, attr.height); row < attr.height; ++row)
        {
            auto dst = data + row * attr.width * sizeof(color);
            memset(dst, 0, attr.width * sizeof(color));
        }
    }
    else
    {
        memset(data, 0, attr.width * attr.height * sizeof(color));
    }

    auto image = XCreateImage(display, attr.visual, attr.depth, ZPixmap, 0, data, attr.width, attr.height, 32, 0);
    if (!image)
    {
        free(data);
        return make_stdlib_error(std::errc::not_enough_memory);
    }
    if (m_image)
    {
        XDestroyImage(m_image);
        m_image = nullptr;
    }
    m_image = image;
    change = true;
    m_size.x = attr.width;
    m_size.y = attr.height;
#endif
    return error_type();
}
error_type window_data_usr::win_handle(void*& handle) const noexcept
{
#if X11_GUI
    return error_type();
#elif _WIN32
    if (auto hwnd = m_handle.load())
    {
        handle = hwnd;
        return error_type();
    }
    auto system = shared_ptr<window_system_data>(m_system);
    if (!system)
        return make_stdlib_error(std::errc::invalid_argument);

    internal_message msg;
    msg.type = internal_message_type::query;
    msg.index = m_index;
    ICY_ERROR(make_shared(msg.query));
    auto query = msg.query;
    ICY_ERROR(query->sync.initialize());
    ICY_ERROR(system->post(std::move(msg)));
    for (auto k = 0u; k < 5u; ++k)
    {
        if (const auto error = query->sync.wait(std::chrono::seconds(1)))
        {
            if (error != make_stdlib_error(std::errc::timed_out))
                return error;
        }
        if (handle = query->hwnd.load())
        {
            m_handle = HWND(handle);
            break;
        }
    }
    return error_type();
#endif
}

#if X11_GUI

#elif _WIN32
LRESULT window_data_sys::proc(uint32_t msg, WPARAM wparam, LPARAM lparam) noexcept
{
    const auto post_message = [this](window_message& message, const event_type type) noexcept
    {
        message.window = m_index;

        if (m_win32_flags & win32_flag::inactive) message.state = message.state | window_state::inactive;
        if (m_win32_flags & win32_flag::minimized) message.state = message.state | window_state::minimized;
        if (m_style & window_style::popup) message.state = message.state | window_state::popup;
        if (m_style & window_style::maximized) message.state = message.state | window_state::maximized;

        RECT rect;
        if (!win32_get_client_rect(m_hwnd, &rect))
        {
            m_system.exit(last_system_error());
            return false;
        }
        message.size.x = rect.right - rect.left;
        message.size.y = rect.bottom - rect.top;


        if (type == event_type::window_input 
            && (message.input.type == input_type::key_press 
            || message.input.type == input_type::key_hold))
        {
            window_action action_message;
            action_message.window = m_index;

            const auto mods = key_mod(message.input.mods);
            switch (message.input.key)
            {
            case key::z:
            {
                if (key_mod_and(key_mod::lctrl | key_mod::rctrl, mods))
                    action_message.type = window_action_type::undo;
                break;
            }
            case key::y:
            {
                if (key_mod_and(key_mod::lctrl | key_mod::rctrl, mods))
                    action_message.type = window_action_type::redo;
                break;
            }
            case key::c:
            {
                if (key_mod_and(key_mod::lctrl | key_mod::rctrl, mods))
                    action_message.type = window_action_type::copy;
                break;
            }
            case key::v:
            {
                if (key_mod_and(key_mod::lctrl | key_mod::rctrl, mods))
                {
                    action_message.type = window_action_type::paste;

                    if (!win32_open_clipboard(m_hwnd))
                        break;

                    ICY_SCOPE_EXIT{ win32_close_clipboard(); };
                    auto handle = win32_get_clipboard_data(CF_UNICODETEXT);
                    if (!handle)
                        break;

                    const auto ptr = static_cast<const wchar_t*>(GlobalLock(handle));
                    if (!ptr)
                        break;
                    ICY_SCOPE_EXIT{ GlobalUnlock(handle); };
                    to_string(const_array_view<wchar_t>(ptr, wcslen(ptr)), action_message.clipboard_text);
                }
                break;
            }
            case key::x:
            {
                if (key_mod_and(key_mod::lctrl | key_mod::rctrl, mods))
                    action_message.type = window_action_type::cut;
                break;
            }
            case key::a:
            {
                if (key_mod_and(key_mod::lctrl | key_mod::rctrl, mods))
                    action_message.type = window_action_type::select_all;
                break;
            }
            }
            if (action_message.type != window_action_type::none)
            {
                if (const auto error = event::post(&m_system, event_type::window_action, std::move(action_message)))
                {
                    m_system.exit(error);
                    return false;
                }
            }
        }

        if (const auto error = event::post(&m_system, type, std::move(message)))
        {
            m_system.exit(error);
            return false;
        }

        return true;
    };
    const auto on_repaint = [this]
    {
         RECT rect;
         if (!win32_get_client_rect(m_hwnd, &rect))
             return last_system_error();

         PAINTSTRUCT ps;
         auto hdc = win32_begin_paint(m_hwnd, &ps);
         if (!hdc)
             return last_system_error();

         auto end_paint_done = false;
         ICY_SCOPE_EXIT{ if (!end_paint_done) win32_end_paint(m_hwnd, &ps); };

         auto& render = m_system.render();
         ICY_COM_ERROR(render.BindDC(hdc, &rect));
         render.BeginDraw();
         com_ptr<ID2D1SolidColorBrush> brush;
         ICY_COM_ERROR(render.CreateSolidColorBrush(D2D1::ColorF(0), &brush));

         auto end_draw_done = false;
         ICY_SCOPE_EXIT{ if (!end_draw_done) render.EndDraw(); };
       

         const auto to_color = [](const color color)
         {
             return D2D1::ColorF(color.to_rgb(), color.a / 255.0f);
         };
         const auto to_rect = [](const window_render_item& item)
         {
             return D2D1::RectF(item.min_x, item.min_y, item.max_x, item.max_y);
         };
         for (auto&& pair : m_repaint)
         {
             for (auto&& item : pair.value)
             {
                 switch (item.type)
                 {
                 case window_render_item_type::clear:
                 {
                     render.Clear(to_color(item.color));
                     break;
                 }
                 case window_render_item_type::clip_push:
                 {
                     render.PushAxisAlignedClip(to_rect(item), D2D1_ANTIALIAS_MODE_PER_PRIMITIVE);
                     break;
                 }
                 case window_render_item_type::clip_pop:
                 {
                     render.PopAxisAlignedClip();
                     break;
                 }
                 case window_render_item_type::rect:
                 {
                     brush->SetColor(to_color(item.color));
                     render.FillRectangle(to_rect(item), brush);
                     break;
                 }
                 case window_render_item_type::text:
                 {
                     brush->SetColor(to_color(item.color));
                     render.DrawTextLayout({ item.min_x, item.min_y }, static_cast<IDWriteTextLayout*>(item.handle), brush);
                     break;
                 }
                 case window_render_item_type::image:
                 {
                     if (auto image = m_system.image(item.blob))
                         render.DrawBitmap(image, to_rect(item));
                     //brush->SetColor(to_color(item.color));
                     //render.DrawTextLayout({ item.min_x, item.min_y }, static_cast<IDWriteTextLayout*>(item.handle), brush);
                     break;
                 }
                 default:
                     break;
                 }
             }
         }
         end_draw_done = true;
         ICY_COM_ERROR(render.EndDraw());

         end_paint_done = true;
         if (!win32_end_paint(m_hwnd, &ps))
             return last_system_error();

        return error_type();
    };

    switch (msg)
    {
    //case WM_CLIPBOARDUPDATE:
    //  break;

    case WM_PAINT:
    {
        if (const auto error = on_repaint())
            m_system.exit(error);
        break;
    }

    case WM_MENUCHAR:
        return MAKELRESULT(0, MNC_CLOSE);

    case WM_SETCURSOR:
        //window->g_cursor.wnd_proc({ hwnd, msg, wparam, lparam });
        if (LOWORD(lparam) == HTCLIENT)
        {
            win32_set_cursor(static_cast<HCURSOR>(m_cursor.handle));
            return TRUE;
        }
        break;

    case WM_SYSCOMMAND:
        if ((wparam & 0xFFF0) == SC_KEYMENU)
            return 0;
        break;

    case WM_CLOSE:
    {
        window_message message;
        message.state = message.state | window_state::closing;
        post_message(message, event_type::window_state);
        if (m_flags & window_flags::quit_on_close)
            post_quit_event();
        return 0;
    }

    case WM_COPYDATA:
    {
        window_copydata message;
        const auto cdata = reinterpret_cast<COPYDATASTRUCT*>(lparam);
        const auto ptr = static_cast<const uint8_t*>(cdata->lpData);
        message.user = cdata->dwData;
        message.window = m_index;
        if (const auto error = message.bytes.append(const_array_view<uint8_t>(ptr, cdata->cbData)))
        {
            m_system.exit(error);
        }
        else
        {
            if (const auto error = event::post(&m_system, event_type::window_data, std::move(message)))
                m_system.exit(error);
        }
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
            if (!post_message(message, event_type::window_resize))
                break;
        }
        m_win32_flags &= ~(win32_flag::resizing1 | win32_flag::resizing2);
        break;

    case WM_SIZE:
    {
        if (wparam == SIZE_MINIMIZED)
        {
            m_win32_flags |= win32_flag::minimized;
            window_message message;
            post_message(message, event_type::window_state);
            break;
        }
        else if (m_win32_flags & win32_flag::minimized)
        {
            m_win32_flags &= ~win32_flag::minimized;
            window_message message;
            post_message(message, event_type::window_state);
            break;
        }
        else if (wparam == SIZE_MAXIMIZED || (wparam == SIZE_RESTORED && (m_win32_flags & win32_flag::resizing1) == 0 && (m_win32_flags & win32_flag::resizing2) == 0))
        {
            window_message message;
            post_message(message, event_type::window_resize);
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
        post_message(message, event_type::window_state);
        break;
    }

    case WM_CHAR:
        // 0xD800–0xDBFF;
        if (wparam < HIGH_SURROGATE_START)
        {
            auto wchr = wchar_t(wparam);
            if (wchr == '\b')
                break;

            string str;
            to_string(const_array_view<wchar_t>(&wchr, 1), str);
            window_message message;
            message.input = input_message(str);
            post_message(message, event_type::window_input);
        }
        else if (IS_HIGH_SURROGATE(wparam))
        {
            m_chr = wchar_t(wparam);
        }
        else if (IS_SURROGATE_PAIR(m_chr, wparam))
        {
            const wchar_t wide[] = { m_chr, wchar_t(wparam) };
            string str;
            to_string(const_array_view<wchar_t>(wide), str);
            window_message message;
            message.input = input_message(str);
            post_message(message, event_type::window_input);
            m_chr = 0;
        }
        break;

    case WM_KEYDOWN:
    case WM_KEYUP:
    case WM_SYSKEYDOWN:
    case WM_SYSKEYUP:
    {
        uint8_t keyboard[0x100];
        if (!win32_get_keyboard_state(keyboard))
            memset(keyboard, 0, sizeof(keyboard));

        input_message key_input;
        auto count = detail::key_from_winapi(key_input, wparam, lparam, keyboard);
        for (auto k = 0u; k < count; ++k)
        {
            window_message message;
            message.input = key_input;
            if (!post_message(message, event_type::window_input))
                break;
        }
        break;
    }
    case WM_MOUSEWHEEL:
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
        uint8_t keyboard[0x100];
        if (!win32_get_keyboard_state(keyboard))
            memset(keyboard, 0, sizeof(keyboard));

        window_message message;
        detail::mouse_from_winapi(message.input, msg, wparam, lparam, keyboard);
        post_message(message, event_type::window_input);
        if (msg == WM_MOUSEWHEEL)
        {
            auto point = POINT{ GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam) };
            if (win32_screen_to_client(m_hwnd, &point))
            {
                message.input.point_x = point.x;
                message.input.point_y = point.y;
            }
        }
        if (message.input.type == input_type::mouse_press || message.input.type == input_type::mouse_double)
        {
            if (++m_capture == 1)
                win32_set_capture(m_hwnd);
        }
        else if (message.input.type == input_type::mouse_release)
        {
            if (--m_capture == 0)
                win32_release_capture();
        }

        break;
    }
    }
    return win32_def_window_proc(m_hwnd, msg, wparam, lparam);
}
#endif

error_type window_data_usr::restyle(const window_style style) noexcept
{
    auto system = shared_ptr<window_system_data>(m_system);
    if (!system)
        return make_stdlib_error(std::errc::invalid_argument);

    internal_message msg;
    msg.type = internal_message_type::restyle;
    msg.index = m_index;
    msg.style = style;
    return system->post(std::move(msg));
}
error_type window_data_usr::rename(const string_view name) noexcept
{
    auto system = shared_ptr<window_system_data>(m_system);
    if (!system)
        return make_stdlib_error(std::errc::invalid_argument);

    internal_message msg;
    msg.type = internal_message_type::rename;
    msg.index = m_index;
    ICY_ERROR(copy(name, msg.name));
    return system->post(std::move(msg));
}
error_type window_data_usr::show(const bool value) noexcept
{
    auto system = shared_ptr<window_system_data>(m_system);
    if (!system)
        return make_stdlib_error(std::errc::invalid_argument);

    internal_message msg;
    msg.type = internal_message_type::show;
    msg.index = m_index;
    msg.show = value;
    return system->post(std::move(msg));
}
error_type window_data_usr::cursor(const uint32_t priority, const shared_ptr<window_cursor> cursor) noexcept
{
    auto system = shared_ptr<window_system_data>(m_system);
    if (!system)
        return make_stdlib_error(std::errc::invalid_argument);

    internal_message msg;
    msg.type = internal_message_type::cursor;
    msg.index = m_index;
    msg.cursor = { cursor, priority };
    return system->post(std::move(msg));
}
error_type window_data_usr::repaint(const string_view tag, array<window_render_item>& items) noexcept
{
    auto system = shared_ptr<window_system_data>(m_system);
    if (!system)
        return make_stdlib_error(std::errc::invalid_argument);

    internal_message msg;
    msg.type = internal_message_type::repaint;
    msg.index = m_index;
    ICY_ERROR(copy(tag, msg.name));
    msg.repaint = std::move(items);
    return system->post(std::move(msg));
}

error_type window_cursor_data::initialize() noexcept
{
#if X11_GUI

#elif _WIN32
    ICY_ERROR(m_library.initialize());
    wchar_t* resource = nullptr;
    switch (m_type)
    {
    case type::arrow:
        resource = IDC_ARROW;
        break;
    case type::arrow_wait:
        resource = IDC_APPSTARTING;
        break;
    case type::wait:
        resource = IDC_WAIT;
        break;
    case type::cross:
        resource = IDC_CROSS;
        break;
    case type::hand:
        resource = IDC_HAND;
        break;
    case type::help:
        resource = IDC_HELP;
        break;
    case type::ibeam:
        resource = IDC_IBEAM;
        break;
    case type::no:
        resource = IDC_NO;
        break;
    case type::size:
        resource = IDC_SIZEALL;
        break;
    case type::size_x:
        resource = IDC_SIZEWE;
        break;
    case type::size_y:
        resource = IDC_SIZENS;
        break;
    case type::size_diag0:
        resource = IDC_SIZENWSE;
        break;

    case type::size_diag1:
        resource = IDC_SIZENESW;
        break;
    }

    if (resource)
    {
        const auto func = ICY_FIND_FUNC(m_library, LoadCursorW);
        if (!func)
            return make_stdlib_error(std::errc::function_not_supported);
        m_handle = static_cast<HCURSOR>(func(nullptr, resource));
    }

    if (!m_handle)
        return last_system_error();
#endif

    return error_type();
}

window_system_data::~window_system_data() noexcept
{
    if (m_thread)
        m_thread->wait();
#if X11_GUI
    if (m_display)
        XCloseDisplay(m_display);
#elif _WIN32
    if (*m_cname)
        win32_unregister_class(m_cname, win32_instance());
#endif
    filter(0);
}
error_type window_system_data::initialize() noexcept
{
    ICY_ERROR(m_lock.initialize());

#if X11_GUI
    if (!XInitThreads())
        return make_stdlib_error(std::errc::not_enough_memory);

    m_display = XOpenDisplay("127.0.0.1:0.0");
    if (!m_display)
        return make_stdlib_error(std::errc::not_enough_memory);
#elif _WIN32
    ICY_ERROR(m_lib_d2d1.initialize());
    ICY_ERROR(m_lib_dwrite.initialize());
    ICY_ERROR(m_lib_user32.initialize());
    ICY_ERROR(m_lib_gdi32.initialize());

    if (const auto func = ICY_FIND_FUNC(m_lib_user32, SetThreadDpiAwarenessContext))
        func(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);

    decltype(&::RegisterClassExW) win32_register_class = nullptr;
    
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
    ICY_USER32_FUNC(win32_set_layered_window_attributes, SetLayeredWindowAttributes);
    ICY_USER32_FUNC(win32_set_window_pos, SetWindowPos);
    ICY_USER32_FUNC(win32_destroy_cursor, DestroyCursor);
    ICY_USER32_FUNC(win32_load_cursor, LoadCursorW);
    ICY_USER32_FUNC(win32_set_cursor, SetCursor);
    ICY_USER32_FUNC(win32_get_dpi_for_window, GetDpiForWindow);
    ICY_USER32_FUNC(win32_set_capture, SetCapture);
    ICY_USER32_FUNC(win32_release_capture, ReleaseCapture);
    ICY_USER32_FUNC(win32_open_clipboard, OpenClipboard);
    ICY_USER32_FUNC(win32_close_clipboard, CloseClipboard);
    ICY_USER32_FUNC(win32_get_clipboard_data, GetClipboardData);
    ICY_USER32_FUNC(win32_screen_to_client, ScreenToClient);

    m_cursor = static_cast<HCURSOR>(win32_load_cursor(nullptr, IDC_ARROW));
    if (!m_cursor)
        return last_system_error();

    swprintf_s(m_cname, L"Icy Window %u.%u", GetCurrentProcessId(), GetCurrentThreadId());

    WNDCLASSEXW cls = { sizeof(WNDCLASSEXW) };
    cls.lpszClassName = m_cname;
    cls.style = CS_HREDRAW | CS_VREDRAW | CS_DBLCLKS;
    cls.lpfnWndProc = win32_def_window_proc;
    cls.hInstance = win32_instance();
    cls.hCursor = m_cursor;
    if (win32_register_class(&cls) == 0)
    {
        memset(m_cname, 0, sizeof(m_cname));
        return last_system_error();
    }


    using d2d1_func_type = HRESULT(WINAPI *)(D2D1_FACTORY_TYPE, REFIID, const D2D1_FACTORY_OPTIONS*, void**);
    if (auto func = static_cast<d2d1_func_type>(m_lib_d2d1.find("D2D1CreateFactory")))
    {
        D2D1_FACTORY_OPTIONS options = {};
#if _DEBUG
        options.debugLevel = D2D1_DEBUG_LEVEL::D2D1_DEBUG_LEVEL_WARNING;
#endif
        ICY_COM_ERROR(func(D2D1_FACTORY_TYPE_MULTI_THREADED, 
            __uuidof(ID2D1Factory), &options, reinterpret_cast<void**>(&m_factory)));
        const auto pixel = D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED);
        const auto desc = D2D1::RenderTargetProperties(D2D1_RENDER_TARGET_TYPE::D2D1_RENDER_TARGET_TYPE_DEFAULT, pixel);
        ICY_COM_ERROR(m_factory->CreateDCRenderTarget(&desc, &m_render));
    }
    else
    {
        return make_stdlib_error(std::errc::function_not_supported);
    }
#endif

    ICY_ERROR(make_shared(m_thread));
    m_thread->system = this;

    uint64_t mask = event_type::system_internal;
#if X11_GUI
    mask |= event_type::global_timer;
#endif
    filter(mask);
    return error_type();
}
error_type window_system_data::exec() noexcept
{
    const auto timeout = max_timeout;
    const auto ms = ms_timeout(timeout);

    map<uint32_t, window_data_sys> windows;
#if X11_GUI
    ICY_ERROR(make_unique(jmp_type(), m_jmp));
    if (setjmp(m_jmp->buf))
    {
        event::post(nullptr, event_type::global_quit);
        return m_error;
    }
    XErrorHandler handler = [](Display* display, XErrorEvent* event)
    {
        auto system = static_cast<window_system_data*>(reinterpret_cast<_XPrivDisplay>(display)->_user);
        system->m_error = make_x11_error(event->error_code);
        return 0;
    };
    XIOErrorHandler io_handler = [](Display* display)
    {
        auto system = static_cast<window_system_data*>(reinterpret_cast<_XPrivDisplay>(display)->_user);
        system->m_error = last_network_error();
        longjmp(system->m_jmp->buf, 1);
        return 0;
    };
    reinterpret_cast<_XPrivDisplay>(m_display)->_user = this;
    XSetErrorHandler(handler);
    XSetIOErrorHandler(io_handler);
    _putenv("_XKB_CHARSET=utf8");
#endif

    while (*this)
    {
        while (auto event = pop())
        {
#if X11_GUI
            if (event->type == event_type::global_timer)
            {
                const auto event_data = event->data<timer::pair>();
                for (auto&& pair : windows)
                {
                    if (pair.value.get_timer() == event_data.timer)
                    {
                        auto change = false;
                        ICY_ERROR(pair.value.resize(*this, change));
                        if (change)
                        {
                            window_message msg;
                            msg.window = pair.key;
                            msg.size.x = pair.value.get_size().x;
                            msg.size.y = pair.value.get_size().y;
                            ICY_ERROR(event::post(this, event_type::window_resize, msg));
                        }
                    }
                }
                continue;
            }
#endif
            if (event->type != event_type::system_internal)
                continue;

            auto& event_data = event->data<internal_message>();
            auto it = windows.find(event_data.index);
            if (event_data.type == internal_message_type::create)
            {
                if (it == windows.end())
                {
                    ICY_ERROR(windows.insert(event_data.index, window_data_sys(*this, event_data.index), &it));
                    ICY_ERROR(it->value.initialize(m_cname, event_data.flags));
                }
            }
            else if (event_data.type == internal_message_type::destroy)
            {
                if (it != windows.end())
                    windows.erase(it);
            }
            else if (event_data.type == internal_message_type::show)
            {
                ICY_ERROR(it->value.show(event_data.show));
            }
            else if (event_data.type == internal_message_type::rename)
            {
                ICY_ERROR(it->value.rename(event_data.name));
            }
            else if (event_data.type == internal_message_type::restyle)
            {
                ICY_ERROR(it->value.restyle(event_data.style));
            }
            else if (event_data.type == internal_message_type::cursor)
            {
                ICY_ERROR(it->value.cursor(event_data.cursor.priority, event_data.cursor.value));
            }
            else if (event_data.type == internal_message_type::repaint)
            {
                for (auto&& item : event_data.repaint)
                {
                    if (item.type == window_render_item_type::image)
                    {
                        auto jt = m_images.find(item.blob.index);
                        if (jt == m_images.end())
                        {
                            const auto bytes = blob_data(item.blob);

                            icy::image image;
                            ICY_ERROR(image.load(global_realloc, nullptr, bytes, image_type::png));
                            matrix<color> colors(image.size().y, image.size().x);
                            if (colors.empty())
                                return make_stdlib_error(std::errc::not_enough_memory);
                            ICY_ERROR(image.view(image_size(), colors));

                            com_ptr<ID2D1Bitmap> bitmap;
                            auto props = D2D1::BitmapProperties(
                                D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED));
                            ICY_COM_ERROR(m_render->CreateBitmap(D2D1::SizeU(image.size().x, image.size().y),
                                colors.data(), uint32_t(colors.cols() * 4), props, &bitmap));
                            ICY_ERROR(m_images.insert(item.blob.index, std::move(bitmap)));
                        }
                    }
                }
                ICY_ERROR(it->value.repaint(std::move(event_data.name), std::move(event_data.repaint)));
            }
            else if (event_data.type == internal_message_type::query)
            {
                event_data.query->hwnd = it->value.handle();
                ICY_ERROR(event_data.query->sync.wake());
            }
        }

#if X11_GUI
        XEvent event;
        while (XPending(m_display))
        {
            XNextEvent(m_display, &event);

            window_data_sys* window = nullptr; 
            window_message msg;
            for (auto&& pair : windows)
            {
                if (pair.value.handle() != event.xany.window)
                    continue;
                window = &pair.value;
                msg.window = pair.key;
                break;
            }
            if (!window)
                continue;

            const auto& set_mods = [](uint32_t state)
            {
                auto mods = key_mod::none;
                if (state & ShiftMask)
                    mods = (mods | key_mod::lshift);
                if (state & ControlMask)
                    mods = (mods | key_mod::lctrl);
                if (state & Mod1Mask)
                    mods = (mods | key_mod::lalt);
                return mods;
            };

            if (event.type == Expose && event.xexpose.count == 0)
            {
                ICY_ERROR(window->repaint(*this, nullptr));
            }
            else if (event.type == ConfigureNotify)
            {
                ICY_ERROR(window->start_timer(std::chrono::milliseconds(250)));
            }
            else if (event.type == MotionNotify)
            {
                msg.input = input_message(input_type::mouse_move, key::none, set_mods(event.xmotion.state));
                msg.input.point_x = event.xmotion.x;
                msg.input.point_y = event.xmotion.y;
                ICY_ERROR(event::post(this, event_type::window_input, msg));
            }
            else if (event.type == KeyPress)
            {
                char text[256];
                KeySym key = 0;
                XComposeStatus compose = {};
                auto count = XLookupString(&event.xkey, text, sizeof(text), &key, &compose);
                continue;
            }
            else if (event.type == ButtonPress || event.type == ButtonRelease)
            {
                auto key = key::none;
                switch (event.xbutton.button)
                {
                case Button1:
                    key = key::mouse_left;
                    break;
                case Button2:
                    key = key::mouse_right;
                    break;
                case Button3:
                    key = key::mouse_mid;
                    break;
                case Button4:
                    key = key::mouse_x1;
                    break;
                case Button5:
                    key = key::mouse_x2;
                    break;
                }
                if (key == key::none)
                    continue;

                auto type = event.type == ButtonPress ? input_type::mouse_press : input_type::mouse_release;
                msg.input = input_message(type, key, set_mods(event.xbutton.state));
                msg.input.point_x = event.xbutton.x;
                msg.input.point_y = event.xbutton.y;
                ICY_ERROR(event::post(this, event_type::window_input, msg));
            }
        }

        const SOCKET socket = ConnectionNumber(m_display);
        timeval timeout;
        timeout.tv_sec = 0;
        timeout.tv_usec = 500000;
        
        fd_set fd_recv;
        FD_ZERO(&fd_recv);

        FD_SET(socket, &fd_recv);
        // FD_SET(m_socket, &fd_recv);

        auto count = select(int(socket) + 1, &fd_recv, nullptr, nullptr, &timeout);
        if (count >= 0)
        {
            
        }
        else
        {
            return last_system_error();
        }
#elif _WIN32
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
#endif
    }
    return error_type();
}
error_type window_system_data::post(internal_message&& msg) noexcept
{
    return event_system::post(nullptr, event_type::system_internal, std::move(msg));
}
error_type window_system_data::create(shared_ptr<window>& new_window, const window_flags flags) noexcept
{
    shared_ptr<window_data_usr> ptr;
    auto index = m_index.fetch_add(1, std::memory_order_acq_rel) + 1;

    ICY_ERROR(make_shared(ptr, make_shared_from_this(this), flags, index));
    internal_message msg;
    msg.type = internal_message_type::create;
    msg.flags = flags;
    msg.index = index;
    ICY_ERROR(post(std::move(msg)));
    new_window = std::move(ptr);
    return error_type();
}
void window_system_data::exit(const error_type error) noexcept
{
    m_error = error;
}

error_type icy::create_window_cursor(shared_ptr<window_cursor>& cursor, const window_cursor::type type) noexcept
{
    shared_ptr<window_cursor_data> new_ptr;
    if (type != window_cursor::type::none)
    {
        ICY_ERROR(make_shared(new_ptr, type));
        ICY_ERROR(new_ptr->initialize());
    }
    cursor = std::move(new_ptr);
    return error_type();
}
error_type icy::create_window_cursor(shared_ptr<window_cursor>& cursor, const_array_view<uint8_t> bytes) noexcept
{
    return make_stdlib_error(std::errc::function_not_supported);
}
error_type icy::create_window_system(shared_ptr<window_system>& system) noexcept
{
    shared_ptr<window_system_data> new_ptr;
    ICY_ERROR(make_shared(new_ptr));
    ICY_ERROR(new_ptr->initialize());
    system = std::move(new_ptr);
    return error_type();
}

window_render_item::~window_render_item() noexcept
{
    if (handle)
        static_cast<IUnknown*>(handle)->Release();
}
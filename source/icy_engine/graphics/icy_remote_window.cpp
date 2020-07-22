#include <icy_engine/graphics/icy_remote_window.hpp>
#include <icy_engine/core/icy_string.hpp>
#include <dwmapi.h>
#include <Windows.h>

using namespace icy;

#define ICY_WIN32_FUNC(X, Y) X = ICY_FIND_FUNC(lib, Y); if (!X) return make_stdlib_error(std::errc::function_not_supported) 

static decltype(&::SetWindowTextW) win32_set_window_text;
static decltype(&::GetWindowTextW) win32_get_window_text;
static decltype(&::GetWindowTextLengthW) win32_get_window_text_length;
static decltype(&::GetWindowRect) win32_get_window_rect;
static decltype(&::PostThreadMessageW) win32_post_thread_message;
static decltype(&::SetWindowsHookExW) win32_set_windows_hook_ex;
static decltype(&::UnhookWindowsHookEx) win32_unhook_windows_hook_ex;
static decltype(&::SendMessageTimeoutW) win32_send_message_timeout;
static decltype(&::SendMessageW) win32_send_message;

class remote_window::data_type
{
public:
    error_type hook(const library& lib, const char* func, int type) noexcept;
public:
    std::atomic<uint32_t> ref = 1;
    HWND handle = nullptr;
    library lib = "user32"_lib;
    uint32_t process = 0;
    uint32_t thread = 0;
    HHOOK msghook = nullptr;
};

error_type remote_window::data_type::hook(const library& lib, const char* const func, const int type) noexcept
{
    if (!lib.handle())
        return make_stdlib_error(std::errc::invalid_argument);

    HOOKPROC proc = nullptr;
    if (func)
        proc = static_cast<HOOKPROC>(lib.find(func));

    if (!win32_unhook_windows_hook_ex || !win32_set_windows_hook_ex)
        return make_stdlib_error(std::errc::function_not_supported);

    const auto hook_ptr = &msghook;
    if (*hook_ptr)
    {
        win32_unhook_windows_hook_ex(*hook_ptr);
        *hook_ptr = nullptr;
    }
    if (proc)
    {
        *hook_ptr = win32_set_windows_hook_ex(type, proc, lib.handle(), thread);
        if (!*hook_ptr)
            return last_system_error();
    }
    return error_type();
}

error_type remote_window::enumerate(array<remote_window>& vec) noexcept
{
    auto lib = "user32"_lib;
    ICY_ERROR(lib.initialize());

    decltype(&EnumWindows) win32_enum_windows = nullptr;
    struct win32_find_window
    {
        error_type error;
        array<HWND> vec;
        decltype(&IsWindowVisible) win32_is_window_visible = nullptr;
        decltype(&IsWindowEnabled) win32_is_window_enabled = nullptr;
        decltype(&GetWindow) win32_get_window = nullptr;
        decltype(&GetWindowLongW) win32_get_window_long = nullptr;
        decltype(&GetTitleBarInfo) win32_title_bar_info = nullptr;
        decltype(&GetAncestor) win32_get_ancestor = nullptr;
        decltype(&GetLastActivePopup) win32_get_last_active_popup = nullptr;

        decltype(&DwmGetWindowAttribute) win32_dwm_get_window_attribute = nullptr;
    };
    win32_find_window data;

    ICY_WIN32_FUNC(win32_enum_windows, EnumWindows);
    ICY_WIN32_FUNC(data.win32_is_window_visible, IsWindowVisible);
    ICY_WIN32_FUNC(data.win32_is_window_enabled, IsWindowEnabled);
    ICY_WIN32_FUNC(data.win32_get_window, GetWindow);
    ICY_WIN32_FUNC(data.win32_get_window_long, GetWindowLongW);
    ICY_WIN32_FUNC(data.win32_title_bar_info, GetTitleBarInfo);
    ICY_WIN32_FUNC(data.win32_get_ancestor, GetAncestor);
    ICY_WIN32_FUNC(data.win32_get_last_active_popup, GetLastActivePopup);

    ICY_WIN32_FUNC(win32_get_window_text_length, GetWindowTextLengthW);
    ICY_WIN32_FUNC(win32_get_window_text, GetWindowTextW);
    ICY_WIN32_FUNC(win32_get_window_rect, GetWindowRect);
    ICY_WIN32_FUNC(win32_send_message_timeout, SendMessageTimeoutW);
    ICY_WIN32_FUNC(win32_send_message, SendMessageW);

    library dwm = "dwmapi"_lib;
    if (dwm.initialize() == error_type())
        data.win32_dwm_get_window_attribute = ICY_FIND_FUNC(dwm, DwmGetWindowAttribute);
    
    const auto proc = [](HWND hwnd, LPARAM ptr)
    {
        const auto self = reinterpret_cast<win32_find_window*>(ptr);
        const auto is_alt_tab_window = [self](HWND hwnd)
        {
            //if (!self->win32_is_window_visible(hwnd))
            //    return false;

            wchar_t wtext[256];
            auto wlen = win32_get_window_text(hwnd, wtext, _countof(wtext));
            if (!wlen)
                return false;

           /* auto hwnd_walk = HWND(nullptr);
            auto hwnd_try = self->win32_get_ancestor(hwnd, GA_ROOTOWNER);
            while (hwnd_try != hwnd_walk)
            {
                hwnd_walk = hwnd_try;
                hwnd_try = self->win32_get_last_active_popup(hwnd_walk);
                if (self->win32_is_window_visible(hwnd_try))
                    break;
            }
            if (hwnd_walk != hwnd)
                return false;*/

            // the following removes some task tray programs and "Program Manager"

            /*TITLEBARINFO ti = { sizeof(ti) };
            self->win32_title_bar_info(hwnd, &ti);
            if (ti.rgstate[0] & STATE_SYSTEM_INVISIBLE)
                return false;*/

            // Tool windows should not be displayed either, these do not appear in the
            // task bar.
            if (self->win32_get_window_long(hwnd, GWL_EXSTYLE) & WS_EX_TOOLWINDOW)
               return false;

            auto style = self->win32_get_window_long(hwnd, GWL_STYLE);
            if ((style & (WS_POPUP | WS_THICKFRAME)) == 0)
                return false;
            if ((style & WS_VISIBLE) == 0)
                return false;


            if (!self->win32_is_window_enabled(hwnd))
                return false;

            auto hidden = 0;
            if (self->win32_dwm_get_window_attribute)
                self->win32_dwm_get_window_attribute(hwnd, DWMWA_CLOAKED, &hidden, sizeof(INT));

            if (hidden)
                return false;

            RECT rect = {};
            win32_get_window_rect(hwnd, &rect);
            if (rect.right <= rect.left || rect.bottom <= rect.top)
                return false;

            return true;
        };
        if (is_alt_tab_window(hwnd))
        {
            self->error = self->vec.push_back(hwnd);
            if (self->error)
                return 0;
        }
        return 1;
    };

    auto ok = win32_enum_windows(proc, reinterpret_cast<LPARAM>(&data));
    ICY_ERROR(data.error);
    if (!ok)
        return last_system_error();

    vec.clear();
    ICY_ERROR(vec.reserve(data.vec.size()));
    
    decltype(&GetWindowThreadProcessId) win32_get_window_pid = nullptr;
    ICY_WIN32_FUNC(win32_get_window_pid, GetWindowThreadProcessId);
    ICY_WIN32_FUNC(win32_set_window_text, SetWindowTextW);
    ICY_WIN32_FUNC(win32_post_thread_message, PostThreadMessageW);
    ICY_WIN32_FUNC(win32_set_windows_hook_ex, SetWindowsHookExW);
    ICY_WIN32_FUNC(win32_unhook_windows_hook_ex, UnhookWindowsHookEx);

    for (auto k = 0u; k < data.vec.size(); ++k)
    {
        const auto handle = data.vec[k];
        remote_window new_window;
        new_window.data = allocator_type::allocate<remote_window::data_type>(1);
        if (!new_window.data)
            return make_stdlib_error(std::errc::not_enough_memory);
        allocator_type::construct(new_window.data);
        
        new_window.data->handle = handle;
        ICY_ERROR(new_window.data->lib.initialize());
        
        auto pid = 0ul;
        new_window.data->thread = win32_get_window_pid(handle, &pid);
        if (!pid)
            return last_system_error();
        new_window.data->process = pid;
        ICY_ERROR(vec.push_back(std::move(new_window)));
    }
    return error_type();
}
remote_window::remote_window(const remote_window& rhs) noexcept : data(rhs.data)
{
    if (data)
        data->ref.fetch_add(1, std::memory_order_release);
}
remote_window::~remote_window() noexcept
{
    if (data && data->ref.fetch_sub(1, std::memory_order_acq_rel) == 1)
    {
        if (data->msghook && win32_unhook_windows_hook_ex) win32_unhook_windows_hook_ex(data->msghook);
        allocator_type::destroy(data);
        allocator_type::deallocate(data);
    }
}

error_type remote_window::name(string& str) const noexcept
{
    if (!data)
        return make_stdlib_error(std::errc::invalid_argument);
    if (!win32_get_window_text_length || !win32_get_window_text)
        return make_stdlib_error(std::errc::function_not_supported);
    
    const auto length = win32_get_window_text_length(data->handle);
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
    win32_get_window_text(data->handle, buffer.data(), length + 1);
    ICY_ERROR(to_string(buffer, str));
    return error_type();
}
uint32_t remote_window::thread() const noexcept
{
    return data ? data->thread : 0;
}
uint32_t remote_window::process() const noexcept
{
    return data ? data->process : 0;
}
HWND__* remote_window::handle() const noexcept
{
    return data ? data->handle : nullptr;
}
error_type remote_window::screen(const icy::render_d2d_rectangle_u& rect, matrix_view<color>& colors) const noexcept
{
    return make_stdlib_error(std::errc::function_not_supported);
}
error_type remote_window::hook(const library& lib, const char* func) noexcept
{
    if (!data)
        return make_stdlib_error(std::errc::invalid_argument);
    return data->hook(lib, func, WH_GETMESSAGE);
}
error_type remote_window::send(const input_message& msg, const duration_type timeout) noexcept
{
    if (!data)
        return make_stdlib_error(std::errc::invalid_argument);
    if (!win32_send_message_timeout)
        return make_stdlib_error(std::errc::function_not_supported);

    auto output = 0_z;
    auto win32_msg = detail::to_winapi(msg);
    if (win32_send_message_timeout(data->handle, win32_msg.message,
        win32_msg.wParam, win32_msg.lParam, SMTO_NOTIMEOUTIFNOTHUNG, ms_timeout(timeout), &output) == 0)
    //win32_send_message(data->handle, win32_msg.message, win32_msg.wParam, win32_msg.lParam);
    //if (true)
    {
        const auto error = last_system_error();
        if (error.code == make_system_error_code(ERROR_TIMEOUT))
            return make_stdlib_error(std::errc::timed_out);
        return error;
    }
    return error_type();
}
error_type remote_window::post(const uint32_t index, const size_t p0, const ptrdiff_t p1) noexcept
{
    return make_stdlib_error(std::errc::function_not_supported);
}
error_type remote_window::rename(const string_view name) noexcept
{
    if (!data)
        return make_stdlib_error(std::errc::invalid_argument);
    if (!win32_set_window_text)
        return make_stdlib_error(std::errc::function_not_supported);

    array<wchar_t> wname;
    ICY_ERROR(to_utf16(name, wname));
    if (!win32_set_window_text(data->handle, wname.data()))
        return last_system_error();
    return error_type();
}
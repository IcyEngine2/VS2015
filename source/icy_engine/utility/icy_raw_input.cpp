#include <icy_engine/core/icy_array.hpp>
#include <icy_engine/core/icy_string.hpp>
#include <icy_engine/utility/icy_raw_input.hpp>
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <Windows.h>

using namespace icy;

#define ICY_WIN32_FUNC(X, Y) X = ICY_FIND_FUNC(m_library, Y); if (!X) return make_stdlib_error(std::errc::function_not_supported) 

ICY_STATIC_NAMESPACE_BEG
static decltype(&::GetRawInputData) win32_get_raw_input_data;
static decltype(&::TranslateMessage) win32_translate_message;
static decltype(&::DispatchMessageW) win32_dispatch_message;
static decltype(&::GetKeyboardState) win32_get_keyboard_state;
static decltype(&::PeekMessageW) win32_peek_message;
static decltype(&::PostMessageW) win32_post_message;
static decltype(&::DefWindowProcW) win32_def_window_proc;
static decltype(&::DestroyWindow) win32_destroy_window;
static decltype(&::MsgWaitForMultipleObjectsEx) win32_msg_wait_for_multiple_objects_ex;
static decltype(&::GetWindowLongPtrW) win32_get_window_longptr;
static decltype(&::SetWindowLongPtrW) win32_set_window_longptr;
static decltype(&::UnregisterClassW) win32_unregister_class;
static decltype(&::GetCursorPos) win32_get_cursor_pos;

class raw_input_system : public event_loop
{
public:
    static LRESULT CALLBACK win32_proc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam);
public:
    raw_input_system() noexcept = default;
    ~raw_input_system() noexcept
    {   
        shutdown();
    }
private:
    error_type signal(const event_data&) noexcept override
    {
        if (!SetEvent(m_event))
            return last_system_error();
        return {};
    }
    error_type loop(event& event, const duration_type timeout) noexcept;
    void shutdown() noexcept
    {
        filter(0);
        if (m_window && win32_destroy_window)
        {
            win32_destroy_window(m_window);
            m_window = nullptr;
        }
        if (*m_cname && win32_unregister_class)
        {
            win32_unregister_class(m_cname, win32_instance());
            memset(m_cname, 0, sizeof(m_cname));
        }
        if (m_event)
        {
            CloseHandle(m_event);
            m_event = nullptr;
        }
        m_init = false;
    }
    error_type initialize() noexcept;
    void proc(HRAWINPUT input) noexcept;
private:
    void* m_event = nullptr;
    library m_library = "user32"_lib;
    wchar_t m_cname[64] = {};
    HWND m_window;
    error_type m_error;
    std::bitset<256> m_keyboard;
    bool m_init = false;
};

LRESULT CALLBACK raw_input_system::win32_proc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)
{
    if (msg == WM_INPUT)
    {
        const auto user = reinterpret_cast<raw_input_system*>(win32_get_window_longptr(hwnd, GWLP_USERDATA));
        user->proc(reinterpret_cast<HRAWINPUT>(lparam));
    }
    return win32_def_window_proc(hwnd, msg, wparam, lparam);
}
ICY_STATIC_NAMESPACE_END

void raw_input_system::proc(const HRAWINPUT input) noexcept
{
    auto size = 0u;
    if (win32_get_raw_input_data(input, RID_INPUT, nullptr, &size, sizeof(RAWINPUTHEADER)) == UINT32_MAX)
    {
        m_error = last_system_error();
        return;
    }

    array<uint8_t> bytes;
    if (const auto error = bytes.resize(size))
    {
        m_error = error;
        return;
    }
    
    if (win32_get_raw_input_data(input, RID_INPUT, bytes.data(), &size, sizeof(RAWINPUTHEADER)) != bytes.size())
    {
        m_error = last_system_error();
        return;
    }

    auto raw = reinterpret_cast<RAWINPUT*>(bytes.data());
    auto mods = key_mod::none;
    if (m_keyboard[size_t(key::left_ctrl)]) mods = mods | key_mod::lctrl;
    if (m_keyboard[size_t(key::left_alt)]) mods = mods | key_mod::lalt;
    if (m_keyboard[size_t(key::left_shift)]) mods = mods | key_mod::lshift;
    if (m_keyboard[size_t(key::right_ctrl)]) mods = mods | key_mod::rctrl;
    if (m_keyboard[size_t(key::right_alt)]) mods = mods | key_mod::ralt;
    if (m_keyboard[size_t(key::right_shift)]) mods = mods | key_mod::rshift;

    if (m_keyboard[size_t(key::mouse_left)]) mods = mods | key_mod::lmb;
    if (m_keyboard[size_t(key::mouse_right)]) mods = mods | key_mod::rmb;
    if (m_keyboard[size_t(key::mouse_mid)]) mods = mods | key_mod::mmb;
    if (m_keyboard[size_t(key::mouse_x1)]) mods = mods | key_mod::x1mb;
    if (m_keyboard[size_t(key::mouse_x2)]) mods = mods | key_mod::x2mb;

    if (raw->header.dwType == RIM_TYPEKEYBOARD)
    {
        auto type = input_type::none;
        const auto& key = raw->data.keyboard;
        const auto button = icy::detail::scan_vk_to_key(key.VKey, key.MakeCode, !!(key.Flags & RI_KEY_E0));
        if (key.Message == WM_KEYDOWN || key.Message == WM_SYSKEYDOWN)
        {
            type = input_type::key_press;
        }
        else if (key.Message == WM_KEYUP || key.Message == WM_SYSKEYUP)
        {
            type = input_type::key_release;
        }
        if (to_string(button).empty())
            return;

        m_keyboard[size_t(button)] = type == input_type::key_press;

        if (const auto error = event::post(this, event_type::window_input, input_message(type, button, mods)))
            m_error = error;
    }
    else if (raw->header.dwType == RIM_TYPEMOUSE)
    {
        auto type = input_type::none;
        const auto& mouse = raw->data.mouse;

        auto flags_press = 0;
        flags_press |= RI_MOUSE_BUTTON_1_DOWN;
        flags_press |= RI_MOUSE_BUTTON_2_DOWN;
        flags_press |= RI_MOUSE_BUTTON_3_DOWN;
        flags_press |= RI_MOUSE_BUTTON_4_DOWN;
        flags_press |= RI_MOUSE_BUTTON_5_DOWN;
        
        auto flags_release = 0;
        flags_release |= RI_MOUSE_BUTTON_1_UP;
        flags_release |= RI_MOUSE_BUTTON_2_UP;
        flags_release |= RI_MOUSE_BUTTON_3_UP;
        flags_release |= RI_MOUSE_BUTTON_4_UP;
        flags_release |= RI_MOUSE_BUTTON_5_UP;
        
        if (mouse.usButtonFlags & flags_press)
        {
            type = input_type::mouse_press;
        }
        else if (mouse.usButtonFlags & flags_release)
        {
            type = input_type::mouse_release;
        }
        else if (mouse.usButtonFlags & RI_MOUSE_WHEEL)
        {
            type = input_type::mouse_wheel;
        }
        else
        {
            type = input_type::mouse_move;
        }

        auto button = key::none;
        if (mouse.usButtonFlags & (RI_MOUSE_BUTTON_1_DOWN | RI_MOUSE_BUTTON_1_UP))
            button = key::mouse_left;
        else if (mouse.usButtonFlags & (RI_MOUSE_BUTTON_2_DOWN | RI_MOUSE_BUTTON_2_UP))
            button = key::mouse_right;
        else if (mouse.usButtonFlags & (RI_MOUSE_BUTTON_3_DOWN | RI_MOUSE_BUTTON_3_UP))
            button = key::mouse_mid;
        else if (mouse.usButtonFlags & (RI_MOUSE_BUTTON_4_DOWN | RI_MOUSE_BUTTON_4_UP))
            button = key::mouse_x1;
        else if (mouse.usButtonFlags & (RI_MOUSE_BUTTON_5_DOWN | RI_MOUSE_BUTTON_5_UP))
            button = key::mouse_x2;

        input_message msg(type, button, mods);
        if (type == input_type::mouse_wheel)
            msg.wheel = mouse.usButtonData;

        if (mouse.usFlags & MOUSE_MOVE_ABSOLUTE)
        {
            msg.point_x = mouse.lLastX;
            msg.point_y = mouse.lLastY;
        }
        else
        {
            POINT point = {};
            if (!win32_get_cursor_pos(&point))
            {
                m_error = last_system_error();
                return;
            }
            msg.point_x = point.x;
            msg.point_y = point.y;
        }
        
        if (const auto error = event::post(this, event_type::window_input, msg))
            m_error = error;
    }
}
error_type icy::make_raw_input(shared_ptr<event_loop>& loop) noexcept
{
    shared_ptr<raw_input_system> system;
    ICY_ERROR(make_shared(system));
    loop = std::move(system);
    return {};
}

error_type raw_input_system::initialize() noexcept
{
    shutdown();

    m_event = CreateEventW(nullptr, FALSE, FALSE, nullptr);
    if (!m_event)
        return last_system_error();

    ICY_ERROR(m_library.initialize());

    decltype(&::RegisterClassExW) win32_register_class = nullptr;
    decltype(&::CreateWindowExW) win32_create_window = nullptr;
    decltype(&::RegisterRawInputDevices) win32_register_raw_input = nullptr;

    ICY_WIN32_FUNC(win32_translate_message, TranslateMessage);
    ICY_WIN32_FUNC(win32_dispatch_message, DispatchMessageW);
    ICY_WIN32_FUNC(win32_get_keyboard_state, GetKeyboardState);
    ICY_WIN32_FUNC(win32_peek_message, PeekMessageW);
    ICY_WIN32_FUNC(win32_post_message, PostMessageW);
    ICY_WIN32_FUNC(win32_def_window_proc, DefWindowProcW);
    ICY_WIN32_FUNC(win32_unregister_class, UnregisterClassW);
    ICY_WIN32_FUNC(win32_register_class, RegisterClassExW);
    ICY_WIN32_FUNC(win32_create_window, CreateWindowExW);
    ICY_WIN32_FUNC(win32_destroy_window, DestroyWindow);
    ICY_WIN32_FUNC(win32_msg_wait_for_multiple_objects_ex, MsgWaitForMultipleObjectsEx);
    ICY_WIN32_FUNC(win32_get_window_longptr, GetWindowLongPtrW);
    ICY_WIN32_FUNC(win32_set_window_longptr, SetWindowLongPtrW);
    ICY_WIN32_FUNC(win32_get_raw_input_data, GetRawInputData);
    ICY_WIN32_FUNC(win32_get_cursor_pos, GetCursorPos);
    ICY_WIN32_FUNC(win32_register_raw_input, RegisterRawInputDevices);

    swprintf_s(m_cname, L"Icy Message Window %u.%u", GetCurrentProcessId(), GetCurrentThreadId());
    
    WNDCLASSEX wx = {};
    wx.cbSize = sizeof(WNDCLASSEX);
    wx.lpfnWndProc = win32_def_window_proc;
    wx.hInstance = win32_instance();
    wx.lpszClassName = m_cname;

    if (!win32_register_class(&wx))
        return last_system_error();

    m_window = win32_create_window(0, wx.lpszClassName, nullptr, 0, 0, 0, 0, 0,
        HWND_MESSAGE, nullptr, wx.hInstance, nullptr);
    if (!m_window)
        return last_system_error();

    uint8_t keys[256] = {};
    if (!win32_get_keyboard_state(keys))
        return last_system_error();

    for (auto n = 0u; n < _countof(keys); ++n)
        m_keyboard[n] = (keys[n] & 0x80) != 0;

    RAWINPUTDEVICE device[2] = {};
    device[0].usUsagePage = 0x01;
    device[0].usUsage = 0x02;
    //device[0].dwFlags = RIDEV_INPUTSINK;
    device[0].hwndTarget = m_window;

    device[1].usUsagePage = 0x01;
    device[1].usUsage = 0x06;
    //device[1].dwFlags = RIDEV_INPUTSINK;
    device[1].hwndTarget = m_window;

    if (!win32_register_raw_input(device, _countof(device), sizeof(device[0])))
        return last_system_error();

    win32_set_window_longptr(m_window, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(this));
    win32_set_window_longptr(m_window, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(&win32_proc));
    
    filter(event_type::global_quit);
    m_init = true;
    
    return {};
}

error_type raw_input_system::loop(event& event, const duration_type timeout) noexcept
{
    if (timeout.count() < 0)
        return make_stdlib_error(std::errc::invalid_argument);

    if (m_error)
    {
        error_type error;
        std::swap(m_error, error);
        return error;
    }
    if (!m_init)
        ICY_ERROR(initialize());
    
    const auto ms = ms_timeout(timeout);

    while (true)
    {
        const auto index =  win32_msg_wait_for_multiple_objects_ex(1, &m_event, ms, QS_RAWINPUT, MWMO_ALERTABLE);
        if (index == WAIT_OBJECT_0)
        {
            if (event = pop())
                break;
        }
        else if (index == WAIT_OBJECT_0 + 1)
        {
            MSG win32_msg = {};
            while (win32_peek_message(&win32_msg, m_window, 0, 0, PM_REMOVE))
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
        else if (index == WAIT_FAILED)
        {
            return last_system_error();
        }
    }
    return {};
}
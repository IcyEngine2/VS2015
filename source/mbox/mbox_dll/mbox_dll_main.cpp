#include <icy_engine/core/icy_input.hpp>
#include <icy_engine/core/icy_json.hpp>
#include <icy_engine/core/icy_file.hpp>
#include <icy_engine/graphics/icy_remote_window.hpp>
#include "../mbox_dll.h"
#define NOMINMAX
#include <Windows.h>
#if _DEBUG
#pragma comment(lib, "icy_engine_cored")
#pragma comment(lib, "icy_engine_graphicsd")
#else
#pragma comment(lib, "icy_engine_core")
#pragma comment(lib, "icy_engine_graphics")
#endif
#pragma comment(lib, "user32")

using namespace icy;

class mbox_application : public named_read_pipe
{
public:
    error_type create() noexcept;
    void read(const error_type error, const const_array_view<uint8_t> bytes) noexcept override;
    mbox::mbox_dll_config config;
    //WNDPROC proc = nullptr;
    //void* user = nullptr;
};

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

error_type mbox_application::create() noexcept
{
    array<remote_window> vec;
    ICY_ERROR(remote_window::enumerate(vec, GetCurrentThreadId()));
    if (vec.empty())
        return make_unexpected_error();

    ICY_ERROR(config.load());
    string name;
    ICY_ERROR(name.appendf("mbox_dll_%1"_s, GetCurrentProcessId()));
    ICY_ERROR(named_read_pipe::create(name, sizeof(input_message)));

    /*proc = reinterpret_cast<WNDPROC>(GetWindowLongPtrW(client_hwnd, GWLP_WNDPROC));
    user = reinterpret_cast<void*>(GetWindowLongPtrW(client_hwnd, GWLP_USERDATA));

    const auto wndproc = [](HWND, UINT, WPARAM, LPARAM) -> LRESULT
    {
        return 0;
    };
    SetWindowLongPtrW(client_hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(this));
    SetWindowLongPtrW(client_hwnd, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(WNDPROC(wndproc)));
    */
    return error_type();
}
void mbox_application::read(const error_type error, const const_array_view<uint8_t> bytes) noexcept
{
    if (error || bytes.size() != sizeof(input_message))
        return;

    const auto msg = reinterpret_cast<const input_message*>(bytes.data());
    SetForegroundWindow(config.client_hwnd);
    SetFocus(config.client_hwnd);
    auto win_msg = detail::to_winapi(*msg);
    SendMessageW(config.client_hwnd, win_msg.message, win_msg.wParam, win_msg.lParam);
}

LRESULT WINAPI GetMessageHook(int code, WPARAM wParam, LPARAM lParam)
{
    static thread_local error_type error;
    if (error)
        return CallNextHookEx(nullptr, code, wParam, lParam);

    const auto show_error = [](const error_type& error)
    {
        string str;
        to_string(error, str);
        icy::message_box(str, "Runtime error"_s);
        return 0;
    };

    static thread_local mbox_application app;

    if (!app.config.server_hwnd)
    {
        error = app.create();
        if (error)
        {
            show_error(error);
            return CallNextHookEx(nullptr, code, wParam, lParam);
        }
    }

    static thread_local bool g_pause = false;
    
    if (code != HC_ACTION || wParam != PM_REMOVE)
        return CallNextHookEx(nullptr, code, wParam, lParam);

    const auto msg = reinterpret_cast<MSG*>(lParam);

    switch (msg->message)
    {
    case WM_LBUTTONUP:
    case WM_RBUTTONUP:
    case WM_MBUTTONUP:
    case WM_XBUTTONUP:
    case WM_LBUTTONDBLCLK:
    case WM_RBUTTONDBLCLK:
    case WM_MBUTTONDBLCLK:
    case WM_XBUTTONDBLCLK:
    case WM_RBUTTONDOWN:
    case WM_MBUTTONDOWN:
    case WM_XBUTTONDOWN:
    case WM_MOUSEWHEEL:
    case WM_KEYDOWN:
    case WM_KEYUP:
        break;
    default:
        return CallNextHookEx(nullptr, code, wParam, lParam);
    }

    input_message input;
    if (msg->message == WM_KEYDOWN || msg->message == WM_KEYUP)
    {
        detail::from_winapi(input, *msg, key_state());
    }
    else
    {
        detail::from_winapi(input, 0, 0, *msg, key_state());
    }
    auto pause_type = mbox::mbox_pause_type::none;
    for (auto k = 0u; k < app.config.pause_count; ++k)
    {
        if (true
            && app.config.pause_array[k].input.type == input.type
            && app.config.pause_array[k].input.key == input.key
            && key_mod_and(key_mod(app.config.pause_array[k].input.mods), key_mod(input.mods)))
        {
            pause_type = app.config.pause_array[k].type;
        }
    }

    auto toggle = false;
    if (pause_type == mbox::mbox_pause_type::toggle)
    {
        g_pause = !g_pause;
        toggle = !g_pause;
    }
    else if (pause_type == mbox::mbox_pause_type::enable)
    {
        g_pause = true;
    }
    else if (pause_type == mbox::mbox_pause_type::disable)
    {
        g_pause = false;
    }

    if (g_pause || toggle)
        return CallNextHookEx(nullptr, code, wParam, lParam);

    auto skip = true;
    for (auto k = 0u; k < app.config.event_count; ++k)
    {
        if (true
            && app.config.event_array[k].type == input.type
            && app.config.event_array[k].key == input.key
            && key_mod_and(key_mod(app.config.event_array[k].mods), key_mod(input.mods)))
        {
            skip = false;
        }
    }
    if (skip)
        return CallNextHookEx(nullptr, code, wParam, lParam);

    COPYDATASTRUCT cdata = {};
    cdata.dwData = GetCurrentProcessId();
    cdata.cbData = sizeof(input);
    cdata.lpData = &input;
    SendMessageW(app.config.server_hwnd, WM_COPYDATA, 0, reinterpret_cast<LPARAM>(&cdata));
    if (const auto code = GetLastError())
    {
        error = make_system_error(make_system_error_code(code));
        show_error(error);
    }

    const auto is_key_event = false
        || input.type == input_type::key_press 
        || input.type == input_type::key_hold 
        || input.type == input_type::key_release;

    if (is_key_event)
    {
        msg->message = WM_NULL;
        msg->wParam = 0;
        msg->lParam = 0;
    }
    return CallNextHookEx(nullptr, code, wParam, lParam);
}

error_type mbox::mbox_dll_config::load() noexcept
{
    string path;
    ICY_ERROR(process_directory(path));
    ICY_ERROR(path.appendf("%1%2.txt"_s, "mbox_config/"_s, GetCurrentProcessId()));
    
    json json;
    {
        file input;
        ICY_ERROR(input.open(path, file_access::read, file_open::open_existing, file_share::read | file_share::write, file_flag::delete_on_close));
        array<char> bytes;
        auto count = input.info().size;
        ICY_ERROR(bytes.resize(count));
        ICY_ERROR(input.read(bytes.data(), count));
        ICY_ERROR(to_value(string_view(bytes.data(), count), json));
    }

    const auto json_event = json.find("event"_s);
    const auto json_pause = json.find("pause"_s);

    if (false
        || !json_event || json_event->type() != json_type::array
        || !json_pause || json_pause->type() != json_type::array)
        return make_stdlib_error(std::errc::invalid_argument);

    uint64_t server_hwnd_val = 0;
    uint64_t client_hwnd_val = 0;
    ICY_ERROR(json.get("server_thread"_s, server_thread));
    ICY_ERROR(json.get("client_thread"_s, client_thread));
    ICY_ERROR(json.get("server_window"_s, server_hwnd_val));
    ICY_ERROR(json.get("client_window"_s, client_hwnd_val));
    server_hwnd = HWND(server_hwnd_val);
    client_hwnd = HWND(server_hwnd_val);

    pause_count = std::min(_countof(pause_array), json_pause->size());
    for (auto k = 0u; k < pause_count; ++k)
    {
        const auto pause = json_pause->at(k);
        ICY_ERROR(pause_array[k].from_json(*pause));
    }

    event_count = std::min(_countof(event_array), json_event->size());
    for (auto k = 0u; k < event_count; ++k)
    {
        const auto event = json_event->at(k);
        auto int_input_type = 0u;
        auto int_input_key = 0u;
        auto int_input_mods = 0u;
        ICY_ERROR(event->get("input.type"_s, int_input_type));
        ICY_ERROR(event->get("input.key"_s, int_input_key));
        ICY_ERROR(event->get("input.mods"_s, int_input_mods));
        event_array[k] = input_message(input_type(int_input_type), key(int_input_key), key_mod(int_input_mods));
    }
    return error_type();
}

int __stdcall DllMain(HINSTANCE hinst, DWORD reason, LPVOID reserved)
{
    if (reason == DLL_PROCESS_ATTACH)
    {
        icy::detail::global_heap.realloc = [](const void* ptr, size_t size, void*) { return ::realloc(const_cast<void*>(ptr), size); };
        icy::detail::global_heap.memsize = [](const void*, void*) { return 0_z; };
        {
            string path;
            if (process_directory(path))
                return 0;
            if (path.appendf("%1%2.txt"_s, "mbox_config/"_s, GetCurrentProcessId()))
                return 0;
            if (file_info().initialize(path))
                return 0;
        }
    }
    return 1;

  /*  const auto append = [hinst](const bool attach)
    {
        wchar_t wbuf[4096];
        auto wlen = GetModuleFileNameW(hinst, wbuf, _countof(wbuf) - 0x20);
        while (wlen && wbuf[wlen - 1] != '.')
            --wlen;
        wcscat_s(wbuf, L"log.txt");
        auto file = CreateFileW(wbuf, FILE_APPEND_DATA, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr,
            OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (file != INVALID_HANDLE_VALUE)
        {
            char buf[256];
            auto len = sprintf_s(buf, "\r\n%u %u", GetCurrentThreadId(), attach ? 1 : 0);
            auto count = 0ul;
            WriteFile(file, buf, len, &count, nullptr);
            ICY_SCOPE_EXIT{ CloseHandle(file); };
        }
    };
    switch (reason)
    {
    case DLL_THREAD_ATTACH:
    {
        append(true);
        break;
    }
    case DLL_THREAD_DETACH:
    {
        append(false);
        break;
    }
    }*/
}
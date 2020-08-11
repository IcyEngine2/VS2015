#include <icy_engine/core/icy_input.hpp>
#include <icy_engine/core/icy_json.hpp>
#include <icy_engine/core/icy_file.hpp>
#include "../mbox_dll.h"
#define NOMINMAX
#include <Windows.h>
#if _DEBUG
#pragma comment(lib, "icy_engine_cored")
#else
#pragma comment(lib, "icy_engine_core")
#endif
#pragma comment(lib, "user32")

using namespace icy;

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

LRESULT WINAPI GetMessageHook(int code, WPARAM wParam, LPARAM lParam)
{
    static thread_local error_type error;
    if (error)
        return CallNextHookEx(nullptr, code, wParam, lParam);

    icy::detail::global_heap.realloc = [](const void* ptr, size_t size, void*) { return ::realloc(const_cast<void*>(ptr), size); };
    icy::detail::global_heap.memsize = [](const void*, void*) { return 0_z; };

    const auto show_error = [](const error_type& error)
    {
        string str;
        if (const auto e = to_string(error, str))
            return MessageBoxW(nullptr, L"Error printing error to string", L"MBox: to_string error", MB_OK);

        icy::win32_message(str, "Runtime error"_s);
        return 0;
    };

    static thread_local mbox::mbox_dll_config g_config;
    if (!g_config.hwnd)
    {
        error = g_config.load();
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
        detail::from_winapi(input, *msg, key_state());
    else
        detail::from_winapi(input, 0, 0, *msg, key_state());
    
    auto pause_type = mbox::mbox_pause_type::none;
    for (auto k = 0u; k < g_config.pause_count; ++k)
    {
        if (true
            && g_config.pause_array[k].input.type == input.type 
            && g_config.pause_array[k].input.key == input.key
            && key_mod_and(key_mod(g_config.pause_array[k].input.mods), key_mod(input.mods)))
        {
            pause_type = g_config.pause_array[k].type;
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
    for (auto k = 0u; k < g_config.event_count; ++k)
    {
        if (true
            && g_config.event_array[k].type == input.type
            && g_config.event_array[k].key == input.key
            && key_mod_and(key_mod(g_config.event_array[k].mods), key_mod(input.mods)))
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
    SendMessageW(g_config.hwnd, WM_COPYDATA, 0, reinterpret_cast<LPARAM>(&cdata));
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
    file input;
    ICY_ERROR(input.open(path, file_access::read, file_open::open_existing, file_share::read | file_share::write));
    char bytes[0x4000];
    auto count = _countof(bytes);
    ICY_ERROR(input.read(bytes, count));

    json json;
    ICY_ERROR(to_value(string_view(bytes, bytes + count), json));

    const auto json_hwnd = json.find("hwnd"_s);
    const auto json_event = json.find("event"_s);
    const auto json_pause = json.find("pause"_s);

    if (false
        || !json_hwnd || json_hwnd->type() != json_type::integer
        || !json_event || json_event->type() != json_type::array
        || !json_pause || json_pause->type() != json_type::array)
        return make_stdlib_error(std::errc::invalid_argument);

    uint64_t hwnd_value = 0;
    ICY_ERROR(to_value(json_hwnd->get(), hwnd_value));
    hwnd = reinterpret_cast<HWND__*>(hwnd_value);

    pause_count = std::min(_countof(pause_array), json_pause->size());
    for (auto k = 0u; k < pause_count; ++k)
    {
        const auto pause = json_pause->at(k);
        ICY_ERROR(pause_array[k].from_json(*pause));
    }

    event_count = std::min(_countof(event_array), json_event->size());
    for (auto k = 0u; k < event_count; ++k)
    {
        const auto pause = json_event->at(k);
        auto int_input_type = 0u;
        auto int_input_key = 0u;
        auto int_input_mods = 0u;
        ICY_ERROR(pause->get("input.type"_s, int_input_type));
        ICY_ERROR(pause->get("input.key"_s, int_input_key));
        ICY_ERROR(pause->get("input.mods"_s, int_input_mods));
        event_array[k] = input_message(input_type(int_input_type), key(int_input_key), key_mod(int_input_mods));
    }
    return error_type();
}

/*error_type send_http(const input_message& msg) noexcept
{
    struct global_type : public heap
    {
        global_type()
        {
            initialize(heap_init::global(g_memory));

            string path;
            process_directory(path);
            path.appendf("%1%2.txt"_s, mbox::config_path, GetCurrentProcessId());

            json jconfig;
            {
                file fconfig;
                fconfig.open(path, file_access::read, file_open::open_existing, file_share::read);
                char buf[4096];
                auto len = sizeof(buf);
                fconfig.read(buf, len);
                to_value(string_view(buf, len), jconfig);
            }
            file::remove(path);

            jconfig.get(mbox::json_key_version, version);
            jconfig.get(mbox::json_key_character, character);
            auto addr_str = jconfig.get(mbox::json_key_connect);
            if (addr_str.find(":") == addr_str.end())
                return;

            jconfig.get(mbox::json_key_pause_toggle, g_pause_toggle);
            jconfig.get(mbox::json_key_pause_begin, g_pause_begin);
            jconfig.get(mbox::json_key_pause_end, g_pause_end);

            auto addr_host = string_view(addr_str.begin(), addr_str.find(":"));
            auto addr_port = string_view(addr_str.find(":") + 1, addr_str.end());

            array<network_address> vec;
            network_address::query(vec, addr_host, addr_port);
            for (auto&& elem : vec)
            {
                if (elem.addr_type() == network_address_type::ip_v4)
                {
                    addr = std::move(elem);
                    sock.initialize(0);
                    break;
                }
            }
        }
        ~global_type()
        {
            WSACleanup();
        }
        network_address addr;
        network_udp_socket sock;
        string version;
        string character;
    };
    static global_type global;

    if (!global.addr)
        return make_stdlib_error(std::errc::address_not_available);

    using namespace mbox;

    json j = json_type::object;
    ICY_ERROR(j.insert(json_key_version, string_view(global.version)));
    ICY_ERROR(j.insert(json_key_type, uint32_t(msg.type)));
    if (msg.type == input_type::key_hold ||
        msg.type == input_type::key_press ||
        msg.type == input_type::key_release)
    {
        ICY_ERROR(j.insert(json_key_key, uint32_t(msg.key)));
    }
    else
    {
        ICY_ERROR(j.insert(json_key_key, 0u));
    }
    ICY_ERROR(j.insert(json_key_mods, msg.mods));
    ICY_ERROR(j.insert(json_key_wheel, msg.wheel));
    ICY_ERROR(j.insert(json_key_point_x, msg.point_x));
    ICY_ERROR(j.insert(json_key_point_y, msg.point_y));
    ICY_ERROR(j.insert(json_key_character, global.character));

    string json_str;
    ICY_ERROR(to_string(j, json_str));

    http_request req;
    req.type = http_request_type::post;
    req.content = http_content_type::application_json;
    ICY_ERROR(copy(json_str.ubytes(), req.body));

    global.sock.send(global.addr, req.body);

    return error_type();
}*/

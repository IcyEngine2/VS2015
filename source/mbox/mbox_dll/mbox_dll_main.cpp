#include <icy_engine/core/icy_input.hpp>
#include <icy_engine/core/icy_json.hpp>
#include <icy_engine/core/icy_file.hpp>
#include <icy_engine/network/icy_http.hpp>
#include <icy_engine/network/icy_network.hpp>
#include <Windows.h>
#include "../mbox.h"
#pragma comment(lib, "user32")
#pragma comment(lib, "ws2_32")

#if _DEBUG
#pragma comment(lib, "icy_engine_cored")
#pragma comment(lib, "icy_engine_networkd")
#else
#pragma comment(lib, "icy_engine_core")
#pragma comment(lib, "icy_engine_network")
#endif

using namespace icy;

static const auto g_memory = 256_mb;
static uint32_t g_pause_toggle;
static uint32_t g_pause_begin;
static uint32_t g_pause_end;

error_type send_http(const input_message& msg) noexcept
{
    struct global_type : public heap
    {
        global_type()
        {
            initialize(heap_init::global(g_memory));
            
            string path;


           /* HMODULE module = nullptr;
            if (!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                (LPCWSTR)&send_http, &module))
                return;

            wchar_t wbuf[MAX_PATH];
            auto wlen = GetModuleFileNameW(module, wbuf, _countof(wbuf));
            if (wlen == 0)
                return last_system_error();
                */

            process_directory(path);
            path.appendf("%1%2.txt"_s, mbox::config_path, GetCurrentProcessId());

            file fconfig;
            json jconfig;

            fconfig.open(path, file_access::read, file_open::open_existing, file_share::read);
            char buf[4096];
            auto len = sizeof(buf);
            fconfig.read(buf, len);
            
            to_value(string_view(buf, len), jconfig);            
            jconfig.get(mbox::json_key_version, version);
            jconfig.get(mbox::json_key_profile, profile);
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
        string profile;
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
    ICY_ERROR(j.insert(json_key_thread, GetCurrentThreadId()));
    ICY_ERROR(j.insert(json_key_process, GetCurrentProcessId()));
    ICY_ERROR(j.insert(json_key_profile, global.profile));
    
    string json_str;
    ICY_ERROR(to_string(j, json_str));

    http_request req;
    req.type = http_request_type::post;
    req.content = http_content_type::application_json;
    ICY_ERROR(copy(json_str.ubytes(), req.body));

    global.sock.send(global.addr, req.body);

    return error_type();
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

LRESULT WINAPI GetMessageHook(int code, WPARAM wParam, LPARAM lParam)
{
    static bool g_pause = false;

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

    auto toggle = false;
    if (msg->message == WM_KEYDOWN && msg->wParam == g_pause_toggle)
    {
        g_pause = !g_pause;
        toggle = !g_pause;
    }
    else if (msg->message == WM_KEYDOWN && msg->wParam == g_pause_begin)
        g_pause = true;
    else if (msg->message == WM_KEYUP && (msg->wParam == g_pause_end))// || msg->wParam == g_pause_toggle && g_pause))
        g_pause = false;

    if (g_pause || toggle)
        return CallNextHookEx(nullptr, code, wParam, lParam);

    input_message input;
    if (msg->message == WM_KEYDOWN ||
        msg->message == WM_KEYUP)
    {
        detail::from_winapi(input, *msg, key_state());
    }
    else
    {
        detail::from_winapi(input, 0, 0, *msg, key_state());
    }
    send_http(input);

    if (input.type == input_type::key_press ||
        input.type == input_type::key_hold ||
        input.type == input_type::key_release)
    {
        msg->message = WM_NULL;
        msg->wParam = 0;
        msg->lParam = 0;
    }
    return CallNextHookEx(nullptr, code, wParam, lParam);
}
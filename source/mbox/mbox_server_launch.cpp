#include <icy_engine/core/icy_core.hpp>
#include <icy_engine/core/icy_thread.hpp>
#include <icy_engine/core/icy_console.hpp>
#include <icy_engine/network/icy_network.hpp>
#include "mbox_server.hpp"
#if _DEBUG
#pragma comment(lib, "icy_engine_cored")
#pragma comment(lib, "icy_engine_networkd")
#else
#pragma comment(lib, "icy_engine_core")
#pragma comment(lib, "icy_engine_network")
#endif

using namespace icy;

template<typename... T>
error_type console_print(console_system& con, string_view format, color color, T&&... args)
{
    char tbuf[64];
    auto tlen = 0_z;
    auto now = time(nullptr);
    tm tm;
    if (localtime_s(&tm, &now) == 0)
    {
        tlen = strftime(tbuf, sizeof(tbuf), "[%H:%M:%S]: ", &tm);
    }
    string fmt;
    ICY_ERROR(fmt.append("\r\n"_s));
    ICY_ERROR(fmt.append(string_view(tbuf, tlen)));
    ICY_ERROR(fmt.append(format));
    
    string str;
    ICY_ERROR(to_string(fmt, str, std::forward<T>(args)...));
    return con.write(str, color);
}

error_type main_ex(heap& heap)
{
    ICY_ERROR(event_system::initialize());
   
    shared_ptr<event_queue> loop;
    ICY_ERROR(create_event_system(loop, 0
        | event_type::global_timer
        | event_type::network_recv
        | event_type::console_read_key
        | event_type::console_read_line
    ));

    shared_ptr<console_system> con;
    ICY_ERROR(create_console_system(con));
    ICY_ERROR(con->thread().launch());
    ICY_ERROR(con->thread().rename("Console Thread"_s));

    {
        string str;
        ICY_ERROR(str.appendf("Mbox Launcher v%1.%2."
            "\r\nPress 'X' to shutdown."
            ""_s, MBOX_VERSION_MAJOR, MBOX_VERSION_MINOR));
        ICY_ERROR(con->write(str));
    }
    ICY_ERROR(con->read_key());

    array<network_address> addr_list;
    ICY_ERROR(network_address::query(addr_list, MBOX_MULTICAST_ADDR, MBOX_MULTICAST_PORT));
    if (addr_list.empty())
    {
        ICY_ERROR(console_print(*con, "Error resolving network multicast address (%1:%2)"_s, 
            colors::red, MBOX_MULTICAST_ADDR, MBOX_MULTICAST_PORT));
    }

    network_server_config config;
    config.capacity = 1;
    config.port = addr_list[0].port();

    shared_ptr<network_system_udp_client> udp;
    ICY_ERROR(create_network_udp_client(udp, config));
    ICY_ERROR(udp->join(addr_list[0]));
    ICY_ERROR(udp->thread().launch());
    ICY_ERROR(udp->thread().rename("UDP Thread"_s));

    shared_ptr<network_system_http_client> http;

    while (*loop)
    {
        event event;
        ICY_ERROR(loop->pop(event));
        if (!event)
            continue;

        if (event->type == event_type::console_read_key)
        {
            const auto& event_data = event->data<console_event>();
            if (event_data.key == key::x)
            {
                break;
            }
            ICY_ERROR(con->read_key());
        }
        else if (event->type == event_type::network_recv)
        {
            if (http)
                continue;

            auto& event_data = event->data<network_event>();
            event_data.address.port(config.port);
            string str;
            ICY_ERROR(to_string(event_data.address, str));

            http_request request;
            request.type = http_request_type::post;
            request.content = http_content_type::application_json;
            const auto error = create_network_http_client(http, event_data.address, 
                request, std::chrono::seconds(1), network_default_buffer);
            if (error)
            {
                ICY_ERROR(console_print(*con, 
                    "Error establishing HTTP Launcher-Server connection to %1: code %2"_s,
                    colors::red, string_view(str), error.code));
            }
            else
            {
                ICY_ERROR(console_print(*con,
                    "OK HTTP Launcher-Server connection: %1"_s, colors::green, string_view(str)));
                ICY_ERROR(http->thread().launch());
                ICY_ERROR(http->thread().rename("HTTP Thread"_s));
            }

        }
    }

    return error_type();
}

int main()
{
    heap gheap;
    if (gheap.initialize(heap_init::global(256_gb)))
        return ENOMEM;
    if (const auto error = main_ex(gheap))
        return error.code;
    return 0;
}

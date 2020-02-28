#include <icy_engine/core/icy_file.hpp>
#include <icy_engine/core/icy_memory.hpp>
#include <icy_engine/core/icy_string.hpp>
#include <icy_engine/core/icy_array.hpp>
#include <icy_engine/core/icy_map.hpp>
#include <icy_engine/network/icy_network.hpp>
#include <icy_engine/network/icy_http.hpp>
#include <icy_engine/core/icy_thread.hpp>
#include <icy_engine/core/icy_console.hpp>
//#include <icy_engine/image/icy_image.hpp>
//#include <icy_qtgui/icy_qtgui.hpp>
#if _DEBUG
#pragma comment(lib, "icy_engine_cored.lib")
#pragma comment(lib, "icy_engine_networkd.lib")
#else
#pragma comment(lib, "icy_engine_core.lib")
#endif

using namespace icy;

error_type func(heap& heap)
{
    network_server_config cfg;
    cfg.port = 1581;
    cfg.capacity = 1;

    shared_ptr<thread> thr_con;
    shared_ptr<thread> thr_net;
    ICY_SCOPE_EXIT
    {
        event::post(nullptr, event_type::global_quit);
        if (thr_con) thr_con->wait();
        if (thr_net) thr_net->wait();
    };


    shared_ptr<console_system> con;
    shared_ptr<network_system_http_server> net;
    ICY_ERROR(create_console(con));
    ICY_ERROR(create_network_http_server(net, cfg));
    
    ICY_ERROR(create_event_thread(thr_con, con));
    ICY_ERROR(create_event_thread(thr_net, net));
    ICY_ERROR(thr_con->launch());
    ICY_ERROR(thr_con->rename("Console"_s));
    ICY_ERROR(thr_net->launch());
    ICY_ERROR(thr_net->rename("Network"_s)); 

    ICY_ERROR(con->write("Server launched!"_s));

    shared_ptr<event_queue> loop;
    ICY_ERROR(create_event_queue(loop, event_type::network_any));
    while (true)
    {
        event event;
        ICY_ERROR(loop->pop(event));
        if (event->type & event_type::network_any)
        {
            const auto& event_data = event->data<network_event>();
            if (event->type == event_type::network_recv)
            {
                http_response response;
                response.type = http_content_type::text_plain;
                response.body.append("Hello"_s.ubytes());
                ICY_ERROR(net->reply(event_data.conn, response));
                ICY_ERROR(con->write("\r\nUser ["_s));
                string addr_str;
                to_string(event_data.address, addr_str);
                con->write(addr_str);
                con->write("]: "_s);

                const auto& body = event_data.http.request->body;

                ICY_ERROR(con->write(string_view(reinterpret_cast<const char*>(body.data()), body.size())));                
            }
        }
        else
            break;

    }

    return {};
}

int main()
{   
    heap gheap;
    if (const auto error = gheap.initialize(heap_init::global(64_mb)))
        return error.code;
    
    if (const auto error = func(gheap))
    {
        string msg;
        to_string("Error: %1", msg, error);
        win32_message(msg, "Error"_s);
        return error.code;
    }
    return 0;
}
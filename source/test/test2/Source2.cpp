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

    array<network_address> vec;
    ICY_ERROR(network_address::query(vec, "localhost"_s, "1581"_s));

    shared_ptr<thread> thr_con;
    shared_ptr<thread> thr_net;
    ICY_SCOPE_EXIT
    {
        event::post(nullptr, event_type::global_quit);
        if (thr_con) thr_con->wait();
        if (thr_net) thr_net->wait();
    };

    shared_ptr<console_system> con;
    shared_ptr<network_system_http_client> net;
    ICY_ERROR(create_console(con));
    ICY_ERROR(create_event_thread(thr_con, con));
    ICY_ERROR(thr_con->launch());
    ICY_ERROR(thr_con->rename("Console"_s));


    shared_ptr<event_queue> loop;
    ICY_ERROR(create_event_queue(loop, event_type::network_any | event_type::console_any));
    ICY_ERROR(con->write("User input: "_s));
    ICY_ERROR(con->read_line());
    
    string str;
    auto offset = 0u;

    while (true)
    {
        event event;
        ICY_ERROR(loop->pop(event));
        if (event->type == event_type::console_read_line)
        {
            const string_view msg = event->data<console_event>().text;
            http_request request;
            request.body.append(msg.ubytes());
            request.content = http_content_type::text_plain;
            request.type = http_request_type::post;
            to_string(request, str);
            str.append(msg);

            con->write("\r\nRequest: \""_s);
            con->write(str);
            con->write("\""_s);

            ICY_ERROR(create_network_http_client(net, vec[1], request));
            ICY_ERROR(create_event_thread(thr_net, net));
            ICY_ERROR(thr_net->launch());
            ICY_ERROR(thr_net->rename("Network"_s));


            /*
            con->write("\r\nPress any key to send next char ("_s);
            con->write(string_view(&str.bytes()[0], 1));
            con->write("): "_s);
            con->read_key();*/
        }
        else if (event->type == event_type::network_recv)
        {
            auto& response = event->data<network_event>().http.response;
            string s1;
            if (response)
            {
                to_string(*response, s1);
                s1.append(string_view(reinterpret_cast<const char*>(response->body.data()), response->body.size()));
            }
            con->write("Server: "_s);
            con->write(s1);            
            con->write("\r\nPress ESC key to exit: "_s);
            con->read_key();
        }
        else if (event->type == event_type::console_read_key)
        {
            if (event->data<console_event>().key == key::esc)
                break;
            /*

            auto chr = str.ubytes()[offset];
            const auto view = const_array_view<uint8_t>(&chr, 1);
            ICY_ERROR(net->send(view));
            ++offset;
            if (offset == str.ubytes().size())
                break;

            con->write("\r\nPress any key to send next char ("_s);
            con->write(string_view(&str.bytes()[offset], 1));
            con->write("): "_s);
            con->read_key();*/
        }
        else if (event->type == event_type::global_quit)
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
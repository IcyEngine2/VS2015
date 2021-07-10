#include <icy_engine/core/icy_core.hpp>
#include <icy_engine/core/icy_thread.hpp>
#include <icy_engine/core/icy_console.hpp>
#include <icy_engine/core/icy_json.hpp>
#include <icy_engine/core/icy_file.hpp>
#include <icy_engine/network/icy_network.hpp>
#include <icy_engine/utility/icy_process.hpp>
#include "mbox_server.hpp"
#if _DEBUG
#pragma comment(lib, "icy_engine_cored")
#pragma comment(lib, "icy_engine_networkd")
#else
#pragma comment(lib, "icy_engine_core")
#pragma comment(lib, "icy_engine_network")
#endif

using namespace icy;

struct character_type 
{
    process_handle proc;
};
class main_system : public event_system
{
public:
    ~main_system() noexcept
    {
        filter(0);
    }
    error_type initialize() noexcept;
    error_type exec() noexcept;
private:
    error_type signal(const event_data* event) noexcept
    {
        return m_sync.wake();
    }
    template<typename... T>
    error_type print(string_view format, color color, T&&... args)
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
        return m_con->write(str, color);
    }
    error_type launch(const uint32_t character) noexcept;
private:
    string m_inject;
    sync_handle m_sync;
    shared_ptr<console_system> m_con;
    shared_ptr<process_system> m_proc;
    network_address m_addr;
    shared_ptr<network_system_udp_client> m_udp;
    shared_ptr<network_system_http_client> m_http;
    string m_launch_path;
    string m_process_path;
    map<uint32_t, character_type> m_characters;
};

error_type main_system::initialize() noexcept
{
    {
        string pname;
        ICY_ERROR(process_name(nullptr, pname));
        ICY_ERROR(m_inject.append(icy::file_name(pname).directory));
        ICY_ERROR(m_inject.append("mbox_client"_s));
#if _DEBUG
        ICY_ERROR(m_inject.append("d"_s));
#endif
        ICY_ERROR(m_inject.append(".dll"_s));
    }
    ICY_ERROR(m_sync.initialize());

    ICY_ERROR(create_process_system(m_proc));
    ICY_ERROR(m_proc->thread().launch());
    ICY_ERROR(m_proc->thread().rename("Process Thread"_s));

    ICY_ERROR(create_console_system(m_con));
    ICY_ERROR(m_con->thread().launch());
    ICY_ERROR(m_con->thread().rename("Console Thread"_s));

    string str;
    ICY_ERROR(str.appendf("Mbox Launcher v%1.%2."
        "\r\nPress 'X' to shutdown."
        ""_s, MBOX_VERSION_MAJOR, MBOX_VERSION_MINOR));
    ICY_ERROR(m_con->write(str));
    ICY_ERROR(m_con->read_key());

    array<network_address> addr_list;
    ICY_ERROR(network_address::query(addr_list, MBOX_MULTICAST_ADDR, MBOX_MULTICAST_LAUNCHER_PORT));
    if (addr_list.empty())
    {
        ICY_ERROR(print("Error resolving network multicast address (%1:%2)"_s,
            colors::red, MBOX_MULTICAST_ADDR, MBOX_MULTICAST_LAUNCHER_PORT));
        return error_type();
    }
    m_addr = std::move(addr_list[0]);

    network_server_config config;
    config.capacity = 1;
    config.port = m_addr.port();
    config.timeout = max_timeout;

    ICY_ERROR(create_network_udp_client(m_udp, config));
    ICY_ERROR(m_udp->join(m_addr));
    ICY_ERROR(m_udp->thread().launch());
    ICY_ERROR(m_udp->thread().rename("UDP Thread"_s));

    filter(0
        | event_type::global_timer
        | event_type::network_recv
        | event_type::network_disconnect
        | event_type::console_read_key
        | event_type::console_read_line
        | event_type_process);
    return error_type();
}
error_type main_system::exec() noexcept
{
    string server_addr_str;
    while (*this)
    {
        while (auto event = event_system::pop())
        {
            if (event->type == event_type::console_read_key)
            {
                const auto& event_data = event->data<console_event>();
                if (event_data.key == key::x)
                    return error_type();

                ICY_ERROR(m_con->read_key());
            }
            else if (event->type == event_type_process)
            {
                const auto& event_data = event->data<process_event>();
                for (auto it = m_characters.begin(); it != m_characters.end(); ++it)
                {
                    if (it->value.proc.index != event_data.handle.index)
                        continue;

                    if (event_data.error)
                    {
                        ICY_ERROR(print("WARN character %1 was disconnected: error code %2"_s, 
                            colors::yellow, it->key, event_data.error.code));
                        m_characters.erase(it);
                        break;
                    }
                    else if (event_data.type == process_event_type::close)
                    {
                        ICY_ERROR(print("WARN character %1 process has closed"_s, 
                            colors::yellow, it->key));
                        m_characters.erase(it);
                        break;
                    }
                    if (event_data.type == process_event_type::launch)
                    {
                        ICY_ERROR(m_proc->inject(event_data.handle, m_inject));
                    }
                    else if (event_data.type == process_event_type::inject)
                    {
                        //ICY_ERROR(m_proc->send(event_data.handle, ""_s));
                    }
                    else if (event_data.type == process_event_type::send)
                    {
                        ;
                    }
                    else if (event_data.type == process_event_type::recv)
                    {
                        error_type error;
                        json json;
                        array<input_message> msg;
                        http_request http_request;

                        if (!error) error = to_value(event_data.recv, json);
                        if (!error) error = mbox_json_to_input(json, msg);
                        if (!error)
                        {
                            mbox_http_request request;
                            request.type = mbox_request_type::recv_input;
                            request.character = it->key;
                            request.msg = std::move(msg);
                            error = mbox_send_request(request, http_request);
                        }
                        if (!error) error = m_http->send(std::move(http_request));

                        if (error)
                        {
                            ICY_ERROR(print("WARN character %1 parse error: %2"_s,
                                colors::yellow, it->key, event_data.error.code));
                        }
                    }
                }
            }
            else if (event->type == event_type::network_recv)
            {
                auto& event_data = event->data<network_event>();

                if (shared_ptr<event_system>(event->source) == m_udp)
                {
                    if (m_http)
                        continue;
                    
                    if (event_data.bytes.size() != sizeof(mbox_udp_request))
                        continue;

                    const auto udp_request = reinterpret_cast<const mbox_udp_request*>(event_data.bytes.data());
                    if (compare(udp_request->uid, MBOX_MULTICAST_PING) != 0)
                        continue;

                    event_data.address.port(m_addr.port());

                    ICY_ERROR(to_string(event_data.address, server_addr_str));

                    const auto error = create_network_http_client(m_http, event_data.address,
                        http_request(), std::chrono::seconds(5), network_default_buffer);
                    if (error)
                    {
                        ICY_ERROR(print(
                            "ERR establishing HTTP Launcher-Server connection to %1: code %2"_s,
                            colors::red, string_view(server_addr_str), error.code));
                    }
                    else
                    {
                        ICY_ERROR(print(
                            "OK HTTP Launcher-Server connection: %1"_s, colors::green, string_view(server_addr_str)));
                        ICY_ERROR(m_http->thread().launch());
                        ICY_ERROR(m_http->thread().rename("HTTP Thread"_s));
                    }
                }
                else if (event_data.http.request)
                {
                    http_response response;
                    error_type error;

                    mbox_http_request request;
                    if (!error)
                        error = mbox_recv_request(*event_data.http.request, request);
                        
                    if (!error)
                    {
                        switch (request.type)
                        {
                        case mbox_request_type::set_config:
                        {
                            if (request.process_path.empty())
                            {
                                ICY_ERROR(print("WARN Invalid config: null process path"_s, colors::yellow));
                            }
                            else
                            {
                                m_launch_path = std::move(request.launch_path);
                                m_process_path = std::move(request.process_path);
                                ICY_ERROR(print(
                                    "CMD Config:\r\n--Launch = \"%1\"\r\n--Process = \"%2\""_s,
                                    colors::green, string_view(m_launch_path), string_view(m_process_path)));
                            }
                            break;
                        }
                        case mbox_request_type::launch_app:
                        {
                            if (m_process_path.empty())
                            {
                                ICY_ERROR(print("WARN Invalid config: null process path"_s, colors::yellow));
                            }
                            else if (const auto error = launch(request.character))
                            {
                                ICY_ERROR(print("ERR Launch character [%1]: code %2"_s, colors::red,
                                    request.character, error.code));
                            }
                            else
                            {
                                ICY_ERROR(print("CMD Launch character [%1]"_s, colors::green, request.character));
                            }
                            break;
                        }
                        }
                    }

                    response.herror = error ? http_error::bad_request : http_error::success;
                    ICY_ERROR(m_http->send(response));
                    ICY_ERROR(m_http->recv());
                }
                else if (event_data.http.response)
                {
                    ICY_ERROR(m_http->recv());
                }
            }
            else if (event->type == event_type::network_disconnect)
            {
                const auto& event_data = event->data<network_event>();
                ICY_ERROR(print("WARN disconnected from server: code %1"_s,
                    colors::yellow, event_data.error.code));
                m_http = nullptr;
            }
        }
        ICY_ERROR(m_sync.wait());        
    }
    return error_type();
}
error_type main_system::launch(const uint32_t character) noexcept
{
    if (m_process_path.empty())
        return make_stdlib_error(std::errc::invalid_argument);

    if (m_characters.try_find(character))
        return make_stdlib_error(std::errc::invalid_argument);

    character_type new_character;
    ICY_ERROR(m_proc->launch(m_process_path, new_character.proc));
    ICY_ERROR(m_characters.insert(character, std::move(new_character)));
    return error_type();
}

error_type main_ex(heap& heap)
{
    ICY_ERROR(event_system::initialize());
   
    shared_ptr< main_system> sys;
    ICY_ERROR(make_shared(sys));
    ICY_ERROR(sys->initialize());
    ICY_ERROR(sys->exec());
    
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

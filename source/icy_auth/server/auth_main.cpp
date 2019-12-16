#include <icy_engine/core/icy_json.hpp>
#include <icy_engine/core/icy_file.hpp>
#include "auth_config.hpp"
#include "auth_database.hpp"
#include "auth_console.hpp"
#include "auth_http.hpp"

#pragma warning(disable:4063) // case 'identifier' is not a valid value for switch of enum 'enumeration'

#if _DEBUG
#pragma comment(lib, "icy_engine_cored")
#pragma comment(lib, "icy_engine_networkd")
#else
#pragma comment(lib, "icy_engine_core")
#pragma comment(lib, "icy_engine_network")
#endif

using namespace icy;

class auth_core
{
public:
    error_type initialize(const auth_config& config) noexcept;
    error_type run() noexcept;
private:
    error_type wait() noexcept;
private:
    auth_config m_config;
    auth_database m_dbase;
    auth_console_thread m_console;
    auth_network_system m_client{ m_dbase };
    auth_network_system m_module{ m_dbase };
    auth_network_system m_admin{ m_dbase };
};
class auth_query_callback_client : public auth_query_callback
{
public:
    auth_query_callback_client(string& output, const auth_query_flag flag, const uint32_t max_index = 1000) noexcept :
        m_output(output), m_flag(flag), m_max(max_index)
    {

    }
private:
    error_type operator()(const auth_client_query_data& data) noexcept
    {
        if (m_index >= m_max)
            return {};
        
        if (m_index == 0)
        {
            ICY_ERROR(m_output.append("\r\n[index]"_s));
            const auto func = [this](const string_view text, const auth_query_flag flag)
            {
                if (m_flag & flag)
                    ICY_ERROR(m_output.appendf(" ; %1"_s, text));
                return error_type();
            };
            ICY_ERROR(func("name(hash)"_s, auth_query_flag::client_name));
            ICY_ERROR(func("date"_s, auth_query_flag::client_date));
            ICY_ERROR(func("guid"_s, auth_query_flag::client_guid));
        }

        char index_str[5] = {};
        sprintf_s(index_str, "%03u", ++m_index);
        ICY_ERROR(m_output.appendf("\r\n[%1]", string_view(index_str)));
        
        if (m_flag & auth_query_flag::client_name)
        {
            string name_str;
            ICY_ERROR(to_string(data.name, 0x10, name_str));
            ICY_ERROR(m_output.appendf(" %1"_s, string_view(name_str)));
        }
        if (m_flag & auth_query_flag::client_date)
        {
            string time_str;
            ICY_ERROR(to_string(data.date, time_str));
            ICY_ERROR(m_output.appendf(" %1"_s, string_view(time_str)));
        }
        if (m_flag & auth_query_flag::client_guid)
        {
            ICY_ERROR(m_output.appendf(" {%1}"_s, data.guid));
        }
        return {};
    }
private:
    string& m_output;
    const auth_query_flag m_flag;
    const uint32_t m_max;
    uint32_t m_index = 0;
};
class auth_query_callback_module : public auth_query_callback
{
public:
    auth_query_callback_module(string& output, const auth_query_flag flag) noexcept : m_output(output), m_flag(flag)
    {

    }
private:
    error_type operator()(const auth_module_query_data& data) noexcept
    {
        if (m_index == 0)
        {
            ICY_ERROR(m_output.append("\r\n[index]"_s));
            const auto func = [this](const string_view text, const auth_query_flag flag)
            {
                if (m_flag & flag)
                {
                    ICY_ERROR(m_output.appendf("; %1"_s, text));
                }
                return error_type();
            };
            ICY_ERROR(func("name(hash)"_s, auth_query_flag::module_name));
            ICY_ERROR(func("addr"_s, auth_query_flag::module_addr));
            ICY_ERROR(func("timeout(ms)"_s, auth_query_flag::module_time));
            ICY_ERROR(func("date"_s, auth_query_flag::module_date));
            ICY_ERROR(func("guid"_s, auth_query_flag::module_guid));            
        }

        char index_str[5] = {};
        sprintf_s(index_str, "%03u", ++m_index);
        ICY_ERROR(m_output.appendf("\r\n[%1]", string_view(index_str)));

        if (m_flag & auth_query_flag::module_name)
        {
            string name_str;
            ICY_ERROR(to_string(data.name, 0x10, name_str));
            ICY_ERROR(m_output.appendf(" %1"_s, string_view(name_str)));
        }
        if (m_flag & auth_query_flag::module_addr)
        {
            ICY_ERROR(m_output.appendf(" %1"_s, string_view(data.addr, strnlen(data.addr, auth_addr_length))));
        }
        if (m_flag & auth_query_flag::module_time)
        {
            ICY_ERROR(m_output.appendf(" %1(ms)"_s, std::chrono::duration_cast<std::chrono::milliseconds>(data.timeout).count()));
        }
        if (m_flag & auth_query_flag::module_date)
        {
            string time_str;
            ICY_ERROR(to_string(data.date, time_str));
            ICY_ERROR(m_output.appendf(" %1"_s, string_view(time_str)));
        }
        if (m_flag & auth_query_flag::module_guid)
        {
            ICY_ERROR(m_output.appendf(" {%1}"_s, data.guid));
        }
        return {};
    }
private:
    const auth_query_flag m_flag;
    string& m_output;
    uint32_t m_index = 0;
};

error_type auth_core::initialize(const auth_config& config) noexcept
{
    const auth_request_type client_types[] =
    {
        auth_request_type::client_create,
        auth_request_type::client_ticket,
        auth_request_type::client_connect,
    };
    const auth_request_type module_types[] =
    {
        auth_request_type::module_create,
        auth_request_type::module_update,
    };
    const auth_request_type admin_types[] =
    {
        auth_request_type::client_create,
        auth_request_type::client_ticket,
        auth_request_type::client_connect,
        auth_request_type::module_create,
        auth_request_type::module_update,
    };
    ICY_ERROR(m_dbase.initialize(config.dbase));
    ICY_ERROR(m_console.initialize(config));
    ICY_ERROR(m_client.initialize(config.client, 1, client_types));
    ICY_ERROR(m_module.initialize(config.module, 1, module_types));
    ICY_ERROR(m_admin.initialize(config.admin, 1, admin_types));
    ICY_ERROR(auth_config::copy(config, m_config));
    return {};
}
error_type auth_core::wait() noexcept
{
    const error_type errors[] =
    {
        m_client.wait(),
        m_module.wait(),
        m_admin.wait(),
        m_console.wait(),
    };
    for (auto&& error : errors)
        ICY_ERROR(error);
    return {};
}
error_type auth_core::run() noexcept
{
    ICY_SCOPE_EXIT
    {
        event::post(nullptr, event_type::global_quit); 
        wait();
    };
    ICY_ERROR(m_console.launch());

    shared_ptr<event_loop> loop;
    ICY_ERROR(event_loop::create(loop, event_type::user_any));
    
    while (true)
    {
        event event;
        ICY_ERROR(loop->loop(event));
        if (event->type == event_type::global_quit)
            break;
        
        auto etype = event_type::none;
        switch (event->type)
        {
        case auth_event_type::run_http_admin:
        case auth_event_type::run_http_module:
        case auth_event_type::run_http_client:
        case auth_event_type::stop_http_admin:
        case auth_event_type::stop_http_module:
        case auth_event_type::stop_http_client:
        case auth_event_type::clear_clients:
        case auth_event_type::clear_modules:
            etype = auth_event_type::console_error;
            break;

        case auth_event_type::print_config:
        case auth_event_type::print_status:
        case auth_event_type::print_clients:
        case auth_event_type::print_modules:
            etype = auth_event_type::console_write;
            break;
        }

        error_type error;
        string str;

        switch (event->type)
        {
        case auth_event_type::run_http_admin: 
            error = m_admin.launch(); 
            break;

        case auth_event_type::run_http_module: 
            error = m_module.launch();
            break;

        case auth_event_type::run_http_client:
            error = m_client.launch();
            break;

        case auth_event_type::stop_http_admin: 
            error = m_admin.wait(); 
            break;

        case auth_event_type::stop_http_module: 
            error = m_module.wait(); 
            break;

        case auth_event_type::stop_http_client: 
            error = m_client.wait(); 
            break;

        case auth_event_type::clear_clients:
            error = m_dbase.clear_clients(); 
            break;

        case auth_event_type::clear_modules:
            error = m_dbase.clear_modules();
            break;

        case auth_event_type::print_config:
        {
            json output;
            ICY_ERROR(m_config.to_json(output));
            ICY_ERROR(to_string(output, str, "  "_s));
            break;
        }
        case auth_event_type::print_status:
        {
            ICY_ERROR(str.append( "\r\nHTTP status:"_s));
            ICY_ERROR(m_client.status(str));
            ICY_ERROR(m_module.status(str));
            ICY_ERROR(m_admin.status(str));
            break;
        }
        case auth_event_type::print_clients:
        {
            auth_query_callback_client callback(str, event->data<auth_query_flag>());
            ICY_ERROR(m_dbase.query(event->data<auth_query_flag>(), callback));
            break;
        }
        case auth_event_type::print_modules:
        {
            auth_query_callback_module callback(str, event->data<auth_query_flag>());
            ICY_ERROR(m_dbase.query(event->data<auth_query_flag>(), callback));
            break;
        }


        }

        if (etype == auth_event_type::console_error)
        {
            ICY_ERROR(event::post(nullptr, event_type(auth_event_type::console_error), error));
        }
        else if (etype == auth_event_type::console_write)
        {
            ICY_ERROR(event::post(nullptr, event_type(auth_event_type::console_write), std::move(str)));
        }
    }

    return {};
}

int main()
{
    heap gheap;
    auth_config config;
    auth_core core;

    if (const auto error = gheap.initialize(heap_init::global(auth_config::default_values::gheap_size)))
        return ENOMEM;
    
    auto show_error = [](const error_type error) -> int
    {
        string msg;
        icy::to_string("\r\nError: (%1) code %2 - %3"_s, msg, error.source, long(error.code), error);
        console_write(msg);
        return error.code;
    };
    {
        string msg;
        if (const auto error = to_string("Auth server: startup at [%1] local time."_s, msg, clock_type::now()))
            return show_error(error);
        if (const auto error = console_write(msg))
            return show_error(error);
    }


    const auto init = [&core, &config]() -> error_type
    {
        file file;
        string path;
        array<string> args;
        ICY_ERROR(win32_parse_cargs(args));
        
        if (args.size() == 1)
        {
            ICY_ERROR(path.append(file_name(args[0]).dir));
            ICY_ERROR(path.append(auth_config::default_values::file_path));
        }
        else if (args.size() > 1)
        {
            ICY_ERROR(to_string(args[1], path));
        }           

        ICY_ERROR(file.open(path, file_access::read, file_open::open_existing, file_share::read));

        char buffer[0x1000];
        auto size = _countof(buffer);
        ICY_ERROR(file.read(buffer, size));

        json json;
        ICY_ERROR(json::create(string_view(buffer, buffer + size), json));

        ICY_ERROR(config.from_json(json));
        ICY_ERROR(core.initialize(config));

        return {};
    };

    if (const auto error = init())
        return show_error(error);
    
    if (const auto error = core.run())
        return show_error(error);

    return 0;
}
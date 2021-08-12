#include <icy_engine/core/icy_json.hpp>
#include <icy_engine/core/icy_file.hpp>
#include <icy_engine/core/icy_console.hpp>
#include <icy_engine/parser/icy_parser_cmd.hpp>
#include "icy_auth_config.hpp"
#include "icy_auth_database.hpp"
#include "icy_auth_http.hpp"

#pragma warning(disable:4063) // case 'identifier' is not a valid value for switch of enum 'enumeration'

#if _DEBUG
#pragma comment(lib, "icy_engine_cored")
#pragma comment(lib, "icy_engine_networkd")
#else
#pragma comment(lib, "icy_engine_core")
#pragma comment(lib, "icy_engine_network")
#endif

using namespace icy;


struct command_pair
{
    icy::string help;
    icy::event_type type = icy::event_type::none;
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
            return error_type();

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
        ICY_ERROR(m_output.appendf("\r\n[%1]"_s, string_view(index_str, strlen(index_str))));

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
            string guid_str;
            ICY_ERROR(to_string(data.guid, guid_str));
            ICY_ERROR(m_output.appendf(" {%1}"_s, string_view(guid_str)));
        }
        return error_type();
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
        ICY_ERROR(m_output.appendf("\r\n[%1]"_s, string_view(index_str, strlen(index_str))));

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
            string guid_str;
            ICY_ERROR(to_string(data.guid, guid_str));
            ICY_ERROR(m_output.appendf(" {%1}"_s, string_view(guid_str)));
        }
        return error_type();
    }
private:
    const auth_query_flag m_flag;
    string& m_output;
    uint32_t m_index = 0;
};

template<typename... arg_types>
static error_type append(map<string_view, command_pair>& cmds,
    string_view cmd, const string_view format, const event_type event, arg_types&&... args) noexcept
{
    command_pair pair;
    ICY_ERROR(to_string(format, pair.help, std::forward<arg_types>(args)...));
    pair.type = event;
    ICY_ERROR(cmds.insert(std::move(cmd), std::move(pair)));
    return error_type();
}



error_type cmd_init(map<string_view, command_pair>& cmds, const auth_config& config) noexcept
{
    ICY_ERROR(append(cmds, "run_http_admin"_s,
        "Launch HTTP [Type = Admin] socket at [Port = %1]"_s,
        auth_event_type::run_http_admin, config.admin.http.port));

    ICY_ERROR(append(cmds, "run_http_module"_s,
        "Launch HTTP [Type = Module] sockets [Count = %1] at [Port = %2]"_s,
        auth_event_type::run_http_module, config.module.http.capacity, config.module.http.port));

    ICY_ERROR(append(cmds, "run_http_client"_s,
        "Launch HTTP [Type = Client] sockets [Count = %1] at [Port = %2] with [T/O = %3 ms]"_s,
        auth_event_type::run_http_client, config.client.http.capacity, config.client.http.port,
        std::chrono::duration_cast<std::chrono::milliseconds>(config.client.http.timeout).count()));

    ICY_ERROR(append(cmds, "stop_http_admin"_s,
        "Stop HTTP [Type = Admin]"_s,
        auth_event_type::stop_http_admin));

    ICY_ERROR(append(cmds, "stop_http_module"_s,
        "Stop HTTP [Type = Module]"_s,
        auth_event_type::stop_http_module));

    ICY_ERROR(append(cmds, "stop_http_client"_s,
        "Stop HTTP [Type = Client]"_s,
        auth_event_type::stop_http_client));

    ICY_ERROR(append(cmds, "clear_clients"_s,
        "Clear clients database (requires admin auth code)"_s,
        auth_event_type::clear_clients));

    ICY_ERROR(append(cmds, "clear_modules"_s,
        "Clear modules database (requires admin auth code)"_s,
        auth_event_type::clear_modules));

    ICY_ERROR(append(cmds, "exit"_s,
        "Shutdown Auth Server"_s,
        auth_event_type::global_exit));

    ICY_ERROR(append(cmds, "print_config"_s,
        "Print config file"_s,
        auth_event_type::print_config));

    ICY_ERROR(append(cmds, "print_clients"_s,
        "Enumerate clients (opt boolean: 'guid=1', 'date=1', 'name=1')"_s,
        auth_event_type::print_clients));

    ICY_ERROR(append(cmds, "print_modules"_s,
        "Enumerate modules (opt boolean: 'guid=1', 'date=1', 'name=1', 'addr=1', 'time=1')"_s,
        auth_event_type::print_modules));

    ICY_ERROR(append(cmds, "print_status"_s,
        "Display current public/module/admin HTTP status"_s,
        auth_event_type::print_status));

    return error_type();
}

error_type main_ex() noexcept
{
    ICY_ERROR(event_system::initialize());
    shared_ptr<event_queue> loop;
    ICY_ERROR(create_event_system(loop, 0
        | event_type::network_connect
        | event_type::network_recv
        | event_type::console_read_line));
    
    shared_ptr<console_system> console;
    ICY_ERROR(create_console_system(console));
    ICY_ERROR(console->thread().launch());
    ICY_ERROR(console->thread().rename("Console Thread"_s));

    {
        string msg;
        string time_str;
        ICY_ERROR(to_string(clock_type::now(), time_str));
        ICY_ERROR(to_string("Auth server: startup at [%1] local time."_s, msg, string_view(time_str)));
        ICY_ERROR(console->write(msg));
    }
    auth_config config;
    {
        file file;
        string path;
        array<string> args;
        ICY_ERROR(win32_parse_cargs(args));

        if (args.size() == 1)
        {
            ICY_ERROR(path.append(file_name(args[0]).directory));
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
        ICY_ERROR(to_value(string_view(buffer, buffer + size), json));

        ICY_ERROR(config.from_json(json));
    }


    const auto hint = [&console]
    {
        ICY_ERROR(console->write("\r\nEnter: "_s));
        ICY_ERROR(console->read_line());
        return error_type();
    };
    ICY_ERROR(hint());

    auth_database dbase;
    unique_ptr<auth_network_system> client;
    unique_ptr<auth_network_system> module;
    unique_ptr<auth_network_system> admin;

    map<string_view, command_pair> cmds;
    ICY_ERROR(cmd_init(cmds, config));

    auto mode = event_type::none;
    char base64[base64_encode_size(sizeof(crypto_salt))];
    const auto str_code = string_view(base64, _countof(base64) / 2);

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

    ICY_ERROR(dbase.initialize(config.dbase));

    const auto exec_console = [&](event event)
    {
        const auto& event_data = event->data<console_event>();
        const string_view cmd = event_data.text;

        string str;
        error_type error;

        if (mode == event_type::none)
        {
            if (cmd == "help"_s)
            {
                ICY_ERROR(console->write("\r\nConsole commands: "_s));
                auto max_length = 0_z;
                for (auto&& cmd : cmds)
                    max_length = std::max(cmd.key.bytes().size(), max_length);

                for (auto&& cmd : cmds)
                {
                    string spaces;
                    ICY_ERROR(spaces.resize(max_length - cmd.key.bytes().size(), ' '));

                    string str;
                    ICY_ERROR(str.appendf("\r\n  %1  %2: %3"_s, cmd.key, string_view(spaces), string_view(cmd.value.help)));
                    ICY_ERROR(console->write(str));
                }
                ICY_ERROR(hint());
                return error_type();
            }

            auto etype = event_type::none;
            cmd_parser parser;

            const auto e0 = parser(cmd.bytes());
            const auto e1 = parser({});
            if (!e0 && !e1)
            {
                const auto ptr = cmds.try_find(parser.cmd());
                if (ptr)
                    etype = ptr->type;
            }

            if (e0 == make_stdlib_error(std::errc::illegal_byte_sequence) ||
                e1 == make_stdlib_error(std::errc::illegal_byte_sequence) ||
                etype == event_type::none)
            {
                ICY_ERROR(console->write("\r\nInvalid command or arguments! Type 'help' for more info."_s));
                ICY_ERROR(hint());
                return error_type();
            }
            else
            {
                ICY_ERROR(e0);
                ICY_ERROR(e1);
            }
            switch (etype)
            {
            case auth_event_type::clear_clients:
            case auth_event_type::clear_modules:
            {
                string msg;
                crypto_salt random = crypto_random;
                base64_encode(random, base64);
                ICY_ERROR(to_string("\r\nPlease confirm operation (input code '%1'): "_s, msg, str_code));
                ICY_ERROR(console->write(msg));
                mode = etype;
                ICY_ERROR(console->read_line());
                return error_type();
            }
            case auth_event_type::run_http_admin:
                if (!error) error = make_unique(auth_network_system(dbase), admin);
                if (!error) error = admin->initialize(config.admin, 1, admin_types);
                if (!error) error = error = admin->thread().launch();
                if (!error) error = admin->thread().rename("Admin Network Thread"_s);
                ICY_ERROR(copy("\r\nAdmin HTTP thread launched"_s, str));
                break;
            case auth_event_type::run_http_module:
                if (!error) error = make_unique(auth_network_system(dbase), module);
                if (!error) error = module->initialize(config.module, 1, module_types);
                if (!error) error = error = module->thread().launch();
                if (!error) error = module->thread().rename("Module Network Thread"_s);
                ICY_ERROR(copy("\r\nModule HTTP thread launched"_s, str));
                break;
            case auth_event_type::run_http_client:
                if (!error) error = make_unique(auth_network_system(dbase), client);
                if (!error) error = client->initialize(config.client, 1, client_types);
                if (!error) error = error = client->thread().launch();
                if (!error) error = client->thread().rename("Client Network Thread"_s);
                ICY_ERROR(copy("\r\nClient HTTP thread launched"_s, str));
                break;
            case auth_event_type::stop_http_admin:
                if (admin)
                {
                    error = admin->thread().wait();
                    admin = nullptr;
                }
                ICY_ERROR(copy("\r\nAdmin HTTP thread stopped"_s, str));
                break;
            case auth_event_type::stop_http_module:
                if (module)
                {
                    error = module->thread().wait();
                    module = nullptr;
                }
                ICY_ERROR(copy("\r\nModule HTTP thread stopped"_s, str));
                break;
            case auth_event_type::stop_http_client:
                if (client)
                {
                    error = client->thread().wait();
                    client = nullptr;
                }
                ICY_ERROR(copy("\r\nClient HTTP thread stopped"_s, str));
                break;
            case auth_event_type::print_config:
            {
                json output;
                ICY_ERROR(config.to_json(output));
                ICY_ERROR(to_string(output, str, "  "_s));
                break;
            }
            case auth_event_type::print_status:
                ICY_ERROR(str.append("\r\nHTTP status:"_s));
                ICY_ERROR(str.append("\r\nClient: "_s));
                ICY_ERROR(client ? client->status(str) : error_type());
                ICY_ERROR(str.append("\r\nModule: "_s));
                ICY_ERROR(module ? module->status(str) : error_type());
                ICY_ERROR(str.append("\r\nAdmin : "_s));
                ICY_ERROR(admin ? admin->status(str) : error_type());
                break;

            case auth_event_type::print_clients:
            case auth_event_type::print_modules:
            {
                bool exit = false;
                bool name = true;
                bool guid = false;
                bool date = false;
                bool addr = false;
                bool time = false;

                const auto func = [&console, &parser, &exit](const string_view text, bool& boolean)
                {
                    if (const auto ptr = parser.args().try_find(text))
                    {
                        if (*ptr == "1"_s)
                        {
                            boolean = true;
                        }
                        else if (*ptr == "0"_s)
                        {
                            boolean = false;
                        }
                        else
                        {
                            string str;
                            ICY_ERROR(str.appendf("\r\nPrint error: '%1' must be boolean"_s, text));
                            ICY_ERROR(console->write(str));
                            exit = true;
                        }
                    }
                    return error_type();
                };
                ICY_ERROR(func("name"_s, name)); if (exit) break;
                ICY_ERROR(func("guid"_s, guid)); if (exit) break;
                ICY_ERROR(func("date"_s, date)); if (exit) break;
                ICY_ERROR(func("addr"_s, addr)); if (exit) break;
                ICY_ERROR(func("time"_s, time)); if (exit) break;

                auto flag = auth_query_flag::none;
                if (etype == auth_event_type::print_clients)
                {
                    if (guid) flag = flag | auth_query_flag::client_guid;
                    if (date) flag = flag | auth_query_flag::client_date;
                    if (name) flag = flag | auth_query_flag::client_name;
                }
                else
                {
                    if (guid) flag = flag | auth_query_flag::module_guid;
                    if (date) flag = flag | auth_query_flag::module_date;
                    if (name) flag = flag | auth_query_flag::module_name;
                    if (addr) flag = flag | auth_query_flag::module_addr;
                    if (time) flag = flag | auth_query_flag::module_time;
                }
                if (etype == auth_event_type::print_clients)
                {
                    auth_query_callback_client callback(str, flag);
                    error = dbase.query(flag, callback);
                }
                else if (etype == auth_event_type::print_modules)
                {
                    auth_query_callback_module callback(str, flag);
                    error = dbase.query(flag, callback);
                }
                break;
            }
            }
        }
        else
        {
            const auto& event_data = event->data<console_event>();

            if (event_data.text != str_code)
            {
                ICY_ERROR(console->write("\r\nInvalid confirmation code: admin operation cancelled!"_s));
                mode = event_type::none;
                ICY_ERROR(hint());
                return error_type();
            }
            error_type error;
            if (mode == auth_event_type::clear_modules)
                error = dbase.clear_modules();
            else if (mode == auth_event_type::clear_clients)
                error = dbase.clear_clients();
            mode = event_type::none;
        }

        if (error)
        {
            string msg;
            to_string("\r\nError: (%1) code %2 - %3"_s, msg, error.source, long(error.code), error);
            ICY_ERROR(console->write(msg));
        }
        else if (!str.empty())
        {
            ICY_ERROR(console->write(str));
        }
        ICY_ERROR(hint());
        return error_type();
    };

    while (*loop)
    {
        event event;
        ICY_ERROR(loop->pop(event));
        if (!event)
            continue;

        if (event->type == event_type::console_read_line)
        {
            ICY_ERROR(exec_console(event));
            continue;
        }

        const auto& event_data = event->data<network_event>();
        auto system = shared_ptr<event_system>(event->source);
        auto network = shared_ptr<network_system_http_server>(system);

        if (event_data.error)
            continue;

        if (event->type == event_type::network_connect)
        {
            ICY_ERROR(network->recv(event_data.conn));
        }
        else if (event->type == event_type::network_recv)
        {
            http_response http_response;
            string output;
            http_response.herror = http_error::bad_request;

            if (!event_data.error
                && event_data.http.request
                && event_data.http.request->type == http_request_type::post
                && event_data.http.request->content == http_content_type::application_json)
            {
                const auto& body = event_data.http.request->body;
                json response;

                auth_network_system* auth_system = nullptr;
                if (false);
                else if (client && system == client->system_ptr()) auth_system = client.get();
                else if (module && system == module->system_ptr()) auth_system = module.get();
                else if (admin && system == admin->system_ptr()) auth_system = admin.get();

                if (auth_system)
                {
                    const auto error = auth_system->process(string_view((const char*)body.data(), body.size()), event_data.address, response);
                    if (response.type() != json_type::none)
                    {
                        if (!response.find("Error"_s))
                            http_response.herror = http_error::success;
                        http_response.type = http_content_type::application_json;
                    }
                    if (error.source == error_source_auth)
                        ;
                    else
                        ICY_ERROR(error);
                    ICY_ERROR(to_string(response, output));
                }

            }
            ICY_ERROR(http_response.body.assign(output.ubytes()));
            ICY_ERROR(network->send(event_data.conn, http_response));
            ICY_ERROR(network->disc(event_data.conn));
        }
    }
    return error_type();
}
int main()
{
    heap gheap;
    if (const auto error = gheap.initialize(heap_init::global(64_mb)))
        return ENOMEM;

    if (auto error = main_ex())
        return error.code;

    return 0;
}
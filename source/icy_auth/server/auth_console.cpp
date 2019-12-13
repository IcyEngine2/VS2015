#include <icy_engine/icy_string.hpp>
#include <icy_engine/icy_array.hpp>
#include <icy_engine/icy_win32.hpp>
#include <icy_engine/icy_json.hpp>
#include <icy_engine/icy_parser.hpp>
#include <icy_engine/icy_input.hpp>
#include "auth_console.hpp"
#include "auth_core.hpp"
#include "auth_database.hpp"

#define AUTH_CONFIRMATION_CODE "1581"_s

using namespace icy;

template<typename... arg_types>
static error_type append(map<string_view, auth_console_thread::pair_type>& cmds,
    string_view cmd, const string_view format, const event_type event, arg_types&&... args) noexcept
{
    auth_console_thread::pair_type pair;
    ICY_ERROR(to_string(format, pair.help, std::forward<arg_types>(args)...));
    pair.type = event;
    ICY_ERROR(cmds.insert(std::move(cmd), std::move(pair)));
    return {};
}


error_type auth_console_thread::initialize(const auth_config& config) noexcept
{
    ICY_ERROR(append(m_commands, "run_http_admin"_s, 
        "Launch HTTP [Type = Admin] socket at [Port = %1]"_s,
        auth_event_type::run_http_admin, config.admin.http.port));

    ICY_ERROR(append(m_commands, "run_http_module"_s, 
        "Launch HTTP [Type = Module] sockets [Count = %1] at [Port = %1]"_s, 
        auth_event_type::run_http_module, config.module.http.max_conn, config.module.http.port));

    ICY_ERROR(append(m_commands, "run_http_client"_s, 
        "Launch HTTP [Type = Client] sockets [Count = %1] at [Port = %2] with [T/O = %3 ms]",
        auth_event_type::run_http_client, config.client.http.max_conn, config.client.http.port, 
        std::chrono::duration_cast<std::chrono::milliseconds>(config.client.http.timeout).count()));

    ICY_ERROR(append(m_commands, "stop_http_admin"_s, 
        "Stop HTTP [Type = Admin]"_s,
        auth_event_type::stop_http_admin));

    ICY_ERROR(append(m_commands, "stop_http_module"_s,
        "Stop HTTP [Type = Module]"_s,
        auth_event_type::stop_http_module));

    ICY_ERROR(append(m_commands, "stop_http_client"_s, 
        "Stop HTTP [Type = Client]"_s, 
        auth_event_type::stop_http_client));

    ICY_ERROR(append(m_commands, "clear_clients"_s,
        "Clear clients database (requires admin auth code)"_s,
        auth_event_type::clear_clients));

    ICY_ERROR(append(m_commands, "clear_modules"_s,
        "Clear modules database (requires admin auth code)"_s,
        auth_event_type::clear_modules));
    
    ICY_ERROR(append(m_commands, "exit"_s, 
        "Shutdown Auth Server"_s, 
        event_type::global_quit));

    ICY_ERROR(append(m_commands, "print_config"_s, 
        "Print config file"_s, 
        auth_event_type::print_config));

    ICY_ERROR(append(m_commands, "print_clients"_s, 
        "Enumerate clients (opt boolean: 'guid=1', 'date=1', 'name=1')"_s, 
        auth_event_type::print_clients));

    ICY_ERROR(append(m_commands, "print_modules"_s, 
        "Enumerate modules (opt boolean: 'guid=1', 'date=1', 'name=1', 'addr=1', 'time=1')"_s, 
        auth_event_type::print_modules));

    ICY_ERROR(append(m_commands, "print_status"_s,
        "Display current public/module/admin HTTP status"_s,
        auth_event_type::print_status));

    return {};
}

void auth_console_thread::cancel() noexcept
{
    console_exit();
}
error_type auth_console_thread::run() noexcept
{
    ICY_SCOPE_EXIT{ event::post(nullptr, event_type::global_quit); };
    auto done = false;
    while (!done)
    {
        ICY_ERROR(console_write("\r\nEnter: "_s));

        string str_cmd;
        ICY_ERROR(console_read_line(str_cmd, done));
        if (done)
            break;

        if (str_cmd == "help"_s)
        {
            ICY_ERROR(console_write("\r\nConsole commands: "_s));
            auto max_length = 0_z;
            for (auto&& cmd : m_commands)
                max_length = std::max(cmd.key.bytes().size(), max_length);

            for (auto&& cmd : m_commands)
            {
                string spaces;
                ICY_ERROR(spaces.resize(max_length - cmd.key.bytes().size(), ' '));

                string str;
                ICY_ERROR(str.appendf("\r\n  %1  %2: %3"_s, cmd.key, string_view(spaces), string_view(cmd.value.help)));
                ICY_ERROR(console_write(str));
            }
            continue;
        }

        auto etype = event_type::none;
        cmd_parser parser;
        
        const auto e0 = parser(str_cmd.bytes());
        const auto e1 = parser({});
        if (!e0 && !e1)
        {
            const auto ptr = m_commands.try_find(parser.cmd());
            if (ptr)
                etype = ptr->type;
        }

        if (etype != event_type::none)
        {
            if (etype == auth_event_type::clear_clients ||
                etype == auth_event_type::clear_modules)
            {
                ICY_ERROR(console_write("\r\nThis admin operation can't be undone! Confirmation code: "_s));
                string code;
                ICY_ERROR(console_read_line(code, done));
                if (done)
                    break;
                if (code != AUTH_CONFIRMATION_CODE)
                {
                    ICY_ERROR(console_write("\r\nInvalid confirmation code: admin operation cancelled!"_s));
                    continue;
                }
            }

            switch (etype)
            {
            case auth_event_type::run_http_admin:
            case auth_event_type::run_http_module:
            case auth_event_type::run_http_client:
            case auth_event_type::stop_http_admin:
            case auth_event_type::stop_http_module:
            case auth_event_type::stop_http_client:
            case auth_event_type::clear_clients:
            case auth_event_type::clear_modules:
            {
                ICY_ERROR(server_console_loop(auth_event_type::console_error, etype, done));
                break;
            }

            case auth_event_type::print_config:
            case auth_event_type::print_status:
            case auth_event_type::print_clients:
            case auth_event_type::print_modules:
            {
                shared_ptr<event_loop> loop;
                ICY_ERROR(event_loop::create(loop, uint64_t(auth_event_type::console_write)));
                event event;

                if (etype == auth_event_type::print_clients ||
                    etype == auth_event_type::print_modules)
                {
                    bool exit = false;
                    bool name = true;
                    bool guid = false;
                    bool date = false;
                    bool addr = false;
                    bool time = false;

                    const auto func = [&parser, &exit](const string_view text, bool& boolean)
                    {
                        if (const auto ptr = parser.args().try_find(text))
                        {
                            if (*ptr == "1"_s)
                                boolean = true;
                            else if (*ptr == "0"_s)
                                boolean = false;
                            else
                            {
                                string str;
                                ICY_ERROR(str.appendf("\r\nPrint error: '%1' must be boolean"_s, text));
                                ICY_ERROR(console_write(str));
                                exit = true;
                            }
                        }
                        return error_type{};
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
                    ICY_ERROR(event::post(nullptr, event_type(etype), flag));
                }
                else
                {
                    ICY_ERROR(event::post(nullptr, event_type(etype)));
                }
                ICY_ERROR(loop->loop(event));
                if (event && event->type == auth_event_type::console_write)
                    ICY_ERROR(console_write(event->data<string>()));
                break;
            }
            
            case event_type::global_quit:
                return {};
            
            default:
                break;
            }
        }

        
        if (e0 == make_stdlib_error(std::errc::illegal_byte_sequence) ||
            e1 == make_stdlib_error(std::errc::illegal_byte_sequence) || 
            etype == event_type::none)
        {
            ICY_ERROR(console_write("\r\nInvalid command or arguments! Type 'help' for more info."_s));
        }
        else
        {
            ICY_ERROR(e0);
            ICY_ERROR(e1);
        }
    }
    return {};
}
#include <icy_engine/icy_win32.hpp>
#include <icy_engine/icy_parser.hpp>
#include <icy_engine/icy_network.hpp>
#include <icy_engine/icy_http.hpp>
#include <icy_engine/icy_json.hpp>
#include <icy_module/icy_auth.hpp>
#include <icy_lib/icy_crypto.hpp>

#if _DEBUG
#pragma comment(lib, "icy_engined")
#else
#pragma comment(lib, "icy_engine")
#endif

using namespace icy;

namespace auth_test_event_enum
{
    enum
    {
        none,
        exit,
        set_address,
        client_create,
        client_ticket,
        client_connect,
        module_create,
        module_update,
    };
};
using auth_test_event = decltype(auth_test_event_enum::none);


static error_type print_error(const error_type error) noexcept
{
    string msg;
    ICY_ERROR(icy::to_string("Error: (%1) code %2 - %3"_s, msg, error.source, error.source.hash == error_source_system.hash ?
        (error.code & 0xFFFF) : error.code, error));
    ICY_ERROR(console_write(msg));
    return {};
};
class test_core
{
public:
    error_type initialize() noexcept;
    error_type run();
private:
    struct pair_type
    {
        pair_type() noexcept = default;
        pair_type(const string_view help, const auth_test_event type) noexcept : help(help), type(type)
        {

        }
        string_view help;
        auth_test_event type = auth_test_event::none;
    };
private:
    error_type show_help() noexcept;
    error_type get_addr(const string_view str_addr, array<network_address>& address) noexcept;
    error_type execute(auth_request& auth_request, json& output) noexcept;
    error_type execute2(const const_array_view<network_address> addr, const http_request& request, 
        http_response& header, array<uint8_t>& body) noexcept;
private:
    map<string_view, pair_type> m_commands;
    icy::network_system m_network;
    array<network_address> m_address;
};

error_type test_core::initialize() noexcept
{
    ICY_ERROR(m_commands.insert("exit"_s,
        { "Exit program"_s,
        auth_test_event::exit }));

    ICY_ERROR(m_commands.insert("set_address"_s,
        { "Set connect address. Parameters: address=\"127.0.0.1:1234\""_s,
        auth_test_event::set_address }));

    ICY_ERROR(m_commands.insert("client_create"_s,
        { "Create new client. Required parameters: login;password"_s,
        auth_test_event::client_create }));

    ICY_ERROR(m_commands.insert("client_ticket"_s,
        { "Acquire global auth ticket. Required parameters: login;password"_s,
        auth_test_event::client_ticket }));

    ICY_ERROR(m_commands.insert("client_connect"_s,
        { "Begin client-module session (prev global auth ticket). Required parameters: module"_s,
        auth_test_event::client_connect }));

    ICY_ERROR(m_commands.insert("module_create"_s,
        { "Create new module. Required parameters: module"_s,
        auth_test_event::module_create }));

    ICY_ERROR(m_commands.insert("module_update"_s,
        { "Updates existing module. Required parameters: module;address;timeout(ms);password"_s,
        auth_test_event::module_update }));

    ICY_ERROR(m_network.initialize());
    return {};
}
error_type test_core::show_help() noexcept
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
    return {};
}
error_type test_core::get_addr(const string_view str_addr, array<network_address>& address) noexcept
{
    address.clear();
    error_type error;
    const auto it = str_addr.find(":");
    if (it != str_addr.end())
    {
        const auto str_host = string_view(str_addr.begin(), it);
        const auto str_port = string_view(it + 1, str_addr.end());
        error = m_network.address(str_host, str_port, network_default_timeout, network_socket_type::TCP, address);
    }
    else
    {
        error = make_stdlib_error(std::errc::illegal_byte_sequence);
    }
    if (error)
    {
        ICY_ERROR(print_error(error));
    }
    else
    {
        ICY_ERROR(console_write("Success! IP List: "_s));
        auto first = true;
        for (auto&& addr : address)
        {
            string next_addr;
            ICY_ERROR(to_string(addr, next_addr));
            if (!first)
                ICY_ERROR(console_write(", "_s));
            ICY_ERROR(console_write(next_addr));
            first = false;
        }
    }

    return {};
}
error_type test_core::execute(auth_request& auth_request, json& output) noexcept
{
    ICY_ERROR(create_guid(auth_request.guid));
    
    http_request http_request;
    http_request.type = http_request_type::post;
    http_request.content = http_content_type::application_json;

    json json_request;
    ICY_ERROR(auth_request.to_json(json_request));
    ICY_ERROR(to_string(json_request, http_request.body));

    http_response header;
    array<uint8_t> body;
    if (const auto error = execute2(m_address, http_request, header, body))
    {
        ICY_ERROR(print_error(error));
        return {};
    }
    if (body.empty())
    {
        ICY_ERROR(console_write("\r\nAuth request error: no reply!"_s));
        return {};
    }

    ICY_ERROR(console_write("\r\nValidating Auth JSON response... "_s));
    auth_response auth_response(auth_request);

    json json_response;
    if (const auto error = json::create(string_view(reinterpret_cast<const char*>(body.data()), body.size()), json_response))
    {
        ICY_ERROR(print_error(make_auth_error(auth_error_code::invalid_json)));
    }
    else if (const auto error = auth_response.from_json(json_response))
    {
        ICY_ERROR(print_error(error));
    }
    else if (auth_response.error != auth_error_code::none)
    {
        ICY_ERROR(print_error(make_auth_error(auth_response.error)));
    }
    else
    {
        if (auth_response.time < auth_request.time - network_default_timeout || 
            auth_response.time > auth_request.time + network_default_timeout)
        {
            ICY_ERROR(print_error(make_auth_error(auth_error_code::timeout)));
        }
        else
        {
            ICY_ERROR(auth_response.message.copy(output));
            ICY_ERROR(console_write("Success!"_s));
        }
    }
    return {};
}
error_type test_core::execute2(const const_array_view<network_address> address, const http_request& request, http_response& header, array<uint8_t>& body) noexcept
{
    bool exit = false;
    string str;
    ICY_ERROR(to_string(request, str));
    network_connection conn(network_connection_type::http);
    conn.timeout(network_default_timeout);

    for (auto&& addr : address)
    {
        string msg;
        string addr_str;
        to_string(addr, addr_str);
        ICY_ERROR(to_string("\r\nConnecting to address: %1... "_s, msg, string_view(addr_str)));
        ICY_ERROR(console_write(msg));
        if (const auto error = m_network.connect(conn, addr, str.ubytes()))
        {
            ICY_ERROR(print_error(error));
            continue;
        }
       
        network_reply reply;
        const auto check = [this, &reply](const network_request_type type, int& exit) -> error_type
        {
            auto done = false;
            while (!done)
            {
                ICY_ERROR(m_network.loop(reply, done));
                if (done)
                {
                    exit = 1;
                    return {};
                }
                if (reply.type == network_request_type::timeout)
                {
                    ICY_ERROR(console_write("Error: connection timeout."_s));
                    exit = -1;
                    return {};
                }
                else if (
                    reply.type == network_request_type::disconnect ||
                    reply.type == network_request_type::shutdown)
                {
                    ICY_ERROR(console_write("Error: connection reset (shutdown or disconnect)."_s));
                    exit = -1;
                    return {};
                }
                else if (reply.error)
                {
                    ICY_ERROR(print_error(reply.error));
                    exit = -1;
                    return {};
                }
                else if (reply.type != type)
                {
                    //ICY_ERROR(console_write("Error: unknown network reply."_s));
                    //exit = -1;
                    //return {};
                    continue;
                }
                else
                {
                    break;
                }
            }
            exit = 0;
            return {};
        };

        auto exit = 0;
        ICY_ERROR(check(network_request_type::connect, exit));
        if (exit == -1)
            continue;
        else if (exit == 1)
            break;

        ICY_ERROR(console_write("Success!"_s));
        ICY_ERROR(console_write("\r\nWaiting for HTTP response... "_s));
        if (const auto error = m_network.recv(conn, 4_kb))
        {
            ICY_ERROR(print_error(error));
            continue;
        }
        ICY_ERROR(check(network_request_type::recv, exit));
        if (exit == -1)
            continue;
        else if (exit == 1)
            break;
        ICY_ERROR(console_write("Success!"_s));

        ICY_ERROR(console_write("\r\nParsing HTTP response... "_s));
        const_array_view<uint8_t> bytes;
        if (const auto error = http_response::create(string_view(reinterpret_cast<const char*>(reply.bytes.data()), reply.bytes.size()), header, bytes))
        {
            ICY_ERROR(print_error(error));
            continue;
        }
        ICY_ERROR(console_write("Success!"_s));
        ICY_ERROR(body.assign(bytes));
        break;
    }
    return {};
}

error_type test_core::run()
{
    bool done = false;
    unique_ptr<crypto_msg<icy::auth_client_ticket_server>> cache_ticket;
    uint64_t cache_client = 0;
    crypto_key cache_password;

    while (!done)
    {
        ICY_ERROR(console_write("\r\n\r\nEnter: "_s));

        string str_cmd;
        ICY_ERROR(console_read_line(str_cmd, done));
        if (done)
            break;

        if (str_cmd == "help"_s)
        {
            ICY_ERROR(show_help());
            continue;
        }
        auto etype = auth_test_event::none;
        cmd_parser parser;
        const auto e0 = parser(str_cmd.bytes());
        const auto e1 = parser({});
        if (!e0 && !e1)
        {
            const auto ptr = m_commands.try_find(parser.cmd());
            if (ptr)
                etype = ptr->type;
        }
        switch (etype)
        {
        case auth_test_event::set_address:        
        {
            if (const auto ptr = parser.args().try_find("address"_s))
            {
                ICY_ERROR(console_write("\r\nCreating IP list... "_s));
                ICY_ERROR(get_addr(*ptr, m_address));
            }
            else
            {
                ICY_ERROR(console_write("\r\nSet address: 'address' parameter not found!"_s));
            }
            break;
        }

        case auth_test_event::client_create:
        {
            if (m_address.empty())
            {
                ICY_ERROR(console_write("\r\nClient create error: IP address not set!"_s));
                break;
            }
            const auto ptr_name = parser.args().try_find("login"_s); 
            if (!ptr_name)
            {
                ICY_ERROR(console_write("\r\nClient create error: 'login' parameter not found!"_s));
                break;
            }
            const auto ptr_pass = parser.args().try_find("password"_s);
            if (!ptr_pass)
            {
                ICY_ERROR(console_write("\r\nClient create error: 'password' parameter not found!"_s));
                break;
            }

            ICY_ERROR(console_write("\r\nEncrypting user password... "_s));
            const auto password = crypto_key(*ptr_name, *ptr_pass);
            ICY_ERROR(console_write("Success!"_s));
            const auto username = hash64(*ptr_name);

            auth_request auth_request;
            auth_request.type = auth_request_type::client_create;
            auth_request.username = username;
            auth_request.time = std::chrono::system_clock::now();
            
            auth_request_msg_client_create msg;
            msg.password = password;
            ICY_ERROR(msg.to_json(auth_request.message));
            
            json json;
            ICY_ERROR(execute(auth_request, json));

            break;
        }
        case auth_test_event::client_ticket:
        {
            if (m_address.empty())
            {
                ICY_ERROR(console_write("\r\nUser ticket error: IP address not set!"_s));
                break;
            }
            const auto ptr_name = parser.args().try_find("login"_s);
            if (!ptr_name)
            {
                ICY_ERROR(console_write("\r\nUser ticket error: 'login' parameter not found!"_s));
                break;
            }
            const auto ptr_pass = parser.args().try_find("password"_s);
            if (!ptr_pass)
            {
                ICY_ERROR(console_write("\r\nUser ticket error: 'password' parameter not found!"_s));
                break;
            }

            ICY_ERROR(console_write("\r\nEncrypting user password... "_s));
            const auto password = crypto_key(*ptr_name, *ptr_pass);
            ICY_ERROR(console_write("Success!"_s));
            const auto username = hash64(*ptr_name);

            auth_request auth_request;
            auth_request.type = auth_request_type::client_ticket;
            auth_request.username = username;
            auth_request.time = std::chrono::system_clock::now();

            auth_request_msg_client_ticket msg;
            msg.encrypted_time = crypto_msg<std::chrono::system_clock::time_point>(password, auth_request.time);
            ICY_ERROR(msg.to_json(auth_request.message));

            json json;
            ICY_ERROR(execute(auth_request, json));

            if (json.type() == json_type::none)
                break;

            ICY_ERROR(console_write("\r\nParsing user ticket... "_s));
            auth_response_msg_client_ticket encrypted_ticket;
            if (const auto error = encrypted_ticket.from_json(json))
            {
                ICY_ERROR(print_error(error));
                break;
            }
            ICY_ERROR(console_write("Success!"_s));

            ICY_ERROR(console_write("\r\nDecoding user ticket... "_s));
            auth_client_ticket_client client_ticket;
            if (const auto error = encrypted_ticket.encrypted_client_ticket.decode(password, client_ticket))
            {
                ICY_ERROR(print_error(error));
                break;
            }
            ICY_ERROR(console_write("Success!"_s));

            char base64[base64_encode_size(sizeof(encrypted_ticket.encrypted_server_ticket))];
            ICY_ERROR(base64_encode(encrypted_ticket.encrypted_server_ticket, base64));

            string str;
            ICY_ERROR(to_string("\r\nTicket (server): %1"_s, str, string_view(base64, _countof(base64))));
            ICY_ERROR(console_write(str));

            ICY_ERROR(to_string("\r\nTicket expires at: %1"_s, str, client_ticket.expire));
            ICY_ERROR(console_write(str));

            cache_ticket = make_unique<crypto_msg<auth_client_ticket_server>>(encrypted_ticket.encrypted_server_ticket);
            cache_client = username;
            cache_password = password;
            break;
        }
        case auth_test_event::client_connect:
        {
            if (!cache_ticket)
            {
                ICY_ERROR(console_write("\r\nUser connect error: ticket not created!"_s));
                break;
            }
            const auto ptr_name = parser.args().try_find("module"_s);
            if (!ptr_name)
            {
                ICY_ERROR(console_write("\r\nUser connect error: 'module' parameter not found!"_s));
                break;
            }

            auth_request auth_request;
            auth_request.type = auth_request_type::client_connect;
            auth_request.username = cache_client;
            auth_request.time = std::chrono::system_clock::now();
            
            auth_request_msg_client_connect msg;
            msg.encrypted_server_ticket = *cache_ticket;
            msg.module = hash64(*ptr_name);
            ICY_ERROR(msg.to_json(auth_request.message));

            json json;
            ICY_ERROR(execute(auth_request, json));

            if (json.type() == json_type::none)
                break;

            ICY_ERROR(console_write("\r\nParsing client and module pair session msg... "_s));
            auth_response_msg_client_connect encrypted_pair;
            if (const auto error = encrypted_pair.from_json(json))
            {
                ICY_ERROR(print_error(error));
                break;
            }
            ICY_ERROR(console_write("Success!"_s));

            auth_client_connect_client client_connect;
            ICY_ERROR(console_write("\r\nDecoding client session ticket... "_s));
            if (const auto error = encrypted_pair.encrypted_client_connect.decode(cache_password, client_connect))
            {
                ICY_ERROR(print_error(error));
                break;
            }
            ICY_ERROR(console_write("Success!"_s));

            array<network_address> module_address;
            ICY_ERROR(console_write("\r\nDNS of module address... "_s));
            if (const auto error = get_addr(string_view(client_connect.address,
                strnlen(client_connect.address, _countof(client_connect.address))), module_address))
            {
                ICY_ERROR(print_error(error));
                break;
            }

            string module_name;
            ICY_ERROR(to_string(msg.module, 0x10, module_name));

            string str;
            ICY_ERROR(to_string("\r\nClient connect: module %1 is available until %2"_s, str,
                string_view(module_name), client_connect.expire));
            ICY_ERROR(console_write(str));
            break;
        }
        case auth_test_event::module_create:
        {
            if (m_address.empty())
            {
                ICY_ERROR(console_write("\r\nModule create error: IP address not set!"_s));
                break;
            }
            const auto ptr_name = parser.args().try_find("module"_s);
            if (!ptr_name)
            {
                ICY_ERROR(console_write("\r\nModule create error: 'module' parameter not found!"_s));
                break;
            }
            const auto modulename = hash64(*ptr_name);

            auth_request auth_request;
            auth_request.type = auth_request_type::module_create;
            auth_request.time = std::chrono::system_clock::now();

            auth_request_msg_module_create msg;
            msg.module = modulename;
            ICY_ERROR(msg.to_json(auth_request.message));

            json json;
            ICY_ERROR(execute(auth_request, json));
            break;
        }
        case auth_test_event::module_update:
        {
            if (m_address.empty())
            {
                ICY_ERROR(console_write("\r\nModule update error: IP address not set!"_s));
                break;
            }
            const auto ptr_name = parser.args().try_find("module"_s);
            if (!ptr_name)
            {
                ICY_ERROR(console_write("\r\nModule update error: 'module' parameter not found!"_s));
                break;
            }
            const auto ptr_pass = parser.args().try_find("password"_s);
            if (!ptr_pass)
            {
                ICY_ERROR(console_write("\r\nModule update error: 'password' parameter not found!"_s));
                break;
            }
            const auto ptr_addr = parser.args().try_find("address"_s);
            if (!ptr_addr)
            {
                ICY_ERROR(console_write("\r\nModule update error: 'address' parameter not found!"_s));
                break;
            }
            const auto ptr_timeout = parser.args().try_find("timeout"_s);
            if (!ptr_timeout)
            {
                ICY_ERROR(console_write("\r\nModule update error: 'timeout' parameter not found!"_s));
                break;
            }
            auto timeout = 0u;
            ptr_timeout->to_value(timeout);
            if (!timeout)
            {
                ICY_ERROR(console_write("\r\nModule update error: 'timeout' (ms) parameter should be > 0!"_s));
                break;
            }

            const auto modulename = hash64(*ptr_name);
            
            ICY_ERROR(console_write("\r\nEncrypting module password... "_s));
            const auto password = crypto_key(*ptr_name, *ptr_pass);
            ICY_ERROR(console_write("Success!"_s));

            auth_request auth_request;
            auth_request.type = auth_request_type::module_update;
            auth_request.time = std::chrono::system_clock::now();

            auth_request_msg_module_update msg;
            msg.module = modulename;
            msg.timeout = std::chrono::milliseconds(timeout);
            msg.password = password;
            ICY_ERROR(to_string(*ptr_addr, msg.address));
            ICY_ERROR(msg.to_json(auth_request.message));

            json json;
            ICY_ERROR(execute(auth_request, json));
            break;
        }
            
        case auth_test_event::exit:
            return {};
        }

        if (e0 == make_stdlib_error(std::errc::illegal_byte_sequence) ||
            e1 == make_stdlib_error(std::errc::illegal_byte_sequence) ||
            etype == auth_test_event::none)
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

int main()
{
    heap gheap;
    if (const auto error = gheap.initialize(heap_init::global(16_mb)))
        return error.code;

    test_core core;
    {
        string msg;
        if (const auto error = to_string("Auth test: startup at [%1] local time."_s, msg, clock_type::now()))
            return print_error(error), error.code;
        if (const auto error = console_write(msg))
            return print_error(error), error.code;
    }

    if (const auto error = core.initialize())
        return print_error(error), error.code;

    if (const auto error = core.run())
        return print_error(error), error.code;
   
    return 0;
}
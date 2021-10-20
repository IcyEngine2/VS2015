#include <icy_engine/core/icy_core.hpp>
#include <icy_engine/core/icy_thread.hpp>
#include <icy_engine/graphics/icy_window.hpp>
#include <icy_engine/graphics/icy_gpu.hpp>
#include <icy_engine/graphics/icy_display.hpp>
#include <icy_engine/utility/icy_crypto.hpp>
#include <icy_engine/utility/icy_imgui.hpp>
#include <icy_engine/network/icy_network.hpp>
#include <icy_engine/network/icy_http.hpp>
#include <icy_auth/icy_auth.hpp>
#if _DEBUG
#pragma comment(lib, "icy_engine_cored")
#pragma comment(lib, "icy_engine_graphicsd")
#pragma comment(lib, "icy_engine_networkd")
#pragma comment(lib, "icy_engine_resourced")
#else
#pragma comment(lib, "icy_engine_core")
#pragma comment(lib, "icy_engine_graphics")
#pragma comment(lib, "icy_engine_network")
#pragma comment(lib, "icy_engine_resource")
#endif

using namespace icy;

class render_thread : public icy::thread
{
public:
    error_type run() noexcept override
    {
        shared_ptr<event_queue> loop;
        ICY_ERROR(create_event_system(loop, 0
            | event_type::gui_render
            | event_type::global_timer
        ));

        render_gui_frame render;
        timer win_render_timer;
        timer gui_render_timer;
        ICY_ERROR(win_render_timer.initialize(SIZE_MAX, std::chrono::nanoseconds(1'000'000'000 / 60)));
        ICY_ERROR(gui_render_timer.initialize(SIZE_MAX, std::chrono::nanoseconds(1'000'000'000 / 60)));

        auto query = 0u;
        ICY_ERROR(imgui_display->repaint(query));

        while (*loop)
        {
            event event;
            ICY_ERROR(loop->pop(event));
            if (!event)
                break;
            
            if (event->type == event_type::global_timer)
            {
                const auto pair = event->data<timer::pair>();
                if (pair.timer == &win_render_timer)
                {
                    render_gui_frame copy_frame;
                    ICY_ERROR(copy(render, copy_frame));
                    ICY_ERROR(display_system->repaint("ImGui"_s, copy_frame));
                }
                else if (pair.timer == &gui_render_timer)
                {
                    ICY_ERROR(imgui_display->repaint(query));
                }
            }
            else if (event->type == event_type::gui_render)
            {
                auto& event_data = event->data<imgui_event>();
                if (event_data.type == imgui_event_type::display_render)
                {
                    render = std::move(event_data.render);
                }
            }
        }
        return error_type();
    }
    shared_ptr<window_system> window_system;
    shared_ptr<display_system> display_system;
    shared_ptr<imgui_system> imgui_system;
    shared_ptr<imgui_display> imgui_display;
};

error_type main_ex() noexcept
{
    shared_ptr<resource_system> rsystem;
    ICY_ERROR(create_resource_system(rsystem, string_view(), 0));

    shared_ptr<event_queue> loop;
    ICY_ERROR(create_event_system(loop, 0
        | event_type::network_disconnect
        | event_type::network_recv
        | event_type::window_resize
        | event_type::gui_update
    ));

    shared_ptr<window_system> window_system;
    ICY_ERROR(create_window_system(window_system));
    ICY_ERROR(window_system->thread().launch());
    ICY_ERROR(window_system->thread().rename("Window Thread"_s))

    shared_ptr<window> window;
    ICY_ERROR(window_system->create(window));
    ICY_ERROR(window->restyle(window_style::windowed));

    shared_ptr<imgui_system> imgui_system;
    ICY_ERROR(create_imgui_system(imgui_system));
    ICY_ERROR(imgui_system->thread().launch());
    ICY_ERROR(imgui_system->thread().rename("ImGui Thread"_s));

    shared_ptr<imgui_display> imgui_display;
    ICY_ERROR(imgui_system->create_display(imgui_display, window));


    auto imgui_window = 0u;
    enum
    {
        text_hostname,
        line_hostname,
        text_username,
        line_username,
        text_password,
        line_password,
        text_module,
        line_module,
        radio_client_create,
        radio_client_ticket,
        radio_client_connect,
        radio_module_create,
        radio_module_update,
        button_exec,
        text_log,
        _total,
    };
    uint32_t widgets[_total] = {};
    ICY_ERROR(imgui_display->widget_create(0u, imgui_widget_type::window, imgui_window));
    imgui_display->widget_create(imgui_window, imgui_widget_type::text, widgets[text_hostname]);
    imgui_display->widget_create(imgui_window, imgui_widget_type::line_input, widgets[line_hostname]);
    imgui_display->widget_create(imgui_window, imgui_widget_type::text, widgets[text_username]);
    imgui_display->widget_create(imgui_window, imgui_widget_type::line_input, widgets[line_username]);
    imgui_display->widget_create(imgui_window, imgui_widget_type::text, widgets[text_password]);
    imgui_display->widget_create(imgui_window, imgui_widget_type::line_input, widgets[line_password]);
    imgui_display->widget_create(imgui_window, imgui_widget_type::text, widgets[text_module]);
    imgui_display->widget_create(imgui_window, imgui_widget_type::line_input, widgets[line_module]);
    imgui_display->widget_create(imgui_window, imgui_widget_type::button_radio, widgets[radio_client_create]);
    imgui_display->widget_create(imgui_window, imgui_widget_type::button_radio, widgets[radio_client_ticket]);
    imgui_display->widget_create(imgui_window, imgui_widget_type::button_radio, widgets[radio_client_connect]);
    imgui_display->widget_create(imgui_window, imgui_widget_type::button_radio, widgets[radio_module_create]);
    imgui_display->widget_create(imgui_window, imgui_widget_type::button_radio, widgets[radio_module_update]);
    imgui_display->widget_create(imgui_window, imgui_widget_type::button, widgets[button_exec]);
    imgui_display->widget_create(imgui_window, imgui_widget_type::text, widgets[text_log]);

    imgui_display->widget_value(widgets[text_hostname], "Hostname"_s);
    imgui_display->widget_state(widgets[line_hostname], imgui_widget_flag::is_same_line);

    imgui_display->widget_value(widgets[text_username], "Username"_s);
    imgui_display->widget_state(widgets[line_username], imgui_widget_flag::is_same_line);

    imgui_display->widget_value(widgets[text_password], "Password"_s);
    imgui_display->widget_state(widgets[line_password], imgui_widget_flag::is_same_line);

    imgui_display->widget_value(widgets[text_module], "Module"_s);
    imgui_display->widget_state(widgets[line_module], imgui_widget_flag::is_same_line);

    imgui_display->widget_label(widgets[radio_client_create], "Create client"_s);
    imgui_display->widget_label(widgets[radio_client_ticket], "Client authorize (ticket)"_s);
    imgui_display->widget_label(widgets[radio_client_connect], "Connect to module"_s);
    imgui_display->widget_label(widgets[radio_module_create], "Create module"_s);
    imgui_display->widget_label(widgets[radio_module_update], "Update module"_s);

    imgui_display->widget_label(widgets[button_exec], "Execute"_s);
    imgui_display->widget_state(widgets[text_log], imgui_widget_flag::is_same_line);
    
    array<shared_ptr<gpu_device>> gpu;
    ICY_ERROR(create_gpu_list(gpu_flags::vulkan | gpu_flags::hardware | gpu_flags::debug, gpu));

    shared_ptr<display_system> display_system;
    ICY_ERROR(create_display_system(display_system, window, gpu[0]));
    ICY_ERROR(display_system->thread().launch());
    ICY_ERROR(display_system->thread().rename("Display Thread"_s));

    shared_ptr<render_thread> render_thread;
    ICY_ERROR(make_shared(render_thread));
    render_thread->window_system = window_system;
    render_thread->display_system = display_system;
    render_thread->imgui_system = imgui_system;
    render_thread->imgui_display = imgui_display;
    ICY_ERROR(render_thread->launch());
    ICY_ERROR(render_thread->rename("Render Thread"_s));
    ICY_SCOPE_EXIT{ render_thread->wait(); };

    ICY_ERROR(window->show(true));


    string hostname_str;
    string username_str;
    string password_str;
    string module_str;
    auth_request_type type = auth_request_type::none;
    shared_ptr<network_system_http_client> network;
    auto exec_type = type;
    auto exec_time = auth_clock::now();
    guid exec_guid;

    crypto_key key;
    crypto_msg<auth_client_ticket_server> ticket;
    
    

    string log;
    const auto print = [&log, &imgui_display, widgets](string_view msg) -> error_type
    {
        ICY_ERROR(log.append("\r\n"_s));
        ICY_ERROR(log.append(msg));
        ICY_ERROR(imgui_display->widget_value(widgets[text_log], log));
        return error_type();
    };


    while (*loop)
    {
        event event;
        ICY_ERROR(loop->pop(event));
        if (!event)
            break;

        if (event->type == event_type::window_resize)
        {
            const auto& event_data = event->data<window_message>();
            ICY_ERROR(display_system->resize(event_data.size));
        }
        else if (event->type == event_type::network_disconnect)
        {
            network = nullptr;
            exec_type = auth_request_type::none;
            ICY_ERROR(print("Disconnected"_s));
        }
        else if (event->type == event_type::network_recv)
        {
            auth_response response;
            response.type = exec_type;
            response.guid = exec_guid;

            network = nullptr;
            exec_type = auth_request_type::none;
            const auto& event_data = event->data<network_event>();
            if (event_data.http.response)
            {
                const auto& body = event_data.http.response->body;
                json json;
                string_view tmp;
                if (const auto error = to_string(body, tmp))
                {
                    ICY_ERROR(print("Auth error: invalid UTF string"_s));
                    continue;
                }
                if (const auto error = to_value(tmp, json))
                {
                    ICY_ERROR(print("Auth error: invalid json"_s));
                    continue;
                }

                if (const auto error = response.from_json(json))
                {
                    ICY_ERROR(print("Auth error: invalid format"_s));
                    continue;
                }
                if (response.time < exec_time - network_default_timeout ||
                    response.time > exec_time + network_default_timeout)
                {
                    ICY_ERROR(print("Auth error: timeout"_s));
                    continue;
                }
                if (response.error != auth_error_code::none)
                {
                    string str;
                    ICY_ERROR(to_string(make_auth_error(response.error), str));
                    string msg;
                    ICY_ERROR(to_string("Auth error: code %1 - %2"_s, msg, uint32_t(response.error), string_view(str)));
                    ICY_ERROR(print(msg));
                    continue;
                }

                switch (response.type)
                {
                case auth_request_type::client_create:
                {
                    ICY_ERROR(print("Client create: OK"_s));
                    break;
                }
                case auth_request_type::client_ticket:
                {
                    auth_response_msg_client_ticket data;
                    if (data.from_json(response.message))
                    {
                        ICY_ERROR(print("Auth error: invalid ticket JSON format"_s));
                        break;
                    }
                    auth_client_ticket_client client_ticket;
                    if (data.encrypted_client_ticket.decode(key, client_ticket))
                    {
                        ICY_ERROR(print("Auth error: invalid ticket encryption"_s));
                        break;
                    }
                    
                    string msg;
                    string time_str;
                    ICY_ERROR(to_string(client_ticket.expire, time_str));
                    ICY_ERROR(to_string("Client ticket: OK;\r\nExpire: %1"_s, msg, string_view(time_str)));
                    ICY_ERROR(print(msg));
                    ticket = data.encrypted_server_ticket;
                    break;
                }
                case auth_request_type::client_connect:
                {
                    auth_response_msg_client_connect data;
                    if (data.from_json(response.message))
                    {
                        ICY_ERROR(print("Auth error: invalid connect JSON format"_s));
                        break;
                    }
                    auth_client_connect_client client_connect;
                    if (data.encrypted_client_connect.decode(key, client_connect))
                    {
                        ICY_ERROR(print("Auth error: invalid connect encryption"_s));
                        break;
                    }

                    string_view str_addr;
                    if (to_string(const_array_view<char>(client_connect.address), str_addr))
                    {
                        ICY_ERROR(print("Auth error: invalid UTF address string"_s));
                        break;
                    }

                    string msg;
                    string time_str;
                    ICY_ERROR(to_string(client_connect.expire, time_str));
                    ICY_ERROR(to_string("Client connect: OK;\r\nExpire: %1\r\nAddress: %2"_s, msg, string_view(time_str), str_addr));
                    ICY_ERROR(print(msg));
                    break;
                }

                case auth_request_type::module_create:
                {
                    ICY_ERROR(print("Module create: OK"_s));
                    break;
                }

                case auth_request_type::module_update:
                {
                    ICY_ERROR(print("Module update: OK"_s));
                    break;
                }

                default:
                    break;
                }

                
                
            }
            else
            {
                ICY_ERROR(print("Auth error: null"_s));
            }
        }
        else if (event->type == event_type::gui_update)
        {
            auto& event_data = event->data<imgui_event>();
            if (event_data.type == imgui_event_type::widget_edit)
            {
                if (event_data.widget == widgets[line_hostname])
                {
                    ICY_ERROR(copy(event_data.value.str(), hostname_str));
                }
                else if (event_data.widget == widgets[line_username])
                {
                    ICY_ERROR(copy(event_data.value.str(), username_str));
                }
                else if (event_data.widget == widgets[line_password])
                {
                    ICY_ERROR(copy(event_data.value.str(), password_str));
                }
                else if (event_data.widget == widgets[line_module])
                {
                    ICY_ERROR(copy(event_data.value.str(), module_str));
                }
                else if (event_data.widget == widgets[radio_client_create])
                {
                    type = auth_request_type::client_create;
                }
                else if (event_data.widget == widgets[radio_client_ticket])
                {
                    type = auth_request_type::client_ticket;
                }
                else if (event_data.widget == widgets[radio_client_connect])
                {
                    type = auth_request_type::client_connect;
                }
                else if (event_data.widget == widgets[radio_module_create])
                {
                    type = auth_request_type::module_create;
                }
                else if (event_data.widget == widgets[radio_module_update])
                {
                    type = auth_request_type::module_update;
                }
            }
            else if (event_data.type == imgui_event_type::widget_click)
            {
                log.clear();
                ICY_ERROR(imgui_display->widget_value(widgets[text_log], variant()));
                if (event_data.widget == widgets[button_exec] && exec_type == auth_request_type::none)
                {
                    if (type ==auth_request_type::none)
                    {
                        ICY_ERROR(print("Invalid type: select radio button"_s));
                        continue;
                    }
                    if (hostname_str.empty())
                    {
                        ICY_ERROR(print("Invalid hostname: empty string"_s));
                        continue;
                    }
                    if (username_str.empty() && (type == auth_request_type::client_create
                        || type == auth_request_type::client_ticket
                        || type == auth_request_type::client_connect))
                    {
                        ICY_ERROR(print("Invalid username: empty string"_s));
                        continue;
                    }
                    if (password_str.empty() && (type == auth_request_type::client_create
                        || type == auth_request_type::client_ticket))
                    {
                        ICY_ERROR(print("Invalid password: empty string"_s));
                        continue;
                    }
                    if (module_str.empty() && (type == auth_request_type::client_connect 
                            || type == auth_request_type::module_create 
                            || type == auth_request_type::module_update))
                    {
                        ICY_ERROR(print("Invalid module name: empty string"_s));
                        continue;
                    }
                }

                const auto it = hostname_str.find(":"_s);
                if (it == hostname_str.end())
                {
                    ICY_ERROR(print("Invalid hostname: expected 'IP:PORT'"_s));
                    continue;
                }

                if (type == auth_request_type::client_create || type == auth_request_type::client_ticket)
                {
                    ICY_ERROR(print("Encrypting user password..."_s));
                    key = crypto_key(username_str, password_str);
                }
                const auto username = hash64(username_str);
                const auto hostaddr = string_view(hostname_str.begin(), it);
                const auto hostport = string_view(it + 1, hostname_str.end());
                const auto module = hash64(module_str);
                
                ICY_ERROR(print("Resolve hostname..."_s));
                array<network_address> addr_list;
                ICY_ERROR(network_address::query(addr_list, hostaddr, hostport, network_default_timeout));
                if (addr_list.empty())
                {
                    ICY_ERROR(print("Invalid hostname: DNS failed"_s));
                    continue;
                }
                {
                    string msg;
                    ICY_ERROR(to_string("Found list of %1 IP addresses"_s, msg, addr_list.size()));
                    ICY_ERROR(print(msg));
                }
                auth_request auth_request;
                auth_request.username = username;
                auth_request.time = exec_time = auth_clock::now();
                auth_request.type = type;
                auth_request.guid = exec_guid = guid::create();
                switch (type)
                {
                case auth_request_type::client_create:
                {
                    auth_request_msg_client_create msg;
                    msg.password = key;
                    ICY_ERROR(msg.to_json(auth_request.message));
                    break;
                }
                case auth_request_type::client_ticket:
                {
                    auth_request_msg_client_ticket msg;
                    msg.encrypted_time = crypto_msg<std::chrono::system_clock::time_point>(key, auth_request.time);
                    ICY_ERROR(msg.to_json(auth_request.message));
                    break;
                }
                case auth_request_type::client_connect:
                {
                    auth_request_msg_client_connect msg;
                    msg.module = module;
                    msg.encrypted_server_ticket = ticket;
                    ICY_ERROR(msg.to_json(auth_request.message));
                    break;
                }
                case auth_request_type::module_create:
                {
                    auth_request_msg_module_create msg;
                    msg.module = module;
                    ICY_ERROR(msg.to_json(auth_request.message));
                    break;
                }
                case auth_request_type::module_update:
                {
                    auth_request_msg_module_update msg;
                    msg.module = module;
                    msg.password = crypto_random;
                    msg.timeout = std::chrono::minutes(240);
                    ICY_ERROR(msg.address.append("127.0.0.1:8901"_s));
                    ICY_ERROR(msg.to_json(auth_request.message));
                    break;
                }
                default:
                    break;
                }

                json json;
                ICY_ERROR(auth_request.to_json(json));
                string str;
                ICY_ERROR(to_string(json, str));

                for (auto&& addr : addr_list)
                {
                    string addr_str;
                    ICY_ERROR(to_string(addr, addr_str));

                    string msg;
                    ICY_ERROR(to_string("Connect to IP '%1:%2'..."_s, msg, string_view(addr_str), hostport));
                    ICY_ERROR(print(msg));

                    http_request request;
                    request.type = http_request_type::post;
                    request.content = http_content_type::application_json;
                    request.body.assign(str.ubytes());
                    if (auto error = create_network_http_client(network, addr, request, network_default_timeout, 0x1000))
                    {
                        continue;
                    }
                    ICY_ERROR(network->thread().launch());
                    ICY_ERROR(network->thread().rename("HTTP Thread"_s));
                    exec_type = type;
                    ICY_ERROR(print("Connected, await response..."_s));
                    break;
                }
                if (exec_type == auth_request_type::none)
                {
                    ICY_ERROR(print("Auth server not found"_s));
                }
            }
        }
    }
    return error_type();
}

int main()
{
    heap gheap;
    if (gheap.initialize(heap_init::global(64_mb)))
        return ENOMEM;

    if (auto error = main_ex())
        return error.code;

    return 0;
}


/*
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
    error_type print_error(const error_type error) noexcept
    {
        string msg;
        ICY_ERROR(icy::to_string("Error: (%1) code %2 - %3"_s, msg, error.source, error.source.hash == error_source_system.hash ?
            (error.code & 0xFFFF) : error.code, error));
        ICY_ERROR(m_console->write(msg));
        return error_type();
    }
    error_type show_help() noexcept;
    error_type get_addr(const string_view str_addr, array<network_address>& address) noexcept;
    error_type execute(auth_request& auth_request, json& output) noexcept;
    error_type execute2(const const_array_view<network_address> addr, const http_request& request, 
        http_response& header, array<uint8_t>& body) noexcept;
private:
    shared_ptr<console_system> m_console;
    map<string_view, pair_type> m_commands;
    shared_ptr<network_system_http_client> m_network;
    array<network_address> m_address;
};

error_type test_core::initialize() noexcept
{
    ICY_ERROR(event_system::initialize());
    ICY_ERROR(create_console_system(m_console));

    string msg;

    string time_str;
    ICY_ERROR(to_string(clock_type::now(), time_str));
    ICY_ERROR(to_string("Auth test: startup at [%1] local time."_s, msg, string_view(time_str)));
    ICY_ERROR(m_console->write(msg));

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

    return error_type();
}
error_type test_core::show_help() noexcept
{
    ICY_ERROR(m_console->write("\r\nConsole commands: "_s));
    auto max_length = 0_z;
    for (auto&& cmd : m_commands)
        max_length = std::max(cmd.key.bytes().size(), max_length);

    for (auto&& cmd : m_commands)
    {
        string spaces;
        ICY_ERROR(spaces.resize(max_length - cmd.key.bytes().size(), ' '));

        string str;
        ICY_ERROR(str.appendf("\r\n  %1  %2: %3"_s, cmd.key, string_view(spaces), string_view(cmd.value.help)));
        ICY_ERROR(m_console->write(str));
    }
    return error_type();
}
error_type test_core::get_addr(const string_view str_addr, array<network_address>& address) noexcept
{
    address.clear();
    error_type error;
    const auto it = str_addr.find(":"_s);
    if (it != str_addr.end())
    {
        const auto str_host = string_view(str_addr.begin(), it);
        const auto str_port = string_view(it + 1, str_addr.end());
        error = network_address::query(address, str_host, str_port, network_default_timeout);
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
        ICY_ERROR(m_console->write("Success! IP List: "_s));
        auto first = true;
        for (auto&& addr : address)
        {
            string next_addr;
            ICY_ERROR(to_string(addr, next_addr));
            if (!first)
                ICY_ERROR(m_console->write(", "_s));
            ICY_ERROR(m_console->write(next_addr));
            first = false;
        }
    }

    return error_type();
}
error_type test_core::execute(auth_request& auth_request, json& output) noexcept
{
    ICY_ERROR(guid::create(auth_request.guid));
    
    http_request http_request;
    http_request.type = http_request_type::post;
    http_request.content = http_content_type::application_json;

    json json_request;
    ICY_ERROR(auth_request.to_json(json_request));
    string input;
    ICY_ERROR(to_string(json_request, input));
    ICY_ERROR(http_request.body.assign(input.ubytes()));

    http_response header;
    array<uint8_t> body;
    if (const auto error = execute2(m_address, http_request, header, body))
    {
        ICY_ERROR(print_error(error));
        return error_type();
    }
    if (body.empty())
    {
        ICY_ERROR(m_console->write("\r\nAuth request error: no reply!"_s));
        return error_type();
    }

    ICY_ERROR(m_console->write("\r\nValidating Auth JSON response... "_s));
    auth_response auth_response(auth_request);

    json json_response;
    if (const auto error = to_value(string_view(reinterpret_cast<const char*>(body.data()), body.size()), json_response))
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
            ICY_ERROR(copy(output, auth_response.message));
            ICY_ERROR(m_console->write("Success!"_s));
        }
    }
    return error_type();
}
error_type test_core::execute2(const const_array_view<network_address> address, const http_request& request, http_response& header, array<uint8_t>& body) noexcept
{
    bool exit = false;
    string str;
    ICY_ERROR(to_string(request, str));

    shared_ptr<network_system_http_client> network;
    
    for (auto&& addr : address)
    {
        string msg;
        string addr_str;
        to_string(addr, addr_str);
        ICY_ERROR(to_string("\r\nConnecting to address: %1... "_s, msg, string_view(addr_str)));
        ICY_ERROR(m_console->write(msg));

        if (const auto error = create_network_http_client(network, addr, request, network_default_timeout, 4096))
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
    if (const auto error = core.initialize())
        return error.code;

    if (const auto error = core.run())
        return error.code;
    
    return 0;
}*/
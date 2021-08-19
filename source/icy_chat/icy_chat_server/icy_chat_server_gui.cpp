#include "icy_chat_server.hpp"
#include <icy_auth/icy_auth.hpp>

using namespace icy;

enum widget
{
    root = 0u,
    popup_error,
    popup_error_message,
    popup_error_source,
    popup_error_code,
    popup_error_detail,
    menu_main,
    menu_network,
    menu_users,
    menu_rooms,
    window_network,
    window_users,
    window_rooms,
    network_text_auth_hostname,
    network_edit_auth_hostname,
    network_text_module_hostname,
    network_edit_module_hostname,
    network_exec,
    network_text_status,
    users_text_filter,
    users_edit_filter,
    users_exec_filter,
    users_list,    
    rooms_tabs,
    _count,
};
static error_type show_error(chat_server_application& app, const error_type& error, const string_view msg) noexcept;

error_type chat_server_application::run_gui() noexcept
{
    static_assert(sizeof(widgets) / sizeof(widgets[0]) == _count, "INVALID WIDGET COUNT");
    const auto add_widget = [this](const widget parent, const widget index, const imgui_widget_type type, const string_view label = string_view())
    {
        ICY_ERROR(this->imgui_display->widget_create(widgets[parent], type, widgets[index]));
        if (!label.empty())
            ICY_ERROR(this->imgui_display->widget_label(widgets[index], label));
        return error_type();
    };

    ICY_ERROR(add_widget(root, popup_error, imgui_widget_type::window, "Error"_s));
    ICY_ERROR(add_widget(popup_error, popup_error_message, imgui_widget_type::text));
    ICY_ERROR(add_widget(popup_error, popup_error_source, imgui_widget_type::text));
    ICY_ERROR(add_widget(popup_error, popup_error_code, imgui_widget_type::text));
    ICY_ERROR(add_widget(popup_error, popup_error_detail, imgui_widget_type::text));

    ICY_ERROR(add_widget(root, menu_main, imgui_widget_type::main_menu_bar));
    ICY_ERROR(add_widget(menu_main, menu_network, imgui_widget_type::menu_item, "Network"_s));
    ICY_ERROR(add_widget(menu_main, menu_users, imgui_widget_type::menu_item, "Users"_s));
    ICY_ERROR(add_widget(menu_main, menu_rooms, imgui_widget_type::menu_item, "Rooms"_s));

    ICY_ERROR(add_widget(root, window_network, imgui_widget_type::window, "Network"_s));
    ICY_ERROR(add_widget(root, window_users, imgui_widget_type::window, "Users"_s));
    ICY_ERROR(add_widget(root, window_rooms, imgui_widget_type::window, "Rooms"_s));

    ICY_ERROR(add_widget(window_network, network_text_auth_hostname, imgui_widget_type::text));
    ICY_ERROR(add_widget(window_network, network_edit_auth_hostname, imgui_widget_type::line_input));
    ICY_ERROR(add_widget(window_network, network_text_module_hostname, imgui_widget_type::text));
    ICY_ERROR(add_widget(window_network, network_edit_module_hostname, imgui_widget_type::line_input));
    ICY_ERROR(add_widget(window_network, network_exec, imgui_widget_type::button, "Launch"_s));
    ICY_ERROR(add_widget(window_network, network_text_status, imgui_widget_type::text));
    
    ICY_ERROR(add_widget(window_users, users_text_filter, imgui_widget_type::text));
    ICY_ERROR(add_widget(window_users, users_edit_filter, imgui_widget_type::line_input));
    ICY_ERROR(add_widget(window_users, users_exec_filter, imgui_widget_type::button, "Search"_s));
    ICY_ERROR(add_widget(window_rooms, rooms_tabs, imgui_widget_type::tab_bar));

    ICY_ERROR(imgui_display->widget_value(widgets[network_text_auth_hostname], "Auth Hostname"_s));
    ICY_ERROR(imgui_display->widget_value(widgets[network_text_module_hostname], "Public Hostname"_s));
    ICY_ERROR(imgui_display->widget_value(widgets[users_text_filter], "Username"_s));
    
    ICY_ERROR(imgui_display->widget_state(widgets[popup_error],
        imgui_window_state(imgui_window_flag::always_auto_resize | imgui_window_flag::no_resize, imgui_widget_flag::is_hidden)));
    ICY_ERROR(imgui_display->widget_state(widgets[window_network], imgui_window_state(imgui_window_flag::none, imgui_widget_flag::is_hidden)));
    ICY_ERROR(imgui_display->widget_state(widgets[window_users], imgui_window_state(imgui_window_flag::always_auto_resize, imgui_widget_flag::is_hidden)));
    ICY_ERROR(imgui_display->widget_state(widgets[window_rooms], imgui_window_state(imgui_window_flag::always_auto_resize, imgui_widget_flag::is_hidden)));

    ICY_ERROR(imgui_display->widget_state(widgets[network_edit_auth_hostname], imgui_widget_flag::is_same_line));
    ICY_ERROR(imgui_display->widget_state(widgets[network_edit_module_hostname], imgui_widget_flag::is_same_line));
    ICY_ERROR(imgui_display->widget_state(widgets[network_text_status], imgui_widget_flag::is_same_line));
    ICY_ERROR(imgui_display->widget_state(widgets[users_edit_filter], imgui_widget_flag::is_same_line));
    ICY_ERROR(imgui_display->widget_state(widgets[users_exec_filter], imgui_widget_flag::is_same_line));
    
    shared_ptr<event_queue> loop;
    ICY_ERROR(create_event_system(loop, event_type::gui_update | auth_event_type));

    string auth_hostname;
    string module_hostname;
    string log;
    const auto print = [this, &log](string_view msg)
    {
        if (!log.empty())
        {
            ICY_ERROR(log.append("\r\n"_s));
        }
        string time_str;
        ICY_ERROR(to_string(clock_type::now(), time_str));
        ICY_ERROR(log.appendf("[%1] %2"_s, string_view(time_str), msg));
        ICY_ERROR(this->imgui_display->widget_value(widgets[network_text_status], log));
        return error_type();
    };

    ICY_ERROR(module_hostname.append(icy::chat_public_hostname));
    ICY_ERROR(imgui_display->widget_value(widgets[network_edit_module_hostname], module_hostname));


    shared_ptr<auth_system> auth;
    while (*loop)
    {
        event event;
        ICY_ERROR(loop->pop(event));
        if (!event)
            break;

        if (event->type == auth_event_type)
        {
            const auto& event_data = event->data<auth_event>();
            auth = nullptr;
            if (event_data.error)
            {
                string error_str;
                to_string(event_data.error, error_str);
                string msg;
                ICY_ERROR(to_string("Auth error: %1"_s, msg, string_view(error_str)));
                ICY_ERROR(print(msg));
            }
            else
            {
                const auto port_str = string_view(module_hostname.find(":"_s) + 1, module_hostname.end());

                network_server_config config;
                config.buffer = 0x1000;
                config.capacity = 8;
                config.timeout = max_timeout;
                to_value(port_str, config.port);
                if (config.port < 0x400)
                {
                    ICY_ERROR(print("Invalid module port"_s));
                    continue;
                }
                ICY_ERROR(print("IcyChat module updated"_s));

                if (http_thread) http_thread->wait();
                ICY_ERROR(make_shared(http_thread));

                if (const auto error = create_network_http_server(http_server, config))
                {
                    ICY_ERROR(show_error(*this, error, "Error launching HTTP server"_s));
                    continue;
                }
                http_thread->system = http_server.get();
                ICY_ERROR(http_thread->launch());
                ICY_ERROR(http_thread->rename("HTTP Thread"_s));
                ICY_ERROR(print("HTTP server launched"_s));
            }
        }
        else if (event->type == event_type::gui_update)
        {
            auto& event_data = event->data<imgui_event>();
            if (event_data.type == imgui_event_type::widget_click)
            {
                if (false)
                {

                }
                else if (event_data.widget == widgets[menu_users])
                {
                    ICY_ERROR(imgui_display->widget_state(widgets[window_users], imgui_widget_flag::none));
                }
                else if (event_data.widget == widgets[menu_rooms])
                {
                    ICY_ERROR(imgui_display->widget_state(widgets[window_rooms], imgui_widget_flag::none));
                }
                else if (event_data.widget == widgets[menu_network])
                {
                    ICY_ERROR(imgui_display->widget_state(widgets[window_network], imgui_widget_flag::none));
                }
                else if (event_data.widget == widgets[users_exec_filter])
                {

                }
                else if (event_data.widget == widgets[network_exec])
                {
                    log.clear();
                    ICY_ERROR(imgui_display->widget_value(widgets[network_text_status], variant()));
                    if (module_hostname.empty())
                    {
                        ICY_ERROR(print("Invalid public hostname: empty string"_s));
                        continue;
                    }
                    if (module_hostname.find(":"_s) == module_hostname.end())
                    {
                        ICY_ERROR(print("Invalid public hostname: expected 'IP:PORT'"_s));
                        continue;
                    }

                    ICY_ERROR(create_auth_system(auth));
                    ICY_ERROR(print("Connecting to auth server..."_s));
                    if (const auto error = auth->connect(auth_hostname, network_default_timeout))
                    {
                        ICY_ERROR(print("Invalid auth hostname"_s));
                        auth = nullptr;
                        continue;
                    }

                    ICY_ERROR(auth->thread().launch());
                    ICY_ERROR(auth->thread().rename("Auth Thread"_s));
                    ICY_ERROR(auth->module_update(guid::create(), hash64(chat_auth_module),
                        module_hostname, std::chrono::minutes(240), database.password));
                }
            }
            else if (event_data.type == imgui_event_type::widget_edit)
            {
                if (event_data.widget == widgets[network_edit_auth_hostname])
                {
                    ICY_ERROR(copy(event_data.value.str(), auth_hostname));
                }
                else if (event_data.widget == widgets[network_edit_module_hostname])
                {
                    ICY_ERROR(copy(event_data.value.str(), module_hostname));
                }
            }
        }
    }

    return error_type();
}
error_type show_error(chat_server_application& app, const error_type& error, const string_view msg) noexcept
{
    string str_source;
    string msg_source;
    ICY_ERROR(to_string(error.source, str_source));
    ICY_ERROR(to_string("Source: \"%1\""_s, msg_source, string_view(str_source)));

    string str_code;
    string msg_code;
    ICY_ERROR(to_string_unsigned(error.code, 16, str_code));
    ICY_ERROR(to_string("Code: %1"_s, msg_code, string_view(str_code)));

    string str_detail;
    string msg_detail;
    to_string(error, str_detail);
    if (!str_detail.empty())
    {
        ICY_ERROR(to_string("Text: %1"_s, msg_detail, string_view(str_detail)));
    }

    ICY_ERROR(app.imgui_display->widget_value(app.widgets[popup_error_message], msg));
    ICY_ERROR(app.imgui_display->widget_value(app.widgets[popup_error_source], msg_source));
    ICY_ERROR(app.imgui_display->widget_value(app.widgets[popup_error_code], msg_code));
    ICY_ERROR(app.imgui_display->widget_value(app.widgets[popup_error_detail], msg_detail));
    ICY_ERROR(app.imgui_display->widget_state(app.widgets[popup_error], imgui_widget_flag::none));
    return error_type();
}
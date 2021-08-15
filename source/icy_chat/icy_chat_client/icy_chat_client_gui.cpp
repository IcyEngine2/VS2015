#include "icy_chat_client.hpp"
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
    menu_channels,
    window_network,
    window_users,
    window_channels,
    network_text_hostname,
    network_edit_hostname,
    network_text_username,
    network_edit_username,
    network_text_password,
    network_edit_password,
    network_exec,
    network_text_status,
    users_list,
    channels_tabs,
    _count,
};
static error_type show_error(chat_client_application& app, const error_type& error, const string_view msg) noexcept;

error_type chat_client_application::run_gui() noexcept
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
    ICY_ERROR(add_widget(menu_main, menu_channels, imgui_widget_type::menu_item, "Channels"_s));

    ICY_ERROR(add_widget(root, window_network, imgui_widget_type::window, "Network"_s));
    ICY_ERROR(add_widget(root, window_users, imgui_widget_type::window, "Users"_s));
    ICY_ERROR(add_widget(root, window_channels, imgui_widget_type::window, "Channels"_s));

    ICY_ERROR(add_widget(window_network, network_text_hostname, imgui_widget_type::text));
    ICY_ERROR(add_widget(window_network, network_edit_hostname, imgui_widget_type::line_input));
    ICY_ERROR(add_widget(window_network, network_text_username, imgui_widget_type::text));
    ICY_ERROR(add_widget(window_network, network_edit_username, imgui_widget_type::line_input));
    ICY_ERROR(add_widget(window_network, network_text_password, imgui_widget_type::text));
    ICY_ERROR(add_widget(window_network, network_edit_password, imgui_widget_type::line_input));
    ICY_ERROR(add_widget(window_network, network_exec, imgui_widget_type::button, "Connect"_s));
    ICY_ERROR(add_widget(window_network, network_text_status, imgui_widget_type::text));

    ICY_ERROR(add_widget(window_channels, channels_tabs, imgui_widget_type::tab_bar));

    ICY_ERROR(imgui_display->widget_value(widgets[network_text_hostname], "Hostname"_s));
    ICY_ERROR(imgui_display->widget_value(widgets[network_text_username], "Username"_s));
    ICY_ERROR(imgui_display->widget_value(widgets[network_text_password], "Password"_s));
    
    ICY_ERROR(imgui_display->widget_state(widgets[popup_error],
        imgui_window_state(imgui_window_flag::always_auto_resize | imgui_window_flag::no_resize, imgui_widget_flag::is_hidden)));
    ICY_ERROR(imgui_display->widget_state(widgets[window_network], imgui_window_state(imgui_window_flag::none, imgui_widget_flag::is_hidden)));
    ICY_ERROR(imgui_display->widget_state(widgets[window_users], imgui_window_state(imgui_window_flag::always_auto_resize, imgui_widget_flag::is_hidden)));
    ICY_ERROR(imgui_display->widget_state(widgets[window_channels], imgui_window_state(imgui_window_flag::always_auto_resize, imgui_widget_flag::is_hidden)));

    ICY_ERROR(imgui_display->widget_state(widgets[network_edit_hostname], imgui_widget_flag::is_same_line));
    ICY_ERROR(imgui_display->widget_state(widgets[network_edit_username], imgui_widget_flag::is_same_line));
    ICY_ERROR(imgui_display->widget_state(widgets[network_edit_password], imgui_widget_flag::is_same_line));
    ICY_ERROR(imgui_display->widget_state(widgets[network_text_status], imgui_widget_flag::is_same_line));

    shared_ptr<event_queue> loop;
    ICY_ERROR(create_event_system(loop, event_type::gui_update | chat_event_type));

    string hostname;
    string username_str;
    string password_str;
    uint64_t username = 0;
    crypto_key password;
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
        
    shared_ptr<chat_system> chat;
    while (*loop)
    {
        event event;
        ICY_ERROR(loop->pop(event));
        if (!event)
            break;

        if (event->type == chat_event_type)
        {
            const auto& event_data = event->data<chat_event>();
            
            if (event_data.error)
            {
                chat = nullptr;
                ICY_ERROR(show_error(*this, event_data.error, "Chat error"_s));
            }
            else
            {
                
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
                else if (event_data.widget == widgets[menu_channels])
                {
                    ICY_ERROR(imgui_display->widget_state(widgets[window_channels], imgui_widget_flag::none));
                }
                else if (event_data.widget == widgets[menu_network])
                {
                    ICY_ERROR(imgui_display->widget_state(widgets[window_network], imgui_widget_flag::none));
                }
                else if (event_data.widget == widgets[network_exec])
                {
                    log.clear();
                    ICY_ERROR(imgui_display->widget_value(widgets[network_text_status], variant()));
                    username = hash64(username_str);
                    password = crypto_key(username_str, password_str);
                    ICY_ERROR(print("Connecting to chat server..."_s));
                    if (const auto error = create_chat_system(chat, hostname, username, password))
                    {
                        ICY_ERROR(print("Invalid auth hostname"_s));
                        continue;
                    }
                    ICY_ERROR(chat->thread().launch());
                    ICY_ERROR(chat->thread().rename("Chat Thread"_s));
                    ICY_ERROR(chat->connect());
                }
            }
            else if (event_data.type == imgui_event_type::widget_edit)
            {
                if (event_data.widget == widgets[network_edit_hostname])
                {
                    ICY_ERROR(copy(event_data.value.str(), hostname));
                }
                else if (event_data.widget == widgets[network_edit_username])
                {
                    ICY_ERROR(copy(event_data.value.str(), username_str));
                }
                else if (event_data.widget == widgets[network_edit_password])
                {
                    ICY_ERROR(copy(event_data.value.str(), password_str));
                }
            }
        }
    }

    return error_type();
}
error_type show_error(chat_client_application& app, const error_type& error, const string_view msg) noexcept
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
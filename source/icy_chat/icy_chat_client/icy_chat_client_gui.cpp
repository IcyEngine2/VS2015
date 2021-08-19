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
    menu_rooms,
    menu_channels,
    window_network,
    window_users,
    window_rooms,
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
    rooms_list,
    channels_tabs,
    _count,
};
struct channel_type
{
    icy::guid room;
    icy::guid user;
    uint32_t widget_main = 0u;
    uint32_t widget_edit = 0u;
    uint32_t widget_text = 0u;
    string text;
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
    ICY_ERROR(add_widget(menu_main, menu_users, imgui_widget_type::menu_item, "Rooms"_s));
    ICY_ERROR(add_widget(menu_main, menu_channels, imgui_widget_type::menu_item, "Channels"_s));

    ICY_ERROR(add_widget(root, window_network, imgui_widget_type::window, "Network"_s));
    ICY_ERROR(add_widget(root, window_users, imgui_widget_type::window, "Users"_s));
    ICY_ERROR(add_widget(root, window_rooms, imgui_widget_type::window, "Rooms"_s));
    ICY_ERROR(add_widget(root, window_channels, imgui_widget_type::window, "Channels"_s));

    ICY_ERROR(add_widget(window_network, network_text_hostname, imgui_widget_type::text));
    ICY_ERROR(add_widget(window_network, network_edit_hostname, imgui_widget_type::line_input));
    ICY_ERROR(add_widget(window_network, network_text_username, imgui_widget_type::text));
    ICY_ERROR(add_widget(window_network, network_edit_username, imgui_widget_type::line_input));
    ICY_ERROR(add_widget(window_network, network_text_password, imgui_widget_type::text));
    ICY_ERROR(add_widget(window_network, network_edit_password, imgui_widget_type::line_input));
    ICY_ERROR(add_widget(window_network, network_exec, imgui_widget_type::button, "Connect"_s));
    ICY_ERROR(add_widget(window_network, network_text_status, imgui_widget_type::text));
    ICY_ERROR(add_widget(window_users, users_list, imgui_widget_type::list_view));
    ICY_ERROR(add_widget(window_rooms, rooms_list, imgui_widget_type::list_view));

    ICY_ERROR(add_widget(window_channels, channels_tabs, imgui_widget_type::tab_bar));

    ICY_ERROR(imgui_display->widget_value(widgets[network_text_hostname], "Hostname"_s));
    ICY_ERROR(imgui_display->widget_value(widgets[network_text_username], "Username"_s));
    ICY_ERROR(imgui_display->widget_value(widgets[network_text_password], "Password"_s));
    
    ICY_ERROR(imgui_display->widget_state(widgets[popup_error],
        imgui_window_state(imgui_window_flag::always_auto_resize | imgui_window_flag::no_resize, imgui_widget_flag::is_hidden)));
    ICY_ERROR(imgui_display->widget_state(widgets[window_network], imgui_widget_flag::is_hidden));
    ICY_ERROR(imgui_display->widget_state(widgets[window_users], imgui_widget_flag::is_hidden));
    ICY_ERROR(imgui_display->widget_state(widgets[window_rooms], imgui_widget_flag::is_hidden));
    ICY_ERROR(imgui_display->widget_state(widgets[window_channels], imgui_widget_flag::is_hidden));

    ICY_ERROR(imgui_display->widget_state(widgets[network_edit_hostname], imgui_widget_flag::is_same_line));
    ICY_ERROR(imgui_display->widget_state(widgets[network_edit_username], imgui_widget_flag::is_same_line));
    ICY_ERROR(imgui_display->widget_state(widgets[network_edit_password], imgui_widget_flag::is_same_line));
    ICY_ERROR(imgui_display->widget_state(widgets[network_text_status], imgui_widget_flag::is_same_line));


    shared_ptr<event_queue> loop;
    ICY_ERROR(create_event_system(loop, event_type::gui_update | chat_event_type | auth_event_type | event_type::global_timer));

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
    shared_ptr<auth_system> auth;
    map<guid, channel_type> channels;

    for (auto&& user : users)
    {
        auto user_widget = 0u;
        ICY_ERROR(imgui_display->widget_create(widgets[users_list], imgui_widget_type::view_item, user_widget));
        ICY_ERROR(imgui_display->widget_udata(user_widget, user));

        string guid_str;
        ICY_ERROR(to_string(user, guid_str));
        ICY_ERROR(imgui_display->widget_label(user_widget, guid_str));
    }
    for (auto&& room : rooms)
    {
        auto room_widget = 0u;
        ICY_ERROR(imgui_display->widget_create(widgets[rooms_list], imgui_widget_type::view_item, room_widget));
        ICY_ERROR(imgui_display->widget_udata(room_widget, room));

        string guid_str;
        ICY_ERROR(to_string(room, guid_str));
        ICY_ERROR(imgui_display->widget_label(room_widget, guid_str));
    }

    timer timer;
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
                timer.cancel();
                string msg;
                to_string("Chat error: %1"_s, msg, event_data.error);
                ICY_ERROR(print(msg));
            }
            else
            {
                if (!event_data.text.empty())
                {
                    ICY_ERROR(chat->update());
                }
                else
                {
                    continue;
                }
                auto key = event_data.room ? event_data.room : event_data.user;
                auto it = channels.find(key);
                if (it == channels.end())
                {
                    ICY_ERROR(channels.insert(key, channel_type(), &it));
                    ICY_ERROR(imgui_display->widget_create(widgets[channels_tabs], imgui_widget_type::tab_item, it->value.widget_main));
                    ICY_ERROR(imgui_display->widget_create(it->value.widget_main, imgui_widget_type::text_input, it->value.widget_edit));
                    ICY_ERROR(imgui_display->widget_create(it->value.widget_main, imgui_widget_type::text, it->value.widget_text));
                    ICY_ERROR(imgui_display->widget_state(it->value.widget_text, imgui_widget_flag::is_same_line));
                    it->value.room = event_data.room;
                    it->value.user = event_data.user;
                }

                if (!it->value.text.empty())
                {
                    ICY_ERROR(it->value.text.append("\r\n"_s));
                }
                string time_str;
                ICY_ERROR(to_string(event_data.time, time_str));
                ICY_ERROR(it->value.text.appendf("[%1]"_s, string_view(time_str)));
                if (event_data.room)
                {
                    string room_str;
                    ICY_ERROR(to_string(event_data.room, room_str));
                    ICY_ERROR(it->value.text.appendf("[%1]"_s, string_view(room_str)));
                }
                if (event_data.user)
                {
                    string user_str;
                    ICY_ERROR(to_string(event_data.user, user_str));
                    ICY_ERROR(it->value.text.appendf("[%1]"_s, string_view(user_str)));
                }
                ICY_ERROR(it->value.text.append(event_data.text));
                ICY_ERROR(imgui_display->widget_value(it->value.widget_text, it->value.text));
            }
        }
        else if (event->type == event_type::global_timer)
        {
            const auto& event_data = event->data<timer::pair>();
            if (event_data.timer == &timer && chat)
            {
                ICY_ERROR(chat->update());
            }
        }
        else if (event->type == auth_event_type)
        {
            const auto& event_data = event->data<auth_event>();
            if (event_data.error)
            {
                string msg;
                to_string("Auth error: %1"_s, msg, event_data.error);
                ICY_ERROR(print(msg));
            }
            else if (event_data.type == auth_request_type::client_ticket)
            {
                ICY_ERROR(print("Auth ticket acquired"_s));
                ICY_ERROR(print("Query Chat module hostname..."_s));
                ICY_ERROR(auth->client_connect(guid::create(), username, password, hash64(chat_auth_module), event_data.encrypted_server_ticket));
            }
            else if (event_data.type == auth_request_type::client_connect)
            {
                auth = nullptr;
                ICY_ERROR(print("Chat module hostname acquired"_s));
                ICY_ERROR(create_chat_system(chat));
                ICY_ERROR(print("Connecting to chat server..."_s));
                if (const auto error = chat->connect(event_data.client_connect, event_data.encrypted_module_connect))
                {
                    string msg;
                    to_string("Chat error: %1"_s, msg, event_data.error);
                    ICY_ERROR(print(msg));
                    chat = nullptr;
                    continue;
                }
                ICY_ERROR(chat->thread().launch());
                ICY_ERROR(chat->thread().rename("Chat Thread"_s));

                string time_str;
                ICY_ERROR(to_string(event_data.client_connect.expire, time_str));
                string msg;
                ICY_ERROR(to_string("Connected to chat server until %1"_s, msg, string_view(time_str)));
                ICY_ERROR(print(msg));
                ICY_ERROR(chat->update());
                ICY_ERROR(timer.initialize(SIZE_MAX, std::chrono::seconds(1)));

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

                    ICY_ERROR(create_auth_system(auth));
                    ICY_ERROR(print("Connecting to auth server..."_s));
                    if (const auto error = auth->connect(hostname, network_default_timeout))
                    {
                        ICY_ERROR(print("Invalid auth hostname"_s));
                        auth = nullptr;
                        continue;
                    }
                    ICY_ERROR(auth->thread().launch());
                    ICY_ERROR(auth->thread().rename("Auth Thread"_s));
                    ICY_ERROR(auth->client_ticket(guid::create(), username, password));
                }
                else
                {
                    guid guid;
                    if (event_data.udata.get(guid))
                    {
                        auto it = channels.find(guid);
                        if (it == channels.end())
                        {
                            ICY_ERROR(channels.insert(guid, channel_type(), &it));
                            ICY_ERROR(imgui_display->widget_create(widgets[channels_tabs], imgui_widget_type::tab_item, it->value.widget_main));
                            ICY_ERROR(imgui_display->widget_create(it->value.widget_main, imgui_widget_type::text_input, it->value.widget_edit));
                            ICY_ERROR(imgui_display->widget_create(it->value.widget_main, imgui_widget_type::text, it->value.widget_text));
                            ICY_ERROR(imgui_display->widget_state(it->value.widget_text, imgui_widget_flag::is_same_line));
                            if (rooms.try_find(guid))
                                it->value.room = guid;
                            else
                                it->value.user = guid;
                        }
                    }
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
                else if (chat)
                {
                    for (auto it = channels.begin(); it != channels.end(); ++it)
                    {
                        if (it->value.widget_edit != event_data.widget)
                            continue;

                        if (!it->value.text.empty())
                        {
                            ICY_ERROR(it->value.text.append("\r\n"_s));
                        }
                        string time_str;
                        ICY_ERROR(to_string(chat_clock::now(), time_str));
                        ICY_ERROR(it->value.text.appendf("[%1][User]: "_s, string_view(time_str)));
                        ICY_ERROR(it->value.text.append(event_data.value.str()));
                        ICY_ERROR(imgui_display->widget_value(it->value.widget_text, it->value.text));
                        ICY_ERROR(imgui_display->widget_value(it->value.widget_edit, variant()));

                        if (it->value.room)
                        {
                            ICY_ERROR(chat->send_to_room(event_data.value.str(), it->value.room));
                        }
                        else if (it->value.user)
                        {
                            ICY_ERROR(chat->send_to_user(event_data.value.str(), it->value.user));
                        }
                    }

                }
            }
            else if (event_data.type == imgui_event_type::widget_close)
            {
                for (auto it = channels.begin(); it != channels.end(); ++it)
                {
                    if (it->value.widget_main == event_data.widget)
                    {
                        ICY_ERROR(imgui_display->widget_delete(it->value.widget_main));
                        channels.erase(it);
                        break;
                    }
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
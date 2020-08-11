#include <icy_engine/core/icy_core.hpp>
#include <icy_engine/core/icy_json.hpp>
#include <icy_engine/core/icy_file.hpp>
#include <icy_engine/core/icy_thread.hpp>
#include <icy_engine/core/icy_set.hpp>
#include <icy_engine/graphics/icy_remote_window.hpp>
#include <icy_engine/network/icy_network.hpp>
#include <icy_engine/network/icy_http.hpp>
#include <icy_qtgui/icy_xqtgui.hpp>
#include "../mbox.h"
#include "../mbox_script.hpp"
using namespace icy;

#if _DEBUG
#pragma comment(lib, "icy_engine_cored")
#pragma comment(lib, "icy_engine_graphicsd")
#pragma comment(lib, "icy_engine_networkd")
#if ICY_QTGUI_STATIC
#pragma comment(lib, "icy_qtguid")
#endif
#else
#pragma comment(lib, "icy_engine_core")
#pragma comment(lib, "icy_engine_graphics")
#pragma comment(lib, "icy_engine_network")
#if ICY_QTGUI_STATIC
#pragma comment(lib, "icy_qtgui")
#endif
#endif

std::atomic<icy::gui_queue*> icy::global_gui;

class network_thread : public thread
{
public:
    shared_ptr<network_system_udp_server> system;
    error_type run() noexcept
    {
        ICY_SCOPE_EXIT{ event::post(nullptr, event_type::global_quit); };
        return system->exec();
    }
};

struct config
{
    mbox::library script;
    guid group;
    network_address multicast;  //  "230.4.4.2:428444"_s
};
class application : public thread
{
public:
    config config;
    shared_ptr<gui_queue> gui;
    shared_ptr<network_system_udp_server> network;
private:
    struct mbox_send
    {
        array<input_message> input;
    };
    struct mbox_button_event
    {
        bool press = false;
        mbox::button_type button;
        guid command;
    };
    struct mbox_timer_event
    {
        guid timer;
        guid command;
    };
    struct mbox_character
    {
        network_address address;
        set<guid> groups;
        array<mbox_button_event> bevents;
        array<mbox_timer_event> tevents;
        map<guid, guid> virt;
    };
    enum
    {
        column_address,
        column_character,
        column_count,
    };
private:
    error_type run() noexcept;
    error_type reset(xgui_widget& table) noexcept;
    error_type process(map<guid, mbox_character>::pair_type character, const const_array_view<mbox::action> actions, map<guid, mbox_send>& send) noexcept;
    error_type process(map<guid, mbox_character>::pair_type character, const const_array_view<input_message> data, map<guid, mbox_send>& send) noexcept;
private:
    map<guid, mbox_character> m_data;
    gui_node m_data_model;
};

error_type application::run() noexcept
{
    ICY_SCOPE_EXIT{ event::post(nullptr, event_type::global_quit); };

    global_gui.store(gui.get());

    xgui_widget window;
    ICY_ERROR(window.initialize(gui_widget_type::window, gui_widget(), gui_widget_flag::layout_grid));

    xgui_widget btn_ping;
    xgui_widget btn_reset;

    ICY_ERROR(btn_ping.initialize(gui_widget_type::text_button, window));
    ICY_ERROR(btn_reset.initialize(gui_widget_type::text_button, window));
    ICY_ERROR(btn_ping.insert(gui_insert(0, 0, 1, 1)));
    ICY_ERROR(btn_reset.insert(gui_insert(1, 0, 1, 1)));
    ICY_ERROR(btn_ping.text("Ping"_s));
    ICY_ERROR(btn_reset.text("Reset"_s));

    ICY_ERROR(gui->create(m_data_model));

    xgui_widget table;
    ICY_ERROR(table.initialize(gui_widget_type::grid_view, window));
    ICY_ERROR(table.insert(gui_insert(0, 1, 2, 1)));

    shared_ptr<event_queue> queue;
    ICY_ERROR(create_event_system(queue, event_type::gui_any | event_type::network_any));

    ICY_ERROR(window.show(true));
    
    ICY_ERROR(reset(table));

    while (true)
    {
        event event;
        ICY_ERROR(queue->pop(event));
        if (event->type == event_type::global_quit)
            return error_type();

        if (event->type == event_type::network_recv)
        {
            const auto& event_data = event->data<network_event>();
            
            json json_input;
            if (to_value(string_view(reinterpret_cast<const char*>(event_data.bytes.data()), event_data.bytes.size()), json_input))
                continue;

            const auto chr = json_input.find(mbox::json_key_character);
            if (!chr)
                continue;

            if (chr->type() == json_type::array)
            {
                string str_address;
                ICY_ERROR(to_string(event_data.address, str_address));

                auto offset = 0u;
                for (auto&& data : m_data)
                {
                    if (data.value.address == event_data.address)
                    {
                        data.value.address = network_address();
                        const auto node = gui->node(m_data_model, offset, column_address);
                        ICY_ERROR(gui->text(node, ""_s));
                    }
                    ++offset;
                }

                for (auto k = 0u; k < chr->size(); ++k)
                {
                    guid index;
                    if (to_value(chr->at(k)->get(), index))
                        continue;

                    const auto find = m_data.find(index);
                    if (find == m_data.end())
                        continue;

                    ICY_ERROR(copy(event_data.address, find->value.address));
                    const auto node = gui->node(m_data_model, std::distance(m_data.begin(), find), column_address);
                    ICY_ERROR(gui->text(node, str_address));
                }
            }
            else if (chr->type() == json_type::string)
            {
                guid index;
                if (to_value(chr->get(), index))
                    continue;

                auto type = 0u;
                auto key = 0u;
                auto mod = 0u;
                if (false
                    || json_input.get(mbox::json_key_type, type)
                    || json_input.get(mbox::json_key_key, key)
                    || json_input.get(mbox::json_key_key, mod))
                    continue;

                auto find = m_data.find(index);
                if (find == m_data.end())
                    continue;

                auto input = input_message(input_type(type), icy::key(key), key_mod(mod));

                map<guid, mbox_send> send;
                for (auto&& pair : m_data)
                {
                    if (pair.value.address)
                        ICY_ERROR(send.find_or_insert(pair.key, mbox_send()));
                }
                ICY_ERROR(process(*find, const_array_view<input_message>{ input }, send));

                for (auto&& pair : send)
                {
                    const auto target = m_data.find(pair.key);
                    if (pair.value.input.empty() || target == m_data.end())
                        continue;

                    string str_guid;
                    ICY_ERROR(to_string(pair.key, str_guid));

                    json json_object = json_type::object;
                    json json_input_array = json_type::array;
                    for (auto&& input : pair.value.input)
                    {
                        json json_input = json_type::object;
                        ICY_ERROR(json_input.insert(mbox::json_key_type, uint32_t(input.type)));
                        ICY_ERROR(json_input.insert(mbox::json_key_key, uint32_t(input.key)));
                        ICY_ERROR(json_input.insert(mbox::json_key_point_x, input.point_y));
                        ICY_ERROR(json_input.insert(mbox::json_key_point_y, input.point_x));
                        ICY_ERROR(json_input.insert(mbox::json_key_mods, input.mods));
                        ICY_ERROR(json_input_array.push_back(std::move(json_input)));
                    }
                    ICY_ERROR(json_object.insert(mbox::json_key_character, str_guid));
                    ICY_ERROR(json_object.insert(mbox::json_key_input, std::move(json_input_array)));
                    string str;
                    ICY_ERROR(to_string(json_object, str));
                    ICY_ERROR(network->send(target->value.address, str.ubytes()));
                }
            }
            continue;
        }
        else if (!(event->type & event_type::gui_any))
            continue;

        const auto& event_data = event->data<gui_event>();
        if (event_data.widget == window)
            return error_type();

        if (event->type == event_type::gui_update)
        {
            if (event_data.widget == btn_reset)
            {
                ICY_ERROR(reset(table));
            }
            if (event_data.widget == btn_ping)
            {
                json json_object = json_type::object;
                ICY_ERROR(json_object.insert(mbox::json_key_version, string_view(mbox::config_version)));

                json json_character_array = json_type::array;
                for (auto&& pair : m_data)
                {
                    json json_character = json_type::object;
                    string str_guid;
                    string str_name;
                    ICY_ERROR(to_string(pair.key, str_guid));
                    ICY_ERROR(json_character.insert(mbox::json_key_index, str_guid));
                    ICY_ERROR(config.script.path(pair.key, str_name));
                    ICY_ERROR(json_character.insert(mbox::json_key_name, str_name));
                    ICY_ERROR(json_character_array.push_back(std::move(json_character)));
                }
                ICY_ERROR(json_object.insert(mbox::json_key_character, std::move(json_character_array)));

                string str;
                ICY_ERROR(to_string(json_object, str));
                ICY_ERROR(network->send(config.multicast, str.ubytes()));
            }
        }

    }
    return error_type();
}
error_type application::reset(xgui_widget& table) noexcept
{
    m_data.clear();

    ICY_ERROR(gui->clear(m_data_model));
    ICY_ERROR(gui->insert_cols(m_data_model, 0, column_count - 1));
    ICY_ERROR(gui->hheader(m_data_model, column_address, "Network"_s));
    ICY_ERROR(gui->hheader(m_data_model, column_character, "Character"_s));
    ICY_ERROR(table.bind(m_data_model));

    array<mbox::base> vec;
    ICY_ERROR(config.script.enumerate(mbox::type::character, vec));
    if (config.group)
    {
        auto found = false;
        array<mbox::base> filter;
        for (auto&& base : vec)
        {
            auto parent = base.parent;
            while (parent)
            {
                if (parent == config.group)
                    break;
            }
            if (parent)
                continue;

            filter.push_back(std::move(base));
        }
        vec = std::move(filter);
    }
    ICY_ERROR(gui->insert_rows(m_data_model, 0, vec.size()));
    
    for (auto k = 0u; k < vec.size(); ++k)
    {
        mbox_character new_tab;
        map<guid, mbox_send> send;
        auto it = m_data.end();
        ICY_ERROR(m_data.insert(std::move(vec[k].index), std::move(new_tab), &it));
        ICY_ERROR(process(*it, vec[k].actions, send));

        auto node = gui->node(m_data_model, k, column_character);
        if (!node)
            return make_stdlib_error(std::errc::not_enough_memory);

        string character_name;
        ICY_ERROR(config.script.path(vec[k].index, character_name));
        ICY_ERROR(gui->text(node, character_name));
    }
    return error_type();
}
error_type application::process(map<guid, mbox_character>::pair_type character, const const_array_view<mbox::action> actions, map<guid, mbox_send>& send) noexcept
{
    auto& m_lib = config.script;
    const auto find_send = send.find(character->key);
    for (auto&& action : actions)
    {
        switch (action.type())
        {
        case mbox::action_type::group_join:
        case mbox::action_type::group_leave:
        {
            const auto group = m_lib.find(action.group());
            if (group && group->type == mbox::type::group)
            {
                const auto it = character->value.groups.find(action.group());
                if (action.type() == mbox::action_type::group_join)
                {
                    if (it == character->value.groups.end())
                        ICY_ERROR(character->value.groups.insert(action.group()));
                }
                else if (it != character->value.groups.end())
                {
                    character->value.groups.erase(it);
                }
            }
            break;
        }
        case mbox::action_type::focus:
        {
            const auto find_focus = send.find(action.focus());
            if (find_focus != send.end())
                ICY_ERROR(find_focus->value.input.push_back(input_message(true)));
            break;
        }
        case mbox::action_type::button_click:
        case mbox::action_type::button_press:
        case mbox::action_type::button_release:
        {
            if (find_send == send.end())
                break;

            mbox::button_type button = action.button();
            if (button.key == key::none)
                break;

            array<key> mods;
            if (button.mod & key_mod::lctrl) ICY_ERROR(mods.push_back(key::left_ctrl));
            if (button.mod & key_mod::rctrl) ICY_ERROR(mods.push_back(key::right_ctrl));
            if (button.mod & key_mod::lalt) ICY_ERROR(mods.push_back(key::left_alt));
            if (button.mod & key_mod::ralt) ICY_ERROR(mods.push_back(key::right_alt));
            if (button.mod & key_mod::lshift) ICY_ERROR(mods.push_back(key::left_shift));
            if (button.mod & key_mod::rshift) ICY_ERROR(mods.push_back(key::right_shift));

            for (auto&& mod : mods)
                ICY_ERROR(find_send->value.input.push_back(input_message(input_type::key_press, mod, key_mod::none)));

            if (button.key >= key::mouse_left && button.key <= key::mouse_x2)
            {
                /* if (action.type() == mbox::action_type::button_press ||
                     action.type() == mbox::action_type::on_button_press ||
                     action.type() == mbox::action_type::button_click)
                 {
                     input_message msg(input_type::mouse_press, button.key, button.mod);
                     msg.point_x = -1;
                     msg.point_y = -1;
                     ICY_ERROR(find_send->value.input.push_back(msg));
                 }
                 if (action.type() == mbox::action_type::button_release ||
                     action.type() == mbox::action_type::on_button_release ||
                     action.type() == mbox::action_type::button_click)
                 {
                     mouse_message msg;
                     msg.event = mouse_event::btn_release;
                     msg.button = mouse;
                     msg.mod = button.mod;
                     msg.point.x = -1;
                     msg.point.y = -1;
                     ICY_ERROR(find_send->value.input.push_back(msg));
                 }*/
            }
            else
            {
                if (action.type() == mbox::action_type::button_press ||
                    action.type() == mbox::action_type::on_button_press ||
                    action.type() == mbox::action_type::button_click)
                {
                    auto msg = input_message(input_type::key_press, button.key, button.mod);
                    ICY_ERROR(find_send->value.input.push_back(msg));
                }
                if (action.type() == mbox::action_type::button_release ||
                    action.type() == mbox::action_type::on_button_release ||
                    action.type() == mbox::action_type::button_click)
                {
                    auto msg = input_message(input_type::key_release, button.key, button.mod);
                    ICY_ERROR(find_send->value.input.push_back(msg));
                }
            }

            for (auto it = mods.rbegin(); it != mods.rend(); ++it)
                ICY_ERROR(find_send->value.input.push_back(input_message(input_type::key_release, *it, key_mod::none)));
            break;
        }

        case mbox::action_type::on_button_press:
        case mbox::action_type::on_button_release:
        {
            mbox_button_event event;
            event.press = action.type() == mbox::action_type::on_button_press;
            event.button = action.event_button();
            event.command = action.event_command();
            const auto command = m_lib.find(event.command);
            if (command && command->type == mbox::type::command && event.button.key != key::none)
                ICY_ERROR(character->value.bevents.push_back(event));
            break;
        }

        case mbox::action_type::command_execute:
        {
            const auto& execute = action.execute();
            const auto group = m_lib.find(execute.group);
            if (execute.group == guid() || group && group->type == mbox::type::group)
            {
                if (execute.etype == mbox::execute_type::self)
                {
                    const auto virt = character->value.virt.try_find(execute.command);
                    auto command = m_lib.find(virt ? *virt : execute.command);
                    if (command && command->type == mbox::type::command)
                    {
                        ICY_ERROR(process(character, command->actions, send));
                    }
                }
                else
                {
                    for (auto&& other : m_data)
                    {
                        if (other.key == character->key && execute.etype == mbox::execute_type::multicast)
                            continue;

                        if (!group || other.value.groups.try_find(group->index))
                        {
                            auto virt = &execute.command;
                            while (true)
                            {
                                const auto next_virt = other.value.virt.try_find(*virt);
                                if (!next_virt)
                                    break;
                                virt = next_virt;
                            }
                            auto command = m_lib.find(*virt);
                            ICY_ERROR(process(other, command->actions, send));
                        }
                    }
                }
            }

            break;
        }

        case mbox::action_type::command_replace:
        {
            const auto& replace = action.replace();
            const auto source = m_lib.find(replace.source);
            const auto target = m_lib.find(replace.target);
            if (source && source->type == mbox::type::command &&
                (replace.target == guid() || target && target->type == mbox::type::command))
            {
                const auto find_virt = character.value.virt.find(source->index);
                if (find_virt == character.value.virt.end())
                {
                    ICY_ERROR(character.value.virt.insert(replace.source, replace.target));
                }
                else if (target && target != source)
                {
                    find_virt->value = replace.target;
                }
                else
                {
                    character.value.virt.erase(find_virt);
                }
            }
            break;
        }
        }
    }
    return {};
}
error_type application::process(map<guid, mbox_character>::pair_type character, const const_array_view<input_message> data, map<guid, mbox_send>& send) noexcept
{
    auto& m_lib = config.script;
    for (auto&& msg : data)
    {
        if (msg.key == key::none)
            continue;

        auto press = false;

        if (msg.type == input_type::mouse_press ||
            msg.type == input_type::mouse_release)
        {
            press = msg.type == input_type::mouse_press;
        }
        else if (msg.type == input_type::key_press)
        {
            press = true;
        }
        else if (msg.type == input_type::key_release)
        {
            press = false;
        }
        else
            continue;

        for (auto&& bevent : character.value.bevents)
        {
            if (bevent.press != press || bevent.button.key != msg.key || !icy::key_mod_and(bevent.button.mod, key_mod(msg.mods)))
                continue;

            const auto virt = character.value.virt.try_find(bevent.command);
            const auto command = m_lib.find(virt ? *virt : bevent.command);
            if (!command || command->type != mbox::type::command)
                continue;

            ICY_ERROR(process(character, command->actions, send));
        }
    }
    return {};
}

error_type read_config(application& app, bool& success) noexcept
{
    string config_path;
    ICY_ERROR(process_directory(config_path));
    ICY_ERROR(config_path.append("mbox_server_config.txt"_s));

    file config_file;
    if (const auto error = config_file.open(config_path, file_access::read, file_open::open_existing, file_share::read))
    {
        string error_str;
        to_string("Open config file '%1'", error_str, string_view(config_path));
        return show_error(error, error_str);
    }

    char config_data[4096];
    auto config_size = sizeof(config_data);
    if (const auto error = config_file.read(config_data, config_size))
        return show_error(error, "Read config file"_s);

    json config_json;
    if (const auto error = to_value(string_view(config_data, config_size), config_json))
        return show_error(error, "Parse config file to JSON"_s);

    string multicast;
    string script_path;
    string script_group;
    config_json.get(mbox::json_key_multicast, multicast);
    config_json.get(mbox::json_key_script_path, script_path);
    config_json.get(mbox::json_key_script_group, script_group);

    {
        string_view multicast_addr;
        string_view multicast_port;
        const auto config_multicast_port_delim = multicast.find(":"_s);
        if (config_multicast_port_delim != multicast.end())
        {
            multicast_addr = string_view(multicast.begin(), config_multicast_port_delim);
            multicast_port = string_view(config_multicast_port_delim + 1, multicast.end());
        }

        error_type error;
        if (!multicast_addr.empty() && !multicast_port.empty())
        {
            array<network_address> vec;
            error = network_address::query(vec, multicast_addr, multicast_port);
            if (!vec.empty())
                app.config.multicast = std::move(vec[0]);
        }
        else
        {
            error = make_stdlib_error(std::errc::invalid_argument);
        }

        if (!app.config.multicast)
            return show_error(error, "Invalid multicast address"_s);
    }

    network_server_config net_config;
    net_config.buffer = network_default_buffer;
    net_config.capacity = 10;
    if (const auto error = create_event_system(app.network, net_config))
        return show_error(error, "Launch UDP network server"_s);

    if (const auto error = app.config.script.load_from(script_path))
        return show_error(error, "Invalid MBox script"_s);

    success = true;
    return error_type();
}

error_type main_func() noexcept
{
    ICY_SCOPE_EXIT{ event::post(nullptr, event_type::global_quit); };

    shared_ptr<application> app;
    ICY_ERROR(make_shared(app));

    auto success = false;
    ICY_ERROR(read_config(*app, success));
    if (!success)
        return error_type();

    ICY_ERROR(create_event_system(app->gui));
        
    shared_ptr<network_thread> network_thread;
    ICY_ERROR(make_shared(network_thread));

    network_thread->system = app->network;
    ICY_ERROR(network_thread->launch());
    ICY_ERROR(app->launch());
    
    ICY_ERROR(network_thread->rename("Network Thread"_s));
    ICY_ERROR(app->rename("Application Thread"_s));

    const auto error = app->gui->exec();
    global_gui.store(nullptr);
    return error;
}

int main()
{
    heap gheap;
    gheap.initialize(heap_init::global(2_gb));
    return main_func().code;
}
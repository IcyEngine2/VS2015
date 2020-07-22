#include <icy_engine/core/icy_core.hpp>
#include <icy_engine/core/icy_json.hpp>
#include <icy_engine/core/icy_file.hpp>
#include <icy_engine/core/icy_thread.hpp>
#include <icy_engine/core/icy_set.hpp>
#include <icy_engine/graphics/icy_remote_window.hpp>
#include <icy_engine/network/icy_network.hpp>
#include <icy_engine/network/icy_http.hpp>
#define ICY_QTGUI_STATIC 1
#include <icy_qtgui/icy_xqtgui.hpp>
#include "../mbox.h"
#include "../mbox_script.hpp"
using namespace icy;

#if _DEBUG
#pragma comment(lib, "icy_engine_cored")
#pragma comment(lib, "icy_engine_graphicsd")
#pragma comment(lib, "icy_engine_networkd")
#pragma comment(lib, "icy_qtguid")
#else
#pragma comment(lib, "icy_engine_core")
#pragma comment(lib, "icy_engine_graphics")
#pragma comment(lib, "icy_engine_network")
#pragma comment(lib, "icy_qtgui")
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

class application : public thread
{
public:
    string connect;
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
    struct mbox_data_type
    {
        remote_window window;
        set<guid> groups;
        array<mbox_button_event> bevents;
        array<mbox_timer_event> tevents;
        map<guid, guid> virt;
    };
    enum
    {
        column_process,
        column_profile,
        column_count,
    };
private:
    error_type run() noexcept;
    error_type reset() noexcept;
    error_type process(map<guid, mbox_data_type>::pair_type character, const const_array_view<mbox::action> actions, map<guid, mbox_send>& send) noexcept;
    error_type process(map<guid, mbox_data_type>::pair_type character, const const_array_view<input_message> data, map<guid, mbox_send>& send) noexcept;
    error_type find_window(const gui_widget parent, remote_window& output_window, guid& output_index) noexcept;
private:
    mbox::library m_lib;
    map<guid, mbox_data_type> m_data;
    gui_node m_profile_model;
    gui_node m_data_model;
};

error_type application::run() noexcept
{
    ICY_SCOPE_EXIT{ event::post(nullptr, event_type::global_quit); };
#if _DEBUG
    library hook_lib = "mbox_dlld"_lib;
#else
    library hook_lib = "mbox_dll"_lib;
#endif
    ICY_ERROR(hook_lib.initialize());
    ICY_ERROR(network->recv(network_default_buffer));

    global_gui.store(gui.get());

    xgui_widget window;
    ICY_ERROR(window.initialize(gui_widget_type::window, gui_widget(), gui_widget_flag::layout_grid));

    xgui_widget script_path;
    xgui_widget script_exec;
    ICY_ERROR(script_path.initialize(gui_widget_type::line_edit, window));
    ICY_ERROR(script_exec.initialize(gui_widget_type::tool_button, window));
    ICY_ERROR(script_path.insert(gui_insert(0, 0, 5, 1)));
    ICY_ERROR(script_exec.insert(gui_insert(5, 0, 1, 1)));
    ICY_ERROR(script_path.text("path/to/script"_s));
    ICY_ERROR(script_path.enable(false));

    xgui_widget process_launch;
    xgui_widget process_find;
    xgui_widget process_clear;
    ICY_ERROR(process_launch.initialize(gui_widget_type::text_button, window));
    ICY_ERROR(process_find.initialize(gui_widget_type::text_button, window));
    ICY_ERROR(process_clear.initialize(gui_widget_type::text_button, window));
    ICY_ERROR(process_launch.insert(gui_insert(0, 1, 2, 1)));
    ICY_ERROR(process_find.insert(gui_insert(2, 1, 2, 1)));
    ICY_ERROR(process_clear.insert(gui_insert(4, 1, 2, 1)));
    ICY_ERROR(process_launch.text("Launch"_s));
    ICY_ERROR(process_find.text("Find"_s));
    ICY_ERROR(process_clear.text("Clear"_s));


    ICY_ERROR(gui->create(m_profile_model));
    ICY_ERROR(gui->create(m_data_model));

    xgui_widget table;
    ICY_ERROR(table.initialize(gui_widget_type::grid_view, window));
    ICY_ERROR(table.bind(m_data_model));
    ICY_ERROR(table.insert(gui_insert(0, 2, 6, 1)));

    shared_ptr<event_queue> queue;
    ICY_ERROR(create_event_system(queue, event_type::gui_any | event_type::network_any));

    ICY_ERROR(window.show(true));

    while (true)
    {
        event event;
        ICY_ERROR(queue->pop(event));
        if (event->type == event_type::global_quit)
            return error_type();

        if (event->type == event_type::network_recv)
        {
            const auto& event_data = event->data<network_event>();
            ICY_ERROR(network->recv(network_default_buffer));
            json jinput;
            if (to_value(string_view(reinterpret_cast<const char*>(event_data.bytes.data()), event_data.bytes.size()), jinput))
                continue;

            string str_guid;
            if (jinput.get(mbox::json_key_profile, str_guid))
                continue;
            guid index;
            if (to_value(str_guid, index))
                continue;

            auto type = 0u;
            auto key = 0u;
            auto mod = 0u;
            if (false
                || jinput.get(mbox::json_key_type, type)
                || jinput.get(mbox::json_key_key, key)
                || jinput.get(mbox::json_key_key, mod))
                continue;

            auto find = m_data.find(index);
            if (find == m_data.end())
                continue;

            auto input = input_message(input_type(type), icy::key(key), key_mod(mod));
            
            map<guid, mbox_send> send;
            for (auto&& pair : m_data)
            {
                if (pair.value.window.handle())
                    ICY_ERROR(send.find_or_insert(pair.key, mbox_send()));
            }
            ICY_ERROR(process(*find, const_array_view<input_message>{ input }, send));
            
            for (auto&& pair : send)
            {
                if (pair.value.input.empty())
                    continue;

                auto target = m_data.find(pair.key);
                auto erase = false;
                for (auto&& msg : pair.value.input)
                {
                    if (auto error = target->value.window.send(msg))
                    {
                        erase = true;
                        break;
                    }                        
                }
                if (erase)
                    target->value.window = remote_window();
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
            if (event_data.widget == script_exec)
            {
                string file_name;
                ICY_ERROR(dialog_open_file(file_name, { dialog_filter("MBox Script (*.txt)"_s, "*.txt"_s) }));
                if (file_name.empty())
                    continue;
                
                if (const auto error = m_lib.load_from(file_name))
                {
                    ICY_ERROR(show_error(error, "Invalid MBox Script"_s));
                }
                else
                {
                    ICY_ERROR(script_path.text(file_name));
                    ICY_ERROR(reset());
                }
            }
            else if (event_data.widget == process_clear)
            {
                ICY_ERROR(reset());
            }
            else if (event_data.widget == process_find)
            {
                remote_window output_window;
                guid output_index;
                ICY_ERROR(find_window(window, output_window, output_index));
                if (!output_index)
                    continue;
                
                auto count = 0u;
                for (auto&& pair : m_data)
                {
                    if (pair.value.window.handle())
                        ++count;
                }

                ICY_ERROR(gui->insert_rows(m_data_model, count, 1));
                gui_node nodes[] =
                {
                    gui->node(m_data_model, count, column_process),
                    gui->node(m_data_model, count, column_profile),
                };
                for (auto&& node : nodes)
                {
                    if (!node)
                        return make_stdlib_error(std::errc::invalid_argument);
                }
                string str_process;
                string str_window;
                ICY_ERROR(to_string(output_window.process(), str_process));
                ICY_ERROR(to_string("MBox %1 [Proc %2]"_s, str_window,
                    string_view(m_lib.find(output_index)->name), output_window.process()));
                ICY_ERROR(output_window.rename(str_window));

                ICY_ERROR(gui->text(nodes[column_process], str_process));
                ICY_ERROR(gui->text(nodes[column_profile], m_lib.find(output_index)->name));

                string str_guid;
                ICY_ERROR(to_string(output_index, str_guid));

                string path;
                ICY_ERROR(process_directory(path));
                ICY_ERROR(path.appendf("%1", mbox::config_path));
                ICY_ERROR(make_directory(path));
                ICY_ERROR(path.appendf("%1.txt"_s, string_view(str_process)));

                json jconfig = json_type::object;
                ICY_ERROR(jconfig.insert(mbox::json_key_version, string_view(mbox::config_version)));
                ICY_ERROR(jconfig.insert(mbox::json_key_connect, connect));
                ICY_ERROR(jconfig.insert(mbox::json_key_pause_begin, uint32_t(key::slash)));
                ICY_ERROR(jconfig.insert(mbox::json_key_pause_end, uint32_t(key::esc)));
                ICY_ERROR(jconfig.insert(mbox::json_key_pause_toggle, uint32_t(key::enter)));
                ICY_ERROR(jconfig.insert(mbox::json_key_profile, str_guid));

                string jstr;
                ICY_ERROR(to_string(jconfig, jstr));
                file file;
                ICY_ERROR(file.open(path, file_access::write, file_open::create_always, file_share::read | file_share::write));
                ICY_ERROR(file.append(jstr.bytes().data(), jstr.bytes().size()));

                ICY_ERROR(output_window.hook(hook_lib, GetMessageHook));

                m_data.find(output_index)->value.window = std::move(output_window);
            }
        }

    }
    return error_type();
}
error_type application::reset() noexcept
{
    m_data.clear();
    
    ICY_ERROR(gui->clear(m_data_model));
    ICY_ERROR(gui->insert_cols(m_data_model, 0, column_count - 1));
    ICY_ERROR(gui->hheader(m_data_model, column_process, "Process ID"_s));
    ICY_ERROR(gui->hheader(m_data_model, column_profile, "Character"_s));
    ICY_ERROR(gui->clear(m_profile_model));
    
    array<mbox::base> vec;
    ICY_ERROR(m_lib.enumerate(mbox::type::character, vec));
    std::sort(vec.begin(), vec.end(), [](const mbox::base& lhs, const mbox::base& rhs) { return lhs.name < rhs.name; });
    ICY_ERROR(gui->insert_rows(m_profile_model, 0, vec.size() + 1));

    {
        auto node =gui->node(m_profile_model, 0, 0);
        if (!node)
            return make_stdlib_error(std::errc::not_enough_memory);
        ICY_ERROR(gui->text(node, "Select character"_s));
    }

    for (auto k = 0u; k < vec.size(); ++k)
    {
        mbox_data_type new_tab;
        map<guid, mbox_send> send;
        auto it = m_data.end();
        ICY_ERROR(m_data.insert(std::move(vec[k].index), std::move(new_tab), &it));
        ICY_ERROR(process(*it, vec[k].actions, send));

        string profile_name;
        ICY_ERROR(m_lib.reverse_path(vec[k].index, profile_name));
        auto node = gui->node(m_profile_model, k + 1, 0);
        if (!node)
            return make_stdlib_error(std::errc::not_enough_memory);

        ICY_ERROR(gui->text(node, profile_name));
        ICY_ERROR(gui->udata(node, vec[k].index));
    }
    return error_type();
}
error_type application::find_window(const gui_widget parent, remote_window& output_window, guid& output_index) noexcept
{
    array<remote_window> vec;
    {
        array<remote_window> find_windows;
        ICY_ERROR(remote_window::enumerate(find_windows));

        for (auto&& window : find_windows)
        {
            auto skip = false;
            for (auto&& pair : m_data)
            {
                if (pair.value.window.handle() == window.handle())
                {
                    skip = true;
                    break;
                }
            }
            if (window.process() == process_index())
                skip = true;

            if (skip)
                continue;
            vec.push_back(std::move(window));
        }
    }
    if (vec.empty())
        return error_type();

    xgui_model find_model;
    ICY_ERROR(find_model.initialize());
    ICY_ERROR(find_model.insert_rows(0, vec.size()));
    for (auto k = 0u; k < vec.size(); ++k)
    {
        string window_name;
        ICY_ERROR(vec[k].name(window_name));
        
        string str;
        ICY_ERROR(to_string("[%1] %2", str, vec[k].process(), string_view(window_name)));

        auto node = find_model.node(k, 0);
        if (!node)
            return make_stdlib_error(std::errc::not_enough_memory);

        ICY_ERROR(node.text(str));
    }


    xgui_widget window;
    ICY_ERROR(window.initialize(gui_widget_type::window, parent, gui_widget_flag::layout_vbox));
    xgui_widget combo;
    ICY_ERROR(combo.initialize(gui_widget_type::combo_box, window));
    ICY_ERROR(combo.bind(m_profile_model));

    xgui_widget view;
    ICY_ERROR(view.initialize(gui_widget_type::list_view, window));
    ICY_ERROR(view.bind(find_model));

    xgui_widget buttons;
    ICY_ERROR(buttons.initialize(gui_widget_type::none, window, gui_widget_flag::layout_hbox | gui_widget_flag::auto_insert));

    xgui_widget button_okay;
    ICY_ERROR(button_okay.initialize(gui_widget_type::text_button, buttons));
    ICY_ERROR(button_okay.text("Okay"_s));

    xgui_widget button_exit;
    ICY_ERROR(button_exit.initialize(gui_widget_type::text_button, buttons));
    ICY_ERROR(button_exit.text("Cancel"_s));

    ICY_ERROR(window.show(true));

    shared_ptr<event_queue> loop;
    ICY_ERROR(create_event_system(loop, event_type::gui_select | event_type::gui_update));

    remote_window select_window;
    guid select_index;

    while (true)
    {
        event event;
        ICY_ERROR(loop->pop(event));
        if (event->type == event_type::global_quit)
            return event::post(nullptr, event_type::global_quit);

        const auto& event_data = event->data<gui_event>();
        if (event->type == event_type::gui_update)
        {
            if (event_data.widget == button_exit ||
                event_data.widget == window)
                return error_type();

            if (event_data.widget == button_okay)
            {
                if (!select_window.handle())
                {
                    ICY_ERROR(show_error(make_stdlib_error(std::errc::invalid_argument), "No window has been selected"_s));
                    continue;
                }
                if (!select_index)
                {
                    ICY_ERROR(show_error(make_stdlib_error(std::errc::invalid_argument), "No character has been selected"_s));
                    continue;
                }
                if (const auto ptr = m_data.try_find(select_index))
                {
                    if (ptr->window.handle())
                    {
                        ICY_ERROR(show_error(make_stdlib_error(std::errc::invalid_argument), "This character already has a window"_s));
                        continue;
                    }
                }
                output_window = select_window;
                output_index = select_index;
                break;
            }
        }
        else if (event->type == event_type::gui_select)
        {
            auto node = event_data.data.as_node();
            if (node.model() == m_profile_model.model())
            {
                select_index = node.udata().as_guid();
            }
            else if (node.model() == find_model.model())
            {
                select_window = vec[node.row()];
            }
        }
    }
    return error_type();
}
error_type application::process(map<guid, mbox_data_type>::pair_type character, const const_array_view<mbox::action> actions, map<guid, mbox_send>& send) noexcept
{
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
error_type application::process(map<guid, mbox_data_type>::pair_type character, const const_array_view<input_message> data, map<guid, mbox_send>& send) noexcept
{
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

error_type main_func() noexcept
{
    ICY_SCOPE_EXIT{ event::post(nullptr, event_type::global_quit); };

    shared_ptr<gui_queue> gui;
    ICY_ERROR(create_event_system(gui));

    network_server_config config;
    config.port = 40671;
    config.capacity = 10;
    shared_ptr<network_system_udp_server> network;
    ICY_ERROR(create_event_system(network, config));
    
    application app;
    app.gui = gui;
    app.network = network;
    ICY_ERROR(copy("localhost:40671"_s, app.connect));

    network_thread network_thread;
    network_thread.system = network;
    ICY_ERROR(network_thread.launch());

    ICY_ERROR(app.launch());
    const auto error = gui->exec();
    global_gui.store(nullptr);
    return error;
}

int main()
{
    heap gheap;
    gheap.initialize(heap_init::global(2_gb));
    return main_func().code;
}
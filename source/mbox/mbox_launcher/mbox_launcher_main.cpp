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
    shared_ptr<network_system_udp_client> system;
    error_type run() noexcept
    {
        ICY_SCOPE_EXIT{ event::post(nullptr, event_type::global_quit); };
        return system->exec();
    }
};

struct config
{
#if _DEBUG
    library hook_lib = "mbox_dlld"_lib;
#else
    library hook_lib = "mbox_dll"_lib;
#endif
    network_address multicast;
};

class application : public thread
{
public:
    config config;
    shared_ptr<gui_queue> gui;
private:
    error_type run() noexcept;
    error_type find_window(const gui_widget parent, remote_window& output_window) noexcept;
    error_type reset(xgui_widget& table) noexcept;
private:
    struct data_type
    {
        string name;
        remote_window window;
    };
    enum
    {
        column_process,
        column_name,
        column_count,
    };
private:
    mbox::library m_lib;
    map<guid, data_type> m_data;
    gui_node m_data_model;
};

error_type application::run() noexcept
{
    shared_ptr<network_thread> network_thread;
    ICY_SCOPE_EXIT{ if (network_thread) network_thread->wait(); };
    ICY_SCOPE_EXIT{ event::post(nullptr, event_type::global_quit); };
    ICY_ERROR(make_shared(network_thread));

    shared_ptr<network_system_udp_client> network;
    network_server_config network_config;
    network_config.capacity = 10;
    network_config.buffer = network_default_buffer;
    network_config.port = config.multicast.port();
    ICY_ERROR(create_event_system(network, network_config));
    ICY_ERROR(network->join(config.multicast));

    network_thread->system = network;
    ICY_ERROR(network_thread->launch());
    ICY_ERROR(network_thread->rename("Application thread"_s));

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
    ICY_ERROR(btn_ping.enable(false));
    ICY_ERROR(reset(table));

    string connect;
    network_address connect_addr;

    const auto ping = [this,  &network, &connect_addr]
    {
        if (!connect_addr)
            return error_type();

        json json_reply = json_type::object;
        json json_characters = json_type::array;
        for (auto&& pair : m_data)
        {
            if (pair.value.window.handle())
            {
                string str_guid;
                ICY_ERROR(to_string(pair.key, str_guid));
                ICY_ERROR(json_characters.push_back(std::move(str_guid)));
            }
        }
        ICY_ERROR(json_reply.insert(mbox::json_key_character, std::move(json_characters)));
        string str_reply;
        ICY_ERROR(to_string(json_reply, str_reply));
        ICY_ERROR(network->send(connect_addr, str_reply.ubytes()));
        return error_type();
    };

    while (true)
    {
        event event;
        ICY_ERROR(queue->pop(event));
        if (event->type == event_type::global_quit)
            return error_type();

        if (event->type == event_type::network_recv)
        {
            const auto& event_data = event->data<network_event>();
            json json_object;
            if (to_value(string_view(reinterpret_cast<const char*>(event_data.bytes.data()), event_data.bytes.size()), json_object))
                continue;

            const auto chr = json_object.find(mbox::json_key_character);
            if (!chr)
                continue;

            if (chr->type() == json_type::array)
            {
                map<guid, data_type> new_data;
                for (auto k = 0u; k < chr->size(); ++k)
                {
                    guid index;
                    if (to_value(chr->at(k)->get(mbox::json_key_index), index))
                        continue;

                    string name;
                    if (chr->at(k)->get(mbox::json_key_name, name))
                        continue;

                    data_type new_element;
                    new_element.name = std::move(name);
                    ICY_ERROR(new_data.insert(std::move(index), std::move(new_element)));
                }
                for (auto&& pair : new_data)
                {
                    const auto find = m_data.find(pair.key);
                    if (find != m_data.end())
                        pair.value.window = std::move(find->value.window);
                }
                m_data = std::move(new_data);
                ICY_ERROR(copy(event_data.address, connect_addr));
                ICY_ERROR(to_string(connect_addr, connect));

                ICY_ERROR(reset(table));
                ICY_ERROR(btn_ping.enable(true));
                ICY_ERROR(ping());
            }
            else if (chr->type() == json_type::string)
            {
                guid index;
                if (to_value(chr->get(), index))
                    continue;

                auto find = m_data.find(index);
                if (find == m_data.end() || find->value.window.handle() == nullptr)
                    continue;

                auto json_input_array = json_object.find(mbox::json_key_input);
                if (!json_input_array || json_input_array->type() != json_type::array)
                    continue;

                for (auto k = 0u; k < json_input_array->size(); ++k)
                {
                    const auto json_input = json_input_array->at(k);

                    uint32_t type = 0;
                    uint32_t key = 0;
                    uint32_t mods = 0;
                    if (json_input->get(mbox::json_key_type, type))
                        continue;
                    if (json_input->get(mbox::json_key_key, key))
                        continue;
                    if (json_input->get(mbox::json_key_mods, mods))
                        continue;

                    const auto msg = input_message(input_type(type), icy::key(key), icy::key_mod(mods));
                    if (find->value.window.send(msg))
                    {
                        if (find->value.window.hook(config.hook_lib, GetMessageHook))
                        {
                            find->value.window = remote_window();
                            ICY_ERROR(ping());
                            break;
                        }
                    }
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
            else if (event_data.widget == btn_ping)
            {
                ICY_ERROR(ping());
            }
        }
        else if (event->type == event_type::gui_context)
        {
            if (event_data.widget != table || connect.empty())
                continue;

            const auto node = event_data.data.as_node();
            if (!node)
                continue;

            auto it = m_data.begin();
            std::advance(it, node.row());
            
            xgui_action action_launch;
            xgui_action action_find;
            xgui_action action_clear;
            ICY_ERROR(action_launch.initialize("Launch"_s));
            ICY_ERROR(action_find.initialize("Find"_s));
            ICY_ERROR(action_clear.initialize("Clear"_s));

            xgui_widget menu;
            ICY_ERROR(menu.initialize(gui_widget_type::menu, table, gui_widget_flag::none));
            ICY_ERROR(menu.insert(action_launch));
            ICY_ERROR(menu.insert(action_find));
            ICY_ERROR(menu.insert(action_clear));

            if (it->value.window.handle())
            {
                ICY_ERROR(action_launch.enable(false));
                ICY_ERROR(action_find.enable(false));
            }
            else
            {
                ICY_ERROR(action_clear.enable(false));
            }

            gui_action action;
            ICY_ERROR(menu.exec(action));
            if (action == action_launch)
            {
                ;
            }
            else if (action == action_find)
            {
                remote_window output_window;
                ICY_ERROR(find_window(window, output_window));
                if (!output_window.handle())
                    continue;

                string str_process;
                string str_window;
                ICY_ERROR(to_string(output_window.process(), str_process));
                ICY_ERROR(to_string("MBox %1 [Proc %2]"_s, str_window, string_view(it->value.name), output_window.process()));
                ICY_ERROR(output_window.rename(str_window));

                auto node_process = gui->node(m_data_model, node.row(), column_process);
                ICY_ERROR(gui->text(node_process, str_process));

                string str_guid;
                ICY_ERROR(to_string(it->key, str_guid));

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
                ICY_ERROR(jconfig.insert(mbox::json_key_character, str_guid));

                string jstr;
                ICY_ERROR(to_string(jconfig, jstr));
                {
                    file file;
                    ICY_ERROR(file.open(path, file_access::write, file_open::create_always, file_share::read | file_share::write));
                    ICY_ERROR(file.append(jstr.bytes().data(), jstr.bytes().size()));
                }
                ICY_ERROR(output_window.hook(config.hook_lib, GetMessageHook));
                it->value.window = std::move(output_window);
                ICY_ERROR(ping());
            }
            else if (action == action_clear)
            {
                auto node_process = gui->node(m_data_model, node.row(), column_process);
                ICY_ERROR(gui->text(node_process, ""_s));
                it->value.window = remote_window();
                ICY_ERROR(ping());
            }
        }
    }
    return error_type();
}
error_type application::find_window(const gui_widget parent, remote_window& output_window) noexcept
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
                output_window = select_window;
                break;
            }
        }
        else if (event->type == event_type::gui_select)
        {
            auto node = event_data.data.as_node();
            if (node.model() == find_model.model())
                select_window = vec[node.row()];
        }
    }
    return error_type();
}
error_type application::reset(xgui_widget& table) noexcept
{
    ICY_ERROR(gui->clear(m_data_model));
    ICY_ERROR(gui->insert_cols(m_data_model, 0, column_count - 1));
    ICY_ERROR(gui->hheader(m_data_model, column_process, "Process"_s));
    ICY_ERROR(gui->hheader(m_data_model, column_name, "Character"_s));
    ICY_ERROR(table.bind(m_data_model));

    ICY_ERROR(gui->insert_rows(m_data_model, 0, m_data.size()));

    auto row = 0u;
    for (auto&& pair : m_data)
    {
        const auto node_process = gui->node(m_data_model, row, column_process);
        const auto node_name = gui->node(m_data_model, row, column_name);

        string str_process;
        if (const auto pid = pair.value.window.process())
            ICY_ERROR(to_string(pid, str_process));

        ICY_ERROR(gui->text(node_process, str_process));
        ICY_ERROR(gui->text(node_name, pair.value.name));
        ++row;
    }
    return error_type();
}

error_type read_config(application& app, bool& success) noexcept
{
    if (const auto error = app.config.hook_lib.initialize())
        return show_error(error, "Load hook library (mbox_hook.dll)"_s);
    
    const auto hook = app.config.hook_lib.find(GetMessageHook);
    if (!hook)
        return show_error(make_stdlib_error(std::errc::function_not_supported), "Hook library function 'GetMessageHook' was not found"_s);


    string config_path;
    ICY_ERROR(process_directory(config_path));
    ICY_ERROR(config_path.append("mbox_launcher_config.txt"_s));

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
    config_json.get(mbox::json_key_multicast, multicast);

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
    ICY_ERROR(app->launch());
    ICY_ERROR(app->rename("Application thread"_s));

    const auto error0 = app->gui->exec();
    global_gui.store(nullptr);
    const auto error1 = app->wait();
    ICY_ERROR(error0);
    ICY_ERROR(error1);
    return error_type();
}

int main()
{
    heap gheap;
    if (gheap.initialize(heap_init::global(2_gb)))
        return -1;
    auto error = main_func();
    if (error)
        show_error(error, ""_s);
    return error.code;
}
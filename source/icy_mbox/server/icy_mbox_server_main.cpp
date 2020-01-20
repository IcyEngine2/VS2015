#define ICY_QTGUI_STATIC 1
#include <icy_qtgui/icy_qtgui.hpp>
#include <icy_engine/core/icy_file.hpp>
#include <icy_engine/core/icy_json.hpp>
#include <icy_engine/core/icy_thread.hpp>
#include <icy_engine/core/icy_queue.hpp>
#include <icy_engine/process/icy_process.hpp>
#include <icy_engine/utility/icy_minhook.hpp>
#include <icy_engine/network/icy_network.hpp>
#include "../icy_mbox_network.hpp"
#include "../icy_mbox_script2.hpp"
#include "icy_mbox_server_config.hpp"
#include "icy_mbox_server_apps.hpp"
#include "icy_mbox_server_proc.hpp"  
#include "icy_mbox_server_network.hpp"
#include "icy_mbox_server_input_log.hpp"
#include "icy_mbox_server_script2.hpp"
#if _DEBUG
#pragma comment(lib, "icy_engine_cored")
#pragma comment(lib, "icy_engine_networkd")
#pragma comment(lib, "icy_engine_imaged")
#pragma comment(lib, "icy_qtguid")
#else
#pragma comment(lib, "icy_engine_core")
#pragma comment(lib, "icy_engine_network")
#pragma comment(lib, "icy_engine_image")
#pragma comment(lib, "icy_qtgui")
#endif

using namespace icy;

uint32_t show_error(const error_type error, const string_view text) noexcept;

class mbox_application;

class mbox_main_thread : public thread
{
public:
    mbox_main_thread(mbox_application& app) noexcept : m_app(app)
    {

    }
    error_type run() noexcept override;
private:
    mbox_application& m_app;
};
class mbox_application
{
    friend mbox_main_thread;
public:
    error_type init(const mbox_config& config) noexcept;
    error_type exec() noexcept;
private:
    error_type process_name_to_application(const string_view process_name, string& app) const noexcept;
    error_type exec(const event event) noexcept;
    error_type launch() noexcept;
    error_type ping() noexcept;
    error_type context(const gui_node node) noexcept;
private:
    mbox_main_thread m_main = *this;
    mbox_config m_config;
    shared_ptr<gui_queue> m_gui;
    gui_widget m_window;
    mbox_applications m_apps;
    mbox_processes m_proc;
    mbox_server_network m_network;
    gui_widget m_app_path;
    gui_widget m_launch;
    gui_widget m_ping;
    gui_widget m_library_load;
    string m_app;
    unique_ptr<mbox_input_log> m_input;
    mbox::library m_library;
    mbox_script_thread m_script_thread = m_network;
};

int main()
{
    const auto gheap_capacity = 256_mb;

    heap gheap;
    if (const auto error = gheap.initialize(heap_init::global(gheap_capacity)))
        return error.code;
    
    array<string> args;
    if (const auto error = win32_parse_cargs(args))
        return show_error(error, "parse cmdline args"_s);
    if (args.empty())
        return show_error(make_stdlib_error(std::errc::invalid_argument), "invalid cmdline args"_s);
    
    string config_path;
    if (args.size() > 1)
    {
        if (const auto error = to_string(args[1], config_path))
            return show_error(error, "copy cmdline args"_s);
    }
    else
    {
        if (const auto error = to_string("%1mbox_config.txt"_s, config_path, file_name(args[0]).directory))
            return show_error(error, "setup config path"_s);
    }
    file file_config;
    if (const auto error = file_config.open(config_path,
        file_access::read, file_open::open_existing, file_share::read))
    {
        string msg;
        to_string("open config file '%1'"_s, msg, string_view(config_path));
        return show_error(error, msg);
    }

    array<char> text_config;
    if (const auto error = text_config.resize(file_config.info().size))
        return show_error(error, "allocate config memory");
    
    auto size_config = text_config.size();
    {
        const auto error = file_config.read(text_config.data(), size_config);
        file_config.close();
        if (error)
        {
            string msg;
            to_string("read config file '%1'"_s, msg, string_view(config_path));
            return show_error(error, msg);
        }
    }
    json json_config;
    if (const auto error = json::create(string_view(text_config.data(), size_config), json_config))
    {
        string msg;
        to_string("create json from config file '%1'"_s, msg, string_view(config_path));
        return show_error(error, msg);
    }

    mbox_config config;
    if (const auto error = config.from_json(json_config))
    {
        string msg;
        to_string("parse config file '%1'"_s, msg, string_view(config_path));
        return show_error(error, msg);
    }

    mbox_application app;
    if (const auto error = app.init(config))
        return show_error(error, "init application"_s);

    if (const auto error = app.exec())
        return show_error(error, "exec application"_s);

    return 0;
}


error_type mbox_main_thread::run() noexcept
{
    ICY_SCOPE_EXIT{ event::post(nullptr, event_type::global_quit); };

    shared_ptr<event_loop> loop;
    ICY_ERROR(event_loop::create(loop,
        event_type::window_close | event_type::gui_any | event_type::user_any));

    while (true)
    {
        event event;
        ICY_ERROR(loop->loop(event));
        if (event->type == event_type::global_quit)
            break;
        ICY_ERROR(m_app.exec(event));
    }
    return {};
}


error_type mbox_application::init(const mbox_config& config) noexcept
{
  /*  icy::library user32_lib = "user32"_lib;
    ICY_ERROR(user32_lib.initialize());
    using AllowSetForegroundWindowFunc = int(__stdcall*)(unsigned long);
    if (const auto proc = user32_lib.find<AllowSetForegroundWindowFunc>("AllowSetForegroundWindow"))
    {
        if (!proc(UINT32_MAX))
            return last_system_error();
    }
    else
        return make_stdlib_error(std::errc::function_not_supported);*/
    
    ICY_ERROR(mbox_config::copy(config, m_config));
    ICY_ERROR(create_gui(m_gui));
    ICY_ERROR(m_library.initialize());
    ICY_ERROR(m_gui->create(m_window, gui_widget_type::window, {}, gui_widget_flag::layout_grid)); 
    ICY_ERROR(m_apps.initialize(*m_gui, m_window));
    ICY_ERROR(m_proc.initialize(*m_gui, m_library, m_window));
    ICY_ERROR(m_network.initialize(m_config));
    //ICY_ERROR(mbox_grid_proc::create(*m_gui, m_window, m_proc));

    gui_widget label_apps;
    gui_widget label_exe_path;
    gui_widget label_dll_path;
    gui_widget value_dll_path;
    gui_widget buttons;
    ICY_ERROR(m_gui->create(label_apps, gui_widget_type::label, m_window));
    ICY_ERROR(m_gui->create(label_exe_path, gui_widget_type::label, m_window));
    ICY_ERROR(m_gui->create(label_dll_path, gui_widget_type::label, m_window));
    ICY_ERROR(m_gui->create(m_app_path, gui_widget_type::line_edit, m_window));
    ICY_ERROR(m_gui->create(value_dll_path, gui_widget_type::line_edit, m_window));
    ICY_ERROR(m_gui->create(buttons, gui_widget_type::none, m_window, gui_widget_flag::layout_hbox));
    ICY_ERROR(m_gui->create(m_launch, gui_widget_type::text_button, buttons, gui_widget_flag::auto_insert));
    ICY_ERROR(m_gui->create(m_ping, gui_widget_type::text_button, buttons, gui_widget_flag::auto_insert));
    ICY_ERROR(m_gui->create(m_library_load, gui_widget_type::text_button, buttons, gui_widget_flag::auto_insert));

    ICY_ERROR(m_gui->insert(label_apps, gui_insert(0, 0)));
    ICY_ERROR(m_gui->insert(label_exe_path, gui_insert(0, 1)));
    ICY_ERROR(m_gui->insert(label_dll_path, gui_insert(0, 2)));
    ICY_ERROR(m_gui->insert(m_apps.widget(), gui_insert(1, 0)));
    ICY_ERROR(m_gui->insert(m_app_path, gui_insert(1, 1)));
    ICY_ERROR(m_gui->insert(value_dll_path, gui_insert(1, 2)));
    ICY_ERROR(m_gui->insert(buttons, gui_insert(0, 3, 2, 1)));
    ICY_ERROR(m_gui->insert(m_proc.widget(), gui_insert(0, 4, 2, 1)));

    ICY_ERROR(m_gui->text(label_apps, "Applications"_s));
    ICY_ERROR(m_gui->text(label_exe_path, "App File Path"_s));
    ICY_ERROR(m_gui->text(label_dll_path, "Inject DLL Path"_s));
    ICY_ERROR(m_gui->text(m_launch, "Launch"_s));
    ICY_ERROR(m_gui->text(m_ping, "Ping"_s));
    ICY_ERROR(m_gui->text(m_library_load, "Load Script Library"_s));

    for (auto&& pair : m_config.applications)
        ICY_ERROR(m_apps.append(pair.key));
    
    ICY_ERROR(m_gui->text(value_dll_path, m_config.inject));
    ICY_ERROR(m_gui->show(m_window, true));
    
    return {};
}
error_type mbox_application::exec() noexcept
{
    ICY_SCOPE_EXIT{ event::post(nullptr, event_type::global_quit); m_network.wait(); };
    ICY_ERROR(m_main.launch());
    ICY_ERROR(m_script_thread.launch());
    ICY_ERROR(m_network.launch());
    ICY_ERROR(m_gui->loop());
    ICY_ERROR(m_network.error());
    ICY_ERROR(m_main.error());
    return {};
}
error_type mbox_application::exec(const event event) noexcept
{
    if (event->type == event_type::window_close)
    {
        const auto& event_data = event->data<gui_event>();
        if (event_data.widget == m_window)
        {
            ICY_ERROR(event::post(nullptr, event_type::global_quit));
        }
        else
        {
            ICY_ERROR(m_gui->show(event_data.widget, false));
        }
    }
    else if (event->type == event_type::gui_update)
    {
        const auto& event_data = event->data<gui_event>();
        if (event_data.widget == m_launch)
        {
            if (const auto error = launch())
                show_error(error, "Launch new process"_s);
        }
        else if (event_data.widget == m_ping)
        {
            if (const auto error = ping())
                show_error(error, "Ping (update process list)"_s);
        }
        else if (event_data.widget == m_library_load)
        {
            string file_name;
            ICY_ERROR(dialog_open_file(file_name));
            if (!file_name.empty())
            {
                if (const auto error = m_library.load_from(file_name))
                    show_error(error, "Load script library"_s);
                event_user_library new_event;
                ICY_ERROR(mbox::library::copy(m_library, new_event.library));
                ICY_ERROR(event::post(nullptr, event_type_user_library, std::move(new_event)));
            }
        }
    }
    else if (event->type == event_type::gui_select)
    {
        const auto& event_data = event->data<gui_event>();
        if (event_data.widget == m_apps.widget())
        {
            const auto app = m_apps.find(event_data.data.as_node());
            if (const auto str = m_config.applications.try_find(app))
            {
                if (const auto error = m_gui->text(m_app_path, *str))
                    show_error(error, "Select application"_s);
                else
                    ICY_ERROR(to_string(app, m_app));
            }
        }
    }
    else if (event->type == event_type::gui_context)
    {
        const auto& event_data = event->data<gui_event>();
        if (event_data.widget == m_proc.widget())
        {
            if (const auto error = context(event_data.data.as_node()))
                show_error(error, "Inject application"_s);
        }
        
    }
    else if (event->type == event_type_user_connect)
    {
        const auto& msg = event->data<event_user_connect>();
        mbox_process proc;
        proc.key = msg.info.key;
        ICY_ERROR(to_string(msg.address, proc.address));
        const auto copy_str = [](const auto& bytes, string& str)
        {
            return to_string(string_view(bytes, strnlen(bytes, _countof(bytes))), str);
        };
        ICY_ERROR(copy_str(msg.info.computer_name, proc.computer_name));
        proc.profile = msg.info.profile;
        ICY_ERROR(copy_str(msg.info.window_name, proc.window_name));
        ICY_ERROR(process_name_to_application(string_view(msg.info.process_name,
            strnlen(msg.info.process_name, sizeof(msg.info.process_name))), proc.application));
        ICY_ERROR(m_proc.update(proc));
    }
    else if (event->type == event_type_user_disconnect)
    {
        const auto& msg = event->data<event_user_disconnect>();
        for (auto&& val : m_proc.data())
        {
            if (val.key == msg.key)
            {
                mbox_process proc;
                ICY_ERROR(mbox_process::copy(val, proc));
                proc.address.clear();
                ICY_ERROR(m_proc.update(proc));
                break;
            }
        }        
    }
    else if (event->type == event_type_user_recv_image)
    {
        const auto& msg = event->data<event_user_recv_image>();
        //file f1;
        //f1.open("D:/125.png", file_access::write, file_open::create_always, file_share::none);
        //f1.append(msg.png.data(), msg.png.size());
    }
    else if (event->type == event_type_user_recv_input)
    {
        const auto& msg = event->data<event_user_recv_input>();
        for (auto&& proc : m_proc.data())
        {
            if (m_input)
            {
                if (proc.key == msg.key)
                {
                    for (auto&& input : msg.data)
                        ICY_ERROR(m_input->append(proc.profile, input));
                    break;
                }
            }
        }
    }
    return {};
}
error_type mbox_application::launch() noexcept
{
    process app;
    const auto path = m_config.applications.try_find(m_app);
    if (!path)
        return make_stdlib_error(std::errc::no_such_file_or_directory);
    ICY_ERROR(app.launch(*path));
    mbox_process mbox_proc;
    mbox_proc.key.pid = app.index();
    ICY_ERROR(computer_name(mbox_proc.computer_name));
    ICY_ERROR(to_string(m_app, mbox_proc.application));
    ICY_ERROR(m_proc.update(mbox_proc));
    mbox_proc.key.hash = hash(mbox_proc.computer_name);

    return {};
}
error_type mbox_application::ping() noexcept
{
    const auto proc_to_app = [this](const string_view process_name, string& app)
    {
        string proc_exe_name;
        ICY_ERROR(to_lower(file_name(process_name).name, proc_exe_name));
        for (auto&& pair : m_config.applications)
        {
            string app_exe_name;
            ICY_ERROR(to_lower(file_name(pair.value).name, app_exe_name));
            if (proc_exe_name == app_exe_name)
                return to_string(pair.key, app);
        }
        return make_stdlib_error(std::errc::invalid_argument);
    };

    string cname;
    ICY_ERROR(computer_name(cname));

    array<std::pair<string, uint32_t>> local_list;
    ICY_ERROR(process::enumerate(local_list));
    array<mbox_process> local;
    for (auto&& pair : local_list)
    {
        mbox_process proc;
        if (proc_to_app(pair.first, proc.application))
            continue;
        proc.key.pid = pair.second;
        proc.key.hash = hash(cname);
        ICY_ERROR(to_string(cname, proc.computer_name));
        ICY_ERROR(local.push_back(std::move(proc)));
    }
    
    for (auto&& local_proc : local)
    {
        auto found = false;
        for (auto&& proc : m_proc.data())
        {
            if (proc.key == local_proc.key)
            {
                found = true;
                break;
            }
        }
        if (!found)
            ICY_ERROR(m_proc.update(local_proc));
    }
    array<mbox_process> to_erase;
    for (auto&& proc : m_proc.data())
    {
        if (proc.computer_name != cname)
            continue;

        auto found = false;
        for (auto&& local_proc : local)
        {
            if (proc.key == local_proc.key)
            {
                found = true;
                break;
            }
        }
        if (!found)
        {
            mbox_process copy;
            ICY_ERROR(mbox_process::copy(proc, copy));
            copy.application.clear();
            ICY_ERROR(to_erase.push_back(std::move(copy)));
        }
    }    
    for (auto&& proc : to_erase)
    {
        ICY_ERROR(m_proc.update(proc));
    }
    

    network_system_udp network;
    ICY_ERROR(network.initialize());
    array<network_address> address;
    ICY_ERROR(network.address(mbox::multicast_addr, mbox::multicast_port, max_timeout, network_socket_type::UDP, address));
    if (address.empty())
        return make_stdlib_error(std::errc::bad_address);

    ICY_ERROR(network.launch(0, network_address_type::ip_v4, 0));
    
    mbox::ping ping = {};
    copy(m_config.ipaddr, ping.address);
    ICY_ERROR(network.send(address[0], { reinterpret_cast<const uint8_t*>(&ping), sizeof(ping) }));
    
    return {};
}

error_type mbox_application::process_name_to_application(const string_view process_name, string& app) const noexcept
{
    string proc_exe_name;
    ICY_ERROR(to_lower(file_name(process_name).name, proc_exe_name));
    for (auto&& pair : m_config.applications)
    {
        string app_exe_name;
        ICY_ERROR(to_lower(file_name(pair.value).name, app_exe_name));
        if (proc_exe_name == app_exe_name)
            return to_string(pair.key, app);
    }
    return make_stdlib_error(std::errc::invalid_argument);
}
error_type mbox_application::context(const gui_node node) noexcept
{
    if (node.row() >= m_proc.data().size())
        return {};// make_stdlib_error(std::errc::invalid_argument);

    gui_widget_scoped menu(*m_gui);
    gui_widget_scoped menu_profile(*m_gui);
    gui_action_scoped inject(*m_gui);
    gui_action_scoped deject(*m_gui);
    gui_action_scoped render(*m_gui);
    gui_action_scoped input(*m_gui);
    gui_action_scoped profile(*m_gui);

    ICY_ERROR(m_gui->create(menu, gui_widget_type::menu, m_window));
    ICY_ERROR(m_gui->create(menu_profile, gui_widget_type::menu, menu));
    ICY_ERROR(m_gui->create(inject, "Inject"_s));
    ICY_ERROR(m_gui->create(deject, "Deject"_s));
    ICY_ERROR(m_gui->create(render, "Render"_s));
    ICY_ERROR(m_gui->create(input, "Input"_s));
    
    array<guid> profiles;
    ICY_ERROR(m_library.enumerate(mbox::type::character, profiles));

    ICY_ERROR(m_gui->create(profile, "Profile"_s));
    map<string, guid> profile_map;
    for (auto&& index : profiles)
    {
        string str;
        ICY_ERROR(m_library.path(index, str));
        ICY_ERROR(profile_map.insert(std::move(str), std::move(index)));
    }
    map<gui_action_scoped, guid> profile_actions;
    ICY_ERROR(profile_actions.reserve(profile_map.size() + 1));
    
    gui_action_scoped null_profile(*m_gui);
    ICY_ERROR(null_profile.initialize("[Null]"_s));
    ICY_ERROR(m_gui->insert(menu_profile, null_profile));
    ICY_ERROR(profile_actions.insert(std::move(null_profile), guid()));

    for (auto&& pair : profile_map)
    {
        gui_action_scoped action(*m_gui);
        ICY_ERROR(action.initialize(pair.key));
        ICY_ERROR(m_gui->insert(menu_profile, action));
        ICY_ERROR(profile_actions.insert(std::move(action), guid(pair.value)));        
    }
    ICY_ERROR(m_gui->bind(profile, menu_profile));
    
    const auto& value = m_proc.data()[node.row()];
    if (value.address.empty())
    {
        ICY_ERROR(m_gui->enable(deject, false));
        ICY_ERROR(m_gui->enable(render, false));
        ICY_ERROR(m_gui->enable(profile, false));
    }
    else
    {
        ICY_ERROR(m_gui->enable(inject, false));
    }
    ICY_ERROR(m_gui->insert(menu, inject));
    ICY_ERROR(m_gui->insert(menu, deject));
    ICY_ERROR(m_gui->insert(menu, render));
    ICY_ERROR(m_gui->insert(menu, input));
    ICY_ERROR(m_gui->insert(menu, profile));
    gui_action action;
    ICY_ERROR(m_gui->exec(menu, action));

    if (action == inject)
    {
        process proc;
        ICY_ERROR(proc.open(value.key.pid));
        ICY_ERROR(proc.inject(m_config.inject));
    }
    else if (action == deject)
    {
        ICY_ERROR(m_network.disconnect(value.key));
    }
    else if (action == render)
    {
        //rectangle rect;
        //rect.x = 25;
        //rect.y = 25;
        //rect.w = 256;
        //rect.h = 256;
        //ICY_ERROR(m_network.send(value.key, { &rect, 1 }));
    }
    else if (action == input)
    {
        m_input = make_unique<mbox_input_log>();
        ICY_ERROR(m_input->initialize(*m_gui, m_library));
    }
    else if (action.index && action.index != null_profile.index)
    {
        const auto it = profile_actions.find(action);
        if (it != profile_actions.end())
        {
            for (auto&& proc : m_proc.data())
            {
                if (proc.key == value.key)
                {
                    event_user_profile event;
                    event.key = value.key;
                    event.profile = it->value;
                    ICY_ERROR(event::post(nullptr, event_type_user_profile, event));

                    mbox_process new_proc;
                    ICY_ERROR(mbox_process::copy(proc, new_proc));
                    new_proc.profile = it->value;
                    ICY_ERROR(m_proc.update(new_proc));

                    string str;
                    ICY_ERROR(m_library.path(it->value, str));
                    ICY_ERROR(str.insert(str.begin(), " ["_s));
                    ICY_ERROR(str.insert(str.begin(), proc.application));
                    ICY_ERROR(str.append("]"_s));
                    
                    mbox::info info = {};
                    copy(str, info.window_name);
                    info.profile = it->value;

                    ICY_ERROR(m_network.send(value.key, info));
                    break;
                }
            }
        }
    }
 
    return {};
}

uint32_t show_error(const error_type error, const string_view text) noexcept
{
    string msg;
    to_string("Error: %4 - %1 code [%2]: %3", msg, error.source, long(error.code), error, text);
    win32_message(msg, "Error"_s);
    return error.code;
}
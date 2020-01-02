#define ICY_QTGUI_STATIC 1
#include <icy_qtgui/icy_qtgui.hpp>
#include <icy_engine/core/icy_file.hpp>
#include <icy_engine/core/icy_json.hpp>
#include <icy_engine/core/icy_thread.hpp>
#include <icy_engine/core/icy_queue.hpp>
#include <icy_engine/process/icy_process.hpp>
#include <icy_engine/utility/icy_minhook.hpp>
#include <icy_engine/network/icy_network.hpp>
#include "../icy_mbox.hpp"
#include "icy_mbox_server_config.hpp"
#include "icy_mbox_server_apps.hpp"
#include "icy_mbox_server_proc.hpp"  
#if _DEBUG
#pragma comment(lib, "icy_engine_cored")
#pragma comment(lib, "icy_engine_networkd")
#pragma comment(lib, "icy_qtguid")
#else
#pragma comment(lib, "icy_engine_core")
#pragma comment(lib, "icy_engine_network")
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
    error_type update() noexcept;
    //error_type proc_context(const gui_node node) noexcept;
private:
    mbox_main_thread m_main = *this;
    mbox_config m_config;
    shared_ptr<gui_queue> m_gui;
    gui_widget m_window;
    mbox_applications m_apps;
    mbox_processes m_proc;
    gui_widget m_app_path;
    gui_widget m_launch;
    gui_widget m_update;
    string m_app;
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
    ICY_ERROR(mbox_config::copy(config, m_config));
    ICY_ERROR(create_gui(m_gui));
    ICY_ERROR(m_gui->create(m_window, gui_widget_type::window, {}, gui_widget_flag::layout_grid)); 
    ICY_ERROR(m_apps.initialize(*m_gui, m_window));
    ICY_ERROR(m_proc.initialize(*m_gui, m_window));
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
    ICY_ERROR(m_gui->create(m_update, gui_widget_type::text_button, buttons, gui_widget_flag::auto_insert));
    
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
    ICY_ERROR(m_gui->text(m_update, "Update"_s));

    for (auto&& pair : m_config.applications)
        ICY_ERROR(m_apps.append(pair.key));
    
    //ICY_ERROR(m_apps->update(m_config));
    ICY_ERROR(m_gui->text(value_dll_path, m_config.inject));

    ICY_ERROR(m_gui->show(m_window, true));
    
    return {};
}
error_type mbox_application::exec() noexcept
{
    ICY_SCOPE_EXIT{ event::post(nullptr, event_type::global_quit); };
    ICY_ERROR(m_main.launch());
    ICY_ERROR(m_gui->loop());
    ICY_ERROR(m_main.error());
    return {};
}
error_type mbox_application::exec(const event event) noexcept
{
    if (event->type == event_type::window_close && event->data<gui_event>().widget == m_window)
    {
        ICY_ERROR(event::post(nullptr, event_type::global_quit));
    }
    else if (event->type == event_type::gui_update)
    {
        const auto& event_data = event->data<gui_event>();
        if (event_data.widget == m_launch)
        {
            if (const auto error = launch())
                show_error(error, "Launch new process"_s);
        }
        else if (event_data.widget == m_update)
        {
            if (const auto error = update())
                show_error(error, "Update process list (ping)"_s);
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
        /*const auto& event_data = event->data<gui_event>();
        if (event_data.widget == m_proc->widget())
        {
            if (const auto error = proc_context(event_data.data.as_node()))
                show_error(error, "Inject application"_s);
        }
        */
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
    mbox_proc.process_id = app.index();
    ICY_ERROR(computer_name(mbox_proc.computer_name));
    ICY_ERROR(to_string(m_app, mbox_proc.application));
    ICY_ERROR(m_proc.update(mbox_proc));

    return {};
}
error_type mbox_application::update() noexcept
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
        proc.process_id = pair.second;
        ICY_ERROR(to_string(cname, proc.computer_name));
        ICY_ERROR(local.push_back(std::move(proc)));
    }
    
    for (auto&& local_proc : local)
    {
        auto found = false;
        for (auto&& proc : m_proc.data())
        {
            if (proc.process_id == local_proc.process_id && proc.computer_name == local_proc.computer_name)
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
            if (proc.process_id == local_proc.process_id)
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
    return {};
    //m_apps.remove(node);
    
  /*  m_proc.update();
    array<mbox_grid_proc::value_type> local_proc;
    for (auto&& pair : local_list)
    {
        mbox_grid_proc::value_type val;
        if (proc_to_app(pair.first, val.application))
            continue;

        /*auto found = false;
        for (auto&& network_val : network_proc)
        {
            if (network_val.computer_name == cname && network_val.process_id == pair.second)
            {
                found = true;
                break;
            }
        }
        if (found)
            continue;

        ICY_ERROR(to_string(cname, val.computer_name));
        val.process_id = pair.second;
        ICY_ERROR(local_proc.push_back(std::move(val)));
    }
    //for (auto&& proc : local_proc)
     //   network_proc.push_back(std::move(proc));

    ICY_ERROR(m_proc->update(std::move(local_proc)));

    /*if (m_config.ipaddr.empty())
        return make_stdlib_error(std::errc::bad_address);

    string cname;
    ICY_ERROR(computer_name(cname));

    mbox::command send_cmd;
    ICY_ERROR(create_guid(send_cmd.cmd_uid));
    send_cmd.cmd_type = mbox::command_type::info;
    send_cmd.arg_type = mbox::command_arg_type::string;
    send_cmd.binary.size = copy(m_config.ipaddr, send_cmd.binary.bytes);
   
    network_system_udp network;
    ICY_ERROR(network.initialize());
    
    array<network_address> address;
    ICY_ERROR(network.address(mbox::multicast_addr, mbox::multicast_port, max_timeout, network_socket_type::UDP, address));
    if (address.empty())
        return make_stdlib_error(std::errc::bad_address);
    
    ICY_ERROR(network.launch(0, network_address_type::ip_v4, 0));
    ICY_ERROR(network.recv(sizeof(mbox::command)));
    ICY_ERROR(network.send(address[0], { reinterpret_cast<const uint8_t*>(&send_cmd), sizeof(send_cmd) }));

  //  array<mbox_grid_proc::value_type> network_proc;

   

   /* auto code = 0u;
    while (true)
    {
        const auto timeout = std::chrono::milliseconds(250);
        network_udp_request request;
        if (const auto error = network.loop(request, code, timeout))
        {
            if (error == make_stdlib_error(std::errc::timed_out))
                break;
            else
                return error;
        }
        if (code)
            break;

        if (request.type == network_request_type::recv)
        {
            ICY_ERROR(network.recv(sizeof(mbox::command)));
        }

        if (request.type == network_request_type::recv &&
            request.bytes.size() >= sizeof(mbox::command))
        {
            const auto recv_cmd = reinterpret_cast<const mbox::command*>(request.bytes.data());
            if (recv_cmd->version != mbox::protocol_version)
                continue;
            if (recv_cmd->cmd_uid != send_cmd.cmd_uid)
                continue;
            if (recv_cmd->cmd_type != mbox::command_type::info)
                continue;
            if (recv_cmd->arg_type != mbox::command_arg_type::info)
                continue;

            const auto& info = recv_cmd->info;

            const auto make_str = [](const char(&buffer)[32])
            {
                return string_view(buffer, strnlen(buffer, _countof(buffer)));
            };

            mbox_grid_proc::value_type val;
            val.process_id = info.process_id;
            ICY_ERROR(to_string(make_str(info.computer_name), val.computer_name));
            ICY_ERROR(to_string(make_str(info.profile), val.profile));
            ICY_ERROR(to_string(make_str(info.window_name), val.window_name));
            val.address;
            if (proc_to_app(make_str(info.process_name), val.application))
                continue;
            
            ICY_ERROR(network_proc.push_back(std::move(val)));
        }
    }

    array<std::pair<string, uint32_t>> local_list;
    ICY_ERROR(process::enumerate(local_list));

    array<mbox_grid_proc::value_type> local_proc;
    for (auto&& pair : local_list)
    {
        mbox_grid_proc::value_type val;
        if (proc_to_app(pair.first, val.application))
            continue;

        /*auto found = false;
        for (auto&& network_val : network_proc)
        {
            if (network_val.computer_name == cname && network_val.process_id == pair.second)
            {
                found = true;
                break;
            }                
        }
        if (found)
            continue;

        ICY_ERROR(to_string(cname, val.computer_name));
        val.process_id = pair.second; 
        ICY_ERROR(local_proc.push_back(std::move(val)));
    }
    //for (auto&& proc : local_proc)
     //   network_proc.push_back(std::move(proc));

    ICY_ERROR(m_proc->update(std::move(local_proc)));*/
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
}/*
error_type mbox_application::proc_context(const gui_node node) noexcept
{
    gui_widget menu;
    gui_action inject;
    gui_action deject;
    gui_action render;
    ICY_ERROR(m_gui->create(menu, gui_widget_type::menu, m_window));
    ICY_SCOPE_EXIT{ m_gui->destroy(menu); };
    ICY_ERROR(m_gui->create(inject, "Inject"_s));
    ICY_SCOPE_EXIT{ m_gui->destroy(inject); };
    ICY_ERROR(m_gui->create(deject, "Deject"_s));
    ICY_SCOPE_EXIT{ m_gui->destroy(deject); };
    ICY_ERROR(m_gui->create(render, "Render"_s));
    ICY_SCOPE_EXIT{ m_gui->destroy(render); };

    const auto& value = m_proc->data(node);
    if (value.address.empty())
    {
        m_gui->enable(deject, false);
        m_gui->enable(render, false);
    }
    else
    {
        m_gui->enable(inject, false);
    }
    ICY_ERROR(m_gui->insert(menu, inject));
    ICY_ERROR(m_gui->insert(menu, deject));
    ICY_ERROR(m_gui->insert(menu, render));
    gui_action action;
    ICY_ERROR(m_gui->exec(menu, action));

    if (action == inject)
    {
        process proc;
        ICY_ERROR(proc.open(value.process_id));
        ICY_ERROR(proc.inject(m_config.inject));
    }
    else if (action == deject)
    {
        mbox::command cmd;
        cmd.cmd_type = mbox::command_type::exit;
        ICY_ERROR(create_guid(cmd.cmd_uid));
        cmd.proc_uid = value.uid;
        value.address;
        m_proc->data();

        ICY_ERROR(m_send.push(std::move(cmd)));
        m_network;

        network_system_tcp network;
        ICY_ERROR(network.initialize());
        const auto delim = value.address.find(":"_s);
        if (delim == value.address.end())
            return make_stdlib_error(std::errc::bad_address);

        array<network_address> address;
        ICY_ERROR(network.address(string_view(value.address.begin(), delim),
            string_view(delim + 1, value.address.end()), network_default_timeout, network_socket_type::TCP, address));
        if (address.empty())
            return make_stdlib_error(std::errc::bad_address);

     
        network_tcp_connection conn(network_connection_type::none);
        conn.timeout(max_timeout);
        ICY_ERROR(network.connect(conn, address[0], { reinterpret_cast<const uint8_t*>(&cmd), sizeof(cmd) }));
        
        auto code = 0u;
        network_tcp_reply reply;
        ICY_ERROR(network.loop(reply, code));
    }
    return {};
}*/

uint32_t show_error(const error_type error, const string_view text) noexcept
{
    string msg;
    to_string("Error: %4 - %1 code [%2]: %3", msg, error.source, long(error.code), error, text);
    win32_message(msg, "Error"_s);
    return error.code;
}
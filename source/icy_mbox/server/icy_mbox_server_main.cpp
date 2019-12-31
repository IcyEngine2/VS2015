#define ICY_QTGUI_STATIC 1
#include <icy_qtgui/icy_qtgui.hpp>
#include <icy_engine/core/icy_file.hpp>
#include <icy_engine/core/icy_json.hpp>
#include <icy_engine/core/icy_thread.hpp>
#include <icy_engine/process/icy_process.hpp>
#include <icy_engine/utility/icy_minhook.hpp>
#include <icy_engine/network/icy_network.hpp>
#include "../icy_mbox.hpp"
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
class mbox_config
{
public:
    struct key
    {
        static constexpr string_view applications = "Applications"_s;
        static constexpr string_view library = "Library"_s;
    };
public:
    error_type from_json(const json& input) noexcept;
    error_type to_json(json& output) const noexcept;
    static error_type copy(const mbox_config& src, mbox_config& dst) noexcept;
public:
    map<string, string> applications;
    string library;
};
class mbox_list_apps : public gui_model
{
public:
    static error_type create(gui_queue& gui, const gui_widget parent, shared_ptr<mbox_list_apps>& model) noexcept;
    error_type update(const mbox_config& config) noexcept;
    uint32_t data(const gui_node node, const gui_role role, gui_variant& value) const noexcept;
    gui_widget widget() const noexcept
    {
        return m_widget;
    }
private:
    gui_widget m_widget;
    array<string> m_data;
};
class mbox_grid_proc : public gui_model
{
public:
    struct value_type
    {
        uint32_t process_id = 0;
        string computer_name;
        string window_name;
        string profile;
        string application;
        network_address address;
    };
    enum
    {
        col_application,
        col_profile,
        col_address,
        col_window_name,
        col_computer_name,
        col_process_id,
        _col_count,
    };
public:
    static error_type create(gui_queue& gui, const gui_widget parent, shared_ptr<mbox_grid_proc>& model) noexcept;
    error_type update(array<value_type>&& vec) noexcept;
    uint32_t data(const gui_node node, const gui_role role, gui_variant& value) const noexcept;
    const value_type& data(const gui_node node) const noexcept
    {
        return m_data[node.row];
    }
    gui_widget widget() const noexcept
    {
        return m_widget;
    }
private:
    gui_widget m_widget;
    array<value_type> m_data;
};

class mbox_main_thread : public thread
{
public:
    mbox_main_thread(mbox_application& app) noexcept : m_app(app)
    {

    }
    error_type run() noexcept override;
public:
    mbox_application& m_app;
};
class mbox_application
{
    friend mbox_main_thread;
public:
    error_type init(const mbox_config& config) noexcept;
    error_type exec() noexcept;
private:
    error_type exec(const event event) noexcept;
    error_type launch() noexcept;
    error_type update() noexcept;
    error_type apps_select(const gui_node node) noexcept;
    error_type proc_context(const gui_node node) noexcept;
private:
    network_system_udp m_network;
    mbox_main_thread m_main = *this;
    mbox_config m_config;
    shared_ptr<gui_queue> m_gui;
    gui_widget m_window;
    shared_ptr<mbox_list_apps> m_apps;
    shared_ptr<mbox_grid_proc> m_proc;
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

error_type mbox_config::from_json(const json& input) noexcept
{
    if (const auto json_apps = input.find(key::applications))
    {
        for (auto k = 0u; k < json_apps->keys().size(); ++k)
        {
            const string_view key = json_apps->keys()[k];
            const string_view val = json_apps->vals()[k].get();
            if (!val.empty() && !key.empty())
                ICY_ERROR(emplace(applications, key, val));
        }
    }
    input.get(key::library, library);    
    return {};
}
error_type mbox_config::to_json(json& output) const noexcept
{
    output = json_type::object;
    json json_apps = json_type::object;
    for (auto&& pair : applications)
        ICY_ERROR(json_apps.insert(pair.key, pair.value));
    ICY_ERROR(output.insert(key::library, library));
    return {};
}
error_type mbox_config::copy(const mbox_config& src, mbox_config& dst) noexcept
{
    dst.applications.clear();
    for (auto&& pair : src.applications)
        ICY_ERROR(emplace(dst.applications, pair.key, pair.value));
    ICY_ERROR(to_string(src.library, dst.library));
    return {};
}

error_type mbox_list_apps::create(gui_queue& gui, const gui_widget parent, shared_ptr<mbox_list_apps>& model) noexcept
{
    ICY_ERROR(make_shared(model));
    ICY_ERROR(gui.create(model->m_widget, gui_widget_type::combo_box, parent));
    ICY_ERROR(gui.initialize(model));
    return {};
}
error_type mbox_list_apps::update(const mbox_config& config) noexcept
{
    ICY_ERROR(gui_model::clear());
    ICY_ERROR(gui_model::bind({}, m_widget));
    m_data.clear();
    auto row = 0ui64;
    for (auto&& pair : config.applications)
    {
        gui_node node;
        node.row = row++;
        ICY_ERROR(gui_model::init(node));
        string str;
        ICY_ERROR(to_string(pair.key, str));
        ICY_ERROR(m_data.push_back(std::move(str)));
    }
    ICY_ERROR(gui_model::update({}));
    return {};
}
uint32_t mbox_list_apps::data(const gui_node node, const gui_role role, gui_variant& value) const noexcept
{
    if (node.row < m_data.size())
    {
        if (role == gui_role::view)
            return make_variant(value, m_data[node.row]).code;
    }
    return uint32_t(std::errc::invalid_argument);
}

error_type mbox_grid_proc::create(gui_queue& gui, const gui_widget parent, shared_ptr<mbox_grid_proc>& model) noexcept
{
    ICY_ERROR(make_shared(model));
    ICY_ERROR(gui.create(model->m_widget, gui_widget_type::grid_view, parent));
    ICY_ERROR(gui.initialize(model));
    return {};
}
error_type mbox_grid_proc::update(array<value_type>&& vec) noexcept
{
    ICY_ERROR(gui_model::clear());
    ICY_ERROR(gui_model::bind({}, m_widget));
    m_data = std::move(vec);
    auto row = 0_z;
    for (auto&& value : m_data)
    {
        gui_node node;
        node.row = row++;
        for (auto k = 0u; k < _col_count; ++k)
        {
            node.col = k;
            ICY_ERROR(gui_model::init(node));
        }
    }
    ICY_ERROR(gui_model::update({}));
    return {};
}
uint32_t mbox_grid_proc::data(const gui_node node, const gui_role role, gui_variant& value) const noexcept
{
    if (node.row < m_data.size() && role == gui_role::view)
    {
        auto& val = m_data[node.row];
        switch (node.col)
        {
        case col_application:
            return make_variant(value, val.application).code;

        case col_profile:
            return make_variant(value, val.profile).code;

        case col_address:
        {
            string str;
            if (val.address)
            {
                if (const auto error = to_string(val.address, str))
                    return error.code;
            }
            return make_variant(value, str).code;
        }
        case col_window_name:
            return make_variant(value, val.window_name).code;

        case col_computer_name:
            return make_variant(value, val.computer_name).code;

        case col_process_id:
            value = uint64_t(val.process_id);
            return 0;
        }
    }
    return uint32_t(std::errc::invalid_argument);
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
    ICY_ERROR(m_network.initialize());

    ICY_ERROR(mbox_config::copy(config, m_config));
    ICY_ERROR(gui_queue::create(m_gui));
    ICY_ERROR(m_gui->create(m_window, gui_widget_type::window, {}, gui_widget_flag::layout_grid));
    ICY_ERROR(mbox_list_apps::create(*m_gui, m_window, m_apps));
    ICY_ERROR(mbox_grid_proc::create(*m_gui, m_window, m_proc));

    gui_widget label_apps;
    gui_widget label_exe_path;
    gui_widget label_dll_path;
    gui_widget ;
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
    ICY_ERROR(m_gui->insert(m_apps->widget(), gui_insert(1, 0)));
    ICY_ERROR(m_gui->insert(m_app_path, gui_insert(1, 1)));
    ICY_ERROR(m_gui->insert(value_dll_path, gui_insert(1, 2)));
    ICY_ERROR(m_gui->insert(buttons, gui_insert(0, 3, 2, 1)));
    ICY_ERROR(m_gui->insert(m_proc->widget(), gui_insert(0, 4, 2, 1)));

    ICY_ERROR(m_gui->text(label_apps, "Applications"_s));
    ICY_ERROR(m_gui->text(label_exe_path, "App File Path"_s));
    ICY_ERROR(m_gui->text(label_dll_path, "Inject DLL Path"_s));
    ICY_ERROR(m_gui->text(m_launch, "Launch"_s));
    ICY_ERROR(m_gui->text(m_update, "Update"_s));

    ICY_ERROR(m_apps->update(m_config));
    ICY_ERROR(m_gui->text(value_dll_path, m_config.library));

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
        if (event_data.widget == m_apps->widget())
        {
            if (const auto error = apps_select(event_data.data.as_node()))
                show_error(error, "Select application"_s);
        }
    }
    else if (event->type == event_type::gui_context)
    {
        const auto& event_data = event->data<gui_event>();
        if (event_data.widget == m_proc->widget())
        {
            if (const auto error = proc_context(event_data.data.as_node()))
                show_error(error, "Inject application"_s);
        }
        
    }
    return {};
}
error_type mbox_application::launch() noexcept
{
    process app;
    const auto path = m_config.applications.try_find(m_app);
    if (path)
        ICY_ERROR(app.launch(*path));
    return {};
}
error_type mbox_application::update() noexcept
{
    string cname;
    ICY_ERROR(computer_name(cname));

    mbox::command send_cmd;
    ICY_ERROR(create_guid(send_cmd.uid));
    send_cmd.type = mbox::command_type::info;
    send_cmd.arg = mbox::command_arg_type::string;

   
    array<network_address> address;
    ICY_ERROR(m_network.address(mbox::multicast_addr, mbox::multicast_port, max_timeout, network_socket_type::UDP, address));
    if (address.empty())
        return make_stdlib_error(std::errc::bad_address);
    
    ICY_ERROR(m_network.launch(0, network_address_type::ip_v4, 0));
    ICY_ERROR(m_network.recv(sizeof(mbox::command)));
    ICY_ERROR(m_network.send(address[0], { reinterpret_cast<const uint8_t*>(&send_cmd), sizeof(send_cmd) }));

    array<mbox_grid_proc::value_type> network_proc;

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

    auto exit = false;
    while (!exit)
    {
        const auto timeout = std::chrono::milliseconds(500);
        network_udp_request request;
        if (const auto error = m_network.loop(request, exit, timeout))
        {
            if (error == make_stdlib_error(std::errc::timed_out))
                break;
            else
                return error;
        }

        if (request.type == network_request_type::recv)
        {
            ICY_ERROR(m_network.recv(sizeof(mbox::command)));
        }

        if (request.type == network_request_type::recv &&
            request.bytes.size() >= sizeof(mbox::command))
        {
            const auto recv_cmd = reinterpret_cast<const mbox::command*>(request.bytes.data());
            if (recv_cmd->version != mbox::protocol_version)
                continue;
            if (recv_cmd->uid != send_cmd.uid)
                continue;
            if (recv_cmd->type != mbox::command_type::info)
                continue;
            if (recv_cmd->arg != mbox::command_arg_type::info)
                continue;

            const auto& info = recv_cmd->info;

            const auto make_str = [](const char(&buffer)[32])
            {
                return string_view(buffer, strnlen(buffer, _countof(buffer)));
            };

            mbox_grid_proc::value_type val;
            ICY_ERROR(network_address::copy(request.address, val.address));
            val.process_id = info.process_id;
            ICY_ERROR(to_string(make_str(info.computer_name), val.computer_name));
            //ICY_ERROR(to_string(string_view(info.process_name, strnlen(info.process_name, _countof(info.process_name))), val.process_name));
            ICY_ERROR(to_string(make_str(info.profile), val.profile));
            ICY_ERROR(to_string(make_str(info.window_name), val.window_name));
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

        auto found = false;
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
    for (auto&& proc : local_proc)
        network_proc.push_back(std::move(proc));

    ICY_ERROR(m_proc->update(std::move(network_proc)));
    return {};
}
error_type mbox_application::apps_select(const gui_node node) noexcept
{
    gui_variant var_app;
    if (const auto error = m_apps->data(node, gui_role::view, var_app))
        return make_stdlib_error(std::errc(error));

    ICY_ERROR(to_string(var_app.as_string(), m_app));
    if (const auto str = m_config.applications.try_find(m_app))
        ICY_ERROR(m_gui->text(m_app_path, *str));
    return {};
}
error_type mbox_application::proc_context(const gui_node node) noexcept
{
    gui_widget menu;
    gui_action inject;
    gui_action render;
    ICY_ERROR(m_gui->create(menu, gui_widget_type::menu, m_window));
    ICY_SCOPE_EXIT{ m_gui->destroy(menu); };
    ICY_ERROR(m_gui->create(inject, "Inject"_s));
    ICY_SCOPE_EXIT{ m_gui->destroy(inject); };
    ICY_ERROR(m_gui->create(render, "Render"_s));
    ICY_SCOPE_EXIT{ m_gui->destroy(render); };

    const auto& value = m_proc->data(node);
    if (value.address)
        m_gui->enable(inject, false);
    else
        m_gui->enable(render, false);
    
    ICY_ERROR(m_gui->insert(menu, inject));
    ICY_ERROR(m_gui->insert(menu, render));
    gui_action action;
    ICY_ERROR(m_gui->exec(menu, action));

    if (action == inject)
    {
        process proc;
        ICY_ERROR(proc.open(value.process_id));
        ICY_ERROR(proc.inject(m_config.library));
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
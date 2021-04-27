#include "icy_dcl_client_text.hpp"
#include "icy_dcl_client_menu_bar.hpp"
#include "icy_dcl_client_tools.hpp"
#include "icy_dcl_client_explorer.hpp"
#include "icy_dcl_client_properties.hpp"
#include "icy_dcl_client_rename.hpp"
#include <icy_engine/core/icy_core.hpp>
#include <icy_engine/core/icy_string.hpp>
#include <icy_engine/core/icy_thread.hpp>
#include <icy_engine/core/icy_file.hpp>
#include <icy_engine/utility/icy_database.hpp>
#include <icy_engine/network/icy_network_http.hpp>
#include <icy_auth/icy_auth.hpp>
#include <icy_qtgui/icy_qtgui.hpp>
#include <icy_dcl/icy_dcl.hpp>

#if _DEBUG
#pragma comment(lib, "icy_engine_cored.lib")
#if ICY_QTGUI_STATIC
#pragma comment(lib, "icy_qtguid.lib")
#endif
#else
#pragma comment(lib, "icy_engine_core.lib")
#if ICY_QTGUI_STATIC
#pragma comment(lib, "icy_qtgui.lib")
#endif
#endif

using namespace icy;

static const auto gheap_capacity = 64_mb;
static const auto default_window_text = "DCL v1.0"_s;

class dcl_client_application;
class dcl_client_main_thread : public thread
{
public:
    dcl_client_main_thread(dcl_client_application& app) noexcept : m_app(app)
    {

    }
    error_type run() noexcept override;
public:
    dcl_client_application& m_app;
};

class dcl_client_application
{
public:
    error_type init(const dcl_client_config& config) noexcept;
    error_type run() noexcept;
    error_type exec(const event event) noexcept;
private:
    error_type context(const dcl_index key, const dcl_index filter, dcl_text& output) noexcept;
private:
    dcl_client_main_thread m_main = *this;
    dcl_client_config m_config;
    dcl_database m_dbase;
private:
    shared_ptr<dcl_client_explorer_model> m_model_explorer;
    shared_ptr<dcl_client_properties_model> m_model_properties;
private:
    shared_ptr<gui_queue> m_gui;
    gui_widget m_window;
    dcl_client_menu_bar m_menu_bar;
    dcl_client_tools m_tools;
    dcl_client_rename m_rename;
    gui_widget m_view_explorer;
    gui_widget m_view_properties;
    gui_widget m_view_tabs;
};

error_type dcl_client_main_thread::run() noexcept
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

error_type dcl_client_application::init(const dcl_client_config& config) noexcept
{
    ICY_ERROR(dcl_client_config::copy(config, m_config));
    ICY_ERROR(m_dbase.open(m_config.filepath, m_config.filesize));
    ICY_ERROR(gui_queue::create(m_gui));
    
    ICY_ERROR(make_shared(m_model_explorer, m_dbase, m_config.locale));
    ICY_ERROR(m_gui->initialize(m_model_explorer));
    ICY_ERROR(m_model_explorer->initialize());

    ICY_ERROR(make_shared(m_model_properties, m_dbase, m_config.locale));
    ICY_ERROR(m_gui->initialize(m_model_properties));
    
    ICY_ERROR(m_gui->create(m_window, gui_widget_type::window, gui_widget{}, gui_widget_flag::layout_vbox));
    ICY_ERROR(m_menu_bar.init(*m_gui, m_window));
    ICY_ERROR(m_tools.init(*m_gui, m_window));
    ICY_ERROR(m_rename.init(m_dbase, *m_gui, m_window));

    gui_widget hsplitter;
    gui_widget vsplitter;
    ICY_ERROR(m_gui->create(hsplitter, gui_widget_type::hsplitter, m_window, gui_widget_flag::auto_insert));
    ICY_ERROR(m_gui->create(vsplitter, gui_widget_type::vsplitter, hsplitter, gui_widget_flag::none));

    ICY_ERROR(m_gui->create(m_view_explorer, gui_widget_type::tree_view, vsplitter, gui_widget_flag::none));
    ICY_ERROR(m_gui->create(m_view_properties, gui_widget_type::grid_view, vsplitter, gui_widget_flag::none));
    ICY_ERROR(m_gui->create(m_view_tabs, gui_widget_type::tabs, hsplitter, gui_widget_flag::none));
    ICY_ERROR(m_model_explorer->bind(gui_node{}, m_view_explorer));
    ICY_ERROR(m_model_properties->bind(gui_node{}, m_view_properties));
        
    gui_widget_args args;
    gui_widget_args* args_layout = nullptr;
    gui_widget_args* args_stretch = nullptr;
    ICY_ERROR(args.insert(gui_widget_args_keys::layout, args_layout));
    ICY_ERROR(args_layout->insert(gui_widget_args_keys::stretch, args_stretch));
    ICY_ERROR(args_stretch->insert("1"_s, "1"_s));
    string args_str;
    ICY_ERROR(to_string(args, args_str));
    ICY_ERROR(m_gui->modify(m_window, args_str));

    ICY_ERROR(m_gui->show(m_window, true));
    return {};
}
error_type dcl_client_application::run() noexcept
{
    ICY_SCOPE_EXIT{ event::post(nullptr, event_type::global_quit); };
    ICY_ERROR(m_main.launch());    
    ICY_ERROR(m_gui->loop());
    ICY_ERROR(m_main.error());
    return {};
}
error_type dcl_client_application::exec(const event event) noexcept
{
    /*  const auto show_error = [this](const string_view message, const error_type error) -> error_type
    {
        if (error)
        {
            string text;
            ICY_ERROR(to_string("%1 - Source (%2) [code %3]: %4", text, message, error.source, long(error.code), error));
            ICY_ERROR(m_gui->text(m_error, text));
            ICY_ERROR(m_gui->show(m_error, true));
        }
        return {};
    };*/


    ICY_ERROR(m_menu_bar.exec(*m_gui, event));
    ICY_ERROR(m_rename.exec(event));
    ICY_ERROR(m_tools.exec(*m_gui, event));
    
    if (event->type == event_type::window_close && event->data<gui_event>().widget == m_window)
    {
        ICY_ERROR(event::post(nullptr, event_type::global_quit));
    }
    else if (
        event->type == event_type::gui_context ||
        event->type == event_type::gui_select)
    {
        dcl_index key;
        const auto& event_data = event->data<gui_event>();
        if (const auto node = event_data.data.as_node())
        {
            if (event_data.widget == m_view_explorer)
            {
                gui_variant var;
                if (const auto error = m_model_explorer->data(event_data.data.as_node(), gui_role::user, var))
                    return make_stdlib_error(std::errc(error));
                key = var.as_guid();
            }
        }
        if (event->type == event_type::gui_context)
        {
            auto action = dcl_text::none;
            ICY_ERROR(context(key, event_data.widget == m_view_explorer ? dcl_directory : dcl_index(), action));
            if (action == dcl_text::rename)
            {
                ICY_ERROR(m_rename.open(key));
            }
        }
    }
    return {};
}
error_type dcl_client_application::context(const dcl_index key, const dcl_index filter, dcl_text& output) noexcept
{
    if (key == dcl_index{})
    {
        return {};
    }
    dcl_read_txn read;
    dcl_base_val val;
    ICY_ERROR(read.initialize(m_dbase));
    ICY_ERROR(read.data_main(key, val));
    auto flags = dcl_flag::none;
    ICY_ERROR(read.flags(key, flags));

    if (val.type == dcl_directory)
    {
        gui_widget menu;
        gui_widget advanced;
        gui_widget create;

        icy::map<dcl_text, gui_action> actions;
        const auto append = [this, &read, &actions](dcl_text&& key) -> error_type
        {
            gui_action action;
            ICY_ERROR(m_gui->create(action, to_string(key)));
            ICY_ERROR(actions.insert(std::move(key), std::move(action)));
            return {};
        };
        ICY_ERROR(append(dcl_text::open));
        ICY_ERROR(append(dcl_text::open_in_new_tab));
        ICY_ERROR(append(dcl_text::rename));
        ICY_ERROR(append(dcl_text::advanced));
        ICY_ERROR(append(dcl_text::move_to));
        ICY_ERROR(append(dcl_text::hide));
        ICY_ERROR(append(dcl_text::show));
        ICY_ERROR(append(dcl_text::lock));
        ICY_ERROR(append(dcl_text::unlock));
        ICY_ERROR(append(dcl_text::access));
        ICY_ERROR(append(dcl_text::destroy));
        ICY_ERROR(append(dcl_text::create));
        ICY_ERROR(append(dcl_text::directory));

        ICY_ERROR(m_gui->create(menu, gui_widget_type::menu, m_window, gui_widget_flag::none));
        ICY_ERROR(m_gui->create(advanced, gui_widget_type::menu, m_window, gui_widget_flag::none));
        ICY_ERROR(m_gui->create(create, gui_widget_type::menu, m_window, gui_widget_flag::none));
        ICY_ERROR(m_gui->bind(*actions.try_find(dcl_text::advanced), advanced));
        ICY_ERROR(m_gui->bind(*actions.try_find(dcl_text::create), create));

        ICY_ERROR(m_gui->insert(menu, *actions.try_find(dcl_text::open)));
        ICY_ERROR(m_gui->insert(menu, *actions.try_find(dcl_text::open_in_new_tab)));
        ICY_ERROR(m_gui->insert(menu, *actions.try_find(dcl_text::rename)));
        ICY_ERROR(m_gui->insert(menu, *actions.try_find(dcl_text::advanced)));
        ICY_ERROR(m_gui->insert(menu, *actions.try_find(dcl_text::create)));

        ICY_ERROR(m_gui->insert(advanced, *actions.try_find(dcl_text::move_to)));
        ICY_ERROR(m_gui->insert(advanced, *actions.try_find(flags & dcl_flag::is_hidden ? dcl_text::show : dcl_text::hide)));
        ICY_ERROR(m_gui->insert(advanced, *actions.try_find(flags & dcl_flag::is_const ? dcl_text::unlock : dcl_text::lock)));
        ICY_ERROR(m_gui->insert(advanced, *actions.try_find(dcl_text::access)));
        ICY_ERROR(m_gui->insert(advanced, *actions.try_find(flags & dcl_flag::is_deleted ? dcl_text::restore : dcl_text::destroy)));
        ICY_ERROR(m_gui->insert(create, *actions.try_find(dcl_text::directory)));
        
        gui_action action_select;
        ICY_ERROR(m_gui->exec(menu, action_select));

        for (auto&& pair : actions)
        {
            if (action_select.index == pair.value.index)
                output = pair.key;
            ICY_ERROR(m_gui->destroy(pair.value));
        }
        ICY_ERROR(m_gui->destroy(create));
        ICY_ERROR(m_gui->destroy(advanced));
        ICY_ERROR(m_gui->destroy(menu));
    }
    return {};
}

int main()
{
    heap gheap;
    if (const auto error = gheap.initialize(heap_init::global(gheap_capacity)))
        return error.code;

    const auto show_error = [](const error_type error, const string_view text)
    {
        string msg;
        to_string("Error: %4 - %1 code [%2]: %3", msg, error.source, long(error.code), error, text);
        win32_message(msg, "Error"_s);
        return error.code;
    };
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
        if (const auto error = to_string("%1dcl_config.txt"_s, config_path, file_name(args[0]).dir))
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

    char text_config[16_kb];
    auto size_config = sizeof(text_config);
    {
        const auto error = file_config.read(text_config, size_config);
        file_config.close();
        if (error)
        {
            string msg;
            to_string("read config file '%1'"_s, msg, string_view(config_path));
            return show_error(error, msg);
        }
    }
    json json_config;
    if (const auto error = json::create(string_view(text_config, size_config), json_config))
    {
        string msg;
        to_string("create json from config file '%1'"_s, msg, string_view(config_path));
        return show_error(error, msg);
    }
    
    dcl_client_config config;
    if (const auto error = config.from_json(json_config))
    {
        string msg;
        to_string("parse config file '%1'"_s, msg, string_view(config_path));
        return show_error(error, msg);
    }
    
    dcl_client_application app;
    if (const auto error = app.init(config))
        return show_error(error, "initialize application"_s);

    if (const auto error = app.run())
        return show_error(error, "run application"_s);

    return 0;
}
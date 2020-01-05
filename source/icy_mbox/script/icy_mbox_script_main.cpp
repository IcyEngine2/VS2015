#define ICY_QTGUI_STATIC 1
#include <icy_qtgui/icy_qtgui.hpp>
#include <icy_engine/core/icy_file.hpp>
#include <icy_engine/core/icy_json.hpp>
#include <icy_engine/core/icy_thread.hpp>
#include <icy_engine/core/icy_queue.hpp>
#include <icy_engine/network/icy_network.hpp>
#include "icy_mbox_script_explorer.hpp"
#include "icy_mbox_script_editor.hpp"
#include "icy_mbox_script_context.hpp"
#if _DEBUG
#pragma comment(lib, "icy_engine_cored")
#pragma comment(lib, "icy_engine_imaged")
#pragma comment(lib, "icy_qtguid")
#else
#pragma comment(lib, "icy_engine_core")
#pragma comment(lib, "icy_engine_image")
#pragma comment(lib, "icy_qtgui")
#endif

using namespace icy;

uint32_t show_error(const error_type error, const string_view text) noexcept;
class mbox_application;

class mbox_main_thread : public thread
{
public:
    mbox_main_thread(mbox_application& app) : m_app(app)
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
    error_type init() noexcept;
    error_type exec() noexcept;
private:
    struct open_type
    {
        gui_action action;
        gui_widget dialog;
    };
    struct save_type
    {
        gui_action action;
        gui_widget dialog;
    };
private:
    error_type exec(const event event) noexcept;
private:
    mbox_main_thread m_main = *this;
    shared_ptr<gui_queue> m_gui;
    mbox::library m_library;
    gui_widget m_window;
    gui_widget m_splitter;
    mbox_context m_context;
    mbox_explorer m_explorer;
    mbox_editor m_editor;
    open_type m_open;
    save_type m_save_as;
    gui_action m_save;
    string m_path;
};
int main()
{
    const auto gheap_capacity = 256_mb;

    heap gheap;
    if (const auto error = gheap.initialize(heap_init::global(gheap_capacity)))
        return error.code;

    mbox_application app;
    if (const auto error = app.init())
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
error_type mbox_application::init() noexcept
{
    ICY_ERROR(m_library.initialize());
    ICY_ERROR(create_gui(m_gui));
    ICY_ERROR(m_gui->create(m_window, gui_widget_type::window, {}, gui_widget_flag::layout_hbox));
    ICY_ERROR(m_gui->create(m_splitter, gui_widget_type::hsplitter, m_window, gui_widget_flag::auto_insert));
    ICY_ERROR(m_explorer.initialize(*m_gui, m_library, m_splitter));
    ICY_ERROR(m_editor.initialize(*m_gui, m_library, m_splitter));
    ICY_ERROR(m_context.initialize(*m_gui, m_library));

    ICY_ERROR(m_explorer.reset());
    
    gui_widget menu_bar;
    gui_widget menu_file;
    gui_action file;
    ICY_ERROR(m_gui->create(menu_bar, gui_widget_type::menubar, m_window));
    ICY_ERROR(m_gui->create(menu_file, gui_widget_type::menu, m_window));
    ICY_ERROR(m_gui->create(file, "File"_s));
    ICY_ERROR(m_gui->insert(menu_bar, file));
    ICY_ERROR(m_gui->bind(file, menu_file));
    ICY_ERROR(m_gui->create(m_open.action, "Open"_s));
    ICY_ERROR(m_gui->create(m_open.dialog, gui_widget_type::dialog_open_file, m_window));
    ICY_ERROR(m_gui->create(m_save, "Save"_s));
    ICY_ERROR(m_gui->create(m_save_as.action, "Save As"_s));
    ICY_ERROR(m_gui->create(m_save_as.dialog, gui_widget_type::dialog_save_file, m_window));
    ICY_ERROR(m_gui->insert(menu_file, m_open.action));
    ICY_ERROR(m_gui->insert(menu_file, m_save));
    ICY_ERROR(m_gui->insert(menu_file, m_save_as.action));
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
    if (event->type == event_type::window_close)
    {
        const auto& event_data = event->data<gui_event>();
        if (event_data.widget == m_window)
        {
            ICY_ERROR(event::post(nullptr, event_type::global_quit));
        }
    }
    else if (event->type == event_type::gui_update)
    {
        /*const auto& event_data = event->data<gui_event>();
        if (event_data.widget == m_launch)
        {
            if (const auto error = launch())
                show_error(error, "Launch new process"_s);
        }
        else if (event_data.widget == m_ping)
        {
            if (const auto error = ping())
                show_error(error, "Ping (update process list)"_s);
        }*/
    }
    else if (event->type == event_type::gui_select)
    {
        const auto& event_data = event->data<gui_event>();
        if (event_data.widget == m_open.dialog)
        {
            const auto path = event_data.data.as_string();
            if (const auto error = m_library.load_from(path))
            {
                show_error(error, "Open library"_s);
                return {};
            }
            ICY_ERROR(to_string(path, m_path));
            ICY_ERROR(m_explorer.reset());
        }
        else if (event_data.widget == m_save_as.dialog)
        {
            const auto path = event_data.data.as_string();
            if (const auto error = m_library.save_to(path))
            {
                show_error(error, "Save library"_s);
                return {};
            }
            if (m_path.empty())
                ICY_ERROR(to_string(path, m_path));
        }
    }
    else if (event->type == event_type::gui_action)
    {
        const auto& event_data = event->data<gui_event>();
        gui_action action;
        action.index = event_data.data.as_uinteger();
        if (action == m_open.action)
        {
            ICY_ERROR(m_gui->show(m_open.dialog, true));
        }
        else if (action == m_save_as.action || action == m_save && m_path.empty())
        {
            ICY_ERROR(m_gui->show(m_save_as.dialog, true));
        }
        else if (action == m_save)
        {
            if (const auto error = m_library.save_to(m_path))
            {
                show_error(error, "Save library"_s);
                return {};
            }
        }
    }
    else if (event->type == mbox_event_type_create)
    {
        const auto& event_data = event->data<mbox::base>();
        if (const auto error = m_library.insert(event_data))
        {
            show_error(error, "Create new object"_s);
            return {};
        }
    }

    if (const auto error = m_explorer.exec(event))
        show_error(error, "Explorer action"_s);

    if (const auto error = m_context.exec(event))
        show_error(error, "Context action"_s);

    return {};
}

uint32_t show_error(const error_type error, const string_view text) noexcept
{
    string msg;
    to_string("Error: %4 - %1 code [%2]: %3", msg, error.source, long(error.code), error, text);
    win32_message(msg, "Error"_s);
    return error.code;
}
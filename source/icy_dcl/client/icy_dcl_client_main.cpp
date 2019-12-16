#include "icy_dcl_client_text.hpp"
#include <icy_engine/core/icy_core.hpp>
#include <icy_engine/core/icy_string.hpp>
#include <icy_engine/core/icy_thread.hpp>
#include <icy_qtgui/icy_qtgui.hpp>

#if _DEBUG
#pragma comment(lib, "icy_engine_cored.lib")
#else
#pragma comment(lib, "icy_engine_core.lib")
#endif

using namespace icy;

static const auto gheap_capacity = 64_mb;


class main_thread : public thread
{
public:
    error_type initialize(gui_queue& gui) noexcept;
    error_type run() noexcept;
private:
    gui_widget m_window;
    gui_widget m_menubar;
    gui_widget m_config;
    main_menu_user m_user;
};

class application
{
public:
    error_type init() noexcept;
    error_type exec() noexcept;
private:
    heap m_gheap;
    shared_ptr<gui_queue> m_gui;
    main_thread m_main;
};


error_type main_menu_user::initialize(gui_queue& gui, gui_widget menubar) noexcept
{
    gui_action action;
    gui_widget menu;

    ICY_ERROR(gui.create(menu, gui_widget_type::menu, gui_layout::none, menubar));
    ICY_ERROR(gui.create(action, to_string(text::user)));
    ICY_ERROR(gui.create(config, to_string(text::config)));
    ICY_ERROR(gui.create(connect, to_string(text::connect)));
    ICY_ERROR(gui.create(update, to_string(text::update)));
    
    ICY_ERROR(gui.bind(action, menu));
    
    ICY_ERROR(gui.insert(menubar, action));
    ICY_ERROR(gui.insert(menu, config));
    ICY_ERROR(gui.insert(menu, connect));
    ICY_ERROR(gui.insert(menu, update));
    return {};
}
error_type main_thread::initialize(gui_queue& gui) noexcept
{
    ICY_ERROR(gui.create(m_window, gui_widget_type::window));
    ICY_ERROR(gui.show(m_window, true));
    ICY_ERROR(gui.create(m_menubar, gui_widget_type::menubar, gui_layout::none, m_window));
    ICY_ERROR(m_user.initialize(gui, m_menubar));


    return {};
}
error_type main_thread::run() noexcept
{
    ICY_SCOPE_EXIT{ event::post(nullptr, event_type::global_quit); };

    shared_ptr<event_loop> loop;
    ICY_ERROR(event_loop::create(loop, event_type::window_close | event_type::gui_action));
    while (true)
    {
        event event;
        ICY_ERROR(loop->loop(event));
        if (event->type == event_type::window_close)
        {
            break;
        }
        else if (event->type == event_type::global_quit)
        {
            break;
        }
        else if (event->type == event_type::gui_action)
        {
            auto gevent = event->data<gui_event>();
            if (gevent.data.as_uinteger() == m_user.config.index)
            {
                continue;
            }

        }
    }
    return {};
}

error_type application::init() noexcept
{
    ICY_ERROR(m_gheap.initialize(heap_init::global(gheap_capacity)));
    ICY_ERROR(gui_queue::create(m_gui, m_gheap));
    ICY_ERROR(m_main.initialize(*m_gui));
    return {};
}
error_type application::exec() noexcept
{
    ICY_ERROR(m_main.launch());
    return m_gui->loop();
}

int main()
{
    application app;

    const auto show_error = [](const error_type error)
    {
        string msg;
        to_string("Error (%1) code [%2]: %3", msg, error.source, long(error.code), error);
        win32_message(msg, "Error"_s);
        return error.code;
    };

    if (const auto error = app.init())
        return show_error(error);
    if (const auto error = app.exec())
        return show_error(error);

    return 0;
}
extern icy::string_view to_string(const text text) noexcept
{
    return to_string_enUS(text);
}
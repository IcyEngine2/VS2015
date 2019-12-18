#include "icy_dcl_client_text.hpp"
#include "icy_dcl_client_menu.hpp"
#include <icy_engine/core/icy_core.hpp>
#include <icy_engine/core/icy_string.hpp>
#include <icy_engine/core/icy_thread.hpp>
#include <icy_qtgui/icy_qtgui.hpp>

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
    error_type init() noexcept;
    error_type run() noexcept;
    error_type exec(const event_type type, const gui_event event) noexcept;
    error_type exec(const dcl_event_type::msg_config& msg) noexcept;
    error_type exec(const dcl_event_type::msg_network_connect& msg) noexcept;
    error_type exec(const dcl_event_type::msg_network_update& msg) noexcept;
    error_type exec(const dcl_event_type::msg_network_upload& msg) noexcept;
    error_type exec(const dcl_event_type::msg_project_open&) noexcept;
    error_type exec(const dcl_event_type::msg_project_close&) noexcept;
    error_type exec(const dcl_event_type::msg_project_create&) noexcept;
    error_type exec(const dcl_event_type::msg_project_save&) noexcept;
private:
    dcl_client_main_thread m_main = *this;
    shared_ptr<gui_queue> m_gui;
    gui_widget m_window;
    dcl_client_menu m_menu;
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

        switch (event->type)
        {
        case event_type::window_close:
        case event_type::gui_action:
            ICY_ERROR(m_app.exec(event->type, event->data<icy::gui_event>()));
            break;

        case dcl_event_type::config:
            ICY_ERROR(m_app.exec(event->data<dcl_event_type::msg_config>()));
            break;

        case dcl_event_type::network_connect:
            ICY_ERROR(m_app.exec(event->data<dcl_event_type::msg_network_connect>()));
            break;

        case dcl_event_type::network_update:
            ICY_ERROR(m_app.exec(event->data<dcl_event_type::msg_network_update>()));
            break;

        case dcl_event_type::network_upload:
            ICY_ERROR(m_app.exec(event->data<dcl_event_type::msg_network_upload>()));
            break;

        case dcl_event_type::project_open:
            ICY_ERROR(m_app.exec(event->data<dcl_event_type::msg_project_open>()));
            break;

        case dcl_event_type::project_close:
            ICY_ERROR(m_app.exec(dcl_event_type::msg_project_close()));
            break;

        case dcl_event_type::project_create:
            ICY_ERROR(m_app.exec(event->data<dcl_event_type::msg_project_create>()));
            break;

        case dcl_event_type::project_save:
            ICY_ERROR(m_app.exec(dcl_event_type::msg_project_save()));
            break;
        }
    }
    return {};
}

error_type dcl_client_application::init() noexcept
{
    ICY_ERROR(gui_queue::create(m_gui));
    ICY_ERROR(m_gui->create(m_window, gui_widget_type::window));
    ICY_ERROR(m_menu.init(*m_gui, m_window));
    ICY_ERROR(m_gui->show(m_window, true));    
    return {};
}
error_type dcl_client_application::run() noexcept
{
    ICY_SCOPE_EXIT{ event::post(nullptr, event_type::global_quit); };
    ICY_ERROR(m_main.launch());    
    return m_gui->loop();
}
error_type dcl_client_application::exec(const event_type type, const gui_event event) noexcept
{
    if (type == event_type::window_close && event.widget == m_window)
    {
        ICY_ERROR(event::post(nullptr, event_type::global_quit));
    }
    else
    {
        ICY_ERROR(m_menu.exec(*m_gui, type, event));
    }
    return {};
}
error_type dcl_client_application::exec(const dcl_event_type::msg_config& msg) noexcept
{
    return {};
}
error_type dcl_client_application::exec(const dcl_event_type::msg_network_connect& msg) noexcept
{
    return {};
}
error_type dcl_client_application::exec(const dcl_event_type::msg_network_update& msg) noexcept
{
    return {};
}
error_type dcl_client_application::exec(const dcl_event_type::msg_network_upload& msg) noexcept
{
    return {};
}
error_type dcl_client_application::exec(const dcl_event_type::msg_project_open& msg) noexcept
{
    return {};
}
error_type dcl_client_application::exec(const dcl_event_type::msg_project_close&) noexcept
{
    return {};
}
error_type dcl_client_application::exec(const dcl_event_type::msg_project_create&) noexcept
{
    return {};
}
error_type dcl_client_application::exec(const dcl_event_type::msg_project_save&) noexcept
{
    return {};
}

int main()
{
    heap gheap;
    if (const auto error = gheap.initialize(heap_init::global(gheap_capacity)))
        return error.code;

    const auto show_error = [](const error_type error)
    {
        string msg;
        to_string("Error (%1) code [%2]: %3", msg, error.source, long(error.code), error);
        win32_message(msg, "Error"_s);
        return error.code;
    };

    error_type error;
    {
        dcl_client_application app;
        if (!error) error = app.init();
        if (!error) error = app.run();
    }
    if (error)
        return show_error(error);
    return 0;
}
extern icy::string_view to_string(const text text) noexcept
{
    return to_string_enUS(text);
}
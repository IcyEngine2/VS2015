#include "icy_dcl_client_text.hpp"
#include "icy_dcl_client_menu_bar.hpp"
#include "icy_dcl_client_tools.hpp"
#include <icy_engine/core/icy_core.hpp>
#include <icy_engine/core/icy_string.hpp>
#include <icy_engine/core/icy_thread.hpp>
#include <icy_engine/utility/icy_database.hpp>
#include <icy_engine/network/icy_network_http.hpp>
#include <icy_auth/icy_auth.hpp>
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

class dcl_client_model : public gui_model
{
public:

private:

};
class dcl_client_application
{
public:
    error_type init() noexcept;
    error_type run() noexcept;
    error_type exec(const event event) noexcept;
private:
    void project_close() noexcept;
    error_type system_open(const string_view path) noexcept;
    error_type project_open(const string_view path) noexcept;
private:
    dcl_client_main_thread m_main = *this;
    shared_ptr<gui_queue> m_gui;
    gui_widget m_window;
    gui_widget m_error;
    dcl_client_menu_bar m_menu_bar;
    dcl_client_state m_state;
    crypto_msg<auth_client_ticket_server> m_ticket;
    database_system_read m_dbase_system;
    database_system_write m_dbase_user;
    gui_widget m_view;
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

error_type dcl_client_application::init() noexcept
{
    ICY_ERROR(gui_queue::create(m_gui));
    ICY_ERROR(m_gui->create(m_window, gui_widget_type::window, gui_widget{}));
    ICY_ERROR(m_menu_bar.init(*m_gui, m_window));

    gui_action action_ok;
    ICY_ERROR(m_gui->create(m_error, gui_widget_type::message, m_window));
    ICY_ERROR(m_gui->create(action_ok, to_string(text::ok)));
    ICY_ERROR(m_gui->insert(m_error, action_ok));
    ICY_ERROR(m_gui->text(m_window, default_window_text));
       
    ICY_ERROR(m_gui->show(m_window, true));    
    return {};
}
error_type dcl_client_application::run() noexcept
{
    ICY_SCOPE_EXIT{ event::post(nullptr, event_type::global_quit); };
    ICY_ERROR(m_main.launch());    
    return m_gui->loop();
}
error_type dcl_client_application::exec(const event event) noexcept
{
    const auto show_error = [this](const string_view message, const error_type error) -> error_type
    {
        if (error)
        {
            string text;
            ICY_ERROR(to_string("%1 - Source (%2) [code %3]: %4", text, message, error.source, long(error.code), error));
            ICY_ERROR(m_gui->text(m_error, text));
            ICY_ERROR(m_gui->show(m_error, true));
        }
        return {};
    };

    ICY_ERROR(m_menu_bar.exec(*m_gui, event));

    if (event->type == event_type::window_close && event->data<gui_event>().widget == m_window)
    {
        ICY_ERROR(event::post(nullptr, event_type::global_quit));
    }
    else if (event->type == dcl_event_type::project_request_close)
    {
        project_close();
        ICY_ERROR(m_gui->text(m_window, default_window_text));
        ICY_ERROR(event::post(nullptr, dcl_event_type::project_closed));
    }
    else if (
        event->type == dcl_event_type::project_request_open ||
        event->type == dcl_event_type::project_request_create)
    {
        const auto msg = event->type == dcl_event_type::project_request_open ?
            text::error_open_project : text::error_create_project;
        const string_view file = event->data<dcl_event_type::msg_project_request_open>().filename;
        if (const auto error = project_open(file))
        {
            ICY_ERROR(show_error(to_string(msg), error));
        }
        else
        {
            string window_text;
            ICY_ERROR(to_string("%1: \"%2\""_s, window_text, default_window_text, file));
            ICY_ERROR(m_gui->text(m_window, window_text));
            ICY_ERROR(event::post(nullptr, dcl_event_type::project_opened));
        }
    }
    return {};
}
void dcl_client_application::project_close() noexcept
{
    m_dbase_user = {};
}
error_type dcl_client_application::system_open(const string_view path) noexcept
{
    return m_dbase_system.initialize(path, m_state.system_size);
}
error_type dcl_client_application::project_open(const string_view path) noexcept
{
    project_close();
    ICY_ERROR(m_dbase_user.initialize(path, m_state.user_size));
    return {};
}

uint32_t appmain()
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
    dcl_client_application app;
    if (!error) error = app.init();
    if (!error) error = app.run();
    if (error)
        return show_error(error);
    return 0;
}

int main()
{
    __try
    {
        return appmain();
    }
    __except (1)
    {
        return 1;
    }
    return 0;
}
extern icy::string_view to_string(const text text) noexcept
{
    return to_string_enUS(text);
}

uint32_t dcl_client_network_model::data(const gui_node node, const gui_role role, gui_variant& value) const noexcept
{
    string str;
    if (const auto error = data(node, role, str))
    {
        if (error.source == error_source_stdlib)
            return error.code;
        else
            return UINT32_MAX;
    }
    return make_variant(value, detail::global_heap.realloc, detail::global_heap.user, str).code;
}
error_type dcl_client_network_model::data(const gui_node node, const gui_role role, string& str) const noexcept
{
    auto error = 0u;
    if (role == gui_role::view && node.row < m_data.size())
    {
        switch (node.col)
        {
        case col::time:
            return to_string(m_data[node.row].time, str);

        case col::message:
            return to_string(m_data[node.row].message, str);

        case col::error:
            return to_string(m_data[node.row].error, str);
        }
    }
    return make_stdlib_error(std::errc::invalid_argument);
}
uint32_t dcl_client_upload_model::data(const gui_node node, const gui_role role, gui_variant& value) const noexcept
{
    if (node.row < m_data.size() && node.col == 0)
    {
        auto& data = m_data[node.row];
        if (role == gui_role::view)
        {
            return value.initialize(detail::global_heap.realloc, detail::global_heap.user, 
                data.name.bytes().data(), data.name.bytes().size(), gui_variant_type::lstring);
        }
        else if (role == gui_role::check)
        {
            value = uint64_t(data.check);
            return 0;
        }        
    }
    return uint32_t(std::errc::invalid_argument);
}

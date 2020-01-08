#define ICY_QTGUI_STATIC 1
#include <icy_engine/core/icy_string.hpp>
#include <icy_engine/core/icy_array.hpp>
#include <icy_engine/core/icy_thread.hpp>
#include <icy_engine/graphics/icy_window.hpp>
#include <icy_engine/graphics/icy_graphics.hpp>
#include <icy_engine/network/icy_network.hpp>
#include <icy_qtgui/icy_qtgui.hpp>
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
#include "../server/icy_mbox_server_input_log.hpp"

using namespace icy;

uint32_t show_error(const error_type error, const string_view text) noexcept;

class mbox_application;
class mbox_logic_thread : public thread
{
public:
    mbox_logic_thread(mbox_application& app) noexcept : m_app(app)
    {

    }
    error_type run() noexcept override;
private:
    mbox_application& m_app;
};
class mbox_window_thread : public thread
{
public:
    mbox_window_thread(mbox_application& app) noexcept : m_app(app)
    {

    }
    error_type run() noexcept override;
private:
    mbox_application& m_app;
};

class mbox_application
{
public:
    error_type init() noexcept;
    error_type exec() noexcept;
    error_type window_run() noexcept;
    error_type logic_run() noexcept;
private:
    shared_ptr<gui_queue> m_gui;
    mbox_window_thread m_window = *this;
    mbox_logic_thread m_logic = *this;
};

error_type mbox_window_thread::run() noexcept
{
    return m_app.window_run();
}
error_type mbox_logic_thread::run() noexcept
{
    return m_app.logic_run();
}

uint32_t show_error(const error_type error, const string_view text) noexcept
{
    string msg;
    to_string("Error: %4 - %1 code [%2]: %3", msg, error.source, long(error.code), error, text);
    win32_message(msg, "Error"_s);
    return error.code;
}

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


error_type mbox_application::init() noexcept
{
    ICY_ERROR(create_gui(m_gui));
    return {};
}
error_type mbox_application::exec() noexcept
{
    {
        ICY_SCOPE_EXIT{ event::post(nullptr, event_type::global_quit); };
        ICY_ERROR(m_window.launch());
        ICY_ERROR(m_logic.launch());
        ICY_ERROR(m_gui->loop());
    }
    const auto e0 = m_window.wait();
    const auto e1 = m_logic.wait();
    ICY_ERROR(e0);
    ICY_ERROR(e1);
    return {};
}
error_type mbox_application::window_run() noexcept
{
    ICY_SCOPE_EXIT{ event::post(nullptr, event_type::global_quit); };
    shared_ptr<window> window;
    array<adapter> adapters;
    shared_ptr<display> display;

    ICY_ERROR(window::create(window, window_flags::quit_on_close));
    ICY_ERROR(window->rename("Icy MBox Test v1.0"_s));

    ICY_ERROR(adapter::enumerate(adapter_flag::d3d11, adapters));
    if (adapters.empty())
        return make_unexpected_error();
    ICY_ERROR(display::create(display, adapters[0]));
    ICY_ERROR(display->bind(*window));
    ICY_ERROR(window->restyle(window_style::windowed));
    ICY_ERROR(window->loop());
    return {};
}

error_type mbox_application::logic_run() noexcept
{
    ICY_SCOPE_EXIT{ event::post(nullptr, event_type::global_quit); };

    shared_ptr<event_loop> loop;
    ICY_ERROR(event_loop::create(loop, event_type::window_input));

    mbox::library lib;
    mbox_input_log log;
    ICY_ERROR(log.initialize(*m_gui, lib));

    while (true)
    {
        event event;
        ICY_ERROR(loop->loop(event));
        if (event->type == event_type::global_quit)
            break;

        if (event->type == event_type::window_input)
            ICY_ERROR(log.append(guid(), event->data<input_message>()));
        //else if (event->type == event_type::window_close)

    }
    return {};
}
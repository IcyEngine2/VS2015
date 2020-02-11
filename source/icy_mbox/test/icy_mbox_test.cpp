#define ICY_QTGUI_STATIC 1
#include <icy_engine/core/icy_string.hpp>
#include <icy_engine/core/icy_array.hpp>
#include <icy_engine/core/icy_thread.hpp>
#include <icy_engine/graphics/icy_graphics.hpp>
#include <icy_engine/network/icy_network.hpp>
#include <icy_qtgui/icy_xqtgui.hpp>
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
    enum col_type
    {
        col_time,
        col_type,
        col_button,
        col_args,
        _col_count,
    };
public:
    error_type init() noexcept;
    error_type exec() noexcept;
    error_type window_run() noexcept;
    error_type logic_run() noexcept;
private:
    shared_ptr<gui_queue> m_gui;
    mbox_window_thread m_thread_window = *this;
    mbox_logic_thread m_thread_logic = *this;
    shared_ptr<window> m_window;
    xgui_widget m_xwindow;
    xgui_widget m_splitter;
    xgui_model m_input_model;
    xgui_widget m_input_view;
    xgui_widget m_hwnd_view;
    size_t m_rows = 0;
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

gui_queue* icy::global_gui = nullptr;

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
    global_gui = m_gui.get();

    ICY_ERROR(m_input_model.initialize());
    ICY_ERROR(m_input_model.insert_cols(0, _col_count));
    ICY_ERROR(m_input_model.hheader(col_time, "Time"_s));
    ICY_ERROR(m_input_model.hheader(col_type, "Type"_s));
    ICY_ERROR(m_input_model.hheader(col_button, "Button"_s));
    ICY_ERROR(m_input_model.hheader(col_args, "Args"_s));
    
    array<adapter> adapters;
    ICY_ERROR(adapter::enumerate(adapter_flag::d3d11, adapters));
    if (adapters.empty())
        return make_unexpected_error();

    ICY_ERROR(make_window(m_window, adapters[0]));
    ICY_ERROR(m_window->rename("Icy MBox Test v1.0"_s));
    ICY_ERROR(m_window->restyle(window_style::windowed));
    
    return {};
}
error_type mbox_application::exec() noexcept
{
    {
        ICY_SCOPE_EXIT{ event::post(nullptr, event_type::global_quit); };
        ICY_ERROR(m_thread_logic.launch());
        ICY_ERROR(m_thread_window.launch());
        ICY_ERROR(m_gui->loop());
    }
    //m_window->_close();
    const auto e0 = m_thread_window.wait();
    const auto e1 = m_thread_logic.wait();
    ICY_ERROR(e0);
    ICY_ERROR(e1);
    return {};
}
error_type mbox_application::window_run() noexcept
{
    ICY_SCOPE_EXIT{ event::post(nullptr, event_type::global_quit); };
    ICY_ERROR(m_window->initialize());
    ICY_ERROR(m_hwnd_view.initialize(m_window->handle(), m_splitter));
    while (true)
    {
        event event;
        if (const auto error = m_window->loop(event, std::chrono::milliseconds(16)))
        {
            if (error == make_stdlib_error(std::errc::timed_out))
                continue;
        }
        else if (event->type == event_type::global_quit)
            break;
    }

    return {};
}

error_type mbox_application::logic_run() noexcept
{
    ICY_SCOPE_EXIT{ event::post(nullptr, event_type::global_quit); };

    shared_ptr<event_loop> loop;
    ICY_ERROR(event_loop::create(loop, event_type::window_input | event_type::window_close));

    ICY_ERROR(m_xwindow.initialize(gui_widget_type::window, {}, gui_widget_flag::layout_hbox));
    ICY_ERROR(m_splitter.initialize(gui_widget_type::hsplitter, m_xwindow));

    ICY_ERROR(m_input_view.initialize(gui_widget_type::grid_view, m_splitter));
    ICY_ERROR(m_input_view.bind(m_input_model));
    ICY_ERROR(m_xwindow.show(true));

    while (true)
    {
        event event;
        ICY_ERROR(loop->loop(event));
        if (event->type == event_type::global_quit)
            break;
        if (event->type == event_type::window_close)
            break;

        if (event->type == event_type::window_input &&  
            shared_ptr<event_queue>(event->source).get() == m_gui.get())
        {
            auto& event_data = event->data<gui_event>();
            const auto input = event_data.data.as_input();
            //const auto input = event->data<input_message>();
            //
            if (input.type == input_type::mouse_move)
                continue;
            
            ICY_ERROR(m_input_model.insert_rows(m_rows, 1));
            
            string time_str;
            ICY_ERROR(to_string(clock_type::now(), time_str));
            
            string key_str;
            ICY_ERROR(to_string(input, key_str));

            string args_str;
            if (input.type == input_type::active)
            {
                ICY_ERROR(to_string(input.active ? "Active"_s : "Inactive"_s, args_str));
            }
            else if (input.type == input_type::text)
            {
                ICY_ERROR(to_string(const_array_view<wchar_t>(input.text, wcsnlen(input.text, 2)), args_str));
            }
            else if (false ||
                input.type == input_type::key_press ||
                input.type == input_type::key_release ||
                input.type == input_type::key_hold)
            {
                ;
            }
            else if (input.type == input_type::mouse_wheel)
            {
                ICY_ERROR(args_str.appendf("(%1, %2) Z %3"_s,
                    int64_t(input.point_x),
                    int64_t(input.point_y),
                    int64_t(input.wheel)));
            }
            else
            {
                ICY_ERROR(args_str.appendf("(%1, %2)"_s,
                    int64_t(input.point_x),
                    int64_t(input.point_y)));
            }
            ICY_ERROR(m_input_model.node(m_rows, col_time).text(time_str));
            ICY_ERROR(m_input_model.node(m_rows, col_type).text(to_string(input.type)));
            ICY_ERROR(m_input_model.node(m_rows, col_button).text(key_str));
            ICY_ERROR(m_input_model.node(m_rows, col_args).text(args_str));
            ICY_ERROR(m_input_view.scroll(m_input_model.node(m_rows, 0)));
            m_rows++;
        }
    }
    return {};
}
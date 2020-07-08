#include <icy_qtgui/icy_xqtgui.hpp>
#include <icy_engine/core/icy_thread.hpp>
#if _DEBUG
#pragma comment(lib, "icy_engine_cored")
#pragma comment(lib, "icy_qtguid")
#else
#pragma comment(lib, "icy_engine_core")
#pragma comment(lib, "icy_qtgui")
#endif

using namespace icy;

std::atomic<gui_queue*> icy::global_gui = nullptr;

class application : public thread
{
public:
    error_type exec() noexcept;
private:
    error_type run() noexcept override;
private:
    shared_ptr<gui_queue> m_gui_system;
    gui_widget m_window;
};
error_type application::exec() noexcept
{
    ICY_ERROR(create_event_system(m_gui_system));
    const auto shutdown = [this]
    {
        global_gui = nullptr;
        event::post(nullptr, event_type::global_quit);
        return wait();
    };
    ICY_SCOPE_EXIT{ shutdown(); };
    global_gui = m_gui_system.get();
    ICY_ERROR(launch());
    ICY_ERROR(m_gui_system->exec());
    return shutdown();
}
error_type application::run() noexcept
{
    ICY_SCOPE_EXIT{ event::post(nullptr, event_type::global_quit); };
    ICY_ERROR(m_gui_system->create(m_window, gui_widget_type::window, {}, gui_widget_flag::layout_hbox));
    ICY_ERROR(m_gui_system->show(m_window, true));
    xgui_widget splitter;
    ICY_ERROR(splitter.initialize(gui_widget_type::hsplitter, m_window, gui_widget_flag::auto_insert));

    xgui_widget lhs;
    xgui_widget rhs;
    lhs.initialize(gui_widget_type::line_edit, splitter);
    rhs.initialize(gui_widget_type::line_edit, splitter);

    shared_ptr<event_queue> queue;
    ICY_ERROR(create_event_system(queue, event_type::gui_any | event_type::window_close));
    while (true)
    {
        event event;
        ICY_ERROR(queue->pop(event));
        if (event->type == event_type::global_quit)
            break;
        if (event->type == event_type::window_close)
            break;
    }
    return error_type();
}

int main()
{
    heap gheap;
    if (const auto error = gheap.initialize(heap_init::global(16_gb)))
        return error.code;

    application app;
    if (const auto error = app.exec())
    {
        show_error(error, "App exec"_s);
        return error.code;
    }
    return 0;
}
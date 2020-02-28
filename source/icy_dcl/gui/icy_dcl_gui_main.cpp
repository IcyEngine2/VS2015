#include <icy_qtgui/icy_xqtgui.hpp>
#include <icy_engine/core/icy_thread.hpp>
#include "../include/icy_dcl_system.hpp"

#pragma comment(lib, "icy_engine_cored")
#pragma comment(lib, "icy_qtguid")
#pragma comment(lib, "icy_dcl_cored")

using namespace icy;

class application : public thread
{
public:
    error_type init() noexcept;
    error_type exec() noexcept;
private:
    error_type run() noexcept override;
private:
    heap m_heap;
    shared_ptr<gui_queue> m_gui_system;
    shared_ptr<dcl_system> m_dcl_system;
    gui_widget m_window;
};

ICY_DECLARE_GLOBAL(icy::global_gui);

error_type application::init() noexcept
{
    ICY_ERROR(m_heap.initialize(heap_init::global(1_gb)));
    ICY_ERROR(create_gui(m_gui_system));
    icy::global_gui = m_gui_system.get();
    ICY_ERROR(create_dcl_system(m_dcl_system));

    ICY_ERROR(m_gui_system->create(m_window, gui_widget_type::window, {}));

    return {};
}
error_type application::run() noexcept
{
    ICY_SCOPE_EXIT{ event::post(nullptr, event_type::global_quit); };

    shared_ptr<event_queue> queue;
    ICY_ERROR(create_event_queue(queue, dcl_event_type | event_type::gui_any | event_type::window_close));
    ICY_ERROR(m_gui_system->show(m_window, true));

    while (true)
    {
        event event;
        ICY_ERROR(queue->pop(event));
        if (event->type == event_type::global_quit)
            break;
        else if (event->type == dcl_event_type)
        {

        }
        else
        {
            const auto& event_data = event->data<gui_event>();
            if (event->type == event_type::window_close)
            {
                if (event_data.widget == m_window)
                    break;
            }
            else
            {

            }
        }
    }
    return {};
}
error_type application::exec() noexcept
{
    ICY_SCOPE_EXIT{ wait(); };
    ICY_ERROR(launch());
    ICY_ERROR(m_gui_system->exec());
    return {};

}

int main()
{
    application app;
    if (const auto error = app.init())
        return error.code;
    if (const auto error = app.exec())
        return error.code;
    return 0;
}
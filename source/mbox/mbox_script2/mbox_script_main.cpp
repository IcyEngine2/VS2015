#include "mbox_script_dbase.hpp"
#include <icy_engine/core/icy_thread.hpp>
#include <icy_engine/core/icy_file.hpp>
#include <icy_engine/image/icy_image.hpp>
#include <icy_qtgui/icy_xqtgui.hpp>
#include <icy_engine\utility\icy_text_edit.hpp>
#if _DEBUG
#pragma comment(lib, "icy_engine_cored")
#pragma comment(lib, "icy_engine_imaged")
#if ICY_QTGUI_STATIC
#pragma comment(lib, "icy_qtguid")
#endif
#else
#pragma comment(lib, "icy_engine_core")
#pragma comment(lib, "icy_engine_image")
#if ICY_QTGUI_STATIC
#pragma comment(lib, "icy_qtgui")
#endif
#endif


using namespace icy;

weak_ptr<gui_queue> icy::global_gui;

class application : public thread
{
public:
    shared_ptr<gui_queue> gui;
private:
    error_type run() noexcept override;
};
error_type application::run() noexcept
{
    ICY_SCOPE_EXIT{ event::post(nullptr, event_type::global_quit); };

    shared_ptr<text_edit_system> text_system;
    ICY_ERROR(create_event_system(text_system));
    ICY_ERROR(text_system->thread().launch());
    ICY_ERROR(text_system->thread().rename("Text Edit Thread"_s));

    shared_ptr<event_queue> queue;
    ICY_ERROR(create_event_system(queue, event_type::gui_any | text_edit_event_type));

    xgui_widget window;
    ICY_ERROR(window.initialize(gui_widget_type::window, gui_widget(), gui_widget_flag::layout_hbox));

    mbox::mbox_database dbase(*text_system, window);
    ICY_ERROR(dbase.initialize());
    
    ICY_ERROR(window.show(true));
    while (true)
    {
        event event;
        ICY_ERROR(queue->pop(event));
        if (event->type == event_type::global_quit)
            break;
    
        if (event->type & event_type::gui_any)
        {
            const auto& event_data = event->data<gui_event>();
            if (event_data.widget == window)
            {
                auto cancel = false;
                ICY_ERROR(dbase.close(cancel));
                if (cancel)
                    continue;
                break;
            }
            if (const auto error = dbase.exec(event->type, event_data))
                ICY_ERROR(show_error(error, ""_s));
        }
        else if (event->type == text_edit_event_type)
        {
            if (const auto error = dbase.exec(event->data<text_edit_event>()))
                ICY_ERROR(show_error(error, ""_s));
            
        }
    }
    return error_type();
}

error_type main_func(heap& gheap) noexcept
{
    ICY_SCOPE_EXIT{ event::post(nullptr, event_type::global_quit); };

    shared_ptr<application> app;
    ICY_ERROR(make_shared(app));
    ICY_ERROR(create_event_system(app->gui));
    global_gui = app->gui;
    ICY_SCOPE_EXIT{ global_gui = nullptr; };
    ICY_ERROR(app->launch());
    ICY_ERROR(app->rename("Application thread"_s));
    const auto error0 = app->gui->exec();
    const auto error1 = event::post(nullptr, event_type::global_quit);
    const auto error2 = app->wait();
    ICY_ERROR(error0);
    ICY_ERROR(error1);
    ICY_ERROR(error2);
    return error_type();
}

int main()
{
    heap gheap;
    auto gheap_init = heap_init::global(2_gb);
    //gheap_init.debug_trace = true;
    if (gheap.initialize(gheap_init))
        return -1;

    const auto error = main_func(gheap);
    if (error)
        show_error(error, ""_s);
    return error.code;
}
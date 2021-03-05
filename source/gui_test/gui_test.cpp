#include <icy_gui/icy_gui.hpp>
#include <icy_engine/core/icy_thread.hpp>
#include <icy_engine/core/icy_file.hpp>
#include <icy_engine/graphics/icy_adapter.hpp>
#include <icy_engine/graphics/icy_display.hpp>
#include <icy_engine/graphics/icy_window.hpp>
#pragma comment(lib, "icy_engine_cored")
#pragma comment(lib, "icy_engine_graphicsd")
#pragma comment(lib, "icy_guid")

using namespace icy;

error_type main_func()
{
    array<adapter> gpu;
    ICY_ERROR(adapter::enumerate(adapter_flags::d3d11 | adapter_flags::hardware | adapter_flags::debug, gpu));
    if (gpu.empty())
        return error_type();

    const auto adapter = gpu[0];

    shared_ptr<window_system> window_system;
    ICY_ERROR(create_window_system(window_system));
    ICY_ERROR(window_system->thread().launch());
    ICY_ERROR(window_system->thread().rename("Window Thread"_s));

    shared_ptr<window> window;
    ICY_ERROR(window_system->create(window));
    ICY_ERROR(window->show(true));
    ICY_ERROR(window->restyle(window_style::windowed));

    shared_ptr<display_system> display_system;
    ICY_ERROR(create_display_system(display_system, window->handle(), adapter));
    ICY_ERROR(display_system->thread().launch());
    ICY_ERROR(display_system->thread().rename("Display Thread"_s));
    ICY_ERROR(display_system->frame(std::chrono::milliseconds(200)));
        
    shared_ptr<gui_system> gui_system;
    ICY_ERROR(create_gui_system(gui_system, adapter));
    ICY_ERROR(gui_system->thread().launch());
    ICY_ERROR(gui_system->thread().rename("GUI Thread"_s));
    
    shared_ptr<gui_window> gui_window;
    ICY_ERROR(gui_system->create_window(gui_window, window));
    
    array<char> bytes;
    {
        icy::file f1;
        f1.open("D:/VS2015/dat/html_example.html"_s, file_access::read, file_open::open_existing, file_share::read);
        auto size = f1.info().size;

        bytes.resize(size);
        f1.read(bytes.data(), size);
    }

    gui_layout layout;
    layout.initialize(string_view(bytes.data(), bytes.size()));

    gui_window->layout(gui_widget(), layout);


    shared_ptr<event_queue> loop;
    ICY_ERROR(create_event_system(loop, event_type::global_quit 
        | event_type::gui_render
        | event_type::display_update 
        | event_type::window_resize 
        | event_type::system_error));

    auto query = 0u;
    gui_window->render(gui_widget(), query);

    while (true)
    {
        event event;
        ICY_ERROR(loop->pop(event));
        if (event->type == event_type::global_quit)
            break;
        
        //ICY_ERROR(gui->process(event));
        if (event->type == event_type::gui_render)
        {
            const auto& event_data = event->data<gui_event>();
            if (event_data.texture && event_data.query == query)
            {
                ICY_ERROR(display_system->repaint(*event_data.texture, window_size(0, 0),
                    event_data.texture->size()));
            }
        }
        else if (event->type == event_type::window_resize)
        {
            const auto& event_data = event->data<window_message>();
            ICY_ERROR(display_system->resize(event_data.size));
            ICY_ERROR(gui_window->resize(event_data.size));
            ICY_ERROR(gui_window->render(gui_widget(), query));
        }
        else if (event->type == event_type::display_update)
        {
            const auto event_data = event->data<display_message>();
            const auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(event_data.frame);
            string new_name;
            ICY_ERROR(to_string("%1ms [%2]"_s, new_name, ms.count(), event_data.index));
            ICY_ERROR(window->rename(new_name));
        }
        else if (event->type == event_type::system_error)
        {
            return event->data<error_type>();
        }
    }
    
    return error_type();
}

int main()
{
    icy::heap gheap;
    gheap.initialize(heap_init::global(2_gb));
    return main_func().code;
}
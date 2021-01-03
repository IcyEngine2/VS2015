#include <icy_gui\icy_gui.hpp>
#include <icy_engine\core\icy_thread.hpp>
#include <icy_engine\core\icy_string.hpp>
#include <icy_engine\graphics\icy_window.hpp>
#include <icy_engine\graphics\icy_adapter.hpp>
#include <icy_engine\graphics\icy_display.hpp>
#pragma comment(lib, "icy_engine_cored")
#pragma comment(lib, "icy_engine_graphicsd")
#pragma comment(lib, "icy_guid")

using namespace icy;

error_type main_func()
{
    shared_ptr<window_system> ws;
    ICY_ERROR(create_window_system(ws));
    ICY_ERROR(ws->thread().launch());

    array<adapter> gpu;
    ICY_ERROR(adapter::enumerate(adapter_flags::d3d11 | adapter_flags::hardware | adapter_flags::debug, gpu));
    if (gpu.empty())
        return error_type();

    window window;
    ICY_ERROR(ws->create(window));
    ICY_ERROR(window.show(true));

    shared_ptr<gui_system> gui;
    ICY_ERROR(create_gui_system(gui, render_flags::msaa_hardware, gpu[0], window.handle()));
    
    shared_ptr<event_queue> loop;
    ICY_ERROR(create_event_system(loop, 
        event_type::gui_update | 
        event_type::display_update |
        event_type::window_input |
        event_type::window_resize | 
        event_type::system_error));

    auto main_window  = 0u;
    auto child_window = 0u;
    auto action = gui->action();
    ICY_ERROR(action.modify(0, gui_widget_prop::bkcolor, gui_variant(colors::cyan)));

    ICY_ERROR(action.create(gui_widget_type::window, main_window));
    ICY_ERROR(action.modify(main_window, gui_widget_prop::size_width, "10%"_s));
    ICY_ERROR(action.modify(main_window, gui_widget_prop::size_height, "250"_s));
    ICY_ERROR(action.modify(main_window, gui_widget_prop::bkcolor, gui_variant(colors::orange)));
    ICY_ERROR(action.modify(main_window, gui_widget_prop::offset_x, "20%"_s));
    ICY_ERROR(action.modify(main_window, gui_widget_prop::offset_y, "125"_s));
    
    /*ICY_ERROR(action.create(gui_widget_type::window, child_window));
    ICY_ERROR(action.modify(child_window, gui_widget_prop::size_width, "50%"_s));
    ICY_ERROR(action.modify(child_window, gui_widget_prop::size_height, "50%"_s));
    ICY_ERROR(action.modify(child_window, gui_widget_prop::bkcolor, gui_variant(colors::medium_purple)));
    ICY_ERROR(action.modify(child_window, gui_widget_prop::offset_x, "20%"_s));
    ICY_ERROR(action.modify(child_window, gui_widget_prop::offset_y, "20%"_s));
    ICY_ERROR(action.modify(child_window, gui_widget_prop::parent, main_window));*/

    ICY_ERROR(action.exec());

    shared_ptr<texture_system> tex_system;
    ICY_ERROR(create_texture_system(gpu[0], tex_system));
    shared_ptr<render_texture> tex;
    ICY_ERROR(tex_system->create(window_size(500, 500), render_flags::msaa_hardware, tex));

    shared_ptr<display_system> display;
    ICY_ERROR(create_display_system(display, window.handle(), gpu[0]));
    ICY_ERROR(display->thread().launch());
    ICY_ERROR(display->frame(std::chrono::milliseconds(200)));
    
    while (true)
    {
        event event;
        ICY_ERROR(loop->pop(event));
        if (event->type == event_type::global_quit)
            break;
        
        //ICY_ERROR(gui->process(event));
        if (event->type == event_type::gui_update)
        {
            ICY_ERROR(gui->render(*tex));
            ICY_ERROR(display->repaint(*tex));
        }
        else if (event->type == event_type::window_resize)
        {
            const auto& event_data = event->data<window_message>();
            ICY_ERROR(display->resize(event_data.size));
        }
        else if (event->type == event_type::display_update)
        {
            const auto new_frame = event->data<display_event>();
            const auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(new_frame.time);
            string new_name;
            ICY_ERROR(to_string("%1ms [%2]"_s, new_name, ms.count(), new_frame.index));
            ICY_ERROR(window.rename(new_name));
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
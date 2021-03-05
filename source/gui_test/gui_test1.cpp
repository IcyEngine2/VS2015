#include <icy_gui\icy_gui.hpp>
#include <icy_engine\core\icy_thread.hpp>
#include <icy_engine\core\icy_string.hpp>
#include <icy_engine\graphics\icy_window.hpp>
#include <icy_engine\graphics\icy_adapter.hpp>
#include <icy_engine\graphics\icy_display.hpp>
#include <icy_engine\graphics\icy_render.hpp>
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
    ICY_ERROR(create_gui_system(gui_system, adapter, window));
    ICY_ERROR(gui_system->thread().launch());
    ICY_ERROR(gui_system->thread().rename("GUI Thread"_s));
    
    shared_ptr<event_queue> loop;
    ICY_ERROR(create_event_system(loop, event_type::global_quit 
        | event_type::gui_render
        | event_type::display_update 
        | event_type::window_resize 
        | event_type::system_error));

    shared_ptr<gui_model> model;
    ICY_ERROR(gui_system->create_model(model));

    shared_ptr<gui_request> request;
    ICY_ERROR(gui_system->create_request(request));
    ICY_ERROR(request->modify_widget(0, gui_widget_prop::bkcolor, colors::cyan));

    auto style0 = 0u;
    ICY_ERROR(request->create_style(style0));
    ICY_ERROR(request->modify_style(style0, gui_style_prop::cursor, window_cursor::type::hand));

    for (auto k = 0u; k < 3u; ++k)
    {
        auto main_window = 0u;
        ICY_ERROR(request->create_widget(main_window, gui_widget_type::text_edit, k));
        ICY_ERROR(request->modify_widget(main_window, gui_widget_prop::size_x, "55mm"_s));
        ICY_ERROR(request->modify_widget(main_window, gui_widget_prop::size_y, "125"_s));
        ICY_ERROR(request->modify_widget(main_window, gui_widget_prop::bkcolor, k == 0 ?
            colors::orange : k == 1 ? colors::red : colors::purple));
        ICY_ERROR(request->modify_widget(main_window, gui_widget_prop::offset_x, k * 50 + 15));
        ICY_ERROR(request->modify_widget(main_window, gui_widget_prop::offset_y, k * 50 + 15));
        
       // ICY_ERROR(request->modify_widget(main_window, gui_widget_prop::layer, k + 1));
    }
    ICY_ERROR(request->modify_widget(2, gui_widget_prop::style_hovered, style0));

    ICY_ERROR(request->bind(3, model->root()));
    ICY_ERROR(request->modify_node(*model->root(), gui_node_prop::data,
        "This is a very long phrase that we use to test texts/models etc 123456790-="_s));


    /*ICY_ERROR(action.create(gui_widget_type::window, child_window));
    ICY_ERROR(action.modify(child_window, gui_widget_prop::size_width, "50%"_s));
    ICY_ERROR(action.modify(child_window, gui_widget_prop::size_height, "50%"_s));
    ICY_ERROR(action.modify(child_window, gui_widget_prop::bkcolor, gui_variant(colors::medium_purple)));
    ICY_ERROR(action.modify(child_window, gui_widget_prop::offset_x, "20%"_s));
    ICY_ERROR(action.modify(child_window, gui_widget_prop::offset_y, "20%"_s));
    ICY_ERROR(action.modify(child_window, gui_widget_prop::parent, main_window));*/

    auto gui_action_render = 0u;
    ICY_ERROR(request->render(gui_action_render, window_size(1000, 700), render_flags::none));
    ICY_ERROR(request->exec());
    
    
    while (true)
    {
        event event;
        ICY_ERROR(loop->pop(event));
        if (event->type == event_type::global_quit)
            break;
        
        //ICY_ERROR(gui->process(event));
        if (event->type == event_type::gui_render)
        {
            const auto& event_data = event->data<gui_message>();
            ICY_ERROR(display_system->repaint(*event_data.texture, window_size(0, 0), event_data.texture->size()));
        }
        else if (event->type == event_type::window_resize)
        {
            const auto& event_data = event->data<window_message>();
            ICY_ERROR(display_system->resize(event_data.size));
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
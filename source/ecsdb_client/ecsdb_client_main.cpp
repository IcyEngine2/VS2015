#include <icy_engine/core/icy_core.hpp>
#include <icy_engine/core/icy_thread.hpp>
#include <icy_engine/graphics/icy_window.hpp>
#include <icy_engine/graphics/icy_adapter.hpp>
#include <icy_engine/graphics/icy_display.hpp>
#include "ecsdb_client_gui.hpp"
#if _DEBUG
#pragma comment(lib, "icy_engine_cored")
#pragma comment(lib, "icy_engine_graphicsd")
#else
#pragma comment(lib, "icy_engine_core")
#pragma comment(lib, "icy_engine_graphics")
#endif

using namespace icy;


error_type main_ex(heap& heap)
{
    ICY_ERROR(event_system::initialize());
    ICY_ERROR(blob_initialize());
    ICY_SCOPE_EXIT{ blob_shutdown(); };

    shared_ptr<event_queue> loop;
    ICY_ERROR(create_event_system(loop, 0
        | event_type::gui_update
        | event_type::window_resize
        | event_type::global_timer
    ));

    shared_ptr<window_system> window_system;
    ICY_ERROR(create_window_system(window_system));
    ICY_ERROR(window_system->thread().launch());
    ICY_ERROR(window_system->thread().rename("Window Thread"_s))

    shared_ptr<window> window;
    ICY_ERROR(window_system->create(window));
    ICY_ERROR(window->restyle(window_style::windowed));

    shared_ptr<imgui_system> imgui_system;
    ICY_ERROR(create_imgui_system(imgui_system));
    ICY_ERROR(imgui_system->thread().launch());
    ICY_ERROR(imgui_system->thread().rename("ImGui Thread"_s));

    shared_ptr<imgui_display> imgui_display;
    ICY_ERROR(imgui_system->create_display(imgui_display, window));

    shared_ptr<ecsdb_gui_system> ecsdb_gui_system;
    ICY_ERROR(create_ecsdb_gui_system(ecsdb_gui_system, imgui_display));
    ICY_ERROR(ecsdb_gui_system->thread().launch());
    ICY_ERROR(ecsdb_gui_system->thread().rename("ECSDB GUI Thread"_s));

  /*  auto imgui_window = 0u;
    ICY_ERROR(imgui_display->widget_create(0u, imgui_widget_type::window, imgui_window));
    */
    auto query = 0u;
    ICY_ERROR(imgui_display->repaint(query));

    array<adapter> gpu;
    ICY_ERROR(adapter::enumerate(adapter_flags::d3d10 | adapter_flags::hardware, gpu));

    shared_ptr<display_system> display_system;
    ICY_ERROR(create_display_system(display_system, window, gpu[0]));
    ICY_ERROR(display_system->thread().launch());
    ICY_ERROR(display_system->thread().rename("Display Thread"_s));
    
    ICY_ERROR(window->show(true));

    render_gui_frame render;
    timer win_render_timer;
    timer gui_render_timer;
    ICY_ERROR(win_render_timer.initialize(SIZE_MAX, std::chrono::nanoseconds(1'000'000'000 / 60)));
    ICY_ERROR(gui_render_timer.initialize(SIZE_MAX, std::chrono::nanoseconds(1'000'000'000 / 60)));

    while (*loop)
    {
        event event;
        ICY_ERROR(loop->pop(event));
        if (!event)
            continue;

        if (event->type == event_type::gui_update)
        {
            auto& event_data = event->data<imgui_event>();
            if (event_data.type == imgui_event_type::display_render)
            {
                render = std::move(event_data.render);
            }
        }
        else if (event->type == event_type::window_resize)
        {
            const auto& event_data = event->data<window_message>();
            ICY_ERROR(display_system->resize(event_data.size));
        }
        else if (event->type == event_type::global_timer)
        {
            const auto pair = event->data<timer::pair>();
            if (pair.timer == &win_render_timer)
            {
                render_gui_frame copy_frame;
                ICY_ERROR(copy(render, copy_frame));
                ICY_ERROR(display_system->repaint("ImGui"_s, copy_frame));
            }
            else if (pair.timer == &gui_render_timer)
            {
                ICY_ERROR(imgui_display->repaint(query));
            }
        }
    }

    return error_type();
}

int main()
{
    heap gheap;
    if (gheap.initialize(heap_init::global(1_gb)))
        return ENOMEM;
    if (const auto error = main_ex(gheap))
        return error.code;
    return 0;
}

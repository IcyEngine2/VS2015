#include <icy_engine/core/icy_core.hpp>
#include <icy_engine/core/icy_thread.hpp>
#include <icy_engine/core/icy_file.hpp>
#include <icy_engine/graphics/icy_adapter.hpp>
#include <icy_engine/graphics/icy_display.hpp>
#include <icy_engine/graphics/icy_window.hpp>
#include <icy_dcl/icy_dcl.hpp>
#if _DEBUG
#pragma comment(lib, "icy_engine_graphicsd")
#pragma comment(lib, "icy_guid")
#pragma comment(lib, "icy_dcl_libd")
#else
#pragma comment(lib, "icy_engine_graphics")
#pragma comment(lib, "icy_gui")
#pragma comment(lib, "icy_dcl_lib")
#endif

using namespace icy;

error_type main_ex(heap& heap)
{
    ICY_ERROR(event_system::initialize());
    
    array<char> bytes;
    {
        file file;
        ICY_ERROR(file.open("dcl_server_gui.json"_s, file_access::read, file_open::open_existing, file_share::none));
        size_t size = 0;
        ICY_ERROR(bytes.resize(size = file.info().size));
        ICY_ERROR(file.read(bytes.data(), size));
    }

    array<adapter> gpu;
    ICY_ERROR(adapter::enumerate(adapter_flags::d3d11 | adapter_flags::hardware | adapter_flags::debug, gpu));
    if (gpu.empty())
        return error_type();

    const auto adapter = gpu[0];

    shared_ptr<window_system> window_system;
    ICY_ERROR(create_window_system(window_system));
    ICY_ERROR(window_system->thread().launch());
    ICY_ERROR(window_system->thread().rename("Window Thread"_s))

    shared_ptr<window> window;
    ICY_ERROR(window_system->create(window));
    ICY_ERROR(window->restyle(window_style::windowed));

    shared_ptr<display_system> display_system;
    ICY_ERROR(create_display_system(display_system, window, adapter));
    ICY_ERROR(display_system->thread().launch());
    ICY_ERROR(display_system->thread().rename("Display Thread"_s));
    ICY_ERROR(display_system->frame(display_frame_vsync));

    shared_ptr<gui_system> gui_system;
    ICY_ERROR(create_gui_system(gui_system, adapter));
    ICY_ERROR(gui_system->thread().launch());
    ICY_ERROR(gui_system->thread().rename("GUI Thread"_s));

    shared_ptr<dcl_system> dcl_system;
    ICY_ERROR(create_dcl_system(dcl_system, "dcl_1.dat"_s, 16_gb));
    
    shared_ptr<gui_window> gui_window;
    ICY_ERROR(gui_system->create_window(gui_window, window, string_view(bytes.data(), bytes.size())));

    array<gui_widget> widget_tabs;
    array<gui_widget> widget_tree;
    array<gui_widget> widget_info;
    ICY_ERROR(gui_window->find("class"_s, "dcl_tabs_model"_s, widget_tabs));
    ICY_ERROR(gui_window->find("class"_s, "dcl_tree_model"_s, widget_tree));
    ICY_ERROR(gui_window->find("class"_s, "dcl_info_model"_s, widget_info));

    shared_ptr<event_queue> loop;
    ICY_ERROR(create_event_system(loop, 0
        | event_type::gui_render
        | event_type::gui_update
        | event_type::display_update
        | event_type::window_resize
        | event_type::global_timer
    ));

    ICY_ERROR(window->show(true));

    auto query = 0u;
    ICY_ERROR(gui_window->render(gui_widget(), query));

    shared_ptr<gui_data_write_model> tabs_model;
    shared_ptr<gui_data_write_model> tree_model;
    shared_ptr<gui_data_write_model> info_model;

    ICY_ERROR(gui_system->create_model(tabs_model));
    ICY_ERROR(gui_system->create_model(tree_model));
    ICY_ERROR(gui_system->create_model(info_model));

    for (auto&& widget : widget_tabs)
    {
        unique_ptr<gui_data_bind> bind;
        ICY_ERROR(make_unique(gui_data_bind(), bind));
        ICY_ERROR(gui_system->create_bind(std::move(bind), *tabs_model, *gui_window, gui_node(), widget));
    }
    for (auto&& widget : widget_tree)
    {
        unique_ptr<gui_data_bind> bind;
        ICY_ERROR(make_unique(gui_data_bind(), bind));
        ICY_ERROR(gui_system->create_bind(std::move(bind), *tree_model, *gui_window, gui_node(), widget));
    }
    for (auto&& widget : widget_info)
    {
        unique_ptr<gui_data_bind> bind;
        ICY_ERROR(make_unique(gui_data_bind(), bind));
        ICY_ERROR(gui_system->create_bind(std::move(bind), *info_model, *gui_window, gui_node(), widget));
    }

    gui_node root_node;
    ICY_ERROR(tree_model->insert(gui_node(), 0, 0, root_node));

    shared_ptr<dcl_project> project;
    ICY_ERROR(dcl_system->add_project("test1"_s, project));
    ICY_ERROR(project->tree_view(*tree_model, root_node, dcl_index()));
    
    dcl_index index;
    ICY_ERROR(project->create_directory(dcl_index(), "Hello"_s, index));

    shared_ptr<texture> overlay;
    while (*loop)
    {
        event event;
        ICY_ERROR(loop->pop(event));
        if (!event)
            continue;
    
        if (event->type == event_type::gui_render)
        {
            const auto& event_data = event->data<gui_event>();
            if (event_data.texture)
            {
                overlay = event_data.texture;
                ICY_ERROR(display_system->repaint(*overlay, window_size(0, 0), overlay->size()));
            }
        }
        else if (event->type == event_type::gui_update)
        {
            ICY_ERROR(gui_window->render(gui_widget(), query));
        }
        else if (event->type == event_type::window_resize)
        {
            const auto& event_data = event->data<window_message>();
            ICY_ERROR(display_system->resize(event_data.size));
            ICY_ERROR(gui_window->resize(event_data.size));
            ICY_ERROR(gui_window->render(gui_widget(), query));
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

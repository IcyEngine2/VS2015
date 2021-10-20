#include <icy_engine/core/icy_core.hpp>
#include <icy_engine/core/icy_array.hpp>
#include <icy_engine/core/icy_string.hpp>
#include <icy_engine/core/icy_thread.hpp>
#include <icy_engine/graphics/icy_window.hpp>
#include <icy_engine/graphics/icy_gpu.hpp>
#include <icy_engine/graphics/icy_display.hpp>
#include <icy_engine/resource/icy_engine_resource.hpp>
#include <icy_engine/utility/icy_imgui.hpp>

using namespace icy;

#if _DEBUG
#pragma comment(lib, "icy_engine_cored")
#pragma comment(lib, "icy_engine_resourced")
#pragma comment(lib, "icy_engine_graphicsd")
#endif

class render_thread : public icy::thread
{
public:
    error_type run() noexcept override
    {
        shared_ptr<event_queue> loop;
        ICY_ERROR(create_event_system(loop, 0
            | event_type::gui_render
            | event_type::global_timer
            | event_type::resource_load
            | event_type::render_event
        ));

        render_gui_frame gui_render;
        timer win_render_timer;
        timer gui_render_timer;
        ICY_ERROR(win_render_timer.initialize(SIZE_MAX, std::chrono::nanoseconds(1'000'000'000 / 60)));
        ICY_ERROR(gui_render_timer.initialize(SIZE_MAX, std::chrono::nanoseconds(1'000'000'000 / 60)));

        map<guid, resource_data> list;
        auto resource = resource_system::global();
        resource->list(list);

        auto cur_count = 0u;
        auto max_count = 0u;
        render_scene scene;
        for (auto&& pair : list)
        {
            if (pair.value.type == resource_type::mesh ||
                pair.value.type == resource_type::material ||
                pair.value.type == resource_type::node)
            {
                resource_header header;
                header.index = pair.key;
                header.type = pair.value.type;
                ICY_ERROR(resource->load(header));
                ++max_count;

                if (header.type == resource_type::node)
                {
                    ICY_ERROR(scene.nodes.insert(header.index, render_node()));
                }
                else if (header.type == resource_type::material)
                {
                    ICY_ERROR(scene.materials.insert(header.index, render_material()));
                }
                else if (header.type == resource_type::mesh)
                {
                    ICY_ERROR(scene.meshes.insert(header.index, render_mesh()));
                }
            }
        }
        auto query = 0u;
        ICY_ERROR(imgui_display->repaint(query));

        while (*loop)
        {
            event event;
            ICY_ERROR(loop->pop(event));
            if (!event)
                break;

            if (event->type == event_type::global_timer)
            {
                const auto pair = event->data<timer::pair>();
                if (pair.timer == &win_render_timer)
                {
                    render_gui_frame copy_frame;
                    ICY_ERROR(copy(gui_render, copy_frame));
                    ICY_ERROR(display_system->repaint("ImGui"_s, copy_frame));
                }
                else if (pair.timer == &gui_render_timer)
                {
                    ICY_ERROR(imgui_display->repaint(query));
                }
            }
            else if (event->type == event_type::gui_render)
            {
                auto& event_data = event->data<imgui_event>();
                if (event_data.type == imgui_event_type::display_render)
                {
                    gui_render = std::move(event_data.render);
                }
            }
            else if (event->type == event_type::render_event)
            {
                ICY_ERROR(display_system->repaint("3D"_s, render_surface));
            }
            else if (event->type == event_type::resource_load)
            {
                auto& event_data = event->data<resource_event>();
                if (event_data.error)
                    continue;

                if (event_data.header.type == resource_type::material)
                {
                    const auto it = scene.materials.find(event_data.header.index);
                    if (it != scene.materials.end())
                    {
                        it->value = std::move(event_data.material());
                        ++cur_count;
                    }
                }
                else if (event_data.header.type == resource_type::mesh)
                {
                    const auto it = scene.meshes.find(event_data.header.index);
                    if (it != scene.meshes.end())
                    {
                        it->value = std::move(event_data.mesh());
                        ++cur_count;
                    }
                }
                else if (event_data.header.type == resource_type::node)
                {
                    const auto it = scene.nodes.find(event_data.header.index);
                    if (it != scene.nodes.end())
                    {
                        it->value = std::move(event_data.node());
                        ++cur_count;
                    }
                }
                if (cur_count == max_count)
                {
                    render_surface->repaint(scene);
                }
            }
        }
        return error_type();
    }
    shared_ptr<window_system> window_system;
    shared_ptr<display_system> display_system;
    shared_ptr<imgui_system> imgui_system;
    shared_ptr<imgui_display> imgui_display;
    shared_ptr<render_system> render_system;
    shared_ptr<render_surface> render_surface;
    //guid root_node;
};



error_type main_ex(heap& heap)
{
    shared_ptr<event_queue> loop;
    ICY_ERROR(create_event_system(loop, 0
        | event_type::window_resize
        | event_type::gui_update
        | event_type::resource_store
        | event_type::resource_load
    ));

    shared_ptr<resource_system> rsystem;
    ICY_ERROR(create_resource_system(rsystem, "icy-resource.dat"_s, 1_gb));
    ICY_ERROR(rsystem->thread().launch());
    ICY_ERROR(rsystem->thread().rename("Resource Thread"_s))

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

    array<shared_ptr<gpu_device>> gpu;
    ICY_ERROR(create_gpu_list(gpu_flags::d3d11 | gpu_flags::hardware | gpu_flags::debug, gpu));

    shared_ptr<display_system> display_system;
    ICY_ERROR(create_display_system(display_system, window, gpu[0]));
    ICY_ERROR(display_system->thread().launch());
    ICY_ERROR(display_system->thread().rename("Display Thread"_s));


    shared_ptr<render_system> render_system;
    ICY_ERROR(create_render_system(render_system, gpu[0]));
    ICY_ERROR(render_system->thread().launch());
    ICY_ERROR(render_system->thread().rename("DirectX Thread"_s));

    shared_ptr<render_surface> render_surface;
    ICY_ERROR(render_system->create({ 640, 480 }, render_surface));

   /* map<guid, resource_data> list;
    rsystem->list(list);
    guid root_node;
    for (auto&& pair : list)
    {
        if (pair.value.type == resource_type::node && pair.value.name == "RootNode"_s)
        {
            root_node = pair.key;
            break;
        }
    }*/

    shared_ptr<render_thread> render_thread;
    ICY_ERROR(make_shared(render_thread));
    render_thread->window_system = window_system;
    render_thread->display_system = display_system;
    render_thread->imgui_system = imgui_system;
    render_thread->imgui_display = imgui_display;
    render_thread->render_system = render_system;
    render_thread->render_surface = render_surface;
    ICY_SCOPE_EXIT{ render_thread->wait(); };

    ICY_ERROR(window->show(true));

    map<guid, resource_data> list;
    rsystem->list(list);

    if (list.empty())
    {
        resource_header header;
        header.index = guid::create();
        header.type = resource_type::node;
        ICY_ERROR(rsystem->store(header, "D:/Downloads/53-cottage_fbx/cottage_fbx.fbx"_s));
    }
    else
    {
        ICY_ERROR(render_thread->launch());
        ICY_ERROR(render_thread->rename("Render Thread"_s));
    }

    while (*loop)
    {
        event event;
        ICY_ERROR(loop->pop(event));
        if (!event)
            break;

        if (event->type == event_type::window_resize)
        {
            const auto& event_data = event->data<window_message>();
            ICY_ERROR(display_system->resize(event_data.size));
        }
        else if (event->type == event_type::gui_update)
        {
            auto& event_data = event->data<imgui_event>();            
        }
        else if (event->type == event_type::resource_store)
        {
            if (list.empty())
            {
                ICY_ERROR(render_thread->launch());
                ICY_ERROR(render_thread->rename("Render Thread"_s));
            }
        }
    }
    return error_type();
}



int main()
{
    heap gheap;
    gheap.initialize(heap_init::global(16_mb));
    return main_ex(gheap).code;
}

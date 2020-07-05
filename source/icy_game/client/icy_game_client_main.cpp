#include <icy_engine/core/icy_thread.hpp>
#include <icy_engine/graphics/icy_graphics.hpp>
#include <icy_engine/graphics/icy_render.hpp>
#include <icy_engine/core/icy_file.hpp>
#if _DEBUG
#pragma comment(lib, "icy_engine_cored")
#pragma comment(lib, "icy_engine_graphicsd")
#else
#pragma comment(lib, "icy_engine_core")
#pragma comment(lib, "icy_engine_graphics")
#endif

using namespace icy;

class application : public thread
{
public:
    error_type exec(const uint64_t luid) noexcept;
private:
    error_type run() noexcept override;
private:
    shared_ptr<window> m_window;
};
error_type application::exec(const uint64_t luid) noexcept
{
    const auto shutdown = [this]
    {
        event::post(nullptr, event_type::global_quit);
        return wait();
    };
    ICY_SCOPE_EXIT{ shutdown(); };
    array<adapter> adapters;
    ICY_ERROR(adapter::enumerate(!luid ? (adapter_flag::hardware | adapter_flag::d3d12) : adapter_flag::none, adapters));
    adapter adapter;
    if (luid)
    {
        for (auto&& gpu : adapters)
        {
            if (gpu.luid == luid)
            {
                adapter = gpu;
                break;
            }
        }
    }
    else if (!adapters.empty())
    {
        adapter = adapters.front();
    }
    if (!adapter)
        return last_system_error();

    ICY_ERROR(create_window_system(m_window, adapter, default_window_flags | window_flags::msaa_x4));
    ICY_ERROR(m_window->initialize());
    ICY_ERROR(m_window->restyle(window_style::maximized));
    ICY_ERROR(launch());
    ICY_ERROR(m_window->exec());
    return shutdown();
}
error_type application::run() noexcept
{
    ICY_SCOPE_EXIT{ event::post(nullptr, event_type::global_quit); };
    shared_ptr<event_queue> queue;
    ICY_ERROR(create_event_queue(queue, event_type::window_any));

    {
        /*
        render_index idx_mesh;
        render_index idx_obj;
        list->create(render_type::mesh, idx_mesh);
        list->create(render_type::object, idx_obj);

        render_mesh data_mesh;
        render_vertex v0;
        render_vertex v1;
        render_vertex v2;
        v1.pos = { 1, 0, 0 };
        v2.pos = { 0, 1, 0 };
        data_mesh.vertices.push_back(v0);
        data_mesh.vertices.push_back(v1);
        data_mesh.vertices.push_back(v2);

        render_object data_obj;
        data_obj.data.push_back({ render_index(), idx_mesh });

        render_camera data_camera;
        data_camera.pos = { 1, 1, 1 };
        data_camera.dir = { -1, -1, -1 };
        data_camera.min_z = 0.1;
        data_camera.max_z = 100.0;
        data_camera.view = { 0, 0, 800, 500 };


        list->update(idx_mesh, std::move(data_mesh));
        list->update(idx_obj, std::move(data_obj));
        list->camera(data_camera);
        list->exec();*/
    }

    render_svg_geometry svg;
    render_svg_font font;
    render_svg_font_data font_data;
    copy("Arial"_s, font_data.name);
    font_data.flags.push_back(render_svg_text_flag(render_svg_text_flag::font_size, 72));
    font.initialize(font_data);
    svg.initialize(render_d2d_matrix(), colors::white, font, "Hello"_s);

   /* icy::file file;
    file.open("D:/Downloads/tiger.svg"_s, file_access::read, file_open::open_existing, file_share::read);
    array<char> svg_data;
    svg_data.resize(file.info().size);
    auto size = svg_data.size();
    file.read(svg_data.data(), size);

    render_svg_geometry svg2;
    svg2.initialize(render_d2d_matrix(), svg_data);*/

    const auto r = [&]
    {

        render_list list;
        color c;
        c.r = 1 % 255;
        list.clear(c);
        list.draw(svg, render_d2d_vector(0, 0));
        //list.draw(svg2, render_d2d_vector(100, 0));

        m_window->render(std::move(list));
    };
    r();

    auto now_0 = icy::clock_type::now();
    auto frame_0 = 0_z;
    while (true)
    {
        event event;
        ICY_ERROR(queue->pop(event));
        if (event->type == event_type::global_quit)
            break;
        if (event->type == event_type::window_close)
            break;

        if (event->type == event_type::window_repaint)
        {
            const auto now_1 = clock_type::now();
            const auto delta = std::chrono::duration_cast<std::chrono::milliseconds>(now_1 - now_0);
            const auto frame_1 = event->data<size_t>();
            if (delta.count() >= 1000)
            {
                auto fps = (frame_1 - frame_0) * 1000 / delta.count();
                frame_0 = frame_1;
                now_0 = now_1;
                string name;
                to_string("FPS [%1], Frame #%2"_s, name, fps, frame_1);
                m_window->rename(name);
            }
        }
        else if (event->type == event_type::window_resize)
        {
            r();
        }
    }
    return error_type();
}

int main()
{
    heap gheap;
    if (const auto error = gheap.initialize(heap_init::global(16_gb)))
        return error.code;

    shared_ptr<application> app;
    if (const auto error = make_shared(app))
        return error.code;

    if (const auto error = app->exec(0))
    {
        string str;
        to_string(error, str);
        win32_message(str, "App exec");
        return error.code;
    }
    return 0;
}
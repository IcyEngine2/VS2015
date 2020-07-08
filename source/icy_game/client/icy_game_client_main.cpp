#include <icy_engine/core/icy_file.hpp>
#include <icy_engine/core/icy_event_thread.hpp>
#include <icy_engine/graphics/icy_graphics.hpp>
#include <icy_engine/graphics/icy_render.hpp>
#if _DEBUG
#pragma comment(lib, "icy_engine_cored")
#pragma comment(lib, "icy_engine_graphicsd")
#else
#pragma comment(lib, "icy_engine_core")
#pragma comment(lib, "icy_engine_graphics")
#endif

using namespace icy;

struct application;


class game_system : public event_system
{
public:
    game_system() noexcept
    {
        filter(event_type::window_any);
    }
    ~game_system() noexcept
    {
        filter(0);
    }
    error_type exec() noexcept override;
    application* app = nullptr;
    cvar var;
    mutex lock;
private:
    error_type signal(const event_data& event) noexcept override
    {
        var.wake();
        return error_type();
    }
};

struct application
{
    error_type run(const uint64_t luid) noexcept;
    ~application() noexcept
    {
        event::post(nullptr, event_type::global_quit);
        if (game) game->wait();
        if (render) render->wait();
        if (window) window->wait();
    }
    shared_ptr<event_thread<window_system>> window;
    shared_ptr<event_thread<render_system>> render;
    shared_ptr<event_thread<game_system>> game;
};


error_type game_system::exec() noexcept
{
    ICY_SCOPE_EXIT{ event::post(nullptr, event_type::global_quit); };

    render_svg_geometry svg;
    render_svg_font font;
    render_svg_font_data font_data;
    ICY_ERROR(copy("Arial"_s, font_data.name));
    ICY_ERROR(font_data.flags.push_back(render_svg_text_flag(render_svg_text_flag::font_size, 72)));
    ICY_ERROR(font.initialize(font_data));
    ICY_ERROR(svg.initialize(render_d2d_matrix(), colors::white, font, "Hello"_s));

    auto now_0 = clock_type::now();
    auto frame_0 = 0_z;
    
    while (true)
    {
        auto event = pop();
        if (!event)
        {
            ICY_ERROR(var.wait(lock));
            continue;
        }
        if (event->type == event_type::global_quit)
            break;
        if (event->type == event_type::window_close)
            break;

        if (event->type == event_type::window_repaint)
        {
            const auto frame_1 = event->data<size_t>();
            render_list list(frame_1);
            color c;
            c.r = 100 % 255;
            ICY_ERROR(list.clear(c));
            ICY_ERROR(list.draw(svg, render_d2d_vector(0, 0)));
            ICY_ERROR(app->render->system->update(std::move(list)));

            const auto now_1 = clock_type::now();
            const auto delta = std::chrono::duration_cast<std::chrono::milliseconds>(now_1 - now_0);

            if (delta.count() >= 1000)
            {
                auto fps = (frame_1 - frame_0) * 1000 / delta.count();
                frame_0 = frame_1;
                now_0 = now_1;
                string name;
                ICY_ERROR(to_string("FPS [%1], Frame #%2"_s, name, fps, frame_1));
                ICY_ERROR(app->window->rename(name));
            }
        }
    }
    return error_type();
}
namespace icy
{
    error_type create_event_system(shared_ptr<game_system>& system, application* app) noexcept
    {
        shared_ptr<game_system> ptr;
        ICY_ERROR(make_shared(ptr));
        ICY_ERROR(ptr->lock.initialize());
        ptr->app = app;
        system = std::move(ptr);
        return error_type();
    }
}

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
  list->exec();

icy::file file;
 file.open("D:/Downloads/tiger.svg"_s, file_access::read, file_open::open_existing, file_share::read);
 array<char> svg_data;
 svg_data.resize(file.info().size);
 auto size = svg_data.size();
 file.read(svg_data.data(), size);

 render_svg_geometry svg2;
 svg2.initialize(render_d2d_matrix(), svg_data);*/

error_type main_func(const uint64_t luid) noexcept
{
    array<adapter> adapters;
    ICY_ERROR(adapter::enumerate(!luid ? (adapter_flag::d3d12) : adapter_flag::none, adapters));
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

    application app;
    ICY_ERROR(create_event_thread(app.window, adapter, default_window_flags | window_flags::msaa_x4));
    ICY_ERROR(app.window->launch());
    ICY_ERROR(create_event_thread(app.render, std::ref(*app.window->system)));
    ICY_ERROR(app.render->launch());
    ICY_ERROR(create_event_thread(app.game, &app));
    ICY_ERROR(app.game->launch());
    ICY_ERROR(app.game->wait());
    ICY_ERROR(app.render->wait());
    ICY_ERROR(app.window->wait());
    return error_type();
}

int main()
{
    heap gheap;
    if (const auto error = gheap.initialize(heap_init::global(16_gb)))
        return error.code;

    if (const auto error = main_func(0))
    {
        string str;
        to_string(error, str);
        win32_message(str, "App exec");
        return error.code;
    }
    return 0;
}
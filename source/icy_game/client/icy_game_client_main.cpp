#include <icy_engine/core/icy_file.hpp>
#include <icy_engine/core/icy_event_thread.hpp>
#include <icy_engine/graphics/icy_graphics.hpp>
#include <icy_engine/graphics/icy_render_svg.hpp>
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
        filter(event_type::window_any | event_type::render_frame);
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
        if (display) display->wait();
        if (window) window->wait();
    }
    shared_ptr<event_thread<window_system>> window;
    shared_ptr<event_thread<display_system>> display;
    shared_ptr<event_thread<game_system>> game;
    shared_ptr<render_factory> render_10;
    shared_ptr<render_factory> render_11;
};


error_type game_system::exec() noexcept
{
    ICY_SCOPE_EXIT{ event::post(nullptr, event_type::global_quit); };

    render_svg_font font;
    render_svg_font_data font_data;
    ICY_ERROR(copy("Arial"_s, font_data.name));
    ICY_ERROR(font_data.flags.push_back(render_svg_text_flag(render_svg_text_flag::font_size, 72)));
    ICY_ERROR(font.initialize(font_data));
    
    array<char> bytes;
    {
        file f;
        ICY_ERROR(f.open("G:/VS2015/dat/tiger.svg"_s, file_access::read, file_open::open_existing, file_share::none));
        auto size = f.info().size;
        ICY_ERROR(bytes.resize(size));
        ICY_ERROR(f.read(bytes.data(), size));
    }

    render_svg_geometry geo;
    ICY_ERROR(geo.initialize(render_d2d_matrix(), bytes));

    window_size size;
    const auto func = [font, geo, &size, this](size_t index, color clr)
    {
        string str;
        render_svg_geometry svg;
        ICY_ERROR(to_string("Frame %1"_s, str, index));
        ICY_ERROR(svg.initialize(render_d2d_matrix(), colors::white, font, str));

        shared_ptr<render_list> list;
        ICY_ERROR(create_render_list(list));
        ICY_ERROR(list->clear(clr));
        ICY_ERROR(list->draw(geo, render_d2d_matrix::scale(size.x / 1800.0f, size.y / 1800.0f)));
        ICY_ERROR(list->draw(svg, render_d2d_matrix()));
        
        render_frame frame;
        ICY_ERROR(app->render_10->exec(*list, frame));
        ICY_ERROR(app->display->system->render(frame));
        return error_type();
    };
    ICY_ERROR(func(0, colors::red));
   
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
        if (event->type == event_type::window_resize)
        {
            size = size = event->data<window_size>();
            display_options opts;
            opts.size = event->data<window_size>();
            app->display->system->options(opts);
            app->render_10->resize(size);
        }
        if (event->type == event_type::render_frame)
        {
            auto frame = event->data<render_frame>();

            color color;
            color.b = frame.index() % 255;
            func(frame.index() + 1, color);

            const auto now_1 = clock_type::now();
            const auto delta = std::chrono::duration_cast<std::chrono::milliseconds>(now_1 - now_0);
            if (delta.count() >= 1000)
            {
                auto fps = (frame.index() - frame_0) * 1000 / delta.count();
                frame_0 = frame.index();
                now_0 = now_1;
                string name;
                auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(frame.time_cpu());

                ICY_ERROR(to_string("FPS [%1], Frame #%2, Time = %3 ms"_s, name, fps, frame.index(), ms.count()));
                ICY_ERROR(app->window->system->rename(name));
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

error_type main_func(const uint64_t luid) noexcept
{
    //win32_message("Hi"_s, "Hi"_s);
    array<adapter> adapters;
    auto adapter_flags = adapter_flags::debug;
    adapter_flags = adapter_flags | adapter_flags::d3d11 | adapter_flags::d3d10 | adapter_flags::hardware;
    
    ICY_ERROR(adapter::enumerate(adapter_flags, adapters));
    adapter adapter11;
    adapter adapter10;

    if (!adapters.empty())
    {
        for (auto&& ad : adapters)
        {
            if ((ad.flags & adapter_flags::d3d11) && ad.luid == 40746)
            {
                adapter11 = ad;
                break;
            }
        }
        for (auto&& ad : adapters)
        {
            // 40019
            if ((ad.flags & adapter_flags::d3d10) && ad.luid == 40746)
            {
                adapter10 = ad;
                break;
            }
        }
    }
    if (!adapter11 || !adapter10)
        return last_system_error();

    application app;
    ICY_ERROR(create_event_thread(app.window, default_window_flags));
    ICY_ERROR(app.window->launch());

    ICY_ERROR(create_event_thread(app.display, adapter11, app.window->system->handle(),
        display_flags::sRGB));
    ICY_ERROR(app.display->launch());

    display_options opts;
    opts.vsync = display_frame_unlim;
    ICY_ERROR(app.display->system->options(opts));

    ICY_ERROR(create_render_factory(app.render_10, adapter10, render_flags::none));
    app.render_10->resize(window_size(1920, 1080));
    //ICY_ERROR(create_render_factory(app.render_10, adapter10, render_flags::none));
    
    ICY_ERROR(create_event_thread(app.game, &app));
    ICY_ERROR(app.game->launch());
    ICY_ERROR(app.game->wait());
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

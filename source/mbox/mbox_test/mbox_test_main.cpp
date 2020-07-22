#include <icy_engine/core/icy_file.hpp>
#include <icy_engine/core/icy_event_thread.hpp>
#include <icy_engine/graphics/icy_adapter.hpp>
#include <icy_engine/graphics/icy_window.hpp>
#include <icy_engine/graphics/icy_display.hpp>
#include <icy_engine/graphics/icy_render.hpp>
#include <icy_engine/graphics/icy_render_svg.hpp>
#include <icy_engine/core/icy_input.hpp>
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
    ICY_ERROR(app->render_10->create_svg_font(font_data, font));


    const auto read_file = [](const string_view name, array<char>& bytes)
    {
        string str;
        ICY_ERROR(to_string("G:/VS2015/dat/%1"_s, str, name));
        file f;
        ICY_ERROR(f.open(str, file_access::read, file_open::open_existing, file_share::none));
        auto size = f.info().size;
        ICY_ERROR(bytes.resize(size));
        ICY_ERROR(f.read(bytes.data(), size));
        return error_type();
    };

    window_size size = window_size(1424, 720);

    auto key = key::none;

    const auto func = [&](size_t index)
    {
        string str;
        ICY_ERROR(to_string("Frame %1"_s, str, index));

        render_svg_text text1;
        ICY_ERROR(app->render_10->create_svg_text(str, colors::white, window_size(), font, text1));
        render_svg_text text2;
        if (key != key::none)
            ICY_ERROR(app->render_10->create_svg_text(to_string(key), colors::white, window_size(), font, text2));
        
        render_texture texture_num;
        render_texture texture_frame;
        {
            render_commands_2d cmds;
            ICY_ERROR(app->render_10->open_commands_2d(cmds));
            ICY_ERROR(cmds.draw(text1, render_d2d_matrix(), 0));
            if (key != key::none)
                ICY_ERROR(cmds.draw(text2, render_d2d_matrix(0, 200), 0));
            ICY_ERROR(cmds.close(size, texture_num));
        }
        render_commands_3d cmds;
        ICY_ERROR(app->render_10->open_commands_3d(cmds));
        ICY_ERROR(cmds.clear(icy::color::from_rgba(0, 0, index % 255, 255)));
        ICY_ERROR(cmds.draw(render_d2d_rectangle_u(0, 0, size.x, size.y), texture_num));
        ICY_ERROR(cmds.close(size, texture_frame));

        ICY_ERROR(app->display->system->render(texture_frame));
        return error_type();
    };
    ICY_ERROR(func(0));

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
        {
            break;
        }
        if (event->type == event_type::window_resize)
        {
            size = event->data<window_size>();
        }
        else if (event->type == event_type::window_input)
        {
            auto input = event->data<icy::input_message>();
            if (input.type == input_type::key_press)
                key = input.key;
        }
        if (event->type == event_type::render_frame)
        {
            auto frame = event->data<render_frame>();
            ICY_ERROR(func(frame.index + 1));

            const auto now_1 = clock_type::now();
            const auto delta = std::chrono::duration_cast<std::chrono::milliseconds>(now_1 - now_0);
            if (delta.count() >= 1000)
            {
                auto fps = (frame.index - frame_0) * 1000 / delta.count();
                frame_0 = frame.index;
                now_0 = now_1;
                string name;
                auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(frame.time[render_frame::time_display]);
                ICY_ERROR(to_string("FPS [%1], Frame #%2, Time = %3 ms"_s, name, fps, frame.index, ms.count()));
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
            if ((ad.flags() & adapter_flags::d3d11))
            {
                adapter11 = ad;
                break;
            }
        }
    }
    
    adapter10 = adapter11;

    application app;
    ICY_ERROR(create_event_thread(app.window, default_window_flags));
    ICY_ERROR(app.window->launch());

    ICY_ERROR(create_event_thread(app.display, adapter11, app.window->system->handle(), display_flags::none));
    ICY_ERROR(app.display->launch());

    display_options opts;
    opts.vsync = display_frame_vsync;
    ICY_ERROR(app.display->system->options(opts));

    ICY_ERROR(create_render_factory(app.render_10, adapter10, render_flags::sRGB));
    
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
#include "icy_chat_client.hpp"
#if _DEBUG
#pragma comment(lib, "icy_engine_cored")
#pragma comment(lib, "icy_engine_graphicsd")
#pragma comment(lib, "icy_engine_networkd")
#else
#pragma comment(lib, "icy_engine_core")
#pragma comment(lib, "icy_engine_graphics")
#pragma comment(lib, "icy_engine_network")
#endif

using namespace icy;

void chat_client_gui_thread::cancel() noexcept
{
    if (app)
        app->loop->post_quit_event();
}
error_type chat_client_gui_thread::run() noexcept
{
    if (auto error = app->run_gui())
    {
        cancel();
        return error;
    }
    return error_type();
}

error_type chat_client_application::initialize() noexcept
{
    ICY_ERROR(create_window_system(window_system));
    ICY_ERROR(window_system->create(window));
    ICY_ERROR(window->restyle(window_style::windowed));
    ICY_ERROR(window->rename("Icy Chat Client"_s));
    ICY_ERROR(create_imgui_system(imgui_system));
    ICY_ERROR(imgui_system->create_display(imgui_display, window));

    array<adapter> gpu_list;
    ICY_ERROR(adapter::enumerate(adapter_flags::d3d10 | adapter_flags::hardware, gpu_list));
    if (gpu_list.empty())
        return make_unexpected_error();
    gpu = gpu_list[0];


    database_system_read dbase;
    ICY_ERROR(dbase.initialize("chat_database.dat"_s, 1_gb));
    database_txn_read txn;
    ICY_ERROR(txn.initialize(dbase));
    database_dbi dbi_users;
    database_dbi dbi_rooms;
    if (dbi_users.initialize_open_any_key(txn, "users"_s) == error_type())
    {
        database_cursor_read cur_users;
        ICY_ERROR(cur_users.initialize(txn, dbi_users));
        guid key;
        const_array_view<uint8_t> val;
        auto error = cur_users.get_var_by_type(key, val, database_oper_read::none);
        while (error != database_error_not_found)
        {
            ICY_ERROR(users.insert(key));
            error = cur_users.get_var_by_type(key, val, database_oper_read::next);
        }
        if (error != database_error_not_found)
            ICY_ERROR(error);
    }
    if (dbi_rooms.initialize_open_any_key(txn, "rooms"_s) == error_type())
    {
        database_cursor_read cur_rooms;
        ICY_ERROR(cur_rooms.initialize(txn, dbi_rooms));
        guid key;
        const_array_view<uint8_t> val;
        auto error = cur_rooms.get_var_by_type(key, val, database_oper_read::none);
        while (error != database_error_not_found)
        {
            ICY_ERROR(rooms.insert(key));
            error = cur_rooms.get_var_by_type(key, val, database_oper_read::next);
        }
        if (error != database_error_not_found)
            ICY_ERROR(error);
    }
    

    ICY_ERROR(create_event_system(loop, 0
        | event_type::gui_render
        | event_type::window_resize
        | event_type::global_timer
    ));

    ICY_ERROR(make_shared(gui_thread));
    gui_thread->app = this;

    return error_type();
}
error_type chat_client_application::run() noexcept
{
    ICY_ERROR(window_system->thread().launch());
    ICY_ERROR(window_system->thread().rename("Window Thread"_s));
    ICY_ERROR(window->show(true));

    ICY_ERROR(create_display_system(display_system, window, gpu));

    ICY_ERROR(imgui_system->thread().launch());
    ICY_ERROR(imgui_system->thread().rename("ImGui Thread"_s));

    ICY_ERROR(display_system->thread().launch());
    ICY_ERROR(display_system->thread().rename("Display Thread"_s));

    ICY_ERROR(gui_thread->launch());
    ICY_ERROR(gui_thread->rename("GUI Thread"_s));

    auto query = 0u;
    ICY_ERROR(imgui_display->repaint(query));

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

        if (event->type == event_type::gui_render)
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

error_type main_ex(heap& heap)
{
    ICY_ERROR(event_system::initialize());
    ICY_ERROR(blob_initialize());
    ICY_SCOPE_EXIT{ blob_shutdown(); };

    chat_client_application app;
    ICY_ERROR(app.initialize());
    ICY_ERROR(app.run());

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

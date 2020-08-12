#include "mbox_launcher_app.hpp"

#if _DEBUG
#pragma comment(lib, "icy_engine_cored")
#pragma comment(lib, "icy_engine_graphicsd")
#pragma comment(lib, "mbox_script_systemd")
#else
#pragma comment(lib, "icy_engine_core")
#pragma comment(lib, "icy_engine_graphics")
#pragma comment(lib, "mbox_script_system")
#endif

using namespace icy;

weak_ptr<gui_queue> icy::global_gui;

error_type main_func(heap& gheap) noexcept
{
    ICY_SCOPE_EXIT{ event::post(nullptr, event_type::global_quit); };
    ICY_SCOPE_EXIT{ icy::global_gui = nullptr; };
    shared_ptr<mbox_launcher_app> app;
    ICY_ERROR(make_shared(app));
    ICY_ERROR(app->initialize());
    ICY_ERROR(app->launch());
    ICY_ERROR(app->rename("Application thread"_s));
    const auto error0 = shared_ptr<gui_queue>(global_gui)->exec();
    const auto error1 = event::post(nullptr, event_type::global_quit);
    const auto error2 = app->wait();
    ICY_ERROR(error0);
    ICY_ERROR(error1);
    ICY_ERROR(error2);
    return error_type();
}

int main()
{
    heap gheap;
    auto gheap_init = heap_init::global(64_mb);
    if (gheap.initialize(gheap_init))
        return -1;

    const auto error = main_func(gheap);
    if (error)
        show_error(error, ""_s);
    return error.code;
}
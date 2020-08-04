#include "mbox_script_dbase.hpp"
#include <icy_engine/core/icy_thread.hpp>
#include <icy_engine/core/icy_file.hpp>
#include <icy_engine/image/icy_image.hpp>
#include <icy_qtgui/icy_xqtgui.hpp>
#if _DEBUG
#pragma comment(lib, "icy_engine_cored")
#pragma comment(lib, "icy_engine_imaged")
#if ICY_QTGUI_STATIC
#pragma comment(lib, "icy_qtguid")
#endif
#else
#pragma comment(lib, "icy_engine_core")
#pragma comment(lib, "icy_engine_image")
#if ICY_QTGUI_STATIC
#pragma comment(lib, "icy_qtgui")
#endif
#endif

using namespace icy;

weak_ptr<gui_queue> icy::global_gui;

class application : public thread
{
public:
    shared_ptr<gui_queue> gui;
private:
    error_type run() noexcept override;
};

error_type application::run() noexcept
{
    ICY_SCOPE_EXIT{ event::post(nullptr, event_type::global_quit); };
    
    shared_ptr<event_queue> queue;
    ICY_ERROR(create_event_system(queue, event_type::gui_any));

    xgui_widget window;
    ICY_ERROR(window.initialize(gui_widget_type::window, gui_widget(), gui_widget_flag::layout_hbox));

    mbox::mbox_database dbase(window);
    ICY_ERROR(dbase.initialize());
    
    ICY_ERROR(window.show(true));
    while (true)
    {
        event event;
        ICY_ERROR(queue->pop(event));
        if (event->type == event_type::global_quit)
            break;
    
        const auto& event_data = event->data<gui_event>();
        if (event_data.widget == window)
        {
            auto cancel = false;
            ICY_ERROR(dbase.close(cancel));
            if (cancel)
                continue;
            break;
        }
        if (const auto error = dbase.exec(event->type, event_data))
            ICY_ERROR(show_error(error, ""_s));
    }
    return error_type();
}

error_type main_func(heap& gheap) noexcept
{
    ICY_SCOPE_EXIT{ event::post(nullptr, event_type::global_quit); };

    shared_ptr<application> app;
    ICY_ERROR(make_shared(app));
    ICY_ERROR(create_event_system(app->gui));
    global_gui = app->gui;
    ICY_SCOPE_EXIT{ global_gui = nullptr; };
    ICY_ERROR(app->launch());
    ICY_ERROR(app->rename("Application thread"_s));
    const auto error0 = app->gui->exec();
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
    auto gheap_init = heap_init::global(2_gb);
    gheap_init.debug_trace = true;
    if (gheap.initialize(gheap_init))
        return -1;

    const auto error = main_func(gheap);
    if (error)
        show_error(error, ""_s);
    return error.code;
}

/*
  heap_report grep0;
    heap_report::create(grep0, gheap, nullptr, nullptr);

    heap lua_heap;
    auto lua_heap_init = heap_init(64_mb);
    lua_heap_init.debug_trace = 1;
    ICY_ERROR(lua_heap.initialize(lua_heap_init));

    const auto proc = [&]
    {
        lua_system lua;
        ICY_ERROR(lua.initialize(&lua_heap));

        lua_variable math_lib;
        ICY_ERROR(lua.make_library(math_lib, lua_default_library::math));

        struct vars_type
        {

            lua_variable script_1;
            lua_variable script_2;
        } vars;
        ICY_ERROR(lua.parse(vars.script_1, "y.x = math.sqrt(5); return CallScript(2, \"23526f5rwijtrirjwerjefsdfsdfd236 3tjertewrwetrwefdsfg,smrstret\")"_s, "Script 1"_s));
        ICY_ERROR(lua.parse(vars.script_2, "return math.sqrt(y.x)"_s, "Script 2"_s));

        lua_variable env_1;
        ICY_ERROR(lua.enviroment(env_1, vars.script_1));

        lua_variable env_1_y;
        ICY_ERROR(lua.make_table(env_1_y));
        ICY_ERROR(env_1.insert("y"_s, env_1_y));

        ICY_ERROR(env_1.insert(to_string(lua_default_library::math), math_lib));

        array<lua_variable> keys;
        array<lua_variable> vals;
        ICY_ERROR(env_1.map(keys, vals));

        lua_variable env_2;
        ICY_ERROR(lua.enviroment(env_2, vars.script_2));

        ICY_ERROR(env_2.map(keys, vals));



lua_cfunction call_script = [](void* user, const_array_view<lua_variable> input, array<lua_variable>* output)
{
    if (input.size() < 1)
        return make_stdlib_error(std::errc::invalid_argument);
    const auto index = uint32_t(input[0].as_number());

    if (index == 1)
    {
        ICY_ERROR(static_cast<vars_type*>(user)->script_1());
    }
    else if (index == 2)
    {
        ICY_ERROR(static_cast<vars_type*>(user)->script_2({}, *output));
    }
    else
        return make_stdlib_error(std::errc::invalid_argument);

    return error_type();
};
lua_variable func_call_script;
ICY_ERROR(lua.make_function(func_call_script, call_script, &vars));

ICY_ERROR(env_1.insert("CallScript"_s, func_call_script));

array<lua_variable> output;
ICY_ERROR(vars.script_1({}, output));

return error_type();
    };
    if (const auto error = proc())
    {
        win32_debug_print("\r\nLUA error: "_s);
        win32_debug_print(string_view(static_cast<const char*>(error.message->data()), error.message->size()));
    }

    heap_report grep1;
    heap_report::create(grep1, gheap, nullptr, nullptr);


    heap_report lua_report_1;
    heap_report::create(lua_report_1, lua_heap, nullptr, nullptr);
    if (lua_report_1.memory_active)
    {
        file f1;
        f1.open("Trace.log"_s, file_access::write, file_open::create_always, file_share::read | file_share::write);
        const auto print = [](void* f1, const heap_report::trace_info& info)
        {
            string str;
            string str_address;
            ICY_ERROR(to_string(reinterpret_cast<uint64_t>(info.address), 16, str_address));
            ICY_ERROR(str.appendf("\r\n%1 - Memory leak of size [%2] at address (%3); Trace length = %4, hash = %5",
                info.index, info.size, string_view(str_address), info.count, info.hash));
            for (auto k = 0u; k < info.count; ++k)
                ICY_ERROR(str.appendf("\r\n  - %1", string_view(info.trace[k])));
            ICY_ERROR(str.append("\r\n"));
            return static_cast<file*>(f1)->append(str.bytes().data(), str.bytes().size());
            //return win32_debug_print(str);
        };
        heap_report::create(lua_report_1, lua_heap, &f1, print);
    }
    //heap_debug dbg;

*/
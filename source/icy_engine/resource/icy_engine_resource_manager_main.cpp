#include <icy_engine/core/icy_core.hpp>
#include <icy_engine/core/icy_event.hpp>
#include <icy_engine/core/icy_thread.hpp>
#include <icy_engine/core/icy_file.hpp>
#include <icy_engine/graphics/icy_window.hpp>
#include <icy_engine/graphics/icy_adapter.hpp>
#include <icy_engine/graphics/icy_display.hpp>
#include <icy_engine/utility/icy_imgui.hpp>
#include <icy_engine/resource/icy_engine_resource.hpp>
#include <array>

using namespace icy;

#if _DEBUG
#pragma comment(lib, "icy_engine_cored")
#pragma comment(lib, "icy_engine_graphicsd")
#pragma comment(lib, "icy_engine_resourced")
#else
#pragma comment(lib, "icy_engine_core")
#pragma comment(lib, "icy_engine_graphics")
#pragma comment(lib, "icy_engine_resource")
#endif

enum widget
{
    root = 0u,
    popup_error,
    popup_error_message,
    popup_error_source,
    popup_error_code,
    popup_error_detail,
    menu_main,
    menu_file,
    menu_explorer,
    menu_import,
    menu_export,
    window_explorer,
    window_tabs,
    window_import,
    import_text_filename,
    import_edit_filename,
    import_exec_filename,
    import_text_type,
    import_edit_type,
    import_text_name,
    import_edit_name,
    import_text_locale,
    import_edit_locale,
    import_execute,
    import_log,
    explorer_main,
    tabs_main,
    _count,
};

using namespace icy;

class resource_manager_application;

class resource_manager_gui_thread : public icy::thread
{
public:
    void cancel() noexcept override;
    icy::error_type run() noexcept override;
    resource_manager_application* app = nullptr;
};
class resource_manager_application
{
public:
    ~resource_manager_application() noexcept
    {
        if (gui_thread) gui_thread->wait();
    }
    error_type initialize() noexcept;
    error_type run() noexcept;
    error_type run_gui() noexcept;
    error_type show_error(const error_type& error, const string_view msg) noexcept;
public:
    icy::shared_ptr<icy::window_system> window_system;
    icy::shared_ptr<icy::window> window;
    icy::shared_ptr<icy::imgui_system> imgui_system;
    icy::shared_ptr<icy::imgui_display> imgui_display;
    icy::shared_ptr<icy::display_system> display_system;
    shared_ptr<resource_system> resource_system;
    icy::adapter gpu;
    icy::shared_ptr<icy::event_queue> loop;
    icy::shared_ptr<resource_manager_gui_thread> gui_thread;
    std::array<uint32_t, _count> widgets = {};
    map<uint32_t, resource_locale> widgets_locale;
    map<uint32_t, resource_type> widgets_type;
    map<guid, resource_data> resource_list;
};

void resource_manager_gui_thread::cancel() noexcept
{
    if (app)
        app->loop->post_quit_event();
}
error_type resource_manager_gui_thread::run() noexcept
{
    if (auto error = app->run_gui())
    {
        cancel();
        return error;
    }
    return error_type();
}

error_type resource_manager_application::initialize() noexcept
{
    ICY_ERROR(create_window_system(window_system));
    ICY_ERROR(window_system->create(window));
    ICY_ERROR(window->restyle(window_style::windowed));
    ICY_ERROR(window->rename("Icy Resource Manager"_s));
    ICY_ERROR(create_imgui_system(imgui_system));
    ICY_ERROR(imgui_system->create_display(imgui_display, window));
        
    array<adapter> gpu_list;
    ICY_ERROR(adapter::enumerate(adapter_flags::d3d10 | adapter_flags::hardware, gpu_list));
    if (gpu_list.empty())
        return make_unexpected_error();
    gpu = gpu_list[0];

    const auto add_widget = [this](const widget parent, const widget index, const imgui_widget_type type, const string_view label = string_view())
    {
        ICY_ERROR(this->imgui_display->widget_create(widgets[parent], type, widgets[index]));
        if (!label.empty())
            ICY_ERROR(this->imgui_display->widget_label(widgets[index], label));
        return error_type();
    };

    ICY_ERROR(add_widget(root, popup_error, imgui_widget_type::window, "Error"_s));
    ICY_ERROR(add_widget(popup_error, popup_error_message, imgui_widget_type::text));
    ICY_ERROR(add_widget(popup_error, popup_error_source, imgui_widget_type::text));
    ICY_ERROR(add_widget(popup_error, popup_error_code, imgui_widget_type::text));
    ICY_ERROR(add_widget(popup_error, popup_error_detail, imgui_widget_type::text));

    ICY_ERROR(add_widget(root, menu_main, imgui_widget_type::main_menu_bar));
    ICY_ERROR(add_widget(menu_main, menu_file, imgui_widget_type::menu_item, "File"_s));
    ICY_ERROR(add_widget(menu_main, menu_explorer, imgui_widget_type::menu_item, "Explorer"_s));
    ICY_ERROR(add_widget(menu_main, menu_import, imgui_widget_type::menu_item, "Import"_s));
    ICY_ERROR(add_widget(menu_main, menu_export, imgui_widget_type::menu_item, "Export"_s));

    ICY_ERROR(add_widget(root, window_explorer, imgui_widget_type::window, "Explorer"_s));
    ICY_ERROR(add_widget(root, window_import, imgui_widget_type::window, "Import"_s));
    ICY_ERROR(add_widget(root, window_tabs, imgui_widget_type::window, "Tabs"_s));

    ICY_ERROR(add_widget(window_tabs, tabs_main, imgui_widget_type::tab_bar));

    ICY_ERROR(add_widget(window_import, import_text_filename, imgui_widget_type::text));
    ICY_ERROR(add_widget(window_import, import_edit_filename, imgui_widget_type::line_input));
    ICY_ERROR(add_widget(window_import, import_exec_filename, imgui_widget_type::button));
    ICY_ERROR(add_widget(window_import, import_text_type, imgui_widget_type::text));
    ICY_ERROR(add_widget(window_import, import_edit_type, imgui_widget_type::combo_view));
    ICY_ERROR(add_widget(window_import, import_text_name, imgui_widget_type::text));
    ICY_ERROR(add_widget(window_import, import_edit_name, imgui_widget_type::line_input));
    ICY_ERROR(add_widget(window_import, import_text_locale, imgui_widget_type::text));
    ICY_ERROR(add_widget(window_import, import_edit_locale, imgui_widget_type::combo_view));
    ICY_ERROR(add_widget(window_import, import_execute, imgui_widget_type::button));
    ICY_ERROR(add_widget(window_import, import_log, imgui_widget_type::text));

    ICY_ERROR(imgui_display->widget_value(widgets[import_text_filename], "Filename"_s));
    ICY_ERROR(imgui_display->widget_label(widgets[import_exec_filename], "Select"_s));
    ICY_ERROR(imgui_display->widget_value(widgets[import_text_type], "Type"_s));
    ICY_ERROR(imgui_display->widget_value(widgets[import_text_name], "Name"_s));
    ICY_ERROR(imgui_display->widget_value(widgets[import_text_locale], "Locale"_s));
    ICY_ERROR(imgui_display->widget_value(widgets[import_edit_type], to_string(resource_type::user)));
    ICY_ERROR(imgui_display->widget_value(widgets[import_edit_locale], to_string(resource_locale::none)));
    ICY_ERROR(imgui_display->widget_label(widgets[import_execute], "Import"_s));

    for (auto k = 0u; k < uint32_t(resource_locale::_total); ++k)
    {
        auto widget = 0u;
        ICY_ERROR(imgui_display->widget_create(widgets[import_edit_locale], imgui_widget_type::menu_item, widget));
        ICY_ERROR(widgets_locale.insert(widget, resource_locale(k)));
        ICY_ERROR(imgui_display->widget_label(widget, to_string(resource_locale(k))));
    }
    for (auto k = 1u; k < uint32_t(resource_type::_total); ++k)
    {
        auto widget = 0u;
        ICY_ERROR(imgui_display->widget_create(widgets[import_edit_type], imgui_widget_type::menu_item, widget));
        ICY_ERROR(widgets_type.insert(widget, resource_type(k)));
        ICY_ERROR(imgui_display->widget_label(widget, to_string(resource_type(k))));
    }

    ICY_ERROR(imgui_display->widget_state(widgets[popup_error],
        imgui_window_state(imgui_window_flag::always_auto_resize | imgui_window_flag::no_resize, imgui_widget_flag::is_hidden)));
    ICY_ERROR(imgui_display->widget_state(widgets[window_explorer], imgui_window_state(imgui_window_flag::none, imgui_widget_flag::is_hidden)));
    ICY_ERROR(imgui_display->widget_state(widgets[window_import], imgui_widget_flag::is_hidden));
    ICY_ERROR(imgui_display->widget_state(widgets[window_tabs], imgui_widget_flag::is_hidden));

    ICY_ERROR(imgui_display->widget_state(widgets[import_edit_filename], imgui_widget_flag::is_same_line));
    ICY_ERROR(imgui_display->widget_state(widgets[import_exec_filename], imgui_widget_flag::is_same_line));
    ICY_ERROR(imgui_display->widget_state(widgets[import_edit_type], imgui_widget_flag::is_same_line));
    ICY_ERROR(imgui_display->widget_state(widgets[import_edit_name], imgui_widget_flag::is_same_line));
    ICY_ERROR(imgui_display->widget_state(widgets[import_edit_locale], imgui_widget_flag::is_same_line));
    ICY_ERROR(imgui_display->widget_state(widgets[import_log], imgui_widget_flag::is_same_line));



    ICY_ERROR(create_event_system(loop, 0
        | event_type::gui_render
        | event_type::window_resize
        | event_type::global_timer
    ));

    ICY_ERROR(make_shared(gui_thread));
    gui_thread->app = this;

    return error_type();
}
error_type resource_manager_application::run() noexcept
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
error_type resource_manager_application::run_gui() noexcept
{
    shared_ptr<event_queue> loop;
    ICY_ERROR(create_event_system(loop, 0 
        | event_type::gui_update 
        | event_type::resource_load 
        | event_type::resource_store));

    string filename;
    string name;
    string log;
    auto type = resource_type::user;
    auto locale = resource_locale::none;

    const auto print = [this, &log](string_view msg)
    {
        if (!log.empty())
        {
            ICY_ERROR(log.append("\r\n"_s));
        }
        string time_str;
        ICY_ERROR(to_string(clock_type::now(), time_str));
        ICY_ERROR(log.appendf("[%1] %2"_s, string_view(time_str), msg));
        ICY_ERROR(this->imgui_display->widget_value(widgets[import_log], log));
        return error_type();
    };

    while (*loop)
    {
        event event;
        ICY_ERROR(loop->pop(event));
        if (!event)
            break;

        if (event->type == event_type::gui_update)
        {
            auto& event_data = event->data<imgui_event>();
            if (event_data.type == imgui_event_type::widget_click)
            {
                if (false)
                {

                }
                else if (event_data.widget == widgets[menu_file])
                {
                    string dbname;
                    if (const auto error = icy::dialog_open_file(dbname))
                    {
                        ICY_ERROR(show_error(error, "Dialog open file"_s));
                        continue;
                    }
                    if (dbname.empty())
                        continue;

                    if (const auto error = create_resource_system(resource_system, dbname, 1_gb))
                    {
                        ICY_ERROR(show_error(error, "Initialize resource system"_s));
                        continue;
                    }
                    resource_list.clear();
                    if (const auto error = resource_system->list(resource_list))
                    {
                        ICY_ERROR(show_error(error, "Enumerate resources"_s));
                        resource_system = nullptr;
                        continue;
                    }

                    ICY_ERROR(imgui_display->widget_delete(widgets[widget::explorer_main]));
                    for (auto&& pair : resource_list)
                    {
                        auto widget = 0u;
                        ICY_ERROR(imgui_display->widget_create(widgets[window_explorer], imgui_widget_type::list_view, widget));

                    }

                    
                    

                }
                else if (event_data.widget == widgets[menu_explorer])
                {
                    ICY_ERROR(imgui_display->widget_state(widgets[window_explorer], imgui_widget_flag::none));
                }
                else if (event_data.widget == widgets[menu_import])
                {
                    ICY_ERROR(imgui_display->widget_state(widgets[window_import], imgui_widget_flag::none));
                }
                else if (event_data.widget == widgets[menu_export])
                {

                }
                else if (event_data.widget == widgets[import_execute])
                {
                    log.clear();
                    ICY_ERROR(imgui_display->widget_value(widgets[import_log], variant()));

                    if (!resource_system)
                    {
                        ICY_ERROR(print("Error: resource system database not selected"_s));
                        continue;
                    }

                }
                else if (event_data.widget == widgets[import_exec_filename])
                {
                    if (const auto error = icy::dialog_open_file(filename))
                    {
                        ICY_ERROR(show_error(error, "Dialog open file"_s));
                        continue;
                    }
                    if (filename.empty())
                        continue;

                    ICY_ERROR(imgui_display->widget_value(widgets[import_edit_filename], filename));
                    file_name fname;
                    ICY_ERROR(fname.initialize(filename));
                    ICY_ERROR(imgui_display->widget_value(widgets[import_edit_name], fname.name));
                    ICY_ERROR(copy(fname.name, name));
                }
                {
                    auto it = widgets_type.find(event_data.widget);
                    if (it != widgets_type.end())
                    {
                        type = it->value;
                        ICY_ERROR(imgui_display->widget_value(widgets[import_edit_type], to_string(type)));
                        continue;
                    }
                }
            }
            else if (event_data.type == imgui_event_type::widget_edit)
            {
                if (event_data.widget == widgets[import_edit_filename])
                {
                    ICY_ERROR(copy(event_data.value.str(), filename));
                }
                else if (event_data.widget == widgets[import_edit_name])
                {
                    ICY_ERROR(copy(event_data.value.str(), name));
                }
            }
        }
    }

    return error_type();
}
error_type resource_manager_application::show_error(const error_type& error, const string_view msg) noexcept
{
    string str_source;
    string msg_source;
    ICY_ERROR(to_string(error.source, str_source));
    ICY_ERROR(to_string("Source: \"%1\""_s, msg_source, string_view(str_source)));

    string str_code;
    string msg_code;
    ICY_ERROR(to_string_unsigned(error.code, 16, str_code));
    ICY_ERROR(to_string("Code: %1"_s, msg_code, string_view(str_code)));

    string str_detail;
    string msg_detail;
    to_string(error, str_detail);
    if (!str_detail.empty())
    {
        ICY_ERROR(to_string("Text: %1"_s, msg_detail, string_view(str_detail)));
    }

    ICY_ERROR(imgui_display->widget_value(widgets[popup_error_message], msg));
    ICY_ERROR(imgui_display->widget_value(widgets[popup_error_source], msg_source));
    ICY_ERROR(imgui_display->widget_value(widgets[popup_error_code], msg_code));
    ICY_ERROR(imgui_display->widget_value(widgets[popup_error_detail], msg_detail));
    ICY_ERROR(imgui_display->widget_state(widgets[popup_error], imgui_widget_flag::none));
    return error_type();
}


error_type main_ex(heap& heap)
{
    ICY_ERROR(event_system::initialize());
    ICY_ERROR(blob_initialize());
    ICY_SCOPE_EXIT{ blob_shutdown(); };

    resource_manager_application app;
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

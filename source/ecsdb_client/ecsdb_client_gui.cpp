#include "ecsdb_client_gui.hpp"
#include <array>
#include <icy_engine/core/icy_file.hpp>
#include <icy_engine/core/icy_set.hpp>

using namespace icy;

ICY_STATIC_NAMESPACE_BEG
enum ecsdb_gui_widget
{
    none,
    menu_main,
    menu_file,
    menu_file_new,
    menu_file_open,
    menu_file_close,
    menu_file_new_popup,
    menu_file_new_popup_name,
    menu_file_new_popup_status,
    menu_file_new_popup_create,
    menu_file_new_popup_cancel,
    popup_error,
    popup_error_message,
    popup_error_source,
    popup_error_code,
    popup_error_detail,
    menu_view,
    menu_view_explorer,
    menu_view_main,
    window_explorer,
    window_main,
    
    ecsdb_gui_total,
};
struct ecsdb_gui_directory
{
    enum
    {
        none,
        tree,
        menu,
        menu_open,
        menu_create,
        menu_delete,
        menu_rename,
        menu_cut,
        menu_paste,
        menu_create_directory,        
        menu_create_component,
        menu_create_entity,
        _total,
    };
    ecsdb_directory data;
    std::array<uint32_t, _total> widgets;
    set<guid> children;
};
class ecsdb_gui_system_data : public ecsdb_gui_system
{
public:
    error_type initialize(shared_ptr<imgui_display> display) noexcept;
    ~ecsdb_gui_system_data() noexcept
    {
        if (m_thread)
            m_thread->wait();
        filter(0);
    }
private:
    error_type exec() noexcept override;
    error_type signal(const icy::event_data* event) noexcept override
    {
        return m_sync.wake();
    }
    const icy::thread& thread() const noexcept override
    {
        return *m_thread;
    }
    error_type file_path(const string_view project, string& path) noexcept;
    error_type exec_menu_file_new() noexcept;
    error_type exec_menu_file_create(const string_view project) noexcept;
    error_type open(const string_view project, bool initialize = false) noexcept;
    error_type view_directory(const uint32_t parent, ecsdb_gui_directory& directory) noexcept;
    error_type show_error(const error_type& error, const string_view msg) noexcept;
private:
    sync_handle m_sync;
    shared_ptr<event_thread> m_thread;
    shared_ptr<imgui_display> m_display;
    shared_ptr<ecsdb_system> m_ecsdb;
    string m_project_dir;
    string m_project;
    map<string, uint32_t> m_project_list;
    std::array<uint32_t, ecsdb_gui_total> m_widgets;
    size_t m_capacity = 16_gb;
    map<guid, ecsdb_gui_directory> m_directories;
    ecsdb_gui_directory m_directory_root;
};
ICY_STATIC_NAMESPACE_END

error_type ecsdb_gui_system_data::initialize(const shared_ptr<imgui_display> display) noexcept
{
    ICY_ERROR(m_sync.initialize());
    ICY_ERROR(make_shared(m_thread));
    m_thread->system = this;
    m_display = display;
    
    string pname;
    ICY_ERROR(process_name(nullptr, pname));
    ICY_ERROR(copy(file_name(pname).directory, m_project_dir));

    const auto make_item = [this](const uint32_t parent, uint32_t index, const imgui_widget_type type, const string_view name)
    {
        ICY_ERROR(m_display->widget_create(m_widgets[parent], type, m_widgets[index]));
        ICY_ERROR(m_display->widget_label(m_widgets[index], name));
        ICY_ERROR(m_display->widget_udata(m_widgets[index], index));
        return error_type();
    };

    ICY_ERROR(m_display->widget_create(0u, imgui_widget_type::main_menu_bar, m_widgets[menu_main]));
    ICY_ERROR(make_item(menu_main, menu_file, imgui_widget_type::menu, "File"_s));
    ICY_ERROR(make_item(menu_file, menu_file_new, imgui_widget_type::menu_item, "New"_s));
    ICY_ERROR(make_item(menu_file, menu_file_open, imgui_widget_type::menu, "Open"_s));
    ICY_ERROR(make_item(menu_file, menu_file_close, imgui_widget_type::menu_item, "Close"_s));

    ICY_ERROR(m_display->widget_create(0u, imgui_widget_type::window, m_widgets[menu_file_new_popup]));

    ICY_ERROR(make_item(menu_file_new_popup, menu_file_new_popup_name, imgui_widget_type::line_input, string_view()));
    ICY_ERROR(make_item(menu_file_new_popup, menu_file_new_popup_status, imgui_widget_type::text, string_view()));
    ICY_ERROR(make_item(menu_file_new_popup, menu_file_new_popup_create, imgui_widget_type::button, "Create"_s));
    ICY_ERROR(make_item(menu_file_new_popup, menu_file_new_popup_cancel, imgui_widget_type::button, "Close"_s));

    ICY_ERROR(m_display->widget_state(m_widgets[menu_file_new_popup],
        imgui_window_state(imgui_window_flag::always_auto_resize | imgui_window_flag::no_title_bar, imgui_widget_flag::is_hidden)));
    ICY_ERROR(m_display->widget_state(m_widgets[menu_file_new_popup_cancel], imgui_widget_flag::is_same_line));

    auto widget_open_model = 0u;
    ICY_ERROR(m_display->widget_create(m_widgets[menu_file_open], imgui_widget_type::menu_item, widget_open_model));
    ICY_ERROR(m_display->widget_label(widget_open_model, "Model"_s));
    ICY_ERROR(m_display->widget_udata(widget_open_model, uint32_t(menu_file_open)));
    ICY_ERROR(m_display->widget_value(widget_open_model, true));
    ICY_ERROR(m_project_list.insert(string(), widget_open_model));

    auto widget_separator = 0u;
    ICY_ERROR(m_display->widget_create(m_widgets[menu_file_open], imgui_widget_type::separator, widget_separator));

    array<string> files;
    ICY_ERROR(enum_files(m_project_dir, files));

    for (auto&& file : files)
    {
        const auto fname = file_name(file);
        const auto key = "ecsdb_"_s;
        if (fname.name.find(key) == fname.name.begin() && fname.extension == ".dat"_s)
        {
            auto pname = string_view(fname.name.begin() + std::distance(key.begin(), key.end()), fname.name.end());
            auto widget = 0u;
            ICY_ERROR(m_display->widget_create(m_widgets[menu_file_open], imgui_widget_type::menu_item, widget));
            ICY_ERROR(m_display->widget_label(widget, pname));
            ICY_ERROR(m_display->widget_udata(widget, uint32_t(menu_file_open)));

            string str;
            ICY_ERROR(copy(pname, str));
            ICY_ERROR(m_project_list.insert(std::move(str), widget));
        }
    }

    ICY_ERROR(make_item(0u, popup_error, imgui_widget_type::window, "Error"_s));
    ICY_ERROR(make_item(popup_error, popup_error_message, imgui_widget_type::text, ""_s));
    ICY_ERROR(make_item(popup_error, popup_error_source, imgui_widget_type::text, ""_s));
    ICY_ERROR(make_item(popup_error, popup_error_code, imgui_widget_type::text, ""_s));
    ICY_ERROR(make_item(popup_error, popup_error_detail, imgui_widget_type::text, ""_s));
    ICY_ERROR(m_display->widget_state(m_widgets[popup_error],
        imgui_window_state(imgui_window_flag::always_auto_resize | imgui_window_flag::no_resize, imgui_widget_flag::is_hidden)));

    ICY_ERROR(make_item(menu_main, menu_view, imgui_widget_type::menu, "View"_s));
    ICY_ERROR(make_item(menu_view, menu_view_explorer, imgui_widget_type::menu_item, "Explorer"_s));
    ICY_ERROR(make_item(menu_view, menu_view_main, imgui_widget_type::menu_item, "Tabs"_s));

    ICY_ERROR(make_item(0u, window_explorer, imgui_widget_type::window, "Explorer"_s));
    ICY_ERROR(make_item(0u, window_main, imgui_widget_type::window, "Tabs"_s));
    ICY_ERROR(m_display->widget_state(m_widgets[window_explorer], imgui_widget_flag::is_hidden));
    ICY_ERROR(m_display->widget_state(m_widgets[window_main], imgui_widget_flag::is_hidden));

    ICY_ERROR(open(m_project, true));

    const uint64_t event_types = 0
        | event_type::system_internal
        | event_type::gui_update;

    filter(event_types);
    return error_type();
}
error_type ecsdb_gui_system_data::file_path(const string_view project, string& path) noexcept
{
    path.clear();
    ICY_ERROR(path.append(m_project_dir));
    ICY_ERROR(path.append("ecsdb"_s));
    if (!project.empty())
    {
        ICY_ERROR(path.append("_"_s));
        ICY_ERROR(path.append(project));
    }
    ICY_ERROR(path.append(".dat"_s));
    return error_type();
}
error_type ecsdb_gui_system_data::exec_menu_file_new() noexcept
{
    ICY_ERROR(m_display->widget_state(m_widgets[menu_file_new_popup], imgui_widget_flag::none));
    ICY_ERROR(m_display->widget_value(m_widgets[menu_file_new_popup_status], "Input new project name"_s));
    ICY_ERROR(m_display->widget_value(m_widgets[menu_file_new_popup_name], ""_s));
    return error_type();
}
error_type ecsdb_gui_system_data::exec_menu_file_create(const string_view project) noexcept
{
    if (project.empty())
    {
        ICY_ERROR(m_display->widget_value(m_widgets[menu_file_new_popup_status], "Error: empty name"_s));
    }
    else
    {
        string path;
        ICY_ERROR(file_path(project, path));
        file_info info;
        info.initialize(path);
        if (info.type != file_type::none)
        {
            ICY_ERROR(m_display->widget_value(m_widgets[menu_file_new_popup_status], "Error: file already exists"_s));
            return error_type();
        }

        string msg;
        ICY_ERROR(to_string("Wait: project '%1' is being created..."_s, msg, project));
        ICY_ERROR(m_display->widget_value(m_widgets[menu_file_new_popup_status], msg));

        string default_path;
        ICY_ERROR(file_path(string_view(), default_path));

        sleep(std::chrono::seconds(1));
        shared_ptr<ecsdb_system> system;
        ICY_ERROR(create_ecsdb_system(system, default_path, m_capacity));
        ICY_ERROR(system->copy(path));

        ICY_ERROR(to_string("Success: project '%1' has been created"_s, msg, project));
        ICY_ERROR(m_display->widget_value(m_widgets[menu_file_new_popup_status], msg));

        auto widget = 0u;
        ICY_ERROR(m_display->widget_create(m_widgets[menu_file_open], imgui_widget_type::menu_item, widget));
        ICY_ERROR(m_display->widget_label(widget, project));
        ICY_ERROR(m_display->widget_udata(widget, uint32_t(menu_file_open)));

        string copy_str;
        ICY_ERROR(copy(project, copy_str));
        ICY_ERROR(m_project_list.insert(std::move(copy_str), widget));
    }
    return error_type();
}
error_type ecsdb_gui_system_data::open(const string_view project, const bool initialize) noexcept
{
    if (!initialize && m_project == project)
        return error_type();

    auto next_widget = 0u;
    auto prev_widget = 0u;
    for (auto&& pair : m_project_list)
    {
        if (pair.key == project)
            next_widget = pair.value;
        if (pair.key == m_project)
            prev_widget = pair.value;
    }
    if (!next_widget || !prev_widget)
        return error_type();

    string path;
    ICY_ERROR(file_path(project, path));

    shared_ptr<ecsdb_system> ecsdb;
    if (const auto error = create_ecsdb_system(ecsdb, path, m_capacity))
    {
        string msg;
        ICY_ERROR(to_string("Error opening project '%1' (file: '%2')"_s, msg, project, string_view(path)));
        ICY_ERROR(show_error(error, msg));
        return error_type();
    }
    m_ecsdb = ecsdb;

    ICY_ERROR(m_display->widget_value(prev_widget, false));
    ICY_ERROR(m_display->widget_value(next_widget, true));

    ICY_ERROR(copy(project, m_project));
    if (auto window = m_display->handle())
    {
        string msg;
        ICY_ERROR(to_string("ECSDB Client GUI%1%2"_s, msg, project.empty() ? ""_s : ": "_s, project));
        ICY_ERROR(window->rename(msg));
    }

    ICY_ERROR(m_display->widget_delete(m_directory_root.widgets[ecsdb_gui_directory::tree]));
    m_directory_root = ecsdb_gui_directory();
    m_directories.clear();
    array<ecsdb_directory> dir_list;
    ICY_ERROR(m_ecsdb->enum_directories(dir_list));
    for (auto&& dir : dir_list)
    {
        ecsdb_gui_directory new_directory;
        new_directory.data = std::move(dir);
        ICY_ERROR(m_directories.insert(new_directory.data.index, std::move(new_directory)));
    }
    for (auto&& pair : m_directories)
    {
        const auto& value = pair.value.data;
        if (value.directory == ecsdb_root)
        {
            ICY_ERROR(m_directory_root.children.insert(pair.key));
        }
        else
        {
            auto it = m_directories.find(value.directory);
            if (it == m_directories.end())
                continue;

            ICY_ERROR(it->value.children.insert(pair.key));
        }
    }
    ICY_ERROR(copy("Root"_s, m_directory_root.data.name));
    ICY_ERROR(view_directory(m_widgets[window_explorer], m_directory_root));
    return error_type();
}
error_type ecsdb_gui_system_data::show_error(const error_type& error, const string_view msg) noexcept
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

    ICY_ERROR(m_display->widget_value(m_widgets[popup_error_message], msg));
    ICY_ERROR(m_display->widget_value(m_widgets[popup_error_source], msg_source));
    ICY_ERROR(m_display->widget_value(m_widgets[popup_error_code], msg_code));
    ICY_ERROR(m_display->widget_value(m_widgets[popup_error_detail], msg_detail));
    ICY_ERROR(m_display->widget_state(m_widgets[popup_error], imgui_widget_flag::none));
    return error_type();
}
error_type ecsdb_gui_system_data::view_directory(const uint32_t parent, ecsdb_gui_directory& directory) noexcept
{
    auto& widget = directory.widgets[ecsdb_gui_directory::tree];    
    const auto make_item = [this, &directory](const uint32_t parent, const uint32_t index, const imgui_widget_type type, const string_view name)
    {
        ICY_ERROR(m_display->widget_create(directory.widgets[parent], type, directory.widgets[index]));
        ICY_ERROR(m_display->widget_label(directory.widgets[index], name));
        ICY_ERROR(m_display->widget_udata(directory.widgets[index], directory.data.index));
        return error_type();
    };

    ICY_ERROR(m_display->widget_create(parent, imgui_widget_type::tree_view, widget));
    ICY_ERROR(m_display->widget_label(widget, directory.data.name));


    ICY_ERROR(make_item(ecsdb_gui_directory::tree, ecsdb_gui_directory::menu, imgui_widget_type::menu_context, string_view()));
    ICY_ERROR(make_item(ecsdb_gui_directory::menu, ecsdb_gui_directory::menu_open, imgui_widget_type::menu_item, "Open"_s));
    ICY_ERROR(make_item(ecsdb_gui_directory::menu, ecsdb_gui_directory::menu_create, imgui_widget_type::menu, "Create"_s));
    ICY_ERROR(make_item(ecsdb_gui_directory::menu, ecsdb_gui_directory::menu_rename, imgui_widget_type::menu_item, "Rename"_s));
    ICY_ERROR(make_item(ecsdb_gui_directory::menu, ecsdb_gui_directory::menu_cut, imgui_widget_type::menu_item, "Cut"_s));
    ICY_ERROR(make_item(ecsdb_gui_directory::menu, ecsdb_gui_directory::menu_paste, imgui_widget_type::menu_item, "Paste"_s));
    ICY_ERROR(make_item(ecsdb_gui_directory::menu, ecsdb_gui_directory::menu_delete, imgui_widget_type::menu_item, "Delete"_s));
    ICY_ERROR(make_item(ecsdb_gui_directory::menu_create, ecsdb_gui_directory::menu_create_directory, imgui_widget_type::menu_item, "Directory"_s));
    ICY_ERROR(make_item(ecsdb_gui_directory::menu_create, ecsdb_gui_directory::menu_create_component, imgui_widget_type::menu_item, "Component"_s));
    ICY_ERROR(make_item(ecsdb_gui_directory::menu_create, ecsdb_gui_directory::menu_create_entity, imgui_widget_type::menu_item, "Entity"_s));

    for (auto&& index : directory.children)
    {
        const auto it = m_directories.find(index);
        if (it == m_directories.end())
            continue;

        ICY_ERROR(view_directory(widget, it->value));
    }
    return error_type();
}
error_type ecsdb_gui_system_data::exec() noexcept
{
    string new_project;
    while (*this)
    {
        while (auto event = pop())
        {
            if (event->type == event_type::system_internal)
            {

            }
            else if (event->type == event_type::gui_update)
            {
                const auto& event_data = event->data<imgui_event>();
                switch (event_data.type)
                {
                case imgui_event_type::widget_click:
                {
                    auto widget_id = 0u;
                    guid uid;
                    if (event_data.udata.get(widget_id))
                    {
                        switch (widget_id)
                        {
                        case menu_file_new:
                            ICY_ERROR(exec_menu_file_new());
                            new_project.clear();
                            break;
                        case menu_file_new_popup_create:
                            ICY_ERROR(exec_menu_file_create(new_project));
                            break;
                        case menu_file_new_popup_cancel:
                            ICY_ERROR(m_display->widget_state(m_widgets[menu_file_new_popup], imgui_widget_flag::is_hidden));
                            break;
                        case menu_file_close:
                        case menu_file_open:
                        {
                            string_view project;
                            for (auto&& pair : m_project_list)
                            {
                                if (pair.value == event_data.widget)
                                {
                                    project = pair.key;
                                    break;
                                }
                            }
                            ICY_ERROR(open(project, false));
                            break;
                        }
                        case menu_view_explorer:
                            ICY_ERROR(m_display->widget_state(m_widgets[window_explorer], imgui_widget_flag::none));
                            break;
                        case menu_view_main:
                            ICY_ERROR(m_display->widget_state(m_widgets[window_main], imgui_widget_flag::none));
                            break;
                        }
                    }
                    else if (event_data.udata.get(uid))
                    {
                        auto dt = m_directories.find(uid);
                        if (dt != m_directories.end())
                        {
                            const auto& widgets = dt->value.widgets;
                            for (auto k = 1u; k < dt->value.widgets.size(); ++k)
                            {
                                if (widgets[k] != event_data.widget)
                                    continue;

                                switch (k)
                                {
                                case ecsdb_gui_directory::menu_create_directory:
                                {
                                    
                                    break;
                                }
                                }

                                break;
                            }
                        }
                    }
                    break;
                }
                case imgui_event_type::widget_edit:
                {
                    auto widget_id = 0u;
                    if (event_data.udata.get(widget_id))
                    {
                        switch (widget_id)
                        {
                        case menu_file_new_popup_name:
                            ICY_ERROR(copy(event_data.value.str(), new_project));
                            break;
                        default:
                            break;
                        }
                    }
                    break;
                }
                }
            }
        }
        ICY_ERROR(m_sync.wait());
    }
    return error_type();
}

error_type create_ecsdb_gui_system(shared_ptr<ecsdb_gui_system>& system, const shared_ptr<imgui_display> display) noexcept
{
    shared_ptr<ecsdb_gui_system_data> new_system;
    ICY_ERROR(make_shared(new_system));
    ICY_ERROR(new_system->initialize(display));
    system = std::move(new_system);
    return error_type();
}
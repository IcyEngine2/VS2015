#include <icy_qtgui/icy_xqtgui.hpp>
#include <icy_engine/core/icy_thread.hpp>
#include <icy_engine/core/icy_file.hpp>
#include "icy_dcl_gui.hpp"
#include "../core/icy_dcl_system.hpp"

#pragma comment(lib, "icy_engine_cored")
#pragma comment(lib, "icy_qtguid")
#pragma comment(lib, "icy_dcl_cored")

using namespace icy;


class dcl_application_ex : public icy::thread, public dcl_application
{
public:
    error_type init() noexcept;
    error_type exec() noexcept;
private:
    error_type run() noexcept override;
    gui_node root() const noexcept override
    {
        return m_dcl_model;
    }
    error_type show(const dcl_base& base) noexcept override;
    error_type menu(const dcl_gui_text& submenu, const dcl_gui_text& action) noexcept override;
    error_type context(const dcl_index index) noexcept override;
    dcl_writer* writer() noexcept override;
private:
    error_type names(const dcl_writer& reader, const dcl_type type, const dcl_index index, map<string, dcl_index>& map) const noexcept;
    error_type rename(string& str, bool& success) noexcept;
    error_type base_open(const bool create) noexcept;
    error_type base_close() noexcept;
    error_type patch_open(const bool create) noexcept;
    error_type patch_close() noexcept;
    error_type patch_apply() noexcept;
    error_type edit_undo() noexcept;
    error_type edit_redo() noexcept;
    error_type edit_actions() noexcept;
    error_type find_node(const dcl_index index, xgui_node& node, string* const name = nullptr) noexcept;
    error_type redo(const dcl_action& action) noexcept;
    error_type undo(const dcl_action& action) noexcept;
private:
    heap m_heap;
    dcl_index m_locale;
    shared_ptr<gui_queue> m_gui_system;
    shared_ptr<dcl_system> m_dcl_system;
    dcl_writer m_dcl_writer;
    dcl_flag m_dcl_flags = dcl_flag::destroyed | dcl_flag::hidden;
    gui_node m_dcl_model;
    map<gui_widget, unique_ptr<dcl_widget>> m_widgets;
};

ICY_DECLARE_GLOBAL(icy::global_gui);


error_type dcl_application_ex::init() noexcept
{
    ICY_ERROR(m_heap.initialize(heap_init::global(1_gb)));
    array<locale> loc;
    locale::enumerate(loc);
    
    ICY_ERROR(create_gui(m_gui_system));
    icy::global_gui = m_gui_system.get();
    ICY_ERROR(m_gui_system->create(m_dcl_model));
    return {};
}
error_type dcl_application_ex::show(const dcl_base& base) noexcept
{
    unique_ptr<dcl_widget> widget;
    ICY_ERROR(dcl_widget::create(*this, base, widget));
    ICY_ERROR(m_widgets.insert(widget->window(), std::move(widget)));
    return {};
}
error_type dcl_application_ex::run() noexcept
{
    ICY_SCOPE_EXIT{ event::post(nullptr, event_type::global_quit); };

    shared_ptr<event_queue> queue;
    ICY_ERROR(create_event_queue(queue, event_type::gui_any | event_type::window_close));
    dcl_base base;
    base.type = dcl_type::directory;
    base.index = dcl_root;
    ICY_ERROR(show(base));
    while (true)
    {
        event event;
        ICY_ERROR(queue->pop(event));
        if (event->type == event_type::global_quit)
            break;

        if (event->type == event_type::window_close)
        {
            if (shared_ptr<event_system>(event->source).get() == m_gui_system.get())
            {
                const auto& event_data = event->data<gui_event>();
                const auto find_widget = m_widgets.find(event_data.widget);
                if (find_widget != m_widgets.end())
                    m_widgets.erase(find_widget);
                if (m_widgets.empty())
                    break;
            }
        }
        else if (event->type & event_type::gui_any)
        {
            const auto& event_data = event->data<gui_event>();
            if (event->type == event_type::gui_context)
            {
                if (const auto error = context(event_data.data.as_node().udata().as_guid()))
                    ICY_ERROR(show_error(error, dcl_gui_text::Context));
            }

        }

        for (auto&& widget : m_widgets)
        {
            if (const auto error = widget->value->exec(event))
            {
                string_view name;
                m_dcl_writer.find_name(widget->value->base().index, dcl_index(), name);

                string msg;
                ICY_ERROR(to_string("widget (type: %1), ID: \"%2\""_s, msg, to_string(widget->value->type()), name));
                ICY_ERROR(show_error(error, msg));
            }
        }
    }
    return {};
}
error_type dcl_application_ex::exec() noexcept
{
    ICY_ERROR(launch());
    g_running = true;
    const auto error0 = m_gui_system->exec();
    g_running = false;
    const auto error1 = event::post(nullptr, event_type::global_quit);
    const auto error2 = wait();
    if (error0) return error0;
    if (error1) return error1;
    if (error2) return error2;
    return {};
}
error_type dcl_application_ex::menu(const dcl_gui_text& submenu, const dcl_gui_text& text) noexcept
{
    if (&submenu == &dcl_gui_text::Database)
    {
        if (&text == &dcl_gui_text::New)
            return base_open(true);
        else if (&text == &dcl_gui_text::Open)
            return base_open(false);
        else if (&text == &dcl_gui_text::Close)
            return base_close();
    }
    else if (&submenu == &dcl_gui_text::Patch)
    {
        if (&text == &dcl_gui_text::New)
            return patch_open(true);
        else if (&text == &dcl_gui_text::Open)
            return patch_open(false);
        else if (&text == &dcl_gui_text::Close)
            return patch_close();
        else if (&text == &dcl_gui_text::Apply)
            return patch_apply();
    }
    else if (&submenu == &dcl_gui_text::Edit)
    {
        if (&text == &dcl_gui_text::Undo)
            return edit_undo();
        else if (&text == &dcl_gui_text::Redo)
            return edit_redo();
        else if (&text == &dcl_gui_text::Actions)
            return edit_actions();
    }
    return make_stdlib_error(std::errc::function_not_supported);
}
/*
error_type dcl_application_ex::save() noexcept
{
    if (!m_dcl_writer)
        return {};
    dcl_patch patch;
    ICY_ERROR(patch.initialize(*m_dcl_writer));

    dcl_index user;
    string_view text;
    ICY_ERROR(patch.apply(m_dcl_system, user, text));
    return {};
}*/
error_type dcl_application_ex::edit_undo() noexcept
{
    if (m_dcl_writer.action() == 0)
        return {};
    
    const auto action = m_dcl_writer.actions()[m_dcl_writer.action() - 1];
    ICY_ERROR(undo(action));
    ICY_ERROR(m_dcl_writer.undo());
    return {};
}
error_type dcl_application_ex::edit_redo() noexcept
{
    if (m_dcl_writer.action() == m_dcl_writer.actions().size())
        return {};

    ICY_ERROR(m_dcl_writer.redo());
    const auto action = m_dcl_writer.actions()[m_dcl_writer.action() - 1];
    return redo(action);
}
error_type dcl_application_ex::edit_actions() noexcept
{
    return make_stdlib_error(std::errc::function_not_supported);
}
error_type dcl_application_ex::base_open(const bool create) noexcept
{    
    string str;
    dialog_filter filter;
    filter.name = "DCL Database"_s;
    filter.spec = "*.dcl-db"_s;
    ICY_ERROR(create ? dialog_save_file(str, { filter }) : dialog_open_file(str, { filter }));
    if (str.empty())
        return {};

    ICY_ERROR(base_close());

    if (create)
    {
        if (const auto error = file::remove(str))
        {
            if (error == make_stdlib_error(std::errc::no_such_file_or_directory))
                ;
            else
                return error;
        }
    }
    ICY_ERROR(create_dcl_system(m_dcl_system));
    ICY_ERROR(m_dcl_system->load(str));
    dcl_writer writer;
    ICY_ERROR(writer.initialize(m_dcl_system, ""_s));

    static error_type (*func)(dcl_writer& writer, const dcl_index index, const string_view name, dcl_application_ex& app, gui_node root) = nullptr;
    func = [](dcl_writer& writer, const dcl_index index, const string_view name, dcl_application_ex& app, gui_node root) -> error_type
    {
        ICY_ERROR(global_gui->text(root, name));
        ICY_ERROR(global_gui->udata(root, index));
        
        map<string, dcl_index> map;
        ICY_ERROR(app.names(writer, dcl_type::directory, index, map));
        if (map.empty())
            return error_type();

        ICY_ERROR(global_gui->insert_rows(root, 0, map.size()));
        auto row = 0_z;
        for (auto&& pair : map)
        {
            ICY_ERROR(func(writer, pair.value, pair.key, app, global_gui->node(root, row, 0)));
            ++row;
        }
        return error_type();
    };
    ICY_ERROR(m_gui_system->clear(m_dcl_model));
    ICY_ERROR(global_gui->insert_rows(m_dcl_model, 0, 1));
    string_view root_name;
    ICY_ERROR(writer.find_name(dcl_root, m_locale, root_name));
    ICY_ERROR(func(writer, dcl_root, root_name, *this, global_gui->node(m_dcl_model, 0, 0)));
    return {};
}
error_type dcl_application_ex::base_close() noexcept
{
    ICY_ERROR(m_gui_system->clear(m_dcl_model));
    m_dcl_writer = {};
    m_dcl_system = nullptr;
    return {};
}
error_type dcl_application_ex::patch_open(const bool create) noexcept
{
    if (!m_dcl_system)
        return {};

    string str;
    dialog_filter filter;
    filter.name = "DCL Patch"_s;
    filter.spec = "*.dcl-patch"_s;
    ICY_ERROR(create ? dialog_save_file(str, { filter }) : dialog_open_file(str, { filter }));
    if (str.empty())
        return {};

    ICY_ERROR(patch_close());

    if (create)
    {
        if (const auto error = file::remove(str))
        {
            if (error == make_stdlib_error(std::errc::no_such_file_or_directory))
                ;
            else
                return error;
        }
    }
    ICY_ERROR(m_dcl_writer.initialize(m_dcl_system, str));
    const auto action = m_dcl_writer.action();
    for (auto k = action; k; --k)
        ICY_ERROR(m_dcl_writer.undo());

    for (auto k = 0u; k < action; ++k)
    {
        ICY_ERROR(m_dcl_writer.redo());
        ICY_ERROR(redo(m_dcl_writer.actions()[k]));
    }
    return {};
}
error_type dcl_application_ex::patch_close() noexcept
{
    if (!m_dcl_system)
        return {};
    ICY_SCOPE_EXIT{ m_dcl_writer = {}; };
    for (auto k = m_dcl_writer.action(); k--;)
    {
        ICY_ERROR(undo(m_dcl_writer.actions()[k]));
    }
    return {};
}
error_type dcl_application_ex::patch_apply() noexcept
{
    return make_stdlib_error(std::errc::function_not_supported);
}
error_type dcl_application_ex::undo(const dcl_action& action) noexcept
{
    if (action.flag & dcl_flag::create_base)
    {
        dcl_base base;
        xgui_node node;
        ICY_ERROR(m_dcl_writer.find_base(action.base, base));
        ICY_ERROR(find_node(base.index, node));
        if (node)
            ICY_ERROR(node.parent().remove_rows(node.row(), 1));
    }
    else
    {
        return make_stdlib_error(std::errc::function_not_supported);
    }
    return {};
}
error_type dcl_application_ex::redo(const dcl_action& action) noexcept
{
    const auto rename = []
    {

    };
    if (action.flag & dcl_flag::create_base)
    {
        const auto base = dcl_base_from_action(action);
        if (base.type == dcl_type::directory)
        {
            xgui_node node;
            string name;
            ICY_ERROR(find_node(base.index, node, &name));
            if (node)
            {
                ICY_ERROR(node.parent().insert_rows(node.row(), 1));
                ICY_ERROR(node.udata(base.index));
                ICY_ERROR(node.text(name));
            }
        }
        else if (base.type == dcl_type::name)
        {
            string old_name;
            string new_name;
            ICY_ERROR(to_string(base.parent, old_name));
            xgui_node node_0;
            xgui_node node_1;
            ICY_ERROR(find_node(base.parent, node_0, &old_name));
            ICY_ERROR(find_node(base.parent, node_1, &new_name));
            if (node_0 && node_1)
            {
                if (node_0.row() != node_1.row())
                {
                    ICY_ERROR(node_0.parent().move_rows(node_0.row(), 1, node_1.row()));
                }
                ICY_ERROR(node_1.text(new_name));
            }
        }
    }
    else if (action.flag & dcl_flag::change_binary)
    {
        //  create new binary (unreferenced)
        if (!action.values[0].reference)
            return {};

        const auto old_text = action.values[0].reference;
        const auto new_text = action.values[1].reference;
        string_view old_name_view;
        string_view new_name_view;
        ICY_ERROR(m_dcl_writer.find_text(old_text, old_name_view));
        ICY_ERROR(m_dcl_writer.find_text(new_text, new_name_view));
        string old_name;
        string new_name;
        ICY_ERROR(copy(old_name_view, old_name));
        ICY_ERROR(copy(new_name_view, new_name));

        array<dcl_base> vec;
        ICY_ERROR(m_dcl_writer.find_type(dcl_type::name, vec));
        for (auto&& base : vec)
        {
            if (base.value.reference == old_text)
            {                
                xgui_node node_0;
                xgui_node node_1;
                ICY_ERROR(find_node(base.parent, node_0, &old_name));
                ICY_ERROR(find_node(base.parent, node_1, &new_name));
                if (node_0.row() != node_1.row())
                {
                    ICY_ERROR(node_0.parent().move_rows(node_0.row(), 1, node_1.row()));
                }
                ICY_ERROR(node_1.text(new_name));
            }
        }
    }
    else
    {
        return make_stdlib_error(std::errc::function_not_supported);
    }
    return {};
}
error_type dcl_application_ex::context(const dcl_index index) noexcept
{
    dcl_base base;
    ICY_ERROR(m_dcl_writer.find_base(index, base));

    xgui_widget menu;
    xgui_submenu menu_create;
    xgui_submenu menu_create_type;
    xgui_submenu menu_create_other;
    xgui_submenu menu_edit;
    map<dcl_type, xgui_action> create;
    map<dcl_type, xgui_action> create_type;
    map<dcl_type, xgui_action> create_other;
    xgui_action edit_rename;
    xgui_action edit_delete;
    xgui_action edit_hide;  //  hide/show
    xgui_action edit_lock;  //  readonly
    xgui_action open;
    xgui_action show_log;

    ICY_ERROR(menu.initialize(gui_widget_type::menu, {}));
    ICY_ERROR(menu_create.initialize(dcl_gui_text::New));
    ICY_ERROR(menu_create_type.initialize(dcl_gui_text::Type));
    ICY_ERROR(menu_create_other.initialize(dcl_gui_text::Other));
    ICY_ERROR(menu_edit.initialize(dcl_gui_text::Edit));
    ICY_ERROR(edit_rename.initialize(dcl_gui_text::Rename));
    ICY_ERROR(edit_delete.initialize(dcl_gui_text::Delete));
    ICY_ERROR(edit_hide.initialize(dcl_gui_text::Hide));
    ICY_ERROR(edit_lock.initialize(dcl_gui_text::Lock));
    ICY_ERROR(open.initialize(dcl_gui_text::Open));
    ICY_ERROR(show_log.initialize(dcl_gui_text::ShowLog));
  
    if (base.type == dcl_type::directory)
    {
        ICY_ERROR(create.insert(dcl_type::directory, xgui_action()));
        ICY_ERROR(create.insert(dcl_type::object_type, xgui_action()));
        ICY_ERROR(create_type.insert(dcl_type::class_type, xgui_action()));
        ICY_ERROR(create_type.insert(dcl_type::resource_class, xgui_action()));
        ICY_ERROR(create_type.insert(dcl_type::flags_class, xgui_action()));
        ICY_ERROR(create_type.insert(dcl_type::enum_class, xgui_action()));
        ICY_ERROR(create_other.insert(dcl_type::locale, xgui_action()));
        ICY_ERROR(create_other.insert(dcl_type::user, xgui_action()));
    }
    else
    {
        ICY_ERROR(menu_create.action.enable(false));
    }

    for (auto&& pair : create)
    {
        ICY_ERROR(pair.value.initialize(to_string(pair.key)));
        ICY_ERROR(menu_create.widget.insert(pair.value));
    }
    for (auto&& pair : create_type)
    {
        ICY_ERROR(pair.value.initialize(to_string(pair.key)));
        ICY_ERROR(menu_create_type.widget.insert(pair.value));
    }
    for (auto&& pair : create_other)
    {
        ICY_ERROR(pair.value.initialize(to_string(pair.key)));
        ICY_ERROR(menu_create_other.widget.insert(pair.value));
    }
    ICY_ERROR(menu_create.widget.insert(menu_create_type.action));
    ICY_ERROR(menu_create.widget.insert(menu_create_other.action));

    ICY_ERROR(menu_edit.widget.insert(edit_rename));
    ICY_ERROR(menu_edit.widget.insert(edit_delete));
    ICY_ERROR(menu_edit.widget.insert(edit_hide));
    ICY_ERROR(menu_edit.widget.insert(edit_lock));

    ICY_ERROR(menu.insert(menu_create.action));
    ICY_ERROR(menu.insert(menu_edit.action));
    ICY_ERROR(menu.insert(open));
    ICY_ERROR(menu.insert(show_log));

    gui_action action;
    ICY_ERROR(menu.exec(action));

    auto type = dcl_type::none;
    for (auto&& pair : create)
    {
        if (pair.value == action)
            type = pair.key;
    }
    for (auto&& pair : create_type)
    {
        if (pair.value == action)
            type = pair.key;
    }
    for (auto&& pair : create_other)
    {
        if (pair.value == action)
            type = pair.key;
    }
    const auto last_action = m_dcl_writer.action();

    if (action == edit_rename)
    {
        string_view text;
        ICY_ERROR(m_dcl_writer.find_name(index, m_locale, text));
        string str;
        ICY_ERROR(copy(text, str));
        auto success = false;
        ICY_ERROR(rename(str, success));
        if (!success)
            return {};

        ICY_ERROR(m_dcl_writer.change_name(index, m_locale, str));
    }

    if (type == dcl_type::directory)
    {
        auto success = false;
        string str;
        ICY_ERROR(rename(str, success));
        if (!success)
            return {};

        dcl_base new_base;
        new_base.type = type;
        new_base.parent = index;
        ICY_ERROR(m_dcl_writer.create_base(new_base));
        
        if (!str.empty())
            ICY_ERROR(m_dcl_writer.change_name(new_base.index, m_locale, str));        
    }
    else if (type != dcl_type::none)
    {
        dcl_base base;
        base.type = type;
        base.parent = index;
        ICY_ERROR(show(base));
    }

    for (auto k = last_action; k < m_dcl_writer.action(); ++k)
        ICY_ERROR(redo(m_dcl_writer.actions()[k]));
    
    return {};
}
error_type dcl_application_ex::rename(string& str, bool& success) noexcept
{
    xgui_widget dialog;
    ICY_ERROR(dialog.initialize(gui_widget_type::dialog_input_line, {}));
    ICY_ERROR(dialog.text(str));
    ICY_ERROR(dialog.show(true));

    shared_ptr<event_queue> loop;
    ICY_ERROR(create_event_queue(loop, event_type::gui_update));

    while (true)
    {
        event event;
        ICY_ERROR(loop->pop(event));
        if (event->type == event_type::global_quit)
            break;
        
        if (event->type == event_type::gui_update)
        {
            const auto& event_data = event->data<gui_event>();
            if (event_data.widget == dialog)
            {
                if (event_data.data.type() != gui_variant_type::none)
                {
                    ICY_ERROR(copy(event_data.data.as_string(), str));
                    success = true;
                }
                break;
            }
        }
    }
    return {};
}
dcl_writer* dcl_application_ex::writer() noexcept
{
    return m_dcl_writer ? &m_dcl_writer : nullptr;
}
error_type dcl_application_ex::names(const dcl_writer& reader, const dcl_type type, const dcl_index parent, map<string, dcl_index>& map) const noexcept
{
    icy::map<dcl_index, string> alt_names;
    for (auto&& pair : map)
    {
        string str;
        ICY_ERROR(copy(pair.key, str));
        ICY_ERROR(alt_names.insert(pair.value, std::move(str)));
    }
    map.clear();

    array<dcl_base> vec;
    ICY_ERROR(reader.find_children(parent, vec));
    for (auto&& base : vec)
    {
        if (base.flag & m_dcl_flags)
            continue;
        if (type != dcl_type::none && base.type != type)
            continue;
        string_view name;
        
        const auto it = alt_names.find(base.index);
        if (it != alt_names.end())
        {
            name = it->value;
        }
        else
        {
            ICY_ERROR(reader.find_name(base.index, m_locale, name));
        }

        size_t index = 0;
        while (true)
        {
            string str_name;
            ICY_ERROR(copy(name, str_name));
            if (str_name.empty())
                ICY_ERROR(str_name.appendf("{%1}"_s, base.index));
            if (index) 
                ICY_ERROR(str_name.appendf(" (%1)", index));

            if (const auto error = map.insert(std::move(str_name), base.index))
            {
                if (error != make_stdlib_error(std::errc::invalid_argument))
                    ICY_ERROR(error);
                ++index;
            }
            else
                break;
        }        
    }
    return {};
}
error_type dcl_application_ex::find_node(const dcl_index index, xgui_node& node, string* const name) noexcept
{
    if (index == dcl_root)
    {
        node = m_gui_system->node(m_dcl_model, 0, 0);
        if (name)
        {
            string_view str;
            ICY_ERROR(m_dcl_writer.find_name(index, m_locale, str));
            ICY_ERROR(copy(str, *name));
        }
        return {};
    }
    dcl_base base;
    ICY_ERROR(m_dcl_writer.find_base(index, base));
    if (base.type != dcl_type::directory || (base.flag & m_dcl_flags))
        return {};
    
    xgui_node parent_node;
    ICY_ERROR(find_node(base.parent, parent_node));
    if (parent_node)
    {
        map<string, dcl_index> map;
        if (name && !name->empty())
        {
            string str;
            ICY_ERROR(copy(*name, str));
            ICY_ERROR(map.insert(std::move(str), base.index));
        }

        ICY_ERROR(names(m_dcl_writer, dcl_type::directory, base.parent, map));
        
        auto row = 0_z;
        string_view str;
        for (auto&& pair : map)
        {
            if (pair.value == base.index)
            {
                str = pair.key;
                break;
            }
            ++row;
        }
        if (row == map.size())
            return {};

        node = parent_node.node(row, 0);
        if (!node)
            return make_stdlib_error(std::errc::not_enough_memory);
        if (name)
            ICY_ERROR(copy(str, *name));
    }
    return {};
}
int main()
{
    dcl_application_ex app;
    if (const auto error = app.init())
        return show_error(error, "Application init"_s), error.code;
    if (const auto error = app.exec())
        return show_error(error, "Application exec"_s), error.code;
    return 0;
}




/*error_type dcl_application_ex::offset(const dcl_base& base, size_t& offset, string* str) noexcept
{
    offset = 0;
    map<string, dcl_index> map;
    ICY_ERROR(names(dcl_type::directory, base.parent, map));
    for (auto&& pair : map)
    {
        if (pair.value == base.index)
        {
            if (str)
                ICY_ERROR(copy(pair.key, *str));
            break;
        }
        ++offset;
    }
    if (offset == map.size())
        offset = SIZE_MAX;
    return {};
}
error_type dcl_application_ex::rename(const dcl_base& base, gui_node node, const string_view* str) noexcept
{
    if (base.index == dcl_root)
    {
        if (str)
            ICY_ERROR(m_dcl_writer.change_name(base.index, m_locale, *str));
        ICY_ERROR(m_gui_system->text(node, *str));
        return {};
    }
    auto parent_node = m_dcl_nodes.find(base.parent);
    ICY_ASSERT(parent_node != m_dcl_nodes.end(), "SYSTEM CORRUPTED");

    if (!node)
    {
        auto old_row = 0_z;
        ICY_ERROR(offset(base, old_row));
        node = m_gui_system->node(parent_node->value, old_row, 0);
        if (!node)
            return make_stdlib_error(std::errc::not_enough_memory);
    }
    auto old_row = node.row();

    if (str)
        ICY_ERROR(m_dcl_writer.change_name(base.index, m_locale, *str));

    string new_str;
    auto new_row = 0_z;
    ICY_ERROR(offset(base, new_row, &new_str));
    ICY_ERROR(m_gui_system->text(node, new_str));
    if (old_row != new_row)
        ICY_ERROR(m_gui_system->move_rows(parent_node->value, old_row, 1, parent_node->value, new_row));

    return {};
}
*/
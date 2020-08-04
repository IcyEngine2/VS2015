#include "mbox_script_dbase.hpp"
#include "../mbox_script_system.hpp"
#include <icy_engine/core/icy_file.hpp>
#include <icy_engine/core/icy_set.hpp>
#include <icy_engine/image/icy_image.hpp>
#include "icons/directory.h"
#include "icons/profile.h"
#include "icons/timer.h"

#if _DEBUG
#pragma comment(lib, "mbox_script_systemd")
#pragma comment(lib, "icy_engine_imaged")
#else
#pragma comment(lib, "mbox_script_system")
#pragma comment(lib, "icy_engine_image")
#endif

using namespace icy;
using namespace mbox;


static error_type save_changes(const gui_widget parent, bool& save, bool& cancel) noexcept
{
    xgui_widget window;
    xgui_widget label;
    xgui_widget buttons;
    xgui_widget button_yes;
    xgui_widget button_no;

    ICY_ERROR(window.initialize(gui_widget_type::dialog, parent, gui_widget_flag::layout_vbox));
    ICY_ERROR(label.initialize(gui_widget_type::label, window));
    ICY_ERROR(buttons.initialize(gui_widget_type::none, window, gui_widget_flag::layout_hbox | gui_widget_flag::auto_insert));
    ICY_ERROR(button_yes.initialize(gui_widget_type::text_button, buttons));
    ICY_ERROR(button_no.initialize(gui_widget_type::text_button, buttons));
    ICY_ERROR(window.text("Warning!"_s));
    ICY_ERROR(label.text("Save changes?"_s));
    ICY_ERROR(button_yes.text("Yes"_s));
    ICY_ERROR(button_no.text("No"_s));
    ICY_ERROR(window.show(true));

    shared_ptr<event_queue> loop;
    ICY_ERROR(create_event_system(loop, event_type::gui_update));

    while (true)
    {
        event event;
        ICY_ERROR(loop->pop(event));
        if (event->type == event_type::global_quit)
            break;
        if (event->type == event_type::gui_update)
        {
            const auto& event_data = event->data<gui_event>();
            if (event_data.widget == button_yes)
            {
                save = true;
                break;
            }
            else if (event_data.widget == button_no)
            {
                break;
            }
            else if (event_data.widget == window)
            {
                cancel = true;
                break;
            }
        }
    }
    return error_type();
}
static error_type mbox_model_create(xgui_model& model, const mbox_array& data, const map<mbox_type, xgui_image>& icons, const map<mbox_index, bool>* const filter = nullptr) noexcept;
static error_type mbox_model_find(xgui_node& node, const mbox_object* const object, const gui_node root, const mbox_array& data, const map<mbox_index, bool>* const filter) noexcept;
class mbox_model_filter
{
public:
    error_type create(map<mbox_index, bool>& filter, const mbox_array& data, const mbox_object& source) noexcept;
public:
    virtual error_type operator()(mbox_object& source_copy, const mbox_object& modify_with) noexcept = 0;
};

error_type mbox_database::initialize() noexcept
{
    const auto add_icon = [this](const mbox_type type, const_array_view<uint8_t> bytes)
    {
        image image_data;
        ICY_ERROR(image_data.load(icy::global_realloc, nullptr, bytes, image_type::png));

        matrix<color> colors(image_data.size().y, image_data.size().x);
        if (colors.empty())
            return make_stdlib_error(std::errc::not_enough_memory);

        ICY_ERROR(image_data.view(image_size(), colors));

        xgui_image new_image;
        ICY_ERROR(new_image.initialize(colors));
        ICY_ERROR(m_icons.insert(type, std::move(new_image)));
        return error_type();
    };
    ICY_ERROR(add_icon(mbox_type::directory, g_bytes_directory));
    ICY_ERROR(m_model.initialize());

    ICY_ERROR(m_menu.menu_main.initialize(gui_widget_type::menubar, m_window));

    ICY_ERROR(m_menu.menu_file.initialize("File"_s));
    ICY_ERROR(m_menu.menu_main.insert(m_menu.menu_file.action));

    ICY_ERROR(m_menu.menu_edit.initialize("Edit"_s));
    ICY_ERROR(m_menu.menu_main.insert(m_menu.menu_edit.action));

    ICY_ERROR(m_menu.action_create.initialize("Create"_s));
    ICY_ERROR(m_menu.action_open.initialize("Open"_s));
    ICY_ERROR(m_menu.action_save.initialize("Save"_s));
    ICY_ERROR(m_menu.action_close.initialize("Close"_s));
    ICY_ERROR(m_menu.action_undo.initialize("Undo"_s));
    ICY_ERROR(m_menu.action_redo.initialize("Redo"_s));
    ICY_ERROR(m_menu.menu_file.widget.insert(m_menu.action_create));
    ICY_ERROR(m_menu.menu_file.widget.insert(m_menu.action_open));
    ICY_ERROR(m_menu.menu_file.widget.insert(m_menu.action_save));
    ICY_ERROR(m_menu.menu_file.widget.insert(m_menu.action_close));
    ICY_ERROR(m_menu.menu_edit.widget.insert(m_menu.action_undo));
    ICY_ERROR(m_menu.menu_edit.widget.insert(m_menu.action_redo));

    auto cancel = false;
    ICY_ERROR(close(cancel));

    ICY_ERROR(m_tree.initialize(gui_widget_type::tree_view, m_window));
    ICY_ERROR(m_tree.bind(m_model));
    ICY_ERROR(update());

    return error_type();
}
error_type mbox_database::exec(const event_type type, const gui_event& event) noexcept
{
    if (type == event_type::gui_action)
    {
        gui_action action;
        action.index = event.data.as_uinteger();
        if (action == m_menu.action_open || action == m_menu.action_create)
        {
            string file_name;
            ICY_ERROR(action == m_menu.action_open ? dialog_open_file(file_name) : dialog_save_file(file_name));
            if (!file_name.empty())
                ICY_ERROR(reset(file_name, action == m_menu.action_create ? open_mode::create : open_mode::open));
        }
        else if (action == m_menu.action_close)
        {
            auto cancel = false;
            ICY_ERROR(close(cancel));
        }
        else if (action == m_menu.action_save)
        {
            ICY_ERROR(save());
        }
        else if (action == m_menu.action_undo)
        {
            ICY_ERROR(undo());
        }
        else if (action == m_menu.action_redo)
        {
            ICY_ERROR(redo());
        }
    }
    else if (type == event_type::gui_context)
    {
        const auto node = event.data.as_node();
        if (event.widget == m_tree)
        {
            const auto index = node.udata().as_guid();
            ICY_ERROR(context(index));
        }
        else for (auto&& pair : m_windows)
        {
            if (event.widget == pair.value.tab_refs)
            {
                const auto index = node ? node.row() : SIZE_MAX;
                ICY_ERROR(pair.value.context(index));
                break;
            }
        }
    }
    else if (type == event_type::gui_select)
    {
        for (auto it = m_windows.begin(); it != m_windows.end(); ++it)
        {
            if (event.widget == it->value.tabs)
            {
                gui_widget widget;
                widget.index = event.data.as_uinteger();
                ICY_ERROR(it->value.on_focus(widget));
                break;
            }
        }
    }
    else if (type == event_type::gui_update)
    {
        for (auto it = m_windows.begin(); it != m_windows.end(); ++it)
        {
            if (event.widget == it->value.window)
            {
                auto cancel = false;
                ICY_ERROR(it->value.close(cancel));
                if (cancel)
                    break;
                m_windows.erase(it);
                break;
            }
            else if (event.widget == it->value.tab_text)
            {
                ICY_ERROR(copy(event.data.as_string(), it->value.object.value));
                ICY_ERROR(it->value.update());
            }
            else
            {
                auto done = false;
                for (auto&& pair : it->value.tools)
                {
                    if (pair.second == event.widget)
                    {
                        switch (pair.first)
                        {
                        case tool_type::save:
                            ICY_ERROR(it->value.save());
                            break;
                        case tool_type::undo:
                            ICY_ERROR(it->value.undo());
                            break;
                        case tool_type::redo:
                            ICY_ERROR(it->value.redo());
                            break;
                        case tool_type::compile:
                            ICY_ERROR(it->value.compile());
                            break;
                        }
                        done = true;
                        break;
                    }
                }
                if (done)
                    break;
            }
        }
    }
    else if (type == event_type::gui_double)
    {
        const auto node = event.data.as_node();
        if (event.widget == m_tree)
        {
            const auto object = m_objects.try_find(node.udata().as_guid());
            if (object && object->type != mbox_type::directory)
            {
                ICY_ERROR(on_edit(*object, false));
            }
        }
        else
        {
            for (auto&& pair : m_windows)
            {
                if (event.widget == pair.value.tab_refs)
                {
                    if (node && node.row() < pair.value.object.refs.size())
                    {
                        auto it = pair.value.object.refs.begin();
                        std::advance(it, node.row());
                        ICY_ERROR(pair.value.on_edit(it->key));
                    }
                    break;
                }
            }
        }
    }
    return error_type();
}
error_type mbox_database::close(bool& cancel) noexcept
{
    if (m_action)
    {
        auto yes_save = false;
        ICY_ERROR(save_changes(m_window, yes_save, cancel));
        if (cancel)
            return error_type();
        if (yes_save)
            ICY_ERROR(save());
    }

    m_action = 0;
    m_actions.clear();
    m_model.clear();
    m_objects.clear();
    m_path.clear();
    ICY_ERROR(m_menu.action_close.enable(false));
    ICY_ERROR(update());
    return error_type();
}
error_type mbox_database::reset(const string_view file_path, const open_mode mode) noexcept
{
    if (mode == open_mode::open)
    {
        auto saved_objects = std::move(m_objects);
        mbox_errors errors;
        ICY_ERROR(mbox_array::initialize(file_path, errors));
        std::swap(m_objects, saved_objects);
        auto cancel = false;
        ICY_ERROR(close(cancel));
        if (cancel)
            return error_type();
        std::swap(m_objects, saved_objects);
    }
    else
    {
        auto cancel = false;
        ICY_ERROR(close(cancel));
        if (cancel)
            return error_type();
        file::remove(file_path);
    }
    ICY_ERROR(copy(file_path, m_path));
    ICY_ERROR(update());
    if (mode == open_mode::open)
        ICY_ERROR(mbox_model_create(m_model, *this, m_icons, nullptr));
    
    ICY_ERROR(m_menu.action_close.enable(true));
    return error_type();
}
error_type mbox_database::launch(const mbox_index& party) noexcept
{
    xgui_widget window;
    ICY_ERROR(window.initialize(gui_widget_type::dialog, m_window, gui_widget_flag::layout_grid));

    xgui_text_edit main_widget;
    ICY_ERROR(main_widget.initialize(window));
    ICY_ERROR(main_widget.enable(false));

    const auto print = [](void* ptr, const string_view str)
    {
        const auto text = static_cast<xgui_text_edit*>(ptr);
        
        if (str.find("\r\n") == str.begin())
        {
            string time_str;
            ICY_ERROR(icy::to_string(clock_type::now(), time_str));
            string msg;
            ICY_ERROR(icy::to_string("\r\n[%1]: %2", msg, string_view(time_str), string_view(str.begin() + 2, str.end())));
            ICY_ERROR(text->append(msg));
        }
        else
        {
            ICY_ERROR(text->append(str));
        }

        return error_type();
    };
    {
        string time_str;
        ICY_ERROR(icy::to_string(clock_type::now(), time_str));
        string name;
        ICY_ERROR(rpath(party, name));
        string msg;
        ICY_ERROR(icy::to_string("[%1]: Launch party \"%2\"", msg, string_view(time_str), string_view(name)));
        ICY_ERROR(main_widget.append(msg));
    }

    shared_ptr<mbox_system> system;
    if (const auto error = create_event_system(system, *this, party, print, &main_widget))
    {
        if (error != make_mbox_error(mbox_error::execute_lua))
            return error;
    }

    const mbox_system_info null_info;
    const mbox_system_info* info = system ? &system->info() : &null_info;

    array<xgui_widget> labels;
    array<xgui_widget> widgets;

    auto column = 0u;
    for (auto&& chr : info->characters)
    {
        string label;
        ICY_ERROR(icy::to_string("[%1] %2"_s, label, chr.position, string_view(m_objects.try_find(chr.index)->name)));

        xgui_widget new_label;
        ICY_ERROR(new_label.initialize(gui_widget_type::label, window));
        ICY_ERROR(new_label.text(label));
        ICY_ERROR(new_label.insert(gui_insert(column, 0)));
        ICY_ERROR(labels.push_back(std::move(new_label)));

        xgui_widget new_widget;
        ICY_ERROR(new_widget.initialize(gui_widget_type::text_edit, window));
        ICY_ERROR(new_widget.insert(gui_insert(column, 1)));
        ICY_ERROR(widgets.push_back(std::move(new_widget)));

        ++column;
    }
    ICY_ERROR(main_widget.insert(gui_insert(0, 2, column ? column : 1, 1)));

    shared_ptr<event_queue> loop;
    ICY_ERROR(create_event_system(loop, event_type::gui_update | event_type::gui_select));

    ICY_ERROR(window.show(true));

    while (true)
    {
        event event;
        ICY_ERROR(loop->pop(event));
        if (event->type == event_type::global_quit)
            break;
        if (event->type == event_type::gui_update)
        {
            const auto& event_data = event->data<gui_event>();
            if (event_data.widget == window)
            {
                return error_type();
            }
        }
        else if (event->type == mbox_event_type_send_input)
        {

        }
    }
    return error_type();
}
error_type mbox_database::on_move(mbox_object& object) noexcept
{
    class filter_type : public mbox_model_filter
    {
    public:
        filter_type(mbox_database& self, const mbox_object& object) : self(self)
        {
            if (object.type == mbox_type::account ||
                object.type == mbox_type::party ||
                object.type == mbox_type::character)
            {
                const mbox_object* parent_ptr = &object;
                while (parent_ptr)
                {
                    if (parent_ptr->type == mbox_type::profile)
                    {
                        old_profile = parent_ptr->index;
                        break;
                    }
                    parent_ptr = self.find(parent_ptr->parent);
                }
            }
        }
        mbox_database& self;
        mbox_index old_profile;

        error_type operator()(mbox_object& source_copy, const mbox_object& modify_with) noexcept
        {
            set<mbox_index> parent_indices;
            mbox_index new_profile;
            {
                auto parent_ptr = &modify_with;                
                while (parent_ptr)
                {
                    if (parent_ptr->type == mbox_type::profile)
                        new_profile = parent_ptr->index;
                    ICY_ERROR(parent_indices.insert(parent_ptr->index));
                    parent_ptr = self.find(parent_ptr->parent);
                }
            }
            if (parent_indices.try_find(source_copy.index))
            {
                source_copy = mbox_object();
                return error_type();
            }

            if (old_profile && new_profile != old_profile)
            {
                source_copy = mbox_object();
                return error_type();
            }

            source_copy.index = mbox_index::create();
            if (!source_copy.index)
                return last_system_error();

            source_copy.parent = modify_with.index;
            return error_type();
        }
    };
    
    map<mbox_index, bool> filter;
    ICY_ERROR(filter_type(*this, object).create(filter, *this, object));

    xgui_model model;
    ICY_ERROR(model.initialize());
    ICY_ERROR(mbox_model_create(model, *this, m_icons, &filter));

    xgui_widget window;
    xgui_widget view;
    xgui_widget buttons;
    xgui_widget button_select;
    xgui_widget button_cancel;

    ICY_ERROR(window.initialize(gui_widget_type::window, m_window, gui_widget_flag::layout_vbox));
    ICY_ERROR(view.initialize(gui_widget_type::tree_view, window));
    ICY_ERROR(buttons.initialize(gui_widget_type::none, window, gui_widget_flag::layout_hbox | gui_widget_flag::auto_insert));
    ICY_ERROR(button_select.initialize(gui_widget_type::text_button, buttons));
    ICY_ERROR(button_cancel.initialize(gui_widget_type::text_button, buttons));

    ICY_ERROR(view.bind(model));
    ICY_ERROR(window.text("Move to..."_s));
    ICY_ERROR(button_select.text("Select"_s));
    ICY_ERROR(button_cancel.text("Cancel"_s));

    mbox_index select_index = object.parent;
    xgui_node select_node;
    ICY_ERROR(mbox_model_find(select_node, find(select_index), model, *this, &filter));
    ICY_ERROR(view.scroll(select_node));

    shared_ptr<event_queue> loop;
    ICY_ERROR(create_event_system(loop, event_type::gui_update | event_type::gui_select));
    ICY_ERROR(window.show(true));

    while (true)
    {
        event event;
        ICY_ERROR(loop->pop(event));
        if (event->type == event_type::global_quit)
            break;

        const auto& event_data = event->data<gui_event>();

        if (event->type == event_type::gui_update)
        {
            if (event_data.widget == button_select)
            {
                break;
            }
            else if (event_data.widget == button_cancel)
            {
                return error_type();
            }
            else if (event_data.widget == window)
            {
                return error_type();
            }
        }
        else if (event->type == event_type::gui_select)
        {
            select_index = event_data.data.as_node().udata().as_guid();
        }
    }

    mbox_object copy_object;
    ICY_ERROR(copy(object, copy_object));
    copy_object.parent = select_index;
    auto is_valid = false;
    ICY_ERROR(validate(copy_object, is_valid));

    mbox_object new_action;
    new_action = std::move(copy_object);
    ICY_ERROR(execute(new_action));
    array<mbox_object> new_actions;
    ICY_ERROR(new_actions.push_back(std::move(new_action)));
    ICY_ERROR(push(std::move(new_actions)));
    return error_type();
}
error_type mbox_database::on_delete(mbox_object& object) noexcept
{
    array<mbox_object> new_actions;
    set<mbox_index> to_delete;

    ICY_ERROR(to_delete.insert(object.index));
    if (!to_delete.empty())
    {
        while (true)
        {
            const auto old_size = to_delete.size();
            for (auto&& pair : m_objects)
            {
                if (to_delete.try_find(pair.value.parent))
                {
                    if (const auto error = to_delete.insert(pair.key))
                    {
                        if (error != make_stdlib_error(std::errc::invalid_argument))
                            return error;
                    }
                }
            }
            if (to_delete.size() == old_size)
                break;
        }
    }
    set<mbox_index> to_modify;
    for (auto&& pair : m_objects)
    {
        if (to_delete.try_find(pair.key))
            continue;

        for (auto&& ref_index : pair.value.refs)
        {
            if (to_delete.try_find(ref_index.value))
            {
                ICY_ERROR(to_modify.insert(pair.key));
                break;
            }
        }
    }
    if (!to_modify.empty())
    {
        xgui_model model;
        array<mbox_object> modify_list;
        for (auto&& modify_index : to_modify)
        {
            mbox_object new_object;
            ICY_ERROR(copy(*m_objects.try_find(modify_index), new_object));
            ICY_ERROR(modify_list.push_back(std::move(new_object)));
        }
        std::sort(modify_list.begin(), modify_list.end());
        ICY_ERROR(model.initialize());
        ICY_ERROR(model.insert_rows(0, modify_list.size()));
        ICY_ERROR(model.insert_cols(0, 2));
        ICY_ERROR(model.vheader(0, "Name"_s));
        ICY_ERROR(model.vheader(1, "Type"_s));
        ICY_ERROR(model.vheader(2, "Path"_s));
        auto row = 0u;
        for (auto&& obj : modify_list)
        {
            if (auto icon = m_icons.try_find(obj.type))
                ICY_ERROR(model.node(row, 0).icon(*icon));
            ICY_ERROR(model.node(row, 0).text(obj.name));
            ICY_ERROR(model.node(row, 0).udata(obj.index));
            ICY_ERROR(model.node(row, 1).text(icy::to_string(obj.type)));

            string path;
            ICY_ERROR(rpath(obj.index, path));
            ICY_ERROR(model.node(row, 2).text(path));

            ++row;
        }


        xgui_widget window;
        ICY_ERROR(window.initialize(gui_widget_type::window, m_window, gui_widget_flag::layout_vbox));
        ICY_ERROR(window.text("Warning!"_s));

        xgui_widget label;
        ICY_ERROR(label.initialize(gui_widget_type::label, window));
        ICY_ERROR(label.text("This operation will modify selected elements! Continue?"_s));

        xgui_widget tree;
        ICY_ERROR(tree.initialize(gui_widget_type::grid_view, window));
        ICY_ERROR(tree.bind(model));

        xgui_widget buttons;
        xgui_widget button_yes;
        xgui_widget button_no;
        ICY_ERROR(buttons.initialize(gui_widget_type::none, window, gui_widget_flag::layout_hbox | gui_widget_flag::auto_insert));
        ICY_ERROR(button_yes.initialize(gui_widget_type::text_button, buttons));
        ICY_ERROR(button_no.initialize(gui_widget_type::text_button, buttons));
        ICY_ERROR(button_yes.text("Yes"_s));
        ICY_ERROR(button_no.text("No"_s));

        ICY_ERROR(window.show(true));
        shared_ptr<event_queue> loop;
        ICY_ERROR(create_event_system(loop, event_type::gui_update));
        while (true)
        {
            event event;
            ICY_ERROR(loop->pop(event));
            if (event->type == event_type::global_quit)
                return error_type();

            if (event->type == event_type::gui_update)
            {
                auto& event_data = event->data<gui_event>();
                if (event_data.widget == window ||
                    event_data.widget == button_no)
                    return error_type();
                else if (event_data.widget == button_yes)
                    break;
            }
        }
    }
    for (auto&& mod_index : to_modify)
    {
        const auto ptr = m_objects.try_find(mod_index);
        mbox_object new_action;
        ICY_ERROR(copy(*ptr, new_action));
        new_action.refs.clear();
        for (auto it = ptr->refs.begin(); it != ptr->refs.end(); ++it)
        {
            if (!to_delete.try_find(it->value))
            {
                string copy_key;
                ICY_ERROR(copy(it->key, copy_key));
                ICY_ERROR(new_action.refs.insert(std::move(copy_key), it->value));
            }
        }
        ICY_ERROR(new_actions.push_back(std::move(new_action)));
    }

    map<uint32_t, array<mbox_object>> sorted_actions;
    for (auto&& del_index : to_delete)
    {
        auto level = 0u;

        auto ptr = m_objects.try_find(del_index);
        mbox_object new_action;
        ICY_ERROR(copy(*ptr, new_action));

        while (ptr)
        {
            ++level;
            ptr = m_objects.try_find(ptr->parent);
        }
        auto level_find = sorted_actions.find(level);
        if (level_find == sorted_actions.end())
            sorted_actions.insert(level, array<mbox_object>(), &level_find);

        new_action.name.clear();
        ICY_ERROR(level_find->value.push_back(std::move(new_action)));
    }
    //  change refs; delete level N; ... ; delete level 1;
    for (auto it = sorted_actions.rbegin(); it != sorted_actions.rend(); ++it)
    {
        ICY_ERROR(new_actions.append(
            std::make_move_iterator(it->value.begin()),
            std::make_move_iterator(it->value.end())));
    }
    for (auto&& action : new_actions)
        ICY_ERROR(execute(action));

    ICY_ERROR(push(std::move(new_actions)));
    return error_type();
}
error_type mbox_database::on_edit(const mbox_object& object, const bool always_open) noexcept
{
    const auto find = m_windows.find(object.index);
    if (find != m_windows.end())
    {
        ICY_ERROR(find->value.window.show(true));
        return error_type();
    }
    if (always_open == false)
    {
        auto has_children = false;
        for (auto&& pair : m_objects)
        {
            if (pair.value.parent == object.index)
            {
                has_children = true;
                break;
            }
        }
        if (has_children)
            return error_type();
    }

    window_type new_window(this);
    ICY_ERROR(new_window.initialize(object));
    ICY_ERROR(new_window.window.show(true));
    ICY_ERROR(m_windows.insert(object.index, std::move(new_window)));
    return error_type();
}
error_type mbox_database::context(const mbox_index& index) noexcept
{
    xgui_widget menu;
    ICY_ERROR(menu.initialize(gui_widget_type::menu, xgui_widget()));

    xgui_submenu menu_create;
    xgui_action action_rename;
    xgui_action action_move;
    xgui_action action_destroy;
    xgui_action action_launch;
    xgui_action action_edit;

    array<mbox_type> types;

    ICY_ERROR(menu_create.initialize("Create"_s));

    const auto new_object_index = mbox_index::create();
    if (!new_object_index)
        return last_system_error();

    if (index != mbox_index())
    {
        const auto object = m_objects.try_find(index);
        if (!object)
            return make_stdlib_error(std::errc::invalid_argument);

        ICY_ERROR(action_rename.initialize("Rename"_s));
        ICY_ERROR(action_move.initialize("Move"_s));
        ICY_ERROR(action_destroy.initialize("Destroy"_s));
        ICY_ERROR(action_launch.initialize("Launch"_s));
        ICY_ERROR(action_edit.initialize("Edit"_s));

        ICY_ERROR(menu.insert(action_rename));
        ICY_ERROR(menu.insert(action_move));
        ICY_ERROR(menu.insert(action_destroy));
        ICY_ERROR(menu.insert(action_launch));
        ICY_ERROR(menu.insert(action_edit));

        ICY_ERROR(action_launch.enable(object->type == mbox_type::party));
        ICY_ERROR(action_edit.enable(!(object->type == mbox_type::directory || object->type == mbox_type::group)));

        mbox_object new_object;
        new_object.parent = index;
        new_object.index = new_object_index;
        ICY_ERROR(copy(" "_s, new_object.name));

        for (auto type = mbox_type::directory; type != mbox_type::_last; type = mbox_type(uint32_t(type) + 1))
        {
            new_object.type = type;
            array<error_type> errors;
            if (const auto error = validate(new_object, errors))
            {
                if (error == make_mbox_error(mbox_error::database_corrupted))
                    continue;
                else
                    return error;
            }
            ICY_ERROR(types.push_back(type));
        }
    }
    else
    {
        ICY_ERROR(types.push_back(mbox_type::directory));
        ICY_ERROR(types.push_back(mbox_type::profile));
    }

    map<mbox_type, xgui_action> actions;
    if (!types.empty())
    {
        ICY_ERROR(menu.insert(menu_create.action));
        for (auto&& type : types)
        {
            xgui_action new_action;
            ICY_ERROR(new_action.initialize(icy::to_string(type)));
            ICY_ERROR(menu_create.widget.insert(new_action));

            const auto icon = m_icons.find(type);
            if (icon != m_icons.end())
                ICY_ERROR(new_action.icon(icon->value));

            ICY_ERROR(actions.insert(type, std::move(new_action)));
        }

    }
    gui_action action;
    ICY_ERROR(menu.exec(action));

    for (auto&& pair : actions)
    {
        if (pair.value != action)
            continue;

        string str;
        ICY_ERROR(xgui_rename(str));
        if (str.empty())
            return error_type();

        mbox_object new_action;
        new_action.index = new_object_index;
        new_action.type = pair.key;
        new_action.parent = index;
        ICY_ERROR(copy(str, new_action.name));
        ICY_ERROR(execute(new_action));
        array<mbox_object> new_actions;
        ICY_ERROR(new_actions.push_back(std::move(new_action)));
        ICY_ERROR(push(std::move(new_actions)));
        break;
    }
    const auto find_object = m_objects.find(index);
    if (find_object != m_objects.end())
    {
        if (action == action_rename)
        {
            string str;
            ICY_ERROR(copy(find_object->value.name, str));
            ICY_ERROR(xgui_rename(str));
            if (str.empty())
                return make_mbox_error(mbox_error::invalid_object_name);

            if (find_object->value.name != str)
            {
                mbox_object new_action;
                ICY_ERROR(copy(find_object->value, new_action));
                new_action.name = std::move(str);
                ICY_ERROR(execute(new_action));
                array<mbox_object> new_actions;
                ICY_ERROR(new_actions.push_back(std::move(new_action)));
                ICY_ERROR(push(std::move(new_actions)));
            }
        }
        else if (action == action_move)
        {
            ICY_ERROR(on_move(find_object->value));
        }
        else if (action == action_destroy)
        {
            ICY_ERROR(on_delete(find_object->value));
        }
        else if (action == action_edit)
        {
            ICY_ERROR(on_edit(find_object->value, true));
        }
        else if (action == action_launch)
        {
            ICY_ERROR(launch(find_object->value.index));
        }
    }

    return error_type();
}
error_type mbox_database::update() noexcept
{
    if (!m_path.empty())
    {
        const file_name fname(m_path);
        string window_name;
        ICY_ERROR(icy::to_string("%1%2 (%3)"_s, window_name, m_action == 0 ? ""_s : "* "_s, fname.name, string_view(m_path)));

        auto gui = shared_ptr<gui_queue>(global_gui);
        if (gui)
            ICY_ERROR(gui->text(m_window, window_name));
    }
    else
    {
        auto gui = shared_ptr<gui_queue>(global_gui);
        if (gui)
            ICY_ERROR(gui->text(m_window, "MBox Script Editor"_s));
    }

    ICY_ERROR(m_menu.action_undo.enable(m_action > 0));
    ICY_ERROR(m_menu.action_redo.enable(m_action < m_actions.size()));
    ICY_ERROR(m_menu.action_save.enable(m_action > 0));
    return error_type();
}
error_type mbox_database::save() noexcept
{
    if (m_actions.empty())
        return error_type();

    ICY_ERROR(mbox_array::save(m_path, 64_mb));
    m_actions.clear();
    m_action = 0;
    ICY_ERROR(update());

    return error_type();
}
error_type mbox_database::undo() noexcept
{
    if (m_action == 0)
        return error_type();

    auto& actions = m_actions[--m_action];
    for (auto it = actions.rbegin(); it != actions.rend(); ++it)
    {
        ICY_ERROR(execute(*it));
    }
    ICY_ERROR(update());
    return error_type();
}
error_type mbox_database::redo() noexcept
{
    if (m_action == m_actions.size())
        return error_type();

    auto& actions = m_actions[m_action++];
    for (auto it = actions.begin(); it != actions.end(); ++it)
    {
        ICY_ERROR(execute(*it));
    }
    ICY_ERROR(update());
    return error_type();
}
error_type mbox_database::push(array<mbox_object>&& actions) noexcept
{
    for (auto k = m_actions.size(); k > m_action; --k)
        m_actions.pop_back();

    ICY_ERROR(m_actions.push_back(std::move(actions)));
    m_action++;

    ICY_ERROR(update());
    return error_type();
}
error_type mbox_database::execute(mbox_object& action) noexcept
{
    const auto find_object = m_objects.find(action.index);
    if (find_object != m_objects.end())
    {
        const auto& object = find_object->value;

        xgui_node node;
        ICY_ERROR(mbox_model_find(node, &object, m_model, *this, nullptr));

        if (action.name.empty())
        {
            ICY_ERROR(node.parent().remove_rows(node.row(), 1));
            std::swap(action.name, find_object->value.name);
            m_objects.erase(find_object);
        }
        else if (action.name != object.name)
        {
            xgui_node new_node;
            ICY_ERROR(mbox_model_find(new_node, &action, m_model, *this, nullptr));

            if (new_node.row() != node.row())
                ICY_ERROR(node.parent().move_rows(node.row(), 1, new_node.row()));

            ICY_ERROR(new_node.text(action.name));
            std::swap(action.name, find_object->value.name);
        }
        else if (action.parent != object.parent)
        {
            auto gui = shared_ptr<gui_queue>(global_gui);
            xgui_node new_node;
            ICY_ERROR(mbox_model_find(new_node, &action, m_model, *this, nullptr));

            if (gui)
                ICY_ERROR(gui->move_rows(node.parent(), node.row(), 1, new_node.parent(), new_node.row()));
            std::swap(action.parent, find_object->value.parent);
        }
        else if (action.value != object.value)
        {
            std::swap(action.value, find_object->value.value);
        }
        else if (action.refs.keys() != object.refs.keys() || action.refs.vals() != object.refs.vals())
        {
            std::swap(action.refs, find_object->value.refs);
        }
    }
    else
    {
        xgui_node node;
        ICY_ERROR(mbox_model_find(node, &action, m_model, *this, nullptr));
        
        ICY_ERROR(node.parent().insert_rows(node.row(), 1));
        ICY_ERROR(node.udata(action.index));
        ICY_ERROR(node.text(action.name));
        const auto icon = m_icons.find(action.type);
        if (icon != m_icons.end())
            ICY_ERROR(node.icon(icon->value));

        mbox_object new_object;
        ICY_ERROR(copy(action, new_object));
        ICY_ERROR(m_objects.insert(action.index, std::move(new_object)));
        action.name.clear();
    }
    return error_type();
}

error_type mbox_database::window_type::initialize(const mbox_object& object) noexcept
{
    ICY_ERROR(refs_model.initialize());
    ICY_ERROR(refs_model.hheader(0, "Name"_s));

    ICY_ERROR(window.initialize(gui_widget_type::window, gui_widget(), gui_widget_flag::layout_vbox));
    ICY_ERROR(toolbar.initialize(gui_widget_type::none, window, gui_widget_flag::layout_hbox | gui_widget_flag::auto_insert));
    ICY_ERROR(tabs.initialize(gui_widget_type::tabs, window));
    ICY_ERROR(tab_text.initialize(tabs));
    ICY_ERROR(tab_refs.initialize(gui_widget_type::grid_view, tabs));
    ICY_ERROR(tab_debug.initialize(tabs));
    ICY_ERROR(tab_refs.bind(refs_model));
    ICY_ERROR(tab_debug.enable(false));
    
    auto gui = shared_ptr<gui_queue>(global_gui);
    ICY_ERROR(gui->text(tabs, tab_refs, "Refs"_s));
    ICY_ERROR(gui->text(tabs, tab_debug, "Debug"_s));
    
    string_view text;
    switch (object.type)
    {
    case mbox_type::profile:
        text = "LUA Profile OnCreate"_s;
        break;

    case mbox_type::account:
        text = "LUA Account OnCreate"_s;
        break;

    case mbox_type::character:
        text = "LUA Character OnCreate"_s;
        break;

    case mbox_type::party:
        text = "LUA Party OnCreate"_s;
        break;

    case mbox_type::group:
        text = "LUA Group OnJoin"_s;
        break;

    case mbox_type::action_timer:
        text = "LUA OnTimer"_s;
        break;

    case mbox_type::action_script:
        text = "LUA Script"_s;
        break;

    case mbox_type::action_macro:
        text = "Text Macro"_s;
        break;

    case mbox_type::layout:
        text = "HTML Document"_s;
        break;

    case mbox_type::style:
        text = "CSS Document"_s;
        break;

    default:
        return make_stdlib_error(std::errc::invalid_argument);
    }

    ICY_ERROR(gui->text(tabs, tab_text, text));
    ICY_ERROR(copy(object, this->object));
    ICY_ERROR(tab_text.text(object.value));
        
    ICY_ERROR(remodel());
    ICY_ERROR(on_focus(tab_text));
    
    return error_type();
}
error_type mbox_database::window_type::on_focus(gui_widget new_focus) noexcept
{
    tools.clear();

    xgui_widget tool_save;
    ICY_ERROR(tool_save.initialize(gui_widget_type::text_button, toolbar));
    ICY_ERROR(tool_save.text("Save"_s));
    ICY_ERROR(tools.push_back(std::make_pair(tool_type::save, std::move(tool_save))));
    if (new_focus == tab_refs)
    {
        xgui_widget tool_undo;
        ICY_ERROR(tool_undo.initialize(gui_widget_type::text_button, toolbar));
        ICY_ERROR(tool_undo.text("Undo"_s));
        ICY_ERROR(tools.push_back(std::make_pair(tool_type::undo, std::move(tool_undo))));

        xgui_widget tool_redo;
        ICY_ERROR(tool_redo.initialize(gui_widget_type::text_button, toolbar));
        ICY_ERROR(tool_redo.text("Redo"_s));
        ICY_ERROR(tools.push_back(std::make_pair(tool_type::redo, std::move(tool_redo))));
    }
    else if (new_focus == tab_text || new_focus == tab_debug)
    {
        xgui_widget tool_compile;
        ICY_ERROR(tool_compile.initialize(gui_widget_type::text_button, toolbar));
        ICY_ERROR(tool_compile.text("Compile"_s));
        ICY_ERROR(tools.push_back(std::make_pair(tool_type::compile, std::move(tool_compile))));
    }
    focus = new_focus;
    ICY_ERROR(update());
    return error_type();
}
error_type mbox_database::window_type::remodel() noexcept
{
    ICY_ERROR(refs_model.clear());
    ICY_ERROR(refs_model.insert_cols(0, 1));
    ICY_ERROR(refs_model.insert_rows(0, object.refs.size()));

    ICY_ERROR(refs_model.hheader(0, "Name"_s));
    ICY_ERROR(refs_model.hheader(1, "Path"_s));

    auto row = 0u;
    for (auto&& pair : object.refs)
    {
        string path;
        if (pair.value)
            ICY_ERROR(self.rpath(pair.value, path));
        ICY_ERROR(refs_model.node(row, 0).text(pair.key));
        ICY_ERROR(refs_model.node(row, 1).text(path));
        ++row;
    }
    auto gui = shared_ptr<gui_queue>(global_gui);
    ICY_ERROR(gui->resize_columns(tab_refs));

    return error_type();

}
error_type mbox_database::window_type::on_edit(const string_view name) noexcept
{
    mbox_object copy_object;
    ICY_ERROR(copy(object, copy_object));
    const auto find = copy_object.refs.find(name);
    if (find == copy_object.refs.end())
        return make_stdlib_error(std::errc::invalid_argument);

    class filter_type : public mbox_model_filter
    {
    public:
        filter_type(const string_view name) noexcept : name(name)
        {

        }
        const string_view name;
        error_type operator()(mbox_object& source_copy, const mbox_object& modify_with) noexcept override
        {
            source_copy.refs.find(name)->value = modify_with.index;
            return error_type();
        }
    };
    map<mbox_index, bool> filter;
    ICY_ERROR(filter_type(name).create(filter, self, copy_object));
    xgui_model model;
    ICY_ERROR(model.initialize());
    ICY_ERROR(mbox_model_create(model, self, self.m_icons, &filter));

    mbox_index select_index = find->value;
    xgui_node select_node;
    ICY_ERROR(mbox_model_find(select_node, self.find(select_index), model, self, &filter));
    
    xgui_widget edit_window;
    xgui_widget edit_view;
    xgui_widget edit_buttons;
    xgui_widget edit_button_save;
    xgui_widget edit_button_exit;

    ICY_ERROR(edit_window.initialize(gui_widget_type::dialog, gui_widget(), gui_widget_flag::layout_vbox));
    ICY_ERROR(edit_view.initialize(gui_widget_type::tree_view, edit_window));
    ICY_ERROR(edit_buttons.initialize(gui_widget_type::none, edit_window, gui_widget_flag::layout_hbox | gui_widget_flag::auto_insert));
    ICY_ERROR(edit_button_save.initialize(gui_widget_type::text_button, edit_buttons));
    ICY_ERROR(edit_button_exit.initialize(gui_widget_type::text_button, edit_buttons));

    string window_name;
    ICY_ERROR(icy::to_string("Refs [%1]:%2"_s, window_name, string_view(object.name), name));
    ICY_ERROR(edit_window.text(window_name));
    ICY_ERROR(edit_button_save.text("Select"_s));
    ICY_ERROR(edit_button_exit.text("Cancel"_s));
    ICY_ERROR(edit_view.bind(model));
    ICY_ERROR(edit_view.scroll(select_node));

    shared_ptr<event_queue> loop;
    ICY_ERROR(create_event_system(loop, event_type::gui_update | event_type::gui_select));
    ICY_ERROR(edit_window.show(true));

    mbox_index new_index;
    while (true)
    {
        event event;
        ICY_ERROR(loop->pop(event));
        if (event->type == event_type::global_quit)
            break;

        const auto& event_data = event->data<gui_event>();
        if (event->type == event_type::gui_update)
        {
            if (event_data.widget == edit_button_exit ||
                event_data.widget == edit_window)
                return error_type();

            if (event_data.widget == edit_button_save)
            {
                auto is_valid = false;
                find->value = select_index;
                ICY_ERROR(self.validate(copy_object, is_valid));
                if (is_valid)
                {
                    new_index = select_index;
                    break;
                }
                else
                {
                    ICY_ERROR(show_error(error_type(), "Invalid selected reference index"_s));
                }
            }
        }
        else if (event->type == event_type::gui_select)
        {
            select_index = event_data.data.as_node().udata().as_guid();
        }
    }

    find->value = new_index;
    ICY_ERROR(execute(copy_object));
    ICY_ERROR(push_refs(std::move(copy_object)));

    return error_type();
}
error_type mbox_database::window_type::context(const size_t index) noexcept
{
    xgui_widget menu;
    ICY_ERROR(menu.initialize(gui_widget_type::menu, xgui_widget()));

    xgui_action action_create;
    xgui_action action_rename;
    xgui_action action_destroy;
    xgui_action action_edit;

    if (index < object.refs.size())
    {
        ICY_ERROR(action_rename.initialize("Rename"_s));
        ICY_ERROR(action_destroy.initialize("Delete"_s));
        ICY_ERROR(action_edit.initialize("Edit"_s));
        ICY_ERROR(menu.insert(action_rename));
        ICY_ERROR(menu.insert(action_destroy));
        ICY_ERROR(menu.insert(action_edit));
    }
    else
    {
        ICY_ERROR(action_create.initialize("Add"_s));
        ICY_ERROR(menu.insert(action_create));
    }

    gui_action action;
    ICY_ERROR(menu.exec(action));
    if (!action.index)
        return error_type();

    if (action == action_create)
    {
        string str;
        ICY_ERROR(xgui_rename(str));
        if (!str.empty() && object.refs.try_find(str) == nullptr)
        {
            mbox_object new_action;
            ICY_ERROR(copy(object, new_action));
            ICY_ERROR(new_action.refs.insert(std::move(str), mbox_index()));

            auto is_valid = false;
            ICY_ERROR(self.validate(new_action, is_valid));
            if (is_valid)
            {
                ICY_ERROR(execute(new_action));
                ICY_ERROR(push_refs(std::move(new_action)));
                return error_type();
            }
        }
        return show_error(error_type(), "Invalid reference name"_s);
    }
    else if (action == action_rename)
    {
        const auto it = object.refs.begin() + index;
        
        string str;
        ICY_ERROR(copy(it->key, str));
        ICY_ERROR(xgui_rename(str));

        if (str != it->key)
        {
            const auto value = it->value;
            if (!str.empty() && object.refs.try_find(str) == nullptr)
            {
                mbox_object new_action;
                ICY_ERROR(copy(object, new_action));
                new_action.refs.erase(new_action.refs.begin() + index);
                ICY_ERROR(new_action.refs.insert(std::move(str), value));
                auto is_valid = false;
                ICY_ERROR(self.validate(new_action, is_valid));
                if (is_valid)
                {
                    ICY_ERROR(execute(new_action));
                    ICY_ERROR(push_refs(std::move(new_action)));
                    return error_type();
                }
            }
            return show_error(error_type(), "Invalid reference name"_s);
        }
    }
    else if (action == action_destroy)
    {
        mbox_object new_action;
        ICY_ERROR(copy(object, new_action));
        new_action.refs.erase(new_action.refs.begin() + index);
        ICY_ERROR(execute(new_action));
        ICY_ERROR(push_refs(std::move(new_action)));
    }
    else if (action == action_edit)
    {
        ICY_ERROR(on_edit((object.refs.begin() + index)->key));
    }

    return error_type();
}
error_type mbox_database::window_type::update() noexcept
{
    const auto object_ptr = self.find(object.index);
    if (!object_ptr)
        return make_mbox_error(mbox_error::invalid_object_index);

    const auto changed = refs_action || object.value != object_ptr->value;
    string path;
    ICY_ERROR(self.rpath(object.index, path));
    if (object.parent != mbox_index())
    {
        string str;
        ICY_ERROR(str.appendf("%1%2 (%3)"_s, changed ? "* "_s : ""_s, string_view(object.name), string_view(path)));
        path = std::move(str);
    }
    ICY_ERROR(window.text(path));

    for (auto&& pair : tools)
    {
        if (pair.first == tool_type::save)
        {
            ICY_ERROR(pair.second.enable(changed));
        }
        else if (pair.first == tool_type::undo)
        {
            if (focus == tab_refs)
            {
                ICY_ERROR(pair.second.enable(refs_action > 0));
            }
        }
        else if (pair.first == tool_type::redo)
        {
            if (focus == tab_refs)
            {
                ICY_ERROR(pair.second.enable(refs_action < refs_actions.size()));
            }
        }
        else if (pair.first == tool_type::compile)
        {
            ICY_ERROR(pair.second.enable(!object.value.empty()));
        }
    }
    return error_type();
}
error_type mbox_database::window_type::save() noexcept
{
    //text_actions.clear();
    //text_action = 0;
    refs_actions.clear();
    refs_action = 0;
    if (const auto old_object = self.find(object.index))
    {
        mbox_object new_action_refs;
        mbox_object new_action_val;
        if (object.refs.keys() != old_object->refs.keys() ||
            object.refs.vals() != old_object->refs.vals())
        {
            ICY_ERROR(copy(*old_object, new_action_refs));
            ICY_ERROR(copy(object.refs, new_action_refs.refs));
            if (object.value != old_object->value)
            {
                ICY_ERROR(copy(new_action_refs, new_action_val));
                ICY_ERROR(copy(object.value, new_action_val.value));
            }
        }
        else if (object.value != old_object->value)
        {
            ICY_ERROR(copy(*old_object, new_action_val));
            ICY_ERROR(copy(object.value, new_action_val.value));
        }

        array<mbox_object> new_actions;
        if (new_action_refs.index)
        {
            ICY_ERROR(self.execute(new_action_refs));
            ICY_ERROR(new_actions.push_back(std::move(new_action_refs)));
        }
        if (new_action_val.index)
        {
            ICY_ERROR(self.execute(new_action_val));
            ICY_ERROR(new_actions.push_back(std::move(new_action_val)));
        }
        if (!new_actions.empty())
            ICY_ERROR(self.push(std::move(new_actions)));
    }
    ICY_ERROR(update());
    return error_type();
}
error_type mbox_database::window_type::undo() noexcept
{
    if (focus == tab_text)
    {
        /*if (text_action == 0)
            return error_type();

        while (text_action)
        {
            auto& prev_action = text_actions[--text_action];
            auto done = prev_action.value != object.value;
            ICY_ERROR(execute(prev_action));
            auto gui = shared_ptr<gui_queue>(global_gui);
            if (gui)
                ICY_ERROR(gui->undo(tab_text));
            if (done)
                break;
        }
        ICY_ERROR(update());*/
    }
    else if (focus == tab_refs)
    {
        if (refs_action == 0)
            return error_type();

        auto& prev_action = refs_actions[--refs_action];
        ICY_ERROR(execute(prev_action));
        ICY_ERROR(update());
    }
    return error_type();
}
error_type mbox_database::window_type::redo() noexcept
{
    if (focus == tab_text)
    {
        /*if (text_action == text_actions.size())
            return error_type();

        while (text_action != text_actions.size())
        {
            auto& next_action = text_actions[text_action++];
            auto done = next_action.value != object.value;
            ICY_ERROR(execute(next_action));
            auto gui = shared_ptr<gui_queue>(global_gui);
            if (gui)
                ICY_ERROR(gui->redo(tab_text));
            if (done)
                break;
        }
        ICY_ERROR(update());*/
    }
    else if (focus == tab_refs)
    { 
        if (refs_action == refs_actions.size())
            return error_type();

        auto& next_action = refs_actions[refs_action++];
        ICY_ERROR(execute(next_action));
        ICY_ERROR(update());
    }
    return error_type();
}
error_type mbox_database::window_type::push_refs(mbox_object&& new_action) noexcept
{
    for (auto k = refs_actions.size(); k > refs_action; --k)
        refs_actions.pop_back();

    ICY_ERROR(refs_actions.push_back(std::move(new_action)));
    refs_action++;

    ICY_ERROR(update());
    return error_type();
}
error_type mbox_database::window_type::execute(mbox_object& action) noexcept
{
    if (action.index != object.index || action.type != object.type || action.parent != object.parent || action.name != object.name)
        return make_stdlib_error(std::errc::invalid_argument);

    if (action.refs.keys() != object.refs.keys() || action.refs.vals() != object.refs.vals())
    {
        std::swap(action.refs, object.refs);
        ICY_ERROR(remodel());
    }
    else if (action.value != object.value)
    {
        std::swap(action.value, object.value);
        ICY_ERROR(update());
    }
    return error_type();
}
error_type mbox_database::window_type::close(bool& cancel) noexcept
{
    const auto object_ptr = self.find(object.index);
    if (object_ptr && (object_ptr->value != object.value || refs_action))
    {
        auto yes_save = false;
        ICY_ERROR(save_changes(window, yes_save, cancel));
        if (cancel)
            return error_type();
        if (yes_save)
            ICY_ERROR(save());
    }
    return error_type();
}
error_type mbox_database::window_type::compile() noexcept
{
    ICY_ERROR(tabs.scroll(tab_debug));
    ICY_ERROR(tab_debug.text(""_s));

    const auto append = [this](const string_view format, auto&&... args)
    {
        string time_str;
        ICY_ERROR(icy::to_string(clock_type::now(), time_str));
        string str;
        ICY_ERROR(str.appendf("\r\n[%1]: ", string_view(time_str)));
        ICY_ERROR(str.appendf(format, std::forward<decltype(args)>(args)...));
        ICY_ERROR(tab_debug.append(str));
        return error_type();
    };
    { 
        string time_str;
        ICY_ERROR(icy::to_string(clock_type::now(), time_str));
        string str;
        ICY_ERROR(str.appendf("[%1]: Process start"_s, string_view(time_str)));
        ICY_ERROR(tab_debug.append(str));
    }
    
    heap lua_heap;
    ICY_ERROR(lua_heap.initialize(heap_init(64_mb)));
    ICY_ERROR(append("Initialize LUA sandbox: 64_mb"));

    shared_ptr<lua_system> lua;
    ICY_ERROR(create_lua_system(lua, &lua_heap));
    mbox_function func(self.m_objects, object, *lua);

    ICY_ERROR(append("Compile LUA script... "));

    if (const auto error = func.initialize())
    {
        if (error != lua_error_execute)
            return error;   //  memory?

        string msg;
        ICY_ERROR(lua_error_to_string(error, msg));
        ICY_ERROR(tab_debug.append(msg));
        return error_type();
    }
    else
    {
        ICY_ERROR(tab_debug.append("Success!"_s));
    }

    ICY_ERROR(append("Execute LUA script... "));
    array<mbox_action> actions;
    if (const auto error = func(actions))
    {
        if (error != lua_error_execute)
            return error;   //  memory?

        string msg;
        ICY_ERROR(lua_error_to_string(error, msg));
        ICY_ERROR(tab_debug.append(msg));
        return error_type();
    }
    else
    {
        ICY_ERROR(tab_debug.append("Success!"_s));
    }

    if (!actions.empty())
    {
        ICY_ERROR(append("Actions list: "_s));
        for (auto&& cmd : actions)
        {
            string str;
            ICY_ERROR(self.to_string(mbox_index(), cmd, str));
            ICY_ERROR(str.insert(str.begin(), "\r\n  "));
            ICY_ERROR(tab_debug.append(str));
        }
    }
    return error_type();
}

error_type mbox_model_filter::create(map<mbox_index, bool>& filter, const mbox_array& data, const mbox_object& source) noexcept
{
    for (auto&& pair : data.objects())
    {
        mbox_object copy_object;
        ICY_ERROR(copy(source, copy_object));
        ICY_ERROR((*this)(copy_object, pair.value));
        auto is_valid = false;
        ICY_ERROR(data.validate(copy_object, is_valid));
        ICY_ERROR(filter.insert(pair.key, is_valid));
    }
    for (auto&& pair : filter)
    {
        if (!pair.value)
            continue;

        auto object = data.find(pair.key);
        while (object)
        {
            filter.find(object->index)->value = true;
            object = data.find(object->parent);
        }
    }
    return error_type();
}
error_type mbox_model_create(xgui_model& model, const mbox_array& data, const map<mbox_type, xgui_image>& icons, const map<mbox_index, bool>* const filter) noexcept
{
    using func_type = error_type(*)(xgui_node parent_node, const mbox_object& parent_object, const mbox_array& data, const map<mbox_type, xgui_image>& icons, const map<mbox_index, bool>* const filter);
    static func_type func;
    func = [](xgui_node parent_node, const mbox_object& parent_object, const mbox_array& data, const map<mbox_type, xgui_image>& icons, const map<mbox_index, bool>* const filter)
    {
        switch (parent_object.type)
        {
        case mbox_type::none:
        case mbox_type::directory:
        case mbox_type::profile:
        case mbox_type::account:
            break;
        default:
            return error_type();
        }

        array<const mbox_object*> children;
        ICY_ERROR(data.enum_children(parent_object.index, children));
        if (filter)
        {
            array<const mbox_object*> new_children;
            for (auto&& ptr : children)
            {
                if (filter->find(ptr->index)->value)
                    ICY_ERROR(new_children.push_back(ptr));
            }
            children = std::move(new_children);
        }
        std::sort(children.begin(), children.end(), [](const mbox_object* lhs, const mbox_object* rhs) { return *lhs < *rhs; });
        if (!children.empty())
        {
            ICY_ERROR(parent_node.insert_rows(0, children.size()));
            for (auto k = 0u; k < children.size(); ++k)
            {
                auto new_node = parent_node.node(k, 0);
                if (!new_node)
                    return make_stdlib_error(std::errc::not_enough_memory);

                ICY_ERROR(new_node.text(children[k]->name));
                ICY_ERROR(new_node.udata(children[k]->index));
                const auto icon = icons.find(children[k]->type);
                if (icon != icons.end())
                    ICY_ERROR(new_node.icon(icon->value));

                ICY_ERROR(func(new_node, *children[k], data, icons, filter));
            }
        }
        return error_type();
    };
    return func(model, mbox_object(), data, icons, filter); 
}
error_type mbox_model_find(xgui_node& node, const mbox_object* const object, const gui_node root, const mbox_array& data, const map<mbox_index, bool>* const filter) noexcept
{
    if (!object)
    {
        node = root;
        return error_type();
    }

    xgui_node parent_node;
    ICY_ERROR(mbox_model_find(parent_node, data.find(object->parent), root, data, filter));

    array<const mbox_object*> siblings;
    ICY_ERROR(data.enum_children(object->parent, siblings));
    {
        array<const mbox_object*> new_siblings;
        for (auto&& ptr : siblings)
        {
            if (ptr->index == object->index)
                continue;

            if (filter && filter->find(ptr->index)->value == false)
                continue;
            
            ICY_ERROR(new_siblings.push_back(ptr));
        }
        siblings = std::move(new_siblings);
    }
    ICY_ERROR(siblings.push_back(object));
    std::sort(siblings.begin(), siblings.end(), [](const mbox_object* lhs, const mbox_object* rhs) { return *lhs < *rhs; });

    auto row = 0u;
    for (auto&& ptr : siblings)
    {
        if (ptr->index == object->index)
            break;
        ++row;
    }
    node = parent_node.node(row, 0);
    return error_type();
}

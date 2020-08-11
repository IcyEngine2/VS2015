#include "mbox_script_main.hpp"
#include "../mbox_script.hpp"
#include <icy_engine/core/icy_thread.hpp>
#include <icy_engine/core/icy_file.hpp>
#include <icy_engine/image/icy_image.hpp>
#include <icy_qtgui/icy_xqtgui.hpp>
#include "icons/command.h"
#include "icons/binding.h"
#include "icons/directory.h"
#include "icons/event.h"
#include "icons/group.h"
#include "icons/input.h"
#include "icons/profile.h"
#include "icons/timer.h"
#include "icons/variable.h"
#include "icons/create.h"
#include "icons/remove.h"
#include "icons/modify.h"
#include "icons/move_up.h"
#include "icons/move_dn.h"
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

std::atomic<gui_queue*> icy::global_gui;
gui_image g_icons[size_t(mbox_image::_total)];

class mbox_application;
class mbox_main_thread : public thread
{
public:
    mbox_main_thread(mbox_application& app) noexcept : m_app(app)
    {

    }
    error_type run() noexcept override;
private:
    mbox_application& m_app;
};
class mbox_widget
{
public:
    mbox_widget() noexcept = default;
    mbox_widget(const mbox_model_type type, const guid& root) noexcept : m_type(type), m_root(root)
    {

    }
    error_type initialize(const gui_widget_type wtype, const gui_widget parent, const mbox::library& library) noexcept
    {
        ICY_ERROR(mbox_model::create(m_type, m_model));
        ICY_ERROR(m_widget.initialize(wtype, parent));
        ICY_ERROR(m_model->reset(library, m_root));
        ICY_ERROR(m_widget.bind(m_model->root()));
        return {};
    }
    error_type exec(const icy::event event) noexcept
    {
        switch (event->type)
        {
        case mbox_event_type_load_library:
        {
            ICY_ERROR(m_model->reset(*event->data<mbox_event_data_load_library>(), m_root));
            ICY_ERROR(m_widget.bind(m_model->root()));
            break;
        }
        case event_type::gui_context:
        {
            const auto& event_data = event->data<gui_event>();
            if (event_data.widget == m_widget)
            {
                ICY_ERROR(m_model->context(event_data.data.as_node()));
            }
            break;
        }
        }
        ICY_ERROR(m_model->exec(event));
        return {};
    }
    error_type save(mbox::base& base) noexcept
    {
        return m_model->save(base);
    }
    gui_widget widget() const noexcept
    {
        return m_widget;
    }
    bool is_changed() const noexcept
    {
        return m_model->is_changed();
    }
    error_type name(string& str) const noexcept
    {
        return m_model->name(str);
    }
private:
    mbox_model_type m_type = mbox_model_type::none;
    icy::guid m_root;
    icy::xgui_widget m_widget;
    unique_ptr<mbox_model> m_model;
};
class mbox_application
{
    friend mbox_main_thread;
public:
    mbox_application() : m_explorer(mbox_model_type::explorer, mbox::root)
    {

    }
    error_type initialize() noexcept;
    error_type exec() noexcept;
    error_type run_main() noexcept;
private:
    error_type exec(const event event) noexcept;
    error_type rename(string& str) noexcept;
    error_type on_action(gui_action action) noexcept;
    error_type on_save(const mbox_event_data_base& index, error_type& error) noexcept;
    error_type on_create(const mbox_event_data_create& create) noexcept;
    error_type on_rename(const mbox_event_data_base& index) noexcept;
    error_type on_remove(const mbox_event_data_base& index) noexcept;
    error_type on_changed(const mbox_event_data_base& index) noexcept;
    error_type on_view(const mbox_event_data_base& index, const bool view) noexcept;
private:
    mbox_main_thread m_main = *this;
    shared_ptr<gui_queue> m_gui;
    mbox::library m_library;
    xgui_widget m_window;
    xgui_widget m_splitter;
    mbox_widget m_explorer;
    xgui_widget m_tabs;
    map<guid, mbox_widget> m_vals;
    xgui_action m_open;
    xgui_action m_save;
    xgui_action m_save_as;
    xgui_action m_macros;
    string m_path;
};

int main()
{
    const auto gheap_capacity = 256_mb;

    heap gheap;
    if (const auto error = gheap.initialize(heap_init::global(gheap_capacity)))
        return error.code;

    mbox_application app;
    if (const auto error = app.initialize())
        return show_error(error, "init application"_s).code;

    if (const auto error = app.exec())
        return show_error(error, "exec application"_s).code;

    return 0;
}
error_type mbox_main_thread::run() noexcept
{
    return m_app.run_main();
}
error_type mbox_application::initialize() noexcept
{
    ICY_ERROR(m_library.initialize());
    ICY_ERROR(create_event_system(m_gui));
    icy::global_gui = m_gui.get();

    const auto func = [this](const const_array_view<uint8_t> bytes, const mbox_image index)
    {
        icy::image image;
        ICY_ERROR(image.load(detail::global_heap, bytes, image_type::png));
        auto colors = matrix<color>(image.size().y, image.size().x);
        if (colors.empty())
            return make_stdlib_error(std::errc::not_enough_memory);
        ICY_ERROR(image.view({}, colors));
        ICY_ERROR(m_gui->create(g_icons[uint32_t(index)], colors));
        return error_type();
    };
    ICY_ERROR(func(g_bytes_command, mbox_image::type_command));
    //ICY_ERROR(func(g_bytes_binding, mbox_image::type_binding));
    ICY_ERROR(func(g_bytes_directory, mbox_image::type_directory));
    ICY_ERROR(func(g_bytes_event, mbox_image::type_event));
    ICY_ERROR(func(g_bytes_group, mbox_image::type_group));
    ICY_ERROR(func(g_bytes_input, mbox_image::type_input));
    ICY_ERROR(func(g_bytes_profile, mbox_image::type_character));
    ICY_ERROR(func(g_bytes_timer, mbox_image::type_timer));
    //ICY_ERROR(func(g_bytes_variable, mbox_image::type_variable));
    ICY_ERROR(func(g_bytes_create, mbox_image::action_create));
    ICY_ERROR(func(g_bytes_remove, mbox_image::action_remove));
    ICY_ERROR(func(g_bytes_modify, mbox_image::action_modify));
    ICY_ERROR(func(g_bytes_move_up, mbox_image::action_move_up));
    ICY_ERROR(func(g_bytes_move_dn, mbox_image::action_move_dn));

    ICY_ERROR(m_window.initialize(gui_widget_type::window, {}, gui_widget_flag::layout_hbox));
    ICY_ERROR(m_splitter.initialize(gui_widget_type::hsplitter, m_window, gui_widget_flag::auto_insert));
    ICY_ERROR(m_explorer.initialize(gui_widget_type::tree_view, m_splitter, m_library));
    ICY_ERROR(m_tabs.initialize(gui_widget_type::tabs, m_splitter));

    ICY_ERROR(m_open.initialize("Open"_s));
    ICY_ERROR(m_save.initialize("Save"_s));
    ICY_ERROR(m_save_as.initialize("Save As"_s));
    ICY_ERROR(m_macros.initialize("WoW Macros"_s));

    xgui_widget menu_bar;
    xgui_widget menu_file;
    xgui_action action_file;
    ICY_ERROR(menu_bar.initialize(gui_widget_type::menubar, m_window, gui_widget_flag::none));
    ICY_ERROR(menu_file.initialize(gui_widget_type::menu, menu_bar));
    ICY_ERROR(action_file.initialize("File"_s));
    ICY_ERROR(menu_bar.insert(action_file));
    ICY_ERROR(action_file.bind(menu_file));
    ICY_ERROR(menu_file.insert(m_open));
    ICY_ERROR(menu_file.insert(m_save));
    ICY_ERROR(menu_file.insert(m_save_as));
    ICY_ERROR(menu_bar.insert(m_macros));
    ICY_ERROR(m_window.show(true));

    action_file.index = 0;
    menu_file.index = 0;
    menu_bar.index = 0;
    return {};
}
error_type mbox_application::exec() noexcept
{
    ICY_SCOPE_EXIT{ event::post(nullptr, event_type::global_quit); };
    ICY_ERROR(m_main.launch());
    ICY_ERROR(m_gui->exec());
    ICY_ERROR(m_main.error());
    return {};
}
error_type mbox_application::rename(string& str) noexcept
{
    xgui_widget dialog;
    ICY_ERROR(dialog.initialize(gui_widget_type::dialog_input_line, {}));
    ICY_ERROR(dialog.text(str));
    shared_ptr<event_queue> loop;
    ICY_ERROR(create_event_system(loop, event_type::gui_update));
    ICY_ERROR(dialog.show(true));
    event event;
    ICY_ERROR(loop->pop(event));
    if (event->type == event_type::gui_update)
    {
        auto& event_data = event->data<gui_event>();
        if (event_data.widget == dialog)
        {
            const auto new_str = event_data.data.as_string();
            if (!new_str.empty())
                ICY_ERROR(to_string(new_str, str));
        }
    }
    return {};
}
error_type mbox_application::exec(const event event) noexcept
{
    if (event->type == event_type::gui_update)
    {
        const auto& event_data = event->data<gui_event>();
        if (event_data.widget == m_window)
        {
            return event::post(nullptr, event_type::global_quit);
        }
    }

    if (event->type == event_type::gui_action)
    {
        const auto& event_data = event->data<gui_event>();
        gui_action action;
        action.index = event_data.data.as_uinteger();
        ICY_ERROR(on_action(action));
    }
    else if (event->type == mbox_event_type_save)
    {
        error_type error;
        ICY_ERROR(on_save(event->data<mbox_event_data_base>(), error));
        if (error)
            show_error(error, "Save Command"_s);
    }
    else if (event->type == mbox_event_type_create)
    {
        ICY_ERROR(on_create(event->data<mbox_event_data_create>()));
    }
    else if (event->type == mbox_event_type_rename)
    {
        ICY_ERROR(on_rename(event->data<mbox_event_data_base>()));
    }
    else if (event->type == mbox_event_type_delete)
    {
        ICY_ERROR(on_remove(event->data<mbox_event_data_base>()));
    }
    else if (event->type == mbox_event_type_changed)
    {
        ICY_ERROR(on_changed(event->data<mbox_event_data_base>()));        
    }
    else if (event->type == mbox_event_type_view || event->type == mbox_event_type_edit)
    {
        ICY_ERROR(on_view(event->data<mbox_event_data_base>(), event->type == mbox_event_type_view));
    }
    else if (event->type == event_type::gui_update)
    {
        const auto& event_data = event->data<gui_event>();
        if (event_data.widget == m_tabs)
        {
            gui_widget widget;
            widget.index = event_data.data.as_uinteger();
            for (auto it = m_vals.begin(); it != m_vals.end(); ++it)
            {
                if (it->value.widget() == widget)
                {
                    m_vals.erase(it);
                    break;
                }
            }
        }
    }

    if (const auto error = m_explorer.exec(event))
        show_error(error, "Explorer"_s);

    for (auto&& tab : m_vals)
    {
        if(const auto error = tab.value.exec(event))
            show_error(error, "Tabs"_s);
    }
    return {};
}
error_type mbox_application::run_main() noexcept
{
    ICY_SCOPE_EXIT{ event::post(nullptr, event_type::global_quit); };

    shared_ptr<event_queue> loop;
    ICY_ERROR(create_event_system(loop, event_type::gui_any | event_type::user_any));

    while (true)
    {
        event event;
        ICY_ERROR(loop->pop(event));
        if (event->type == event_type::global_quit)
            break;
        ICY_ERROR(exec(event));
    }
    return {};
}
error_type mbox_application::on_action(const gui_action action) noexcept
{
    if (action == m_save_as || action == m_save)
    {
        for (auto&& tab : m_vals)
        {
            if (tab.value.is_changed())
            {
                error_type error;
                ICY_ERROR(on_save(tab.key, error));
                if (error)
                {
                    show_error(error, "Save Command"_s);
                    return {};
                }
            }
        }
    }

    if (action == m_open)
    {
        string path;
        ICY_ERROR(icy::dialog_open_file(path));
        if (!path.empty())
        {
            if (const auto error = m_library.load_from(path))
                show_error(error, "Open library"_s);
            else
            {
                m_path = std::move(path);
                ICY_ERROR(event::post(nullptr, mbox_event_type_load_library,
                    mbox_event_data_load_library(&m_library)));
            }
        }
    }
    else if (action == m_save_as || action == m_save && m_path.empty())
    {
        string path;
        ICY_ERROR(icy::dialog_save_file(path));
        if (!path.empty())
        {
            if (const auto error = m_library.save_to(path))
                show_error(error, "Save library"_s);
        }
    }
    else if (action == m_save)
    {
        if (const auto error = m_library.save_to(m_path))
            show_error(error, "Save library"_s);
    }
    else if (action == m_macros)
    {
        ;
    }
    return {};
}
error_type mbox_application::on_save(const mbox_event_data_base& index, error_type& error) noexcept
{
    for (auto&& val : m_vals)
    {
        if (val.key != index)
            continue;

        auto& ref = *m_library.find(val.key);
        mbox::transaction txn;
        mbox::base copy;
        ICY_ERROR(txn.initialize(m_library));
        ICY_ERROR(mbox::base::copy(ref, copy));
        ICY_ERROR(val.value.save(copy));
        ICY_ERROR(txn.modify(std::move(copy)));
        if (error = txn.execute(m_library))
        {
            ;
        }
        else
        {
            string tab_name;
            ICY_ERROR(val.value.name(tab_name));
            ICY_ERROR(m_gui->text(m_tabs, val->value.widget(), tab_name));
            ICY_ERROR(event::post(nullptr, mbox_event_type_transaction,
                mbox_event_data_transaction(std::move(txn))));
        }
    }
    return {};
}
error_type mbox_application::on_create(const mbox_event_data_create& create) noexcept
{
    string str;
    ICY_ERROR(rename(str));
    if (!str.empty())
    {
        mbox::transaction txn;
        ICY_ERROR(txn.initialize(m_library));
        mbox::base new_base;
        new_base.type = create.type;
        new_base.parent = create.parent;
        ICY_ERROR(to_string(str, new_base.name));
        ICY_ERROR(guid::create(new_base.index));
        ICY_ERROR(txn.insert(new_base));

        if (const auto error = txn.execute(m_library))
        {
            show_error(error, "Create"_s);
        }
        else
        {
            ICY_ERROR(event::post(nullptr, mbox_event_type_transaction,
                mbox_event_data_transaction(std::move(txn))));
        }
    }
    return {};
}
error_type mbox_application::on_rename(const mbox_event_data_base& index) noexcept
{
    const auto& base = *m_library.find(index);
    string str;
    ICY_ERROR(to_string(base.name, str));
    ICY_ERROR(rename(str));
    if (!str.empty())
    {
        mbox::transaction txn;
        mbox::base edit;
        ICY_ERROR(txn.initialize(m_library));
        ICY_ERROR(mbox::base::copy(base, edit));
        edit.name = std::move(str);
        ICY_ERROR(txn.modify(std::move(edit)));
        if (const auto error = txn.execute(m_library))
        {
            show_error(error, "Rename"_s);
        }
        else
        {
            ICY_ERROR(event::post(nullptr, mbox_event_type_transaction,
                mbox_event_data_transaction(std::move(txn))));
        }
    }
    return {};
}
error_type mbox_application::on_remove(const mbox_event_data_base& index) noexcept
{
    mbox::transaction txn;
    ICY_ERROR(txn.initialize(m_library));
    ICY_ERROR(txn.remove(index));

    xgui_model model;
    ICY_ERROR(model.initialize());

    enum
    {
        col_type,
        col_oper,
        col_name,
        _col_count,
    };

    using pair_type = std::pair<icy::guid, mbox::transaction::operation_type>;
    array<pair_type> vec;

    for (auto&& oper : txn.oper())
    {
        auto found = false;
        for (auto&& pair : vec)
        {
            if (pair.first == oper.value.index)
            {
                found = true;
                if (pair.second == mbox::transaction::operation_type::modify &&
                    oper.type == mbox::transaction::operation_type::remove)
                {
                    pair.second = oper.type;
                    break;
                }
            }
        }
        if (!found)
            ICY_ERROR(vec.push_back(std::make_pair(oper.value.index, oper.type)));
    }
    ICY_ERROR(model.insert_rows(0, vec.size()));
    ICY_ERROR(model.insert_cols(0, _col_count - 1));
    ICY_ERROR(model.hheader(col_name, "Path"_s));
    ICY_ERROR(model.hheader(col_type, "Type"_s));
    ICY_ERROR(model.hheader(col_oper, "Operation"_s));
    auto row = 0_z;
    for (auto&& pair : vec)
    {
        string path;
        ICY_ERROR(m_library.reverse_path(pair.first, path));
        const auto ptr = m_library.find(pair.first);
        ICY_ASSERT(ptr, "INVALID PTR");

        auto type = model.node(row, col_type);
        ICY_ERROR(type.text(to_string(ptr->type)));
        
        switch (ptr->type)
        {
        case mbox::type::character:
            ICY_ERROR(type.icon(find_image(mbox_image::type_character)));
            break;
        case mbox::type::command:
            ICY_ERROR(type.icon(find_image(mbox_image::type_command)));
            break;
        case mbox::type::group:
            ICY_ERROR(type.icon(find_image(mbox_image::type_group)));
            break;
        case mbox::type::directory:
            ICY_ERROR(type.icon(find_image(mbox_image::type_directory)));
            break;
        case mbox::type::timer:
            ICY_ERROR(type.icon(find_image(mbox_image::type_timer)));
            break;
        }

        ICY_ERROR(model.node(row, col_name).text(path));
        
        switch (pair.second)
        {
        case mbox::transaction::operation_type::modify:
            ICY_ERROR(model.node(row, col_oper).text("Edit"_s));
            break;

        case mbox::transaction::operation_type::remove:
            ICY_ERROR(model.node(row, col_oper).text("Delete"_s));
            break;

        case mbox::transaction::operation_type::none:
            ICY_ERROR(model.node(row, col_oper).text("Edit (Action)"_s));
            break;

        default:
            ICY_ASSERT(false, "INVALID OPER TYPE");
            break;
        }
        ++row;
    }

    xgui_widget window;
    xgui_widget label;
    xgui_widget grid;
    xgui_widget buttons;
    xgui_widget save;
    xgui_widget exit;
    ICY_ERROR(window.initialize(gui_widget_type::dialog, m_window, gui_widget_flag::layout_vbox));
    ICY_ERROR(label.initialize(gui_widget_type::label, window));
    ICY_ERROR(grid.initialize(gui_widget_type::grid_view, window));
    ICY_ERROR(buttons.initialize(gui_widget_type::none, window, gui_widget_flag::layout_hbox | gui_widget_flag::auto_insert));
    ICY_ERROR(save.initialize(gui_widget_type::text_button, buttons));
    ICY_ERROR(exit.initialize(gui_widget_type::text_button, buttons));

    string msg;
    ICY_ERROR(to_string("Delete/Modify %1 elements?"_s, msg, vec.size()));
    ICY_ERROR(label.text(msg));
    ICY_ERROR(grid.bind(model));
    ICY_ERROR(save.text("Confirm"_s));
    ICY_ERROR(exit.text("Cancel"_s));

    auto confirm = false;
    shared_ptr<event_queue> loop;
    ICY_ERROR(create_event_system(loop, event_type::gui_update));
    ICY_ERROR(window.show(true));
    while (true)
    {
        icy::event event;
        ICY_ERROR(loop->pop(event));
        if (event->type == event_type::global_quit)
            break;

        auto& event_data = event->data<gui_event>();
        if (event->type == event_type::gui_update && event_data.widget == window)
        {
            break;
        }
        else if (event->type == event_type::gui_update)
        {
            if (event_data.widget == save)
            {
                confirm = true;
                break;
            }
            else if (event_data.widget == exit)
            {
                break;
            }
        }
    }
    if (confirm)
    {
        ICY_ERROR(txn.execute(m_library));
        mbox::transaction copy;
        ICY_ERROR(mbox::transaction::copy(txn, copy));
        ICY_ERROR(event::post(nullptr, mbox_event_type_transaction, std::move(copy)));
    }
    return {};

}
error_type mbox_application::on_changed(const mbox_event_data_base& index) noexcept
{
    const auto tab = m_vals.find(index);
    const auto base = m_library.find(index);
    if (tab != m_vals.end() && base)
    {
        string tab_name;
        ICY_ERROR(tab->value.name(tab_name));
        ICY_ERROR(m_gui->text(m_tabs, tab->value.widget(), tab_name));
    }
    return {};
}
error_type mbox_application::on_view(const mbox_event_data_base& index, const bool view) noexcept
{
    const auto base = m_library.find(index);
    if (!base)
        return make_stdlib_error(std::errc::invalid_argument);
    
    auto ptr = m_vals.try_find(index);
    if (!ptr)
    {
        auto mtype = mbox_model_type::none;
        if (base->type == mbox::type::directory)
        {
            mtype = mbox_model_type::directory;
        }
        else if (base->type == mbox::type::character || base->type == mbox::type::command)
        {
            mtype = view ? mbox_model_type::view_command : mbox_model_type::edit_command;
        }
        else
            ICY_ASSERT(false, "INVALID MODEL TYPE");

        mbox_widget widget(mtype, index);
        ICY_ERROR(widget.initialize(gui_widget_type::grid_view, m_tabs, m_library));
        auto it = m_vals.end();
        ICY_ERROR(m_vals.insert(index, std::move(widget), &it));
        ptr = &it->value;
    }
    string tab_name;
    ICY_ERROR(ptr->name(tab_name));
    ICY_ERROR(m_gui->text(m_tabs, ptr->widget(), tab_name));
    ICY_ERROR(m_tabs.scroll(ptr->widget()));
    return {};
}

icy::gui_image find_image(const mbox_image image) noexcept
{
    return g_icons[uint32_t(image)];
}
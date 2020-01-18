#include "icy_mbox_script_main.hpp"
#include "../icy_mbox_script2.hpp"
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
#pragma comment(lib, "icy_engine_cored")
#pragma comment(lib, "icy_engine_imaged")
#pragma comment(lib, "icy_qtguid")

using namespace icy;

gui_queue* icy::global_gui;
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
                ICY_ERROR(m_model->context(event_data.data.as_node().udata()));
            break;
        }
        }
        ICY_ERROR(m_model->exec(event));
        return {};
    }
    gui_widget widget() const noexcept
    {
        return m_widget;
    }
    void lock() noexcept
    {
        if (m_type == mbox_model_type::edit_command)
        {
            m_type = mbox_model_type::view_command;
            m_model->lock();
        }
    }
    void unlock() noexcept
    {
        if (m_type == mbox_model_type::view_command)
        {
            m_type = mbox_model_type::edit_command;         
            m_model->unlock();
        }
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
        return show_error(error, "init application"_s);

    if (const auto error = app.exec())
        return show_error(error, "exec application"_s);

    return 0;
}
error_type mbox_main_thread::run() noexcept
{
    return m_app.run_main();
}
error_type mbox_application::initialize() noexcept
{
    ICY_ERROR(m_library.initialize());
    ICY_ERROR(create_gui(m_gui));
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
    ICY_ERROR(m_gui->loop());
    ICY_ERROR(m_main.error());
    return {};
}
error_type mbox_application::rename(string& str) noexcept
{
    xgui_widget dialog;
    ICY_ERROR(dialog.initialize(gui_widget_type::dialog_input_line, {}));
    ICY_ERROR(dialog.text(str));
    shared_ptr<event_loop> loop;
    ICY_ERROR(event_loop::create(loop, event_type::gui_update));
    ICY_ERROR(dialog.show(true));
    event event;
    ICY_ERROR(loop->loop(event));
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
    if (event->type == event_type::window_close)
    {
        const auto& event_data = event->data<gui_event>();
        if (event_data.widget == m_window)
        {
            ICY_ERROR(event::post(nullptr, event_type::global_quit));
        }
    }
    else if (event->type == event_type::gui_action)
    {
        const auto& event_data = event->data<gui_event>();
        gui_action action;
        action.index = event_data.data.as_uinteger();
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
    }
    else if (event->type == mbox_event_type_create)
    {
        const auto& event_data = event->data<mbox_event_data_create>();
        string str;
        ICY_ERROR(rename(str));
        if (!str.empty())
        {
            mbox::transaction txn;
            ICY_ERROR(txn.initialize(m_library));
            mbox::base new_base;
            new_base.type = event_data.type;
            new_base.parent = event_data.parent;
            ICY_ERROR(to_string(str, new_base.name));
            ICY_ERROR(create_guid(new_base.index));            
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
    }
    else if (
        event->type == mbox_event_type_lock ||
        event->type == mbox_event_type_unlock)
    {
        const auto& event_data = event->data<mbox_event_data_base>();
        const auto tab = m_vals.find(event_data);
        const auto base = m_library.find(event_data);
        if (tab != m_vals.end() && base)
        {
            string tab_name;
            ICY_ERROR(to_string(base->name, tab_name));
            if (event->type == mbox_event_type_lock)
                ICY_ERROR(tab_name.append(" [READ]"_s));
            ICY_ERROR(m_gui->text(m_tabs, tab->value.widget(), tab_name));
            event->type == mbox_event_type_lock ? tab->value.lock() : tab->value.unlock();
        }
    }
    else if (
        event->type == mbox_event_type_view || 
        event->type == mbox_event_type_edit)
    {
        const auto& event_data = event->data<mbox_event_data_base>();
        const auto base = m_library.find(event_data);
        if (!base)
            return make_stdlib_error(std::errc::invalid_argument);

        string tab_name;
        ICY_ERROR(to_string(base->name, tab_name));
        if (event->type == mbox_event_type_view)
            ICY_ERROR(tab_name.append(" [READ]"_s));

        auto ptr = m_vals.try_find(event_data);
        if (!ptr)
        {
            auto mtype = mbox_model_type::none;
            if (base->type == mbox::type::directory)
            {
                mtype = mbox_model_type::directory;
            }
            else if (
                base->type == mbox::type::character ||
                base->type == mbox::type::command)
            {
                mtype = event->type == mbox_event_type_view ? 
                    mbox_model_type::view_command :
                    mbox_model_type::edit_command;
            }
            else
                ICY_ASSERT(false, "INVALID MODEL TYPE");

            mbox_widget widget(mtype, event_data);
            ICY_ERROR(widget.initialize(gui_widget_type::grid_view, m_tabs, m_library));
            auto it = m_vals.end();
            ICY_ERROR(m_vals.insert(event_data, std::move(widget), &it));
            ptr = &it->value;
        }
        ICY_ERROR(m_gui->text(m_tabs, ptr->widget(), tab_name));
        ICY_ERROR(m_tabs.scroll(ptr->widget()));
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

    shared_ptr<event_loop> loop;
    ICY_ERROR(event_loop::create(loop,
        event_type::window_close | event_type::gui_any | event_type::user_any));

    while (true)
    {
        event event;
        ICY_ERROR(loop->loop(event));
        if (event->type == event_type::global_quit)
            break;
        ICY_ERROR(exec(event));
    }
    return {};
}

uint32_t show_error(const error_type error, const string_view text) noexcept
{
    string msg;
    to_string("Error: %4 - %1 code [%2]: %3", msg, error.source, long(error.code), error, text);
    win32_message(msg, "Error"_s);
    return error.code;
}

icy::gui_image find_image(const mbox_image image) noexcept
{
    return g_icons[uint32_t(image)];
}
#include "icy_mbox_script_main.hpp"

using namespace icy;

ICY_STATIC_NAMESPACE_BEG
class mbox_form_action_group : public mbox_form_action
{
public:
    error_type initialize(const mbox::library& library, const mbox::action& action, xgui_widget& parent) noexcept override;
    error_type exec(const event event) noexcept override;
    error_type save(mbox::action& action) const noexcept override;
private:
    const mbox::library* m_library = nullptr;
    unique_ptr<mbox_model> m_explorer_group;
    gui_widget m_widget_group;
    icy::guid m_select_group;
};
class mbox_form_action_timer : public mbox_form_action
{
public:
    error_type initialize(const mbox::library& library, const mbox::action& action, xgui_widget& parent) noexcept override;
    error_type exec(const event event) noexcept override;
    error_type save(mbox::action& action) const noexcept override;
private:
    const mbox::library* m_library = nullptr;
    unique_ptr<mbox_model> m_timer_explorer;
    xgui_widget m_widget_count_label;
    xgui_widget m_widget_count_value;
    xgui_widget m_widget_timer;
    uint32_t m_value_count = 0;
    icy::guid m_select_timer;
};
class mbox_form_action_keymod
{
public: 
    error_type exec(const event event) noexcept;
    key_mod select() const noexcept
    {
        return m_select;
    }
protected:
    error_type initialize(xgui_widget& parent, const uint32_t row, const string_view name, 
        const key_mod left, const key_mod right, const key_mod select) noexcept;
private:
    xgui_widget m_label;
    xgui_widget m_value;
    xgui_model m_model;
    key_mod m_select = key_mod::none;
};
class mbox_form_action_keymod_ctrl : public mbox_form_action_keymod
{
public:
    error_type initialize(xgui_widget& parent, const uint32_t row, const key_mod select) noexcept
    {
        return mbox_form_action_keymod::initialize(parent, row, "Ctrl"_s, key_mod::lctrl, key_mod::rctrl, select);
    }
};
class mbox_form_action_keymod_alt : public mbox_form_action_keymod
{
public:
    error_type initialize(xgui_widget& parent, const uint32_t row, const key_mod select) noexcept
    {
        return mbox_form_action_keymod::initialize(parent, row, "Alt"_s, key_mod::lalt, key_mod::ralt, select);
    }
};
class mbox_form_action_keymod_shift : public mbox_form_action_keymod
{
public:
    error_type initialize(xgui_widget& parent, const uint32_t row, const key_mod select) noexcept
    {
        return mbox_form_action_keymod::initialize(parent, row, "Shift"_s, key_mod::lshift, key_mod::rshift, select);
    }
};
class mbox_form_action_key
{
public:
    error_type initialize(xgui_widget& parent, const uint32_t row, const key select) noexcept;
    error_type exec(const event event) noexcept;
    key select() const noexcept
    {
        return m_select;
    }
private:
    xgui_widget m_label;
    xgui_widget m_value;
    xgui_model m_model;
    key m_select = key::none;
};
class mbox_form_action_input : public mbox_form_action
{
public:
    error_type initialize(const mbox::library& library, const mbox::action& action, xgui_widget& parent) noexcept override;
    error_type exec(const event event) noexcept override;
    error_type save(mbox::action& action) const noexcept override;
private:
    mbox_form_action_key m_key;
    mbox_form_action_keymod_ctrl m_ctrl;
    mbox_form_action_keymod_alt m_alt;
    mbox_form_action_keymod_shift m_shift;
};
class mbox_form_action_event_button : public mbox_form_action
{
public:
    error_type initialize(const mbox::library& library, const mbox::action& action, xgui_widget& parent) noexcept override;
    error_type exec(const event event) noexcept override;
    error_type save(mbox::action& action) const noexcept override;
private:
    const mbox::library* m_library = nullptr;
    xgui_widget m_button_widget;
    xgui_widget m_command_widget;
    mbox_form_action_key m_key;
    mbox_form_action_keymod_ctrl m_ctrl;
    mbox_form_action_keymod_alt m_alt;
    mbox_form_action_keymod_shift m_shift;
    unique_ptr<mbox_model> m_command_explorer;
    icy::guid m_command_select;
};
class mbox_form_action_event_timer : public mbox_form_action
{
public:
    error_type initialize(const mbox::library& library, const mbox::action& action, xgui_widget& parent) noexcept override;
    error_type exec(const event event) noexcept override;
    error_type save(mbox::action& action) const noexcept override;
private:
    const mbox::library* m_library = nullptr;
    unique_ptr<mbox_model> m_timer_explorer;
    unique_ptr<mbox_model> m_command_explorer;
    xgui_widget m_timer_widget;
    xgui_widget m_command_widget;
    icy::guid m_timer_select;
    icy::guid m_command_select;
};
class mbox_form_action_command_replace : public mbox_form_action
{
public:
    error_type initialize(const mbox::library& library, const mbox::action& action, xgui_widget& parent) noexcept override;
    error_type exec(const event event) noexcept override;
    error_type save(mbox::action& action) const noexcept override;
private:
    const mbox::library* m_library = nullptr;
    unique_ptr<mbox_model> m_commands;
    xgui_widget m_layout_source;
    xgui_widget m_layout_target;
    xgui_widget m_label_source;
    xgui_widget m_label_target;
    xgui_widget m_value_source;
    xgui_widget m_value_target;
    mbox::action::replace_type m_value;
};
class mbox_form_action_command_execute : public mbox_form_action
{
public:
    error_type initialize(const mbox::library& library, const mbox::action& action, xgui_widget& parent) noexcept override;
    error_type exec(const event event) noexcept override;
    error_type save(mbox::action& action) const noexcept override;
private:
    const mbox::library* m_library = nullptr;
    unique_ptr<mbox_model> m_commands;
    xgui_model m_type_model;
    xgui_widget m_layout;
    xgui_widget m_type_label;
    xgui_widget m_type_value;
    xgui_widget m_group_value;
    xgui_widget m_group_button;
    xgui_widget m_widget_commands;
    mbox::action::execute_type m_value;
};
ICY_STATIC_NAMESPACE_END

error_type mbox_form_action::initialize(const icy::string_view command, const mbox::action_type type) noexcept
{
    ICY_ERROR(m_window.initialize(gui_widget_type::dialog, {}, gui_widget_flag::layout_grid));
    ICY_ERROR(m_command_label.initialize(gui_widget_type::label, m_window));
    ICY_ERROR(m_command_value.initialize(gui_widget_type::line_edit, m_window));
    ICY_ERROR(m_type_label.initialize(gui_widget_type::label, m_window));
    ICY_ERROR(m_type_value.initialize(gui_widget_type::line_edit, m_window));
    ICY_ERROR(m_buttons.initialize(gui_widget_type::none, m_window, gui_widget_flag::layout_hbox));
    ICY_ERROR(m_save.initialize(gui_widget_type::text_button, m_buttons));
    ICY_ERROR(m_exit.initialize(gui_widget_type::text_button, m_buttons));
    
    ICY_ERROR(m_command_label.text("Command"_s));
    ICY_ERROR(m_command_value.text(command));
    ICY_ERROR(m_type_label.text("Action Type"));
    ICY_ERROR(m_type_value.text(mbox::to_string(type)));
    ICY_ERROR(m_save.text("Save"_s));
    ICY_ERROR(m_exit.text("Cancel"_s));

    ICY_ERROR(m_command_value.enable(false));
    ICY_ERROR(m_type_value.enable(false));
    
    ICY_ERROR(m_command_label.insert(gui_insert(0, 0)));
    ICY_ERROR(m_command_value.insert(gui_insert(1, 0)));
    ICY_ERROR(m_type_label.insert(gui_insert(0, 1)));
    ICY_ERROR(m_type_value.insert(gui_insert(1, 1)));
    ICY_ERROR(m_buttons.insert(gui_insert(0, 3, 2, 1)));

    return {};
}
error_type mbox_form_action::exec(const mbox::library& library, const icy::guid& command, mbox::action& action, bool& saved) noexcept
{
    unique_ptr<mbox_form_action> form;
    switch (action.type())
    {
    case mbox::action_type::group_join:
    case mbox::action_type::group_leave:
        form = make_unique<mbox_form_action_group>();
        break;
    case mbox::action_type::button_press:
    case mbox::action_type::button_release:
    case mbox::action_type::button_click:
        form = make_unique<mbox_form_action_input>();
        break;
    case mbox::action_type::timer_start:
    case mbox::action_type::timer_stop:
    case mbox::action_type::timer_pause:
    case mbox::action_type::timer_resume:
        form = make_unique<mbox_form_action_timer>();
        break;
    case mbox::action_type::command_execute:
        form = make_unique<mbox_form_action_command_execute>();
        break;
    case mbox::action_type::command_replace:
        form = make_unique<mbox_form_action_command_replace>();
        break;
    case mbox::action_type::on_button_press:
    case mbox::action_type::on_button_release:
        form = make_unique<mbox_form_action_event_button>();
        break;
    case mbox::action_type::on_timer:
        form = make_unique<mbox_form_action_event_timer>();
        break;
    default:
        ICY_ASSERT(false, "INVALID TYPE");
    }
    if (!form)
        return make_stdlib_error(std::errc::not_enough_memory);

    string str;
    ICY_ERROR(library.path(command, str));
    ICY_ERROR(form->m_window.initialize(gui_widget_type::dialog, {}, gui_widget_flag::layout_grid));

    ICY_ERROR(form->initialize(str, action.type()));
    ICY_ERROR(form->initialize(library, action, form->m_widget));
    ICY_ERROR(form->m_widget.insert(gui_insert(0, 2, 2, 1)));
    
    if (!saved)
        ICY_ERROR(form->m_save.enable(false));
    
    saved = false;
    ICY_ERROR(form->m_window.show(true));

    shared_ptr<event_loop> loop;
    ICY_ERROR(event_loop::create(loop, event_type::window_close | event_type::gui_select | event_type::gui_update));

    while (true)
    {
        event event;
        ICY_ERROR(loop->loop(event));
        if (event->type == event_type::global_quit)
            break;

        const auto& event_data = event->data<gui_event>();
        if (event->type == event_type::window_close && event_data.widget == form->m_window)
            break;

        if (event->type == event_type::gui_update)
        {
            if (event_data.widget == form->m_save)
            {
                ICY_ERROR(form->save(action));
                saved = true;
                break;
            }
            else if (event_data.widget == form->m_exit)
            {
                break;
            }
        }
        ICY_ERROR(form->exec(event));
    }
    return {};
}

error_type mbox_form_action_key::initialize(xgui_widget& parent, const uint32_t row, const key select) noexcept
{
    ICY_ERROR(m_label.initialize(gui_widget_type::label, parent));
    ICY_ERROR(m_value.initialize(gui_widget_type::combo_box, parent));
    ICY_ERROR(m_model.initialize());
    ICY_ERROR(m_model.insert_cols(0, 1));
    ICY_ERROR(m_value.bind(m_model));

    dictionary<key> keys;
    for (auto k = 0u; k < 256u; ++k)
    {
        const auto str = mbox::to_string(key(k));
        if (str.empty())
            continue;
        ICY_ERROR(keys.insert(str, key(k)));
    }
    ICY_ERROR(m_model.insert_rows(0, keys.size() + 1));
    ICY_ERROR(m_model.node(0, 0).text("None"_s));

    auto key_row = 1_z;
    for (auto&& pair : keys)
    {
        auto node = m_model.node(key_row, 0);
        ICY_ERROR(node.text(pair.key));
        ICY_ERROR(node.udata(uint64_t(pair.value)));
        ++key_row;
        if (pair.value == select)
        {
            m_select = select;
            ICY_ERROR(m_value.scroll(node));
        }
    }
    ICY_ERROR(m_label.text("Button"_s));
    ICY_ERROR(m_label.insert(gui_insert(0, row)));
    ICY_ERROR(m_value.insert(gui_insert(1, row)));
    return {};
}
error_type mbox_form_action_key::exec(const event event) noexcept
{
    if (event->type == event_type::gui_select)
    {
        const auto& event_data = event->data<gui_event>();
        if (event_data.widget == m_value)
            m_select = key(event_data.data.as_node().udata().as_uinteger());
    }
    return {};
}

error_type mbox_form_action_keymod::initialize(xgui_widget& parent, const uint32_t row,
    const string_view name, const key_mod left, const key_mod right, const key_mod select) noexcept
{
    enum
    {
        row_none,
        row_left,
        row_right,
        row_any,
        _row_count,
    };
    ICY_ERROR(m_label.initialize(gui_widget_type::label, parent));
    ICY_ERROR(m_value.initialize(gui_widget_type::combo_box, parent));
    ICY_ERROR(m_model.initialize());
    ICY_ERROR(m_model.insert_cols(0, 1));
    ICY_ERROR(m_model.insert_rows(0, _row_count));
    ICY_ERROR(m_value.bind(m_model));

    const auto append = [this, select](const size_t row, const string_view text, const key_mod value)
    {
        auto node = m_model.node(row, 0);
        if (!node) return make_stdlib_error(std::errc::not_enough_memory);
        ICY_ERROR(node.text(text));
        ICY_ERROR(node.udata(uint64_t(value)));
        if ((select & value) == value)
        {
            m_select = value;
            ICY_ERROR(m_value.scroll(node));
        }
        return error_type();
    };
    ICY_ERROR(append(row_none, "None"_s, key_mod::none));
    ICY_ERROR(append(row_left, "Left"_s, left));
    ICY_ERROR(append(row_right, "Right"_s, right));
    ICY_ERROR(append(row_any, "Any"_s, key_mod(left | right)));

    ICY_ERROR(m_label.text(name));
    ICY_ERROR(m_label.insert(gui_insert(0, row)));
    ICY_ERROR(m_value.insert(gui_insert(1, row)));
    return {};
}
error_type mbox_form_action_keymod::exec(const event event) noexcept
{
    if (event->type == event_type::gui_select)
    {
        const auto& event_data = event->data<gui_event>();
        if (event_data.widget == m_value)
            m_select = key_mod(event_data.data.as_node().udata().as_uinteger());
    }
    return {};
}

error_type mbox_form_action_group::initialize(const mbox::library& library, const mbox::action& action, xgui_widget& parent) noexcept
{
    m_library = &library;
    ICY_ERROR(mbox_model::create(mbox_model_type::explorer, m_explorer_group));
    ICY_ERROR(parent.initialize(gui_widget_type::tree_view, m_window));
    ICY_ERROR(m_explorer_group->reset(library, mbox::root, mbox::type::group));
    ICY_ERROR(parent.bind(m_explorer_group->root()));

    if (auto node = m_explorer_group->find(m_select_group = action.group()))
        ICY_ERROR(parent.scroll(node));
    
    m_widget_group = parent;
    return {};
}
error_type mbox_form_action_group::exec(const event event) noexcept
{
    if (event->type == event_type::gui_select)
    {
        const auto& event_data = event->data<gui_event>();
        if (event_data.widget == m_widget_group)
            m_select_group = event_data.data.as_node().udata().as_guid();
    }
    ICY_ERROR(m_explorer_group->exec(event));
    return {};
}
error_type mbox_form_action_group::save(mbox::action& action) const noexcept
{
    const auto ptr = m_library->find(m_select_group);
    if (!ptr || ptr->type != mbox::type::group)
        return mbox::make_mbox_error(mbox::mbox_error_code::invalid_group);
    switch (action.type())
    {
    case mbox::action_type::group_join:
        action = mbox::action::create_group_join(m_select_group);
        break;
    case mbox::action_type::group_leave:
        action = mbox::action::create_group_leave(m_select_group);
        break;
    default:
        return mbox::make_mbox_error(mbox::mbox_error_code::invalid_type);
    }
    return {};
}

error_type mbox_form_action_timer::initialize(const mbox::library& library, const mbox::action& action, xgui_widget& parent) noexcept
{
    m_library = &library;
    ICY_ERROR(mbox_model::create(mbox_model_type::explorer, m_timer_explorer));
    ICY_ERROR(parent.initialize(gui_widget_type::none, m_window, gui_widget_flag::layout_grid));
    ICY_ERROR(m_timer_explorer->reset(library, mbox::root, mbox::type::timer));
    ICY_ERROR(m_widget_count_label.initialize(gui_widget_type::label, parent));
    ICY_ERROR(m_widget_count_value.initialize(gui_widget_type::line_edit, parent));
    ICY_ERROR(m_widget_timer.initialize(gui_widget_type::tree_view, parent));
    ICY_ERROR(m_widget_count_label.text("Repeat Count"_s));
    ICY_ERROR(m_widget_timer.bind(m_timer_explorer->root()));

    string str_count;
    ICY_ERROR(to_string(m_value_count = action.timer().count, str_count));
    ICY_ERROR(m_widget_count_value.text(str_count));
    
    if (action.type() == mbox::action_type::timer_start)
    {
        ICY_ERROR(m_widget_count_label.insert(gui_insert(0, 0)));
        ICY_ERROR(m_widget_count_value.insert(gui_insert(1, 0)));
        ICY_ERROR(m_widget_timer.insert(gui_insert(0, 1, 2, 1)));
    }
    else
    {
        ICY_ERROR(m_widget_timer.insert(gui_insert(0, 0)));
    }

    if (const auto node = m_timer_explorer->find(m_select_timer = action.timer().timer))
        ICY_ERROR(m_widget_timer.scroll(node));

    return {};
}
error_type mbox_form_action_timer::exec(const event event) noexcept
{
    if (event->type == event_type::gui_select)
    {
        const auto& event_data = event->data<gui_event>();
        if (event_data.widget == m_widget_timer)
            m_select_timer = event_data.data.as_node().udata().as_guid();
    }
    else if (event->type == event_type::gui_update)
    {
        const auto& event_data = event->data<gui_event>();
        if (event_data.widget == m_widget_count_value)
            event_data.data.as_string().to_value(m_value_count);
    }
    ICY_ERROR(m_timer_explorer->exec(event));
    return {};
}
error_type mbox_form_action_timer::save(mbox::action& action) const noexcept
{
    const auto ptr = m_library->find(m_select_timer);
    if (!ptr || ptr->type != mbox::type::timer)
        return mbox::make_mbox_error(mbox::mbox_error_code::invalid_timer);
    switch (action.type())
    {
    case mbox::action_type::timer_start:
        if (!m_value_count)
            return mbox::make_mbox_error(mbox::mbox_error_code::invalid_timer_count);
        action = mbox::action::create_timer_start(m_select_timer, m_value_count);
        break;
    case mbox::action_type::timer_stop:
        action = mbox::action::create_timer_stop(m_select_timer);
        break;
    case mbox::action_type::timer_pause:
        action = mbox::action::create_timer_pause(m_select_timer);
        break;
    case mbox::action_type::timer_resume:
        action = mbox::action::create_timer_resume(m_select_timer);
        break;
    default:
        ICY_ASSERT(false, "INVALID TYPE");
    }
    return {};
}

error_type mbox_form_action_input::initialize(const mbox::library& library, const mbox::action& action, xgui_widget& parent) noexcept
{
    enum
    {
        row_key,
        row_ctrl,
        row_alt,
        row_shift,
    };
    ICY_ERROR(parent.initialize(gui_widget_type::none, m_window, gui_widget_flag::layout_grid));
    ICY_ERROR(m_key.initialize(parent, row_key, action.button().key));
    ICY_ERROR(m_ctrl.initialize(parent, row_ctrl, action.button().mod));
    ICY_ERROR(m_alt.initialize(parent, row_alt, action.button().mod));
    ICY_ERROR(m_shift.initialize(parent, row_shift, action.button().mod));
    return {};
}
error_type mbox_form_action_input::exec(const event event) noexcept
{
    ICY_ERROR(m_key.exec(event));
    ICY_ERROR(m_ctrl.exec(event));
    ICY_ERROR(m_alt.exec(event));
    ICY_ERROR(m_shift.exec(event));
    return {};
}
error_type mbox_form_action_input::save(mbox::action& action) const noexcept
{
    mbox::button_type button;
    button.key = m_key.select();
    button.mod = key_mod(m_ctrl.select() | m_alt.select() | m_shift.select());
    switch (action.type())
    {
    case mbox::action_type::button_press:
        action = mbox::action::create_button_press(button);
        break;
    case mbox::action_type::button_release:
        action = mbox::action::create_button_release(button);
        break;
    case mbox::action_type::button_click:
        action = mbox::action::create_button_click(button);
        break;
    default:
        ICY_ASSERT(false, "INVALID TYPE");
    }
    return {};
}

error_type mbox_form_action_event_button::initialize(const mbox::library& library, const mbox::action& action, xgui_widget& parent) noexcept
{
    m_library = &library;
    enum
    {
        row_key,
        row_ctrl,
        row_alt,
        row_shift,
    };
    ICY_ERROR(parent.initialize(gui_widget_type::hsplitter, m_window));
    ICY_ERROR(m_button_widget.initialize(gui_widget_type::none, parent, gui_widget_flag::layout_grid));
    ICY_ERROR(m_command_widget.initialize(gui_widget_type::tree_view, parent));
  
    ICY_ERROR(m_key.initialize(m_button_widget, row_key, action.event_button().key));
    ICY_ERROR(m_ctrl.initialize(m_button_widget, row_ctrl, action.event_button().mod));
    ICY_ERROR(m_alt.initialize(m_button_widget, row_alt, action.event_button().mod));
    ICY_ERROR(m_shift.initialize(m_button_widget, row_shift, action.event_button().mod));
    
    ICY_ERROR(mbox_model::create(mbox_model_type::explorer, m_command_explorer));
    ICY_ERROR(m_command_explorer->reset(library, mbox::root, mbox::type::command));
    ICY_ERROR(m_command_widget.bind(m_command_explorer->root()));
    
    if (const auto node = m_command_explorer->find(m_command_select = action.event_command()))
        ICY_ERROR(m_command_widget.scroll(node));
    return {};
}
error_type mbox_form_action_event_button::exec(const event event) noexcept
{
    if (event->type == event_type::gui_select)
    {
        const auto& event_data = event->data<gui_event>();
        if (event_data.widget == m_command_widget)
            m_command_select = event_data.data.as_node().udata().as_guid();
    }
    ICY_ERROR(m_key.exec(event));
    ICY_ERROR(m_ctrl.exec(event));
    ICY_ERROR(m_alt.exec(event));
    ICY_ERROR(m_shift.exec(event));
    return {};
}
error_type mbox_form_action_event_button::save(mbox::action& action) const noexcept
{
    const auto command = m_library->find(m_command_select);
    if (!command || command->type != mbox::type::command)
        return mbox::make_mbox_error(mbox::mbox_error_code::invalid_command);

    mbox::button_type button;
    button.key = m_key.select();
    button.mod = key_mod(m_ctrl.select() | m_alt.select() | m_shift.select());
    switch (action.type())
    {
    case mbox::action_type::on_button_press:
        action = mbox::action::create_on_button_press(button, m_command_select);
        break;
    case mbox::action_type::on_button_release:
        action = mbox::action::create_on_button_release(button, m_command_select);
        break;
    default:
        return mbox::make_mbox_error(mbox::mbox_error_code::invalid_type);
    }
    return {};
}

error_type mbox_form_action_event_timer::initialize(const mbox::library& library, const mbox::action& action, xgui_widget& parent) noexcept
{
    m_library = &library;
    ICY_ERROR(parent.initialize(gui_widget_type::hsplitter, m_window));
    ICY_ERROR(mbox_model::create(mbox_model_type::explorer, m_timer_explorer));
    ICY_ERROR(mbox_model::create(mbox_model_type::explorer, m_command_explorer));
    ICY_ERROR(m_timer_widget.initialize(gui_widget_type::tree_view, parent));
    ICY_ERROR(m_command_widget.initialize(gui_widget_type::tree_view, parent));

    ICY_ERROR(m_timer_explorer->reset(library, mbox::root, mbox::type::timer));
    ICY_ERROR(m_timer_widget.bind(m_timer_explorer->root()));

    ICY_ERROR(m_command_explorer->reset(library, mbox::root, mbox::type::command));
    ICY_ERROR(m_command_widget.bind(m_command_explorer->root()));

    if (const auto node = m_timer_explorer->find(m_timer_select = action.event_timer()))
        ICY_ERROR(m_timer_widget.scroll(node));

    if (const auto node = m_command_explorer->find(m_command_select = action.event_command()))
        ICY_ERROR(m_command_widget.scroll(node));

    return {};
}
error_type mbox_form_action_event_timer::exec(const event event) noexcept
{
    if (event->type == event_type::gui_select)
    {
        const auto& event_data = event->data<gui_event>();
        if (event_data.widget == m_timer_widget)
            m_timer_select = event_data.data.as_node().udata().as_guid();
        else if (event_data.widget == m_command_widget)
            m_command_select = event_data.data.as_node().udata().as_guid();
    }
    return {};
}
error_type mbox_form_action_event_timer::save(mbox::action& action) const noexcept
{
    const auto timer = m_library->find(m_timer_select);
    const auto command = m_library->find(m_command_select);    
    if (!timer || timer->type != mbox::type::timer)
        return mbox::make_mbox_error(mbox::mbox_error_code::invalid_timer);
    if (!command || command->type != mbox::type::command)
        return mbox::make_mbox_error(mbox::mbox_error_code::invalid_command);
    
    action = mbox::action::create_on_timer(m_timer_select, m_command_select);
    return {};
}

error_type mbox_form_action_command_replace::initialize(const mbox::library& library, const mbox::action& action, xgui_widget& parent) noexcept
{
    m_library = &library;
    ICY_ERROR(mbox_model::create(mbox_model_type::explorer, m_commands));
    ICY_ERROR(parent.initialize(gui_widget_type::hsplitter, m_window));
    ICY_ERROR(m_commands->reset(library, mbox::root, mbox::type::command));
    ICY_ERROR(m_layout_source.initialize(gui_widget_type::none, parent, gui_widget_flag::layout_vbox));
    ICY_ERROR(m_layout_target.initialize(gui_widget_type::none, parent, gui_widget_flag::layout_vbox));
    ICY_ERROR(m_label_source.initialize(gui_widget_type::label, m_layout_source));
    ICY_ERROR(m_label_target.initialize(gui_widget_type::label, m_layout_target));
    ICY_ERROR(m_value_source.initialize(gui_widget_type::tree_view, m_layout_source));
    ICY_ERROR(m_value_target.initialize(gui_widget_type::tree_view, m_layout_target));
    ICY_ERROR(m_value_source.bind(m_commands->root()));
    ICY_ERROR(m_value_target.bind(m_commands->root()));
    ICY_ERROR(m_layout_source.text("Source"_s));
    ICY_ERROR(m_layout_target.text("Replace"_s));

    if (auto node = m_commands->find(m_value.source = action.replace().source)) 
        ICY_ERROR(m_value_source.scroll(node));

    if (auto node = m_commands->find(m_value.target = action.replace().target))
        ICY_ERROR(m_value_source.scroll(node));

    return {};
}
error_type mbox_form_action_command_replace::exec(const event event) noexcept
{
    if (event->type == event_type::gui_select)
    {
        const auto& event_data = event->data<gui_event>();
        if (event_data.widget == m_value_source)
            m_value.source = event_data.data.as_node().udata().as_guid();
        if (event_data.widget == m_value_target)
            m_value.target = event_data.data.as_node().udata().as_guid();
    }
    return {};
}
error_type mbox_form_action_command_replace::save(mbox::action& action) const noexcept
{
    const auto source = m_library->find(m_value.source);
    const auto target = m_library->find(m_value.target);
    if (!source || source == target 
        || source->type != mbox::type::command 
        || target->type != mbox::type::command)
        return mbox::make_mbox_error(mbox::mbox_error_code::invalid_command);
    action = mbox::action::create_command_replace(m_value);
    return {};
}

error_type mbox_form_action_command_execute::initialize(const mbox::library& library, const mbox::action& action, xgui_widget& parent) noexcept
{
    m_library = &library;
    ICY_ERROR(mbox_model::create(mbox_model_type::explorer, m_commands));
    ICY_ERROR(parent.initialize(gui_widget_type::none, m_window, gui_widget_flag::layout_vbox));
    ICY_ERROR(m_type_model.initialize());
    ICY_ERROR(m_type_model.insert_cols(0, 1));
    ICY_ERROR(m_type_model.insert_rows(0, size_t(mbox::execute_type::_total)));
    for (auto k = 0_z; k < size_t(mbox::execute_type::_total); ++k)
    {
        const auto type = mbox::execute_type(k);
        auto node = m_type_model.node(k, 0);
        ICY_ERROR(node.text(to_string(type)));
        ICY_ERROR(node.udata(k));
    }
    ICY_ERROR(m_layout.initialize(gui_widget_type::none, parent));
    ICY_ERROR(m_widget_commands.initialize(gui_widget_type::tree_view, parent));
    ICY_ERROR(m_type_label.initialize(gui_widget_type::label, m_layout));
    ICY_ERROR(m_type_value.initialize(gui_widget_type::combo_box, m_layout));
    ICY_ERROR(m_group_value.initialize(gui_widget_type::line_edit, m_layout));
    ICY_ERROR(m_group_button.initialize(gui_widget_type::tool_button, m_layout));
    ICY_ERROR(m_type_value.bind(m_type_model));
    ICY_ERROR(m_type_value.scroll(m_type_model.node(size_t(m_value.etype = action.execute().etype), 0)));

    string str_group;
    ICY_ERROR(m_library->path(m_value.group = action.execute().group, str_group));
    ICY_ERROR(m_group_value.text(str_group));
    
    if (const auto node = m_commands->find(m_value.command = action.execute().command))
        ICY_ERROR(m_widget_commands.scroll(node));
    
    return {};
}
error_type mbox_form_action_command_execute::exec(const event event) noexcept
{
    if (event->type == event_type::gui_update)
    {
        const auto& event_data = event->data<gui_event>();
        if (event_data.widget == m_group_button)
        {

        }
    }
    else if (event->type == event_type::gui_select)
    {
        const auto& event_data = event->data<gui_event>();
        if (event_data.widget == m_type_value)
        {
            m_value.etype = mbox::execute_type(event_data.data.as_node().udata().as_uinteger());
        }
        else if (event_data.widget == m_widget_commands)
        {
            m_value.command = event_data.data.as_node().udata().as_guid();
        }
    }
    ICY_ERROR(m_commands->exec(event));
    return {};
}
error_type mbox_form_action_command_execute::save(mbox::action& action) const noexcept
{
    const auto command = m_library->find(m_value.command);
    const auto group = m_library->find(m_value.group);
    if (!command || command->type != mbox::type::command)
        return mbox::make_mbox_error(mbox::mbox_error_code::invalid_command);

    if (m_value.etype != mbox::execute_type::self)
    {
        if (!group || group->type != mbox::type::group)
            return mbox::make_mbox_error(mbox::mbox_error_code::invalid_group);
    }
    action = mbox::action::create_command_execute(m_value);
    return {};
}
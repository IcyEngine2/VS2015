#include "icy_mbox_script_form2.hpp"
#include "icy_mbox_script_common.hpp"
#include <icy_engine/image/icy_image.hpp>
#include <icy_qtgui/icy_qtgui.hpp>
#include "../icy_mbox_script.hpp"
#include "icons/command.h"
#include "icons/binding.h"
#include "icons/directory.h"
#include "icons/event.h"
#include "icons/group.h"
#include "icons/input.h"
#include "icons/profile.h"
#include "icons/timer.h"
#include "icons/variable.h"

using namespace icy;

error_type mbox_form_key_value_base::reset(mbox::library& library, const mbox::base& base) noexcept
{
    m_data.clear();
    m_select = guid();
    ICY_ERROR(model.gui->clear(model));
    if (base.type == mbox::type::none)
        return {};

    ICY_ERROR(model.gui->insert_cols(model, 0, 1));

    array<guid> indices;
    const_array_view<guid> indices_ref;
    map<string, guid> map;
    if (base.index != guid())
    {
        indices_ref = base.value.directory.indices;
    }
    else
    {
        ICY_ERROR(library.enumerate(base.type, indices));
        indices_ref = indices;
    }
    for (auto&& index : indices_ref)
    {
        string path;
        auto ptr = library.find(index);
        ICY_ERROR(to_string(ptr->name, path));
        if (base.index == guid())
        {
            path.clear();
            ICY_ERROR(library.path(index, path));
        }
        ICY_ERROR(map.insert(std::move(path), guid(index)));
    }
    ICY_ERROR(model.gui->insert_rows(model, 0, map.size()));
    auto row = 0u;
    for (auto&& pair : map)
    {
        const auto node = model.gui->node(model, row++, 0);
        ICY_ERROR(model.gui->text(node, pair.key));
        ICY_ERROR(model.gui->udata(node, pair.value));
    }
    if (!map.empty())
        m_select = map.begin()->value;
    
    if (base.index == guid() && base.type == mbox::type::group)
    {
        const std::pair<string_view, guid> group[] = {
        { mbox::str_group_default, mbox::group_default },
        { mbox::str_group_broadcast, mbox::group_broadcast },
        { mbox::str_group_multicast, mbox::group_multicast } };        

        ICY_ERROR(model.gui->insert_rows(model, 0, _countof(group)));
        row = 0u;
        for (auto&& pair : group)
        {
            const auto node = model.gui->node(model, row++, 0);
            ICY_ERROR(model.gui->text(node, pair.first));
            ICY_ERROR(model.gui->udata(node, pair.second));
            ICY_ERROR(m_data.push_back(pair.second));
        }
        m_select = mbox::group_default;
    }
    for (auto&& pair : map)
        ICY_ERROR(m_data.push_back(pair.value));
    return {};
}

error_type mbox_form_value_input::initialize(const gui_widget parent, const mbox::base& base) noexcept
{
    using namespace icy;
    auto& gui = *m_widget.gui;

    ICY_ERROR(m_widget.initialize(parent, gui_widget_type::none, gui_widget_flag::layout_grid));
    ICY_ERROR(m_button.initialize("Button"_s, m_widget));
    ICY_ERROR(m_event.initialize("Event"_s, m_widget));
    ICY_ERROR(m_ctrl.initialize("Ctrl"_s, m_widget));
    ICY_ERROR(m_alt.initialize("Alt"_s, m_widget));
    ICY_ERROR(m_shift.initialize("Shift"_s, m_widget));
    ICY_ERROR(m_macro.initialize("Macro"_s, m_widget, gui_widget_type::text_edit));

    auto row = 0u;
    ICY_ERROR(m_button.insert(row));
    ICY_ERROR(m_event.insert(row));
    ICY_ERROR(m_ctrl.insert(row));
    ICY_ERROR(m_alt.insert(row));
    ICY_ERROR(m_shift.insert(row));
    ICY_ERROR(m_macro.insert(row));

    const auto func = [](mbox_form_key_value_combo& mod)
    {
        const string_view texts[] = { "None"_s, "Left"_s, "Right"_s, "Any"_s };
        for (auto k = 0u; k < _countof(texts); ++k)
            ICY_ERROR(mod.append(k, texts[k]));
        return error_type();
    };
    ICY_ERROR(func(m_ctrl));
    ICY_ERROR(func(m_alt));
    ICY_ERROR(func(m_shift));

    ICY_ERROR(m_button.append(0, "None"_s));
    map<string_view, uint32_t> key_map;
    for (auto k = 0u; k < 256u; ++k)
    {
        auto str = to_string(key(k));
        if (str.empty())
            continue;
        ICY_ERROR(key_map.insert(std::move(str), std::move(k)));
    }
    for (auto&& pair : key_map)
        ICY_ERROR(m_button.append(pair.value, pair.key));

    ICY_ERROR(m_event.append(uint32_t(key_event::none), "None"_s));
    ICY_ERROR(m_event.append(uint32_t(key_event::press), "Press"_s));
    ICY_ERROR(m_event.append(uint32_t(key_event::release), "Release"_s));

    const auto& value = base.value.input;
    ICY_ERROR(m_button.scroll(uint32_t(value.button)));
    ICY_ERROR(m_event.scroll(uint32_t(value.event)));
    ICY_ERROR(m_ctrl.scroll(uint32_t(value.ctrl)));
    ICY_ERROR(m_alt.scroll(uint32_t(value.ctrl)));
    ICY_ERROR(m_shift.scroll(uint32_t(value.ctrl)));
    ICY_ERROR(gui.text(m_macro.value, value.macro));

    if (m_mode == mbox_form_mode::view)
    {
        ICY_ERROR(gui.enable(m_button.value, false));
        ICY_ERROR(gui.enable(m_event.value, false));
        ICY_ERROR(gui.enable(m_ctrl.value, false));
        ICY_ERROR(gui.enable(m_alt.value, false));
        ICY_ERROR(gui.enable(m_shift.value, false));
        ICY_ERROR(gui.enable(m_macro.value, false));
    }

    return {};
}
error_type mbox_form_value_input::create(mbox::base& base) noexcept
{
    using namespace icy;

    auto& value = base.value.input;
    if (!m_button.select || !m_event.select)
        return make_stdlib_error(std::errc::invalid_argument);

    value.ctrl = m_ctrl.select;
    value.alt = m_alt.select;
    value.shift = m_shift.select;
    value.event = key_event(m_event.select);
    value.button = key(m_button.select);
    ICY_ERROR(to_string(m_macro.string, value.macro));

    return {};
}
error_type mbox_form_value_input::exec(const event event) noexcept
{
    ICY_ERROR(m_button.exec(event));
    ICY_ERROR(m_event.exec(event));
    ICY_ERROR(m_ctrl.exec(event));
    ICY_ERROR(m_alt.exec(event));
    ICY_ERROR(m_shift.exec(event));
    ICY_ERROR(m_macro.exec(event));
    return {};
}

error_type mbox_form_value_type::initialize(const gui_widget parent, const mbox::base& base) noexcept
{
    auto& gui = *m_widget.gui;

    ICY_ERROR(m_widget.initialize(parent, gui_widget_type::none, gui_widget_flag::layout_grid | gui_widget_flag::auto_insert));
    ICY_ERROR(m_type.initialize(""_s, m_widget));
    ICY_ERROR(m_group.initialize("Group"_s, m_widget));
    ICY_ERROR(m_reference_list.initialize(""_s, m_widget));
    ICY_ERROR(m_reference_base.initialize(""_s, m_widget));
    ICY_ERROR(m_secondary_list.initialize(""_s, m_widget));
    ICY_ERROR(m_secondary_base.initialize(""_s, m_widget));

    auto row = 0u;
    ICY_ERROR(m_type.insert(row));
    ICY_ERROR(m_group.insert(row));
    ICY_ERROR(m_reference_list.insert(row));
    ICY_ERROR(m_reference_base.insert(row));
    ICY_ERROR(m_secondary_list.insert(row));
    ICY_ERROR(m_secondary_base.insert(row));

    if (base.type == mbox::type::event)
    {
        ICY_ERROR(gui.text(m_type.key, "Event Type"_s));
        ICY_ERROR(m_type.fill<mbox::event_type>(true));
    }
    else if (base.type == mbox::type::command && base.value.command.actions.size() == 1)
    {
        ICY_ERROR(gui.text(m_type.key, "Action Type"_s));
        ICY_ERROR(m_type.fill<mbox::action_type>(true));
    }
    else
    {
        return make_stdlib_error(std::errc::invalid_argument);
    }
    
    mbox::base group;
    group.type = mbox::type::group;
    ICY_ERROR(m_group.reset(m_library, group));
    
    if (m_mode == mbox_form_mode::view)
    {
        ICY_ERROR(gui.enable(m_type.value, false));
        ICY_ERROR(gui.enable(m_group.value, false));
        ICY_ERROR(gui.enable(m_reference_list.value, false));
        ICY_ERROR(gui.enable(m_reference_base.value, false));
        ICY_ERROR(gui.enable(m_secondary_base.value, false));
        ICY_ERROR(gui.enable(m_secondary_list.value, false));
    }
    else
    {
        auto type = 0u;
        guid group;
        guid reference;
        guid secondary;
        if (base.type == mbox::type::event)
        {
            const auto& value = base.value.event;
            type = uint32_t(value.etype);
            group = value.group;
            reference = value.reference;
            secondary = value.compare;
        }
        else // action
        {
            const auto& value = base.value.command.actions[0];
            type = uint32_t(value.atype);
            group = value.group;
            reference = value.reference;
            secondary = value.assign;
        }


        ICY_ERROR(m_type.scroll(type));
        ICY_ERROR(m_group.scroll(group));
        ICY_ERROR(on_select(type));

        const auto ref_base = m_library.find(reference);
        const auto ref_list = ref_base ? m_library.find(ref_base->parent) : nullptr;
        if (ref_base && ref_list)
        {
            ICY_ERROR(m_reference_list.scroll(ref_list->index));
            ICY_ERROR(m_reference_base.reset(m_library, *ref_list));
            ICY_ERROR(m_reference_base.scroll(reference));
        }

        const auto sec_base = m_library.find(secondary);
        const auto sec_list = sec_base ? m_library.find(sec_base->parent) : nullptr;
        if (sec_base && sec_list)
        {
            ICY_ERROR(m_secondary_list.scroll(sec_list->index));
            ICY_ERROR(m_secondary_base.reset(m_library, *sec_list));
            ICY_ERROR(m_secondary_base.scroll(secondary));
        }
    }
    return {};
}
error_type mbox_form_value_type::exec(const event event) noexcept
{
    ICY_ERROR(m_type.exec(event));
    ICY_ERROR(m_group.exec(event)); 
    ICY_ERROR(m_reference_list.exec(event));
    ICY_ERROR(m_reference_base.exec(event));
    ICY_ERROR(m_secondary_list.exec(event));
    ICY_ERROR(m_secondary_base.exec(event));

    if (event->type == event_type::gui_select)
    {
        const auto& event_data = event->data<gui_event>();
        if (event_data.widget == m_type.value)
        {
            ICY_ERROR(on_select(m_type.select));
        }
        else if (event_data.widget == m_reference_list.value)
        {
            if (const auto ptr = m_library.find(m_reference_list.select()))
                ICY_ERROR(m_reference_base.reset(m_library, *ptr));
        }
        else if (event_data.widget == m_secondary_list.value)
        {
            if (const auto ptr = m_library.find(m_secondary_list.select()))
                ICY_ERROR(m_secondary_base.reset(m_library, *ptr));
        }
    }
    return {};
}
error_type mbox_form_value_type::on_select(const uint32_t type) noexcept
{
    auto& gui = *m_widget.gui;
    auto list_1 = mbox::type::none;
    auto base_1 = mbox::type::none;
    auto list_2 = mbox::type::none;
    auto base_2 = mbox::type::none;
    
    if (m_mtype == mbox::type::event)
    {
        switch (auto type = mbox::event_type(m_type.select))
        {
        case mbox::event_type::variable_changed:
        case mbox::event_type::variable_equal:
        case mbox::event_type::variable_not_equal:
        case mbox::event_type::variable_greater:
        case mbox::event_type::variable_lesser:
        case mbox::event_type::variable_greater_or_equal:
        case mbox::event_type::variable_lesser_or_equal:
        case mbox::event_type::variable_str_contains:
        {
            list_1 = mbox::type::list_variables;
            base_1 = mbox::type::variable;
            if (type != mbox::event_type::variable_changed)
            {
                list_2 = list_1;
                base_2 = base_1;
            }
            break;
        }

        case mbox::event_type::recv_input:
        {
            list_1 = mbox::type::list_inputs;
            base_1 = mbox::type::input;
            break;
        }

        case mbox::event_type::timer_first:
        case mbox::event_type::timer_tick:
        case mbox::event_type::timer_last:
        {
            list_1 = mbox::type::list_timers;
            base_1 = mbox::type::timer;
            break;
        }
        }
    }
    else // action
    {
        switch (const auto type = mbox::action_type(m_type.select))
        {
        case mbox::action_type::variable_assign:
        case mbox::action_type::variable_inc:
        case mbox::action_type::variable_dec:
            list_1 = mbox::type::list_variables;
            base_1 = mbox::type::variable;
            if (type == mbox::action_type::variable_assign)
            {
                list_2 = list_1;
                base_2 = base_1;
            }
            break;
        
        case mbox::action_type::execute_command:
            list_1 = mbox::type::list_commands;
            base_1 = mbox::type::command;
            break;

        case mbox::action_type::timer_start:
        case mbox::action_type::timer_stop:
        case mbox::action_type::timer_pause:
        case mbox::action_type::timer_resume:
            list_1 = mbox::type::list_timers;
            base_1 = mbox::type::timer;
            break;
        

        case mbox::action_type::send_input:
            list_1 = mbox::type::list_inputs;
            base_1 = mbox::type::input;
            break;
        
        case mbox::action_type::join_group:
        case mbox::action_type::leave_group:
            list_1 = mbox::type::group;
            break;
        }
    }

    ICY_ERROR(gui.text(m_reference_list.key, mbox::to_string(list_1)));
    ICY_ERROR(gui.text(m_reference_base.key, mbox::to_string(base_1)));
    ICY_ERROR(gui.text(m_secondary_list.key, mbox::to_string(list_2)));
    ICY_ERROR(gui.text(m_secondary_base.key, mbox::to_string(base_2)));

    mbox::base list_reference; list_reference.type = list_1;
    mbox::base list_secondary; list_secondary.type = list_2;
    ICY_ERROR(m_reference_list.reset(m_library, list_reference));
    ICY_ERROR(m_secondary_list.reset(m_library, list_secondary));


    ICY_ERROR(gui.enable(m_reference_list.value, list_1 != mbox::type::none));
    ICY_ERROR(gui.enable(m_reference_base.value, base_1 != mbox::type::none));
    ICY_ERROR(gui.enable(m_secondary_list.value, list_2 != mbox::type::none));
    ICY_ERROR(gui.enable(m_secondary_base.value, base_2 != mbox::type::none));

    if (const auto ptr = m_library.find(m_reference_list.select()))
    {
        ICY_ERROR(m_reference_base.reset(m_library, *ptr));
    }
    else
    {
        ICY_ERROR(m_reference_base.reset(m_library, mbox::base()));
    }

    if (const auto ptr = m_library.find(m_secondary_list.select()))
    {
        ICY_ERROR(m_secondary_base.reset(m_library, *ptr));
    }
    else
    {
        ICY_ERROR(m_secondary_base.reset(m_library, mbox::base()));
    }
    return {};
}
error_type mbox_form_value_event::create(mbox::base& base) noexcept
{
    if (!m_type.select)
        return make_stdlib_error(std::errc::invalid_argument);

    auto& value = base.value.event;
    value.etype = mbox::event_type(m_type.select);
    value.group = m_group.select();

    switch (value.etype)
    {
    case mbox::event_type::focus_acquire:
    case mbox::event_type::focus_release:
        break;

    case mbox::event_type::variable_changed:
    case mbox::event_type::recv_input:
    case mbox::event_type::button_press:
    case mbox::event_type::button_release:
    case mbox::event_type::timer_first:
    case mbox::event_type::timer_tick:
    case mbox::event_type::timer_last:
    {
        if (m_reference_base.select() == guid())
            return make_stdlib_error(std::errc::invalid_argument);
        value.reference = m_reference_base.select();
        break;
    }

    case mbox::event_type::variable_equal:
    case mbox::event_type::variable_not_equal:
    case mbox::event_type::variable_greater:
    case mbox::event_type::variable_lesser:
    case mbox::event_type::variable_greater_or_equal:
    case mbox::event_type::variable_lesser_or_equal:
    case mbox::event_type::variable_str_contains:
    {
        if (m_reference_base.select() == guid() ||
            m_secondary_base.select() == guid())
            return make_stdlib_error(std::errc::invalid_argument);
        value.reference = m_reference_base.select();
        value.compare = m_secondary_base.select();
        break;
    }
    default:
        return make_stdlib_error(std::errc::invalid_argument);
    }
    return {};
}
error_type mbox_form_value_action::create(mbox::base& base) noexcept
{
    if (!m_type.select || base.value.command.actions.empty())
        return make_stdlib_error(std::errc::invalid_argument);

    auto& value = base.value.command.actions.back();
    value.atype = mbox::action_type(m_type.select);
    value.group = m_group.select();

    if (m_reference_base.select() == guid())
        return make_stdlib_error(std::errc::invalid_argument);
    value.reference = m_reference_base.select();

    if (value.atype == mbox::action_type::variable_assign)
    {
        if (m_secondary_base.select() == guid())
            return make_stdlib_error(std::errc::invalid_argument);
        value.assign = m_secondary_base.select();
    }
    return {};
}
error_type mbox_form_value_action::initialize(const gui_widget parent, const mbox::base& base) noexcept
{
    ICY_ERROR(window.initialize(parent, gui_widget_type::dialog, gui_widget_flag::layout_vbox));
    ICY_ERROR(mbox_form_value_type::initialize(window, base));
    ICY_ERROR(buttons.initialize(window, gui_widget_type::none,
        gui_widget_flag::layout_hbox | gui_widget_flag::auto_insert));
    ICY_ERROR(save.initialize(buttons, gui_widget_type::text_button));
    ICY_ERROR(exit.initialize(buttons, gui_widget_type::text_button));
    ICY_ERROR(save.gui->text(save, "Save"_s));
    ICY_ERROR(exit.gui->text(exit, "Cancel"_s));
    ICY_ERROR(window.gui->show(window, true));
    return {};
}
error_type mbox_form_value_command::initialize(const gui_widget parent, const mbox::base& base) noexcept
{
    auto& gui = *m_widget.gui;

    ICY_ERROR(m_widget.initialize(parent, gui_widget_type::none, gui_widget_flag::layout_grid));    
    ICY_ERROR(m_group.initialize("Group"_s, m_widget));
    ICY_ERROR(m_action.initialize(m_widget, gui_widget_type::label));
    ICY_ERROR(m_create.initialize(m_widget, gui_widget_type::tool_button));
    ICY_ERROR(m_delete.initialize(m_widget, gui_widget_type::tool_button));
    ICY_ERROR(m_modify.initialize(m_widget, gui_widget_type::tool_button));
    ICY_ERROR(m_move_up.initialize(m_widget, gui_widget_type::tool_button));
    ICY_ERROR(m_move_dn.initialize(m_widget, gui_widget_type::tool_button));
    ICY_ERROR(m_table.initialize(m_widget, gui_widget_type::grid_view));
    ICY_ERROR(gui.create(m_model));

    ICY_ERROR(gui.insert(m_group.key, gui_insert(0, 0)));
    ICY_ERROR(gui.insert(m_group.value, gui_insert(1, 0, 5, 1)));
    ICY_ERROR(gui.insert(m_action, gui_insert(0, 1)));
    ICY_ERROR(gui.insert(m_create, gui_insert(1, 1)));
    ICY_ERROR(gui.insert(m_delete, gui_insert(2, 1)));
    ICY_ERROR(gui.insert(m_modify, gui_insert(3, 1)));
    ICY_ERROR(gui.insert(m_move_up, gui_insert(4, 1)));
    ICY_ERROR(gui.insert(m_move_dn, gui_insert(5, 1)));
    ICY_ERROR(gui.insert(m_table, gui_insert(0, 2, 6, 1)));
   
    ICY_ERROR(gui.text(m_action, "Actions"_s));
    ICY_ERROR(gui.insert_cols(m_model, 0, 4));
    ICY_ERROR(gui.hheader(m_model, 0, "Group"_s));
    ICY_ERROR(gui.hheader(m_model, 1, "Type"_s));
    ICY_ERROR(gui.hheader(m_model, 2, "Reference"_s));
    ICY_ERROR(gui.hheader(m_model, 3, "Assign"_s));
    ICY_ERROR(gui.bind(m_table, m_model));
    
    mbox::base group;
    group.type = mbox::type::group;
    ICY_ERROR(m_group.reset(m_library, group));

    ICY_ERROR(m_data.assign(base.value.command.actions));
    ICY_ERROR(gui.insert_rows(m_model, 0, m_data.size()));
    for (auto k = 0u; k < m_data.size(); ++k)
    {
        ICY_ERROR(update(k));
    }

    if (m_mode == mbox_form_mode::view)
    {
        ICY_ERROR(gui.enable(m_group.value, false));
        ICY_ERROR(gui.enable(m_create, false));
        ICY_ERROR(gui.enable(m_delete, false));
        ICY_ERROR(gui.enable(m_modify, false));
        ICY_ERROR(gui.enable(m_move_up, false));
        ICY_ERROR(gui.enable(m_move_dn, false));
    }
    else if (m_mode == mbox_form_mode::edit)
    {
        ICY_ERROR(m_group.scroll(base.value.command.group));
    }
    return {};
}
error_type mbox_form_value_command::create(mbox::base& base) noexcept
{
    auto& value = base.value.command;
    value.group = m_group.select();
    ICY_ERROR(value.actions.assign(m_data));
    return {};
}
error_type mbox_form_value_command::append() noexcept
{
    auto& gui = *m_model.gui;
    const auto row = m_data.size() - 1;
    ICY_ERROR(gui.insert_rows(m_model, row, 1));
    ICY_ERROR(update(row));
    return {};
}
error_type mbox_form_value_command::update(const size_t index) noexcept
{
    auto& gui = *m_model.gui;
    const auto node = gui.node(m_model, index, 0);
    const auto& action = m_data[index];

    string str_group;
    string str_reference;
    string str_assign;
    ICY_ERROR(m_library.path(action.group, str_group));
    
    if (action.reference != guid())
        ICY_ERROR(m_library.path(action.reference, str_reference));

    if (action.assign != guid())
        ICY_ERROR(m_library.path(action.assign, str_assign));

    ICY_ERROR(gui.text(gui.node(m_model, index, 0), str_group));
    ICY_ERROR(gui.text(gui.node(m_model, index, 1), mbox::to_string(action.atype)));
    ICY_ERROR(gui.text(gui.node(m_model, index, 2), str_reference));
    ICY_ERROR(gui.text(gui.node(m_model, index, 3), str_assign));
    
    return {};
}
error_type mbox_form_value_command::exec(const event event) noexcept
{
    ICY_ERROR(m_group.exec(event));
    if (event->type == event_type::gui_update)
    {
        const auto& event_data = event->data<gui_event>();
        if (event_data.widget == m_create)
        {
            m_value = make_unique<mbox_form_value_action>(mbox_form_mode::create,
                *m_create.gui, m_library, m_data.size());
            if (!m_value)
                return make_stdlib_error(std::errc::not_enough_memory);
            
            mbox::base base;
            base.type = mbox::type::command;
            ICY_ERROR(base.value.command.actions.resize(1));
            ICY_ERROR(m_value->initialize(m_widget, base));
        }
        else if (m_value)
        {
            if (event_data.widget == m_value->save)
            {
                mbox::base base;
                base.type = mbox::type::command;
                ICY_ERROR(base.value.command.actions.resize(1));
                if (const auto error = m_value->create(base))
                    show_error(error, "Create/Modify action"_s);
                else
                {
                    if (m_value->mode() == mbox_form_mode::create)
                    {
                        ICY_ERROR(m_data.push_back(base.value.command.actions.back()));
                        ICY_ERROR(append());
                    }
                    else if (m_value->mode() == mbox_form_mode::edit)
                    {
                        ICY_ERROR(update(m_value->index));
                    }
                    m_value = nullptr;
                }
            }
            else if (event_data.widget == m_value->exit)
            {
                m_value = nullptr;
            }
        }
    }
    else if (event->type == event_type::window_close)
    {
        const auto& event_data = event->data<gui_event>();
        if (m_value && event_data.widget == m_value->window)
            m_value = nullptr;
    }
    if (m_value)
        ICY_ERROR(m_value->exec(event));

    return {};
}

error_type mbox_form::initialize(const mbox::base& base) noexcept
{
    using namespace icy;
    ICY_ERROR(mbox::base::copy(base, m_base));
    auto& gui = *m_window.gui;
    ICY_ERROR(m_window.initialize(gui_widget(), gui_widget_type::dialog, gui_widget_flag::layout_grid | gui_widget_flag::auto_insert));
    ICY_ERROR(m_name.initialize("Name"_s, m_window));
    ICY_ERROR(m_parent.initialize("Parent"_s, m_window, gui_widget_type::line_edit));
    ICY_ERROR(m_type.initialize("Type"_s, m_window, gui_widget_type::line_edit));
    ICY_ERROR(m_index.initialize("Index"_s, m_window, gui_widget_type::line_edit));
    ICY_ERROR(m_buttons.initialize(m_window, gui_widget_type::none, gui_widget_flag::layout_hbox));
    ICY_ERROR(m_save.initialize(m_buttons, gui_widget_type::text_button));
    ICY_ERROR(m_exit.initialize(m_buttons, gui_widget_type::text_button));
    switch (base.type)
    {
    case mbox::type::directory:
    case mbox::type::list_inputs:
    case mbox::type::list_variables:
    case mbox::type::list_timers:
    case mbox::type::list_events:
    case mbox::type::list_bindings:
    case mbox::type::list_commands:
        break;
    case mbox::type::input:
        m_value = make_unique<mbox_form_value_input>(m_mode, gui, m_library);
        break;
    case mbox::type::event:
        m_value = make_unique<mbox_form_value_event>(m_mode, gui, m_library);
        break;
    case mbox::type::command:
        m_value = make_unique<mbox_form_value_command>(m_mode, gui, m_library);
        break;
    default:
        return make_stdlib_error(std::errc::invalid_argument);
    }

    if (m_value)
    {
        ICY_ERROR(m_tabs.initialize(m_window, gui_widget_type::tabs));
        ICY_ERROR(m_value->initialize(m_tabs, base));
        ICY_ERROR(gui.text(m_tabs, m_value->widget(), "Value"_s));
    }


    ICY_ERROR(gui.text(m_save, "Save"_s));
    ICY_ERROR(gui.text(m_exit, "Cancel"_s));

    auto row = 0u;
    ICY_ERROR(m_name.insert(row));
    ICY_ERROR(m_parent.insert(row));
    ICY_ERROR(m_type.insert(row));
    ICY_ERROR(m_index.insert(row));
    if (m_value)
        ICY_ERROR(gui.insert(m_tabs, gui_insert(0, row++, 2, 1)));
    ICY_ERROR(gui.insert(m_buttons, gui_insert(0, row++, 2, 1)));

    string parent_str;
    string type_str;
    string index_str;
    ICY_ERROR(to_string(base.parent, parent_str));
    ICY_ERROR(to_string(mbox::to_string(base.type), type_str));
    ICY_ERROR(to_string(base.index, index_str));
    ICY_ERROR(gui.text(m_name.value, base.name));
    ICY_ERROR(gui.text(m_parent.value, parent_str));
    ICY_ERROR(gui.text(m_type.value, type_str));
    ICY_ERROR(gui.text(m_index.value, index_str));
    ICY_ERROR(gui.enable(m_parent.value, false));
    ICY_ERROR(gui.enable(m_type.value, false));
    ICY_ERROR(gui.enable(m_index.value, false));
    
    if (m_mode == mbox_form_mode::view)
    {
        ICY_ERROR(gui.enable(m_name.value, false));
        ICY_ERROR(gui.enable(m_save, false));
    }

    ICY_ERROR(m_window.gui->show(m_window, true));

    return{};
}
error_type mbox_form::exec(const event event) noexcept
{
    using namespace icy;
    if (event->type == event_type::gui_update)
    {
        const auto& event_data = event->data<gui_event>();
        if (event_data.widget == m_save && m_mode != mbox_form_mode::view)
        {
            ICY_ERROR(to_string(m_name.string, m_base.name));
            if (m_value)
                ICY_ERROR(m_value->create(m_base));

            if (const auto error = m_library.insert(m_base))
            {
                show_error(error, "Create new object"_s);
                return {};
            }
            ICY_ERROR(event::post(nullptr, mbox_event_type_create, mbox_event_data_create(m_base.index)));
            ICY_ERROR(m_window.gui->show(m_window, false));
            return {};
        }
        else if (event_data.widget == m_exit)
        {
            ICY_ERROR(m_window.gui->show(m_window, false));
            return {};
        }
    }
    ICY_ERROR(m_name.exec(event));

    if (m_value)
        ICY_ERROR(m_value->exec(event));

    return {};
}
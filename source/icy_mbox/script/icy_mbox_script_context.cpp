#include "icy_mbox_script_context.hpp"
#include "icy_mbox_script_form2.hpp"

using namespace icy;

icy::error_type mbox_context::menu(const icy::guid& index) noexcept
{
    using namespace icy;

    const auto base = m_library->find(index);
    if (!base)
        return make_stdlib_error(std::errc::invalid_argument);

    gui_widget_scoped menu(*m_gui);
    gui_widget_scoped menu_create(*m_gui);
    gui_action_scoped action_view(*m_gui);
    gui_action_scoped action_view_new(*m_gui);
    gui_action_scoped action_edit(*m_gui);
    gui_action_scoped action_edit_new(*m_gui);
    gui_action_scoped action_rename(*m_gui);
    gui_action_scoped action_delete(*m_gui);
    gui_action_scoped action_create(*m_gui);

    array<mbox::type> types;

    ICY_ERROR(menu.initialize());
    ICY_ERROR(menu_create.initialize());
    ICY_ERROR(action_view.initialize("View"_s));
    ICY_ERROR(action_edit.initialize("Edit"_s));
    ICY_ERROR(action_view_new.initialize("View [new tab]"_s));
    ICY_ERROR(action_edit_new.initialize("Edit [new tab]"_s));
    ICY_ERROR(action_rename.initialize("Rename"_s));
    ICY_ERROR(action_delete.initialize("Delete"_s));

    switch (base->type)
    {
    case mbox::type::directory:
        if (base->index == mbox::root)
            ICY_ERROR(m_gui->enable(action_delete, false));

        ICY_ERROR(types.push_back(mbox::type::directory));
        ICY_ERROR(types.push_back(mbox::type::list_bindings));
        ICY_ERROR(types.push_back(mbox::type::list_commands));
        ICY_ERROR(types.push_back(mbox::type::list_events));
        ICY_ERROR(types.push_back(mbox::type::list_inputs));
        ICY_ERROR(types.push_back(mbox::type::list_timers));
        ICY_ERROR(types.push_back(mbox::type::list_variables));
        ICY_ERROR(types.push_back(mbox::type::group));
        ICY_ERROR(types.push_back(mbox::type::profile));
        break;

    case mbox::type::input:
    case mbox::type::variable:
    case mbox::type::timer:
    case mbox::type::event:
    case mbox::type::command:
    case mbox::type::binding:
    case mbox::type::group:
    case mbox::type::profile:
        break;

    case mbox::type::list_inputs:
        ICY_ERROR(types.push_back(mbox::type::input));
        break;

    case mbox::type::list_variables:
        ICY_ERROR(types.push_back(mbox::type::variable));
        break;

    case mbox::type::list_timers:
        ICY_ERROR(types.push_back(mbox::type::timer));
        break;

    case mbox::type::list_events:
        ICY_ERROR(types.push_back(mbox::type::event));
        break;

    case mbox::type::list_bindings:
        ICY_ERROR(types.push_back(mbox::type::binding));
        break;

    case mbox::type::list_commands:
        ICY_ERROR(types.push_back(mbox::type::command));
        break;

    default:
        return make_stdlib_error(std::errc::invalid_argument);
    }

    map<mbox::type, gui_action_scoped> actions_create_type;
    for (auto&& type : types)
        ICY_ERROR(actions_create_type.insert(std::move(type), gui_action_scoped(*m_gui)));

    for (auto&& pair : actions_create_type)
    {
        ICY_ERROR(pair.value.initialize(to_string(pair.key)));
        ICY_ERROR(m_gui->insert(menu_create, pair.value));
    }

    ICY_ERROR(m_gui->insert(menu, action_view));
    ICY_ERROR(m_gui->insert(menu, action_edit));
    ICY_ERROR(m_gui->insert(menu, action_view_new));
    ICY_ERROR(m_gui->insert(menu, action_edit_new));
    ICY_ERROR(m_gui->insert(menu, action_rename));
    ICY_ERROR(m_gui->insert(menu, action_delete));
    if (!types.empty())
    {
        ICY_ERROR(action_create.initialize("Create"_s));
        ICY_ERROR(m_gui->bind(action_create, menu_create));
        ICY_ERROR(m_gui->insert(menu, action_create));
    }


    gui_action action;
    ICY_ERROR(m_gui->exec(menu, action));

    if (action == action_view)
    {
        m_form = make_unique<mbox_form>(mbox_form_mode::view, *m_gui, *m_library);
        if (!m_form)
            return make_stdlib_error(std::errc::not_enough_memory);
        ICY_ERROR(m_form->initialize(*base));
    }
    else if (action == action_edit)
    {
        m_form = make_unique<mbox_form>(mbox_form_mode::edit, *m_gui, *m_library);
        if (!m_form)
            return make_stdlib_error(std::errc::not_enough_memory);
        ICY_ERROR(m_form->initialize(*base));
    }
    else if (action == action_rename)
    {
        //ICY_ERROR(event::post(nullptr, mbox_event_type_rename, mbox_event(base->index)));
    }
    else if (action == action_delete)
    {
        //ICY_ERROR(event::post(nullptr, mbox_event_type_delete, mbox_event(base->index)));
    }
    else if (action.index)
    {
        for (auto&& pair : actions_create_type)
        {
            if (pair.value != action)
                continue;

            m_form = make_unique<mbox_form>(mbox_form_mode::create, *m_gui, *m_library);
            if (!m_form)
                return make_stdlib_error(std::errc::not_enough_memory);

            mbox::base new_base;
            ICY_ERROR(create_guid(new_base.index));
            new_base.type = pair.key;
            new_base.parent = base->index;
            ICY_ERROR(m_form->initialize(new_base));
        }
    }
    return {};
}
icy::error_type mbox_context::exec(const icy::event event) noexcept
{
    using namespace icy;

    if (event->type == event_type::gui_context)
    {
        const auto& event_data = event->data<icy::gui_event>();
        const auto node = event_data.data.as_node();
        if (node && node.udata().type() == gui_variant_type::guid)
            ICY_ERROR(menu(node.udata().as_guid()));
    }
    else if (event->type == event_type::window_close)
    {
        const auto& event_data = event->data<icy::gui_event>();
        if (m_form && event_data.widget == m_form->window())
            m_form = nullptr;
    }
    if (m_form)
        ICY_ERROR(m_form->exec(event));
    return {};
}
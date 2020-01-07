#include "icy_mbox_script_explorer.hpp"
#include "icy_mbox_script_common.hpp"
#include "../icy_mbox_script.hpp"
#include <icy_qtgui/icy_qtgui.hpp>

using namespace icy;

icy::error_type mbox_explorer::initialize(icy::gui_queue& gui, mbox::library& library, icy::gui_widget parent) noexcept
{
    using namespace icy;
    m_gui = &gui;
    m_library = &library;
    ICY_ERROR(m_gui->create(m_widget, gui_widget_type::tree_view, parent));
    ICY_ERROR(m_gui->create(m_root));
    ICY_ERROR(m_gui->bind(m_widget, m_root));
    return {};
}
icy::error_type mbox_explorer::reset() noexcept
{
    using namespace icy;

    const auto root = m_library->find(mbox::root);
    if (!root)
        return make_stdlib_error(std::errc::invalid_argument);

    ICY_ERROR(m_gui->clear(m_root));
    ICY_ERROR(m_gui->insert_cols(m_root, 0, 1));
    ICY_ERROR(m_gui->insert_rows(m_root, 0, 1));
    ICY_ERROR(append(m_root, 0, *root));
    return {};
}
icy::error_type mbox_explorer::exec(const icy::event event) noexcept
{
    using namespace icy;
    if (event->type == mbox_event_type_create)
    {
        const auto& event_data = event->data<mbox_event_data_create>();
        const auto base = m_library->find(event_data);
        if (!base)
            return make_stdlib_error(std::errc::invalid_argument);

        gui_node node;
        ICY_ERROR(find(*base, node));
        if (m_library->find(base->parent)->value.directory.indices.size() == 1)
        {
            ICY_ERROR(m_gui->insert_cols(node.parent(), 0, 1));
        }
        ICY_ERROR(m_gui->insert_rows(node.parent(), node.row(), 1));
        ICY_ERROR(append(node.parent(), node.row(), *base));
    }
    else if (event->type == mbox_event_type_modify)
    {
        const auto& event_data = event->data<mbox_event_data_modify>();
        const auto base = m_library->find(event_data.index);
        if (!base)
            return make_stdlib_error(std::errc::invalid_argument);

        if (base->name != event_data.name)
        {
            if (event_data.index == mbox::root)
            {
                ICY_ERROR(m_gui->text(m_gui->node(m_root, 0, 0), base->name));
            }
            else
            {
                mbox::base copy;
                ICY_ERROR(mbox::base::copy(*base, copy));
                ICY_ERROR(to_string(event_data.name, copy.name));

                gui_node old_node;
                gui_node new_node;
                auto new_row = 0_z;
                auto old_row = 0_z;
                ICY_ERROR(find(*base, new_node, &new_row));
                ICY_ERROR(find(copy, old_node, &old_row));
                if (new_node.row() != old_node.row())
                {
                    ICY_ERROR(reset());
                    ICY_ERROR(m_gui->scroll(m_widget, new_node));
                    //   doesnt work properly?
                    //const auto parent = new_node.parent();
                    //ICY_ERROR(m_gui->remove_rows(parent, old_node.row(), 1));
                    //ICY_ERROR(m_gui->insert_rows(parent, new_node.row(), 1));
                    //ICY_ERROR(append(parent, new_node.row(), *base));
                }
                else
                {
                    ICY_ERROR(m_gui->text(new_node, base->name));
                }
                return {};
            }
        }
    }

    return {};
}
icy::error_type mbox_explorer::find(const mbox::base& base, icy::gui_node& node, size_t* const offset) noexcept
{
    using namespace icy;

    if (base.index == mbox::root)
    {
        node = m_gui->node(m_root, 0, 0);
        if (!node)
            return make_stdlib_error(std::errc::invalid_argument);
        return {};
    }
    const auto parent_base = m_library->find(base.parent);
    if (!parent_base)
        return make_stdlib_error(std::errc::invalid_argument);

    gui_node parent_node;
    ICY_ERROR(find(*parent_base, parent_node));

    array<string_view> names;
    for (auto&& index : parent_base->value.directory.indices)
    {
        if (const auto child = m_library->find(index))
            ICY_ERROR(names.push_back(child->name));
    }
    std::sort(names.begin(), names.end());
    names.pop_back(std::distance(std::unique(names.begin(), names.end()), names.end()));

    const auto it = std::lower_bound(names.begin(), names.end(), base.name);
    //if (it == names.end())
    //    return make_stdlib_error(std::errc::invalid_argument);

    const auto row = size_t(std::distance(names.begin(), it));
    if (offset)
        *offset = row;

    node = m_gui->node(parent_node, row, 0);
    if (!node)
        return make_stdlib_error(std::errc::not_enough_memory);

    return {};
}
icy::error_type mbox_explorer::append(const icy::gui_node parent, const size_t offset, const mbox::base& base) noexcept
{
    using namespace icy;
    auto node = m_gui->node(parent, offset, 0);
    ICY_ERROR(m_gui->text(node, base.name));
    ICY_ERROR(m_gui->udata(node, icy::guid(base.index)));

    gui_image image;
    switch (base.type)
    {
    case mbox::type::directory:
        image = find_image(mbox_image::type_directory);
        break;
    case mbox::type::group:
        image = find_image(mbox_image::type_group);
        break;
    case mbox::type::profile:
        image = find_image(mbox_image::type_profile);
        break;
    case mbox::type::list_bindings:
        image = find_image(mbox_image::type_binding);
        break;
    case mbox::type::list_commands:
        image = find_image(mbox_image::type_command);
        break;
    case mbox::type::list_events:
        image = find_image(mbox_image::type_event);
        break;
    case mbox::type::list_inputs:
        image = find_image(mbox_image::type_input);
        break;
    case mbox::type::list_timers:
        image = find_image(mbox_image::type_timer);
        break;
    case mbox::type::list_variables:
        image = find_image(mbox_image::type_variable);
        break;
    default:
        break;
    }

    if (image.index)
        ICY_ERROR(m_gui->icon(node, image));

    switch (base.type)
    {
    case mbox::type::directory:
    case mbox::type::list_inputs:
    case mbox::type::list_variables:
    case mbox::type::list_timers:
    case mbox::type::list_events:
    case mbox::type::list_bindings:
    case mbox::type::list_commands:
    {
        map<string_view, const mbox::base*> children;
        for (auto&& index : base.value.directory.indices)
        {
            auto child = m_library->find(index);
            if (child && !child->name.empty())
                ICY_ERROR(children.insert(string_view(child->name), std::move(child)));
        }
        if (!children.empty())
        {
            size_t row = 0;
            ICY_ERROR(m_gui->insert_cols(node, 0, 1));
            ICY_ERROR(m_gui->insert_rows(node, 0, children.size()));
            for (auto&& pair : children)
            {
                ICY_ERROR(append(node, row++, *pair.value));
            }
            break;
        }
        break;
    }
    }
    return {};
}
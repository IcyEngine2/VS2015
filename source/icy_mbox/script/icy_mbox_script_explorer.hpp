#pragma once

#include <icy_qtgui/icy_qtgui.hpp>
#include "../icy_mbox_script.hpp"
#include "icy_mbox_script_event.hpp"
#include <icy_engine/image/icy_image.hpp>
#include "icons/command.h"
#include "icons/binding.h"
#include "icons/directory.h"
#include "icons/event.h"
#include "icons/group.h"
#include "icons/input.h"
#include "icons/profile.h"
#include "icons/timer.h"
#include "icons/variable.h"

class mbox_explorer
{
public:
    icy::error_type initialize(icy::gui_queue& gui, mbox::library& library, icy::gui_widget parent)
    {
        using namespace icy;
        m_gui = &gui;
        m_library = &library;
        ICY_ERROR(m_gui->create(m_widget, gui_widget_type::tree_view, parent));
        ICY_ERROR(m_gui->create(m_root));
        ICY_ERROR(m_gui->bind(m_widget, m_root));

        const auto func = [this](const const_array_view<uint8_t> bytes, const mbox::type type)
        {
            icy::image image;
            ICY_ERROR(image.load(detail::global_heap, bytes, image_type::png));
            auto colors = matrix<color>(image.size().y, image.size().x);
            if (colors.empty())
                return make_stdlib_error(std::errc::not_enough_memory);
            ICY_ERROR(image.view({}, colors));
            ICY_ERROR(m_gui->create(m_images[uint32_t(type)], colors));
            return error_type();
        };
        ICY_ERROR(func(g_bytes_command, mbox::type::list_commands));
        ICY_ERROR(func(g_bytes_binding, mbox::type::list_bindings));
        ICY_ERROR(func(g_bytes_directory, mbox::type::directory));
        ICY_ERROR(func(g_bytes_event, mbox::type::list_events));
        ICY_ERROR(func(g_bytes_group, mbox::type::group));
        ICY_ERROR(func(g_bytes_input, mbox::type::list_inputs));
        ICY_ERROR(func(g_bytes_profile, mbox::type::profile));
        ICY_ERROR(func(g_bytes_timer, mbox::type::list_timers));
        ICY_ERROR(func(g_bytes_variable, mbox::type::list_variables));
        return {};
    }
    icy::gui_widget widget() const noexcept
    {
        return m_widget;
    }
    icy::error_type reset() noexcept
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
    icy::error_type exec(const icy::event event) noexcept
    {
        using namespace icy;
        if (event->type == mbox_event_type_create)
        {
            const auto& event_data = event->data<mbox::base>();
            const auto base = m_library->find(event_data.index);
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
        return {};
    }
private:
private:
    icy::error_type find(const mbox::base& base, icy::gui_node& node) noexcept
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

        const auto it = binary_search(names.begin(), names.end(), base.name);
        if (it == names.end())
            return make_stdlib_error(std::errc::invalid_argument);
        
        const auto offset = std::distance(names.begin(), it);
        node = m_gui->node(parent_node, offset, 0);
        if (!node)
            return make_stdlib_error(std::errc::not_enough_memory);

        return {};
    }
   
    icy::error_type append(const icy::gui_node parent, const size_t offset, const mbox::base& base)
    {
        using namespace icy;
        auto node = m_gui->node(parent, offset, 0);
        ICY_ERROR(m_gui->text(node, base.name));
        ICY_ERROR(m_gui->udata(node, icy::guid(base.index)));
        if (m_images[uint32_t(base.type)].index)
            ICY_ERROR(m_gui->icon(node, m_images[uint32_t(base.type)]));

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
        }
        }
        return {};
    }
private:
    icy::gui_queue* m_gui = nullptr;
    mbox::library* m_library = nullptr;
    icy::gui_image m_images[uint32_t(mbox::type::_total)];
    icy::gui_widget m_widget;
    icy::gui_node m_root;
};
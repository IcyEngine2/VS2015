#include "icy_gui_model.hpp"
#include "icy_gui_system.hpp"

using namespace icy;

error_type gui_node_data::insert(map<uint32_t, unique_ptr<gui_node_data>>& data, const uint32_t parent, const uint32_t index, const child_type& args) noexcept
{
    auto it = data.find(parent);
    auto& children = it->value->children;
    
    unique_ptr<gui_node_data> new_node;
    ICY_ERROR(make_unique(gui_node_data(it->value.get(), index), new_node));
    ICY_ERROR(children.reserve(children.size() + 1));
    ICY_ERROR(data.reserve(data.size() + 1));
    ICY_ERROR(children.insert(new_node.get(), args));
    ICY_ERROR(data.insert(index, std::move(new_node)));
    return error_type();
}
void gui_node_data::erase(map<uint32_t, unique_ptr<gui_node_data>>& data, const uint32_t index) noexcept
{
    auto it = data.find(index);
    if (it != data.end())
    {
        auto ptr = std::move(it->value);
        data.erase(it);
        auto children = std::move(ptr->children);
        for (auto&& pair : children)
            erase(data, pair.key->index);
        
        auto& parent_children = ptr->parent->children;
        auto jt = parent_children.find(ptr.get());
        if (jt != parent_children.end())
            parent_children.erase(jt);
    }
}
error_type gui_node_data::modify(const gui_node_prop prop, const gui_variant& value) noexcept
{
    auto state_flag = gui_node_state::none;
    switch (prop)
    {
    case gui_node_prop::data:
        data = value;
        break;

    case gui_node_prop::enabled:
        state_flag = gui_node_state::enabled;
        break;
    
    case gui_node_prop::visible:
        state_flag = gui_node_state::visible;
        break;

    case gui_node_prop::checkable:
        state_flag = gui_node_state::checkable;
        break;

    case gui_node_prop::checked:
    {
        state_flag = gui_node_state::checked;
        break;
    }
    case gui_node_prop::user:
    {
        user = value;
        break;
    }

    default:
        break;
    }

    if (state_flag)
    {
        auto flag = false;
        if (to_value(value, flag))
        {
            if (state_flag == gui_node_state::checked)
                state |= gui_node_state::checkable;
            state &= ~state_flag;
            if (flag) state |= state_flag;
        }
        else if (state_flag == gui_node_state::checked && value.type() == 0)
        {
            state &= ~gui_node_state::checkable;
        }
    }

    return error_type();
}

gui_model_data_usr::~gui_model_data_usr() noexcept
{
    while (auto event = static_cast<gui_model_event_type*>(m_events_usr.pop()))
        gui_model_event_type::clear(event);
    while (auto event = static_cast<gui_model_event_type*>(m_events_sys.pop()))
        gui_model_event_type::clear(event);
    if (auto system = shared_ptr<gui_system>(m_system))
        static_cast<gui_system_data*>(system.get())->wake();
}
error_type gui_model_data_usr::update() noexcept
{
    while (auto event = static_cast<gui_model_event_type*>(m_events_usr.pop()))
    {
        ICY_SCOPE_EXIT{ gui_model_event_type::clear(event); };
    }
    return error_type();
}
gui_node gui_model_data_usr::parent(const gui_node node) const noexcept
{
    auto value = 0u;
    auto it = m_data.find(node.index);
    if (it != m_data.end())
    {
        gui_model_query_type new_query;
        new_query.type = gui_model_query_type::index;
        new_query.node = it->value->parent;
        process(new_query).get(value);
    }
    return gui_node{ value };
}
uint32_t gui_model_data_usr::row(const gui_node node) const noexcept
{
    auto value = 0u;
    auto it = m_data.find(node.index);
    if (it != m_data.end())
    {
        gui_model_query_type new_query;
        new_query.type = gui_model_query_type::row;
        new_query.node = it->value.get();
        process(new_query).get(value);
    }
    return value;
}
uint32_t gui_model_data_usr::col(const gui_node node) const noexcept
{
    auto value = 0u;
    auto it = m_data.find(node.index);
    if (it != m_data.end())
    {
        gui_model_query_type new_query;
        new_query.type = gui_model_query_type::col;
        new_query.node = it->value.get();
        process(new_query).get(value);
    }
    return value;
}
gui_variant gui_model_data_usr::query(const gui_node node, const gui_node_prop prop) const noexcept
{
    gui_model_query_type new_query;
    auto it = m_data.find(node.index);
    if (it != m_data.end())
    {
        new_query.type = gui_model_query_type::qprop;
        new_query.node = it->value.get();
        new_query.prop = prop;
        return process(new_query);
    }
    return gui_variant();
}
error_type gui_model_data_usr::modify(const gui_node node, const gui_node_prop prop, const gui_variant& value) noexcept
{
    if (prop == gui_node_prop::none)
        return error_type();

    ICY_ERROR(update());
    auto it = m_data.find(node.index);
    if (it == m_data.end())
        return error_type();

    gui_model_event_type* new_event = nullptr;
    ICY_ERROR(gui_model_event_type::make(new_event, gui_model_event_type::modify));
    ICY_SCOPE_EXIT{ gui_model_event_type::clear(new_event); };

    ICY_ERROR(it->value->modify(prop, value));

    new_event->index = node.index;
    new_event->prop = prop;
    new_event->val = value;

    notify(new_event);
    return error_type();
}
error_type gui_model_data_usr::insert(const gui_node parent, const uint32_t row, const uint32_t col, gui_node& node) noexcept
{
    ICY_ERROR(update());

    auto it = m_data.find(parent.index);
    if (it == m_data.end())
        return error_type();

    auto& children = it->value->children;
    for (auto&& child : children)
    {
        if (child.value.row == row && child.value.col == col)
        {
            node.index = child.key->index;
            return error_type();
        }
    }

    gui_node_data::child_type args;
    args.col = col;
    args.row = row;
    //args.index = m_index;
    
    gui_model_event_type* new_event = nullptr;
    ICY_ERROR(gui_model_event_type::make(new_event, gui_model_event_type::create));
    ICY_SCOPE_EXIT{ gui_model_event_type::clear(new_event); };

    ICY_ERROR(gui_node_data::insert(m_data, parent.index, m_index, args));

    new_event->index = parent.index;
    new_event->val = std::make_pair(m_index, args);
    notify(new_event);
    node.index = m_index++;
    return error_type();
}
error_type gui_model_data_usr::destroy(const gui_node node) noexcept
{
    ICY_ERROR(update());

    gui_model_event_type* new_event = nullptr;
    ICY_ERROR(gui_model_event_type::make(new_event, gui_model_event_type::destroy));
    new_event->index = node.index;
    gui_node_data::erase(m_data, node.index);
    notify(new_event);
    return error_type();
}
void gui_model_data_usr::notify(gui_model_event_type*& event) noexcept
{
    if (auto system = shared_ptr<gui_system>(m_system))
    {
        m_events_sys.push(event);
        static_cast<gui_system_data*>(system.get())->wake();
    }
    event = nullptr;
}
gui_variant gui_model_data_usr::process(const gui_model_query_type& query) const noexcept
{
    switch (query.type)
    {
    case gui_model_query_type::index:
    {
        if (query.node)
            return query.node->index;
        break;
    }
    case gui_model_query_type::row:
    case gui_model_query_type::col:
    {
        if (query.node && query.node->parent)
        {
            auto& children = query.node->parent->children;
            auto it = children.find(query.node);
            if (it != children.end())
            {
                switch (query.type)
                {
                case gui_model_query_type::index:
                    return it->key->index;
                case gui_model_query_type::row:
                    return it->value.row;
                case gui_model_query_type::col:
                    return it->value.col;
                }
            }
        }
        break;
    }
    case gui_model_query_type::qprop:
    {
        if (query.node)
        {
            switch (query.prop)
            {
            case gui_node_prop::visible:
                return (query.node->state & gui_node_state::visible) != 0;
            case gui_node_prop::enabled:
                return (query.node->state & gui_node_state::enabled) != 0;
            case gui_node_prop::checked:
                return (query.node->state & gui_node_state::checked) != 0;
            case gui_node_prop::checkable:
                return (query.node->state & gui_node_state::checkable) != 0;
            case gui_node_prop::data:
                return query.node->data;
            case gui_node_prop::user:
                return query.node->user;
            }
        }
        break;
    }
    }
    return gui_variant();
}

error_type gui_model_data_sys::process(const gui_model_event_type& event, set<uint32_t>& output) noexcept
{
    auto bind_node = 0u;
    switch (event.type)
    {
    case gui_model_event_type::create:
    {
        std::pair<uint32_t, gui_node_data::child_type> args;
        if (event.val.get(args))
        {
            ICY_ERROR(gui_node_data::insert(m_data, event.index, args.first, args.second));
            bind_node = event.index;
        }
        break;
    }
    case gui_model_event_type::destroy:
    {
        auto it = m_data.find(event.index);
        if (it == m_data.end() || event.index == 0)
            return error_type();

        auto parent = it->value->parent;
        gui_node_data::erase(m_data, event.index);
        if (parent)
            bind_node = parent->index;
        break;
    }
    case gui_model_event_type::modify:
    {
        auto it = m_data.find(event.index);
        if (it == m_data.end())
            return error_type();

        ICY_ERROR(it->value->modify(event.prop, event.val));
        if (event.prop == gui_node_prop::user || event.prop == gui_node_prop::none)
            return error_type();

        bind_node = event.index;
        break;
    }
    }

    while (true)
    {
        ICY_ERROR(output.try_insert(bind_node));
        if (bind_node == 0)
            break;

        auto it = m_data.find(bind_node);        
        if (const auto parent = it->value->parent)
        {
            bind_node = parent->index;
        }
        else
        {
            break;
        }
    }
    
    return error_type();
}
error_type gui_model_data_sys::send_data(gui_window_data_sys& window, const gui_widget widget, const gui_node node, const gui_data_bind& func, bool& erase) const noexcept
{
    auto it = m_data.find(node.index);
    if (it == m_data.end())
    {
        erase = true;
        return error_type();
    }
    gui_model_proxy proxy(*this);
    ICY_ERROR(window.recv_data(proxy, *it->value, widget, func, erase));
    return error_type();
}
error_type gui_model_data_sys::recv_data(const gui_widget_data& widget, const gui_node index, const gui_data_bind& func, bool& erase) noexcept
{
    auto it = m_data.find(index.index);
    if (it == m_data.end())
    {
        erase = true;
        return error_type();
    }
    auto& node = *it->value;
    switch (widget.type)
    {
    case gui_widget_type::edit_line:
    case gui_widget_type::edit_text:
    {
        node.data = gui_variant();
        for (auto&& item : widget.items)
        {
            if (item.type == gui_widget_item_type::text)
            {
                node.data = item.value;
                break;
            }
        }
        break;
    }
    case gui_widget_type::view_list:
    {
        break;
    }

    default:
        break;
    }


    return error_type();
}
gui_node gui_model_data_sys::parent(const gui_node node) const noexcept
{
    const auto it = m_data.find(node.index);
    if (it != m_data.end())
    {
        if (const auto parent = it->value->parent)
            return { parent->index };
    }
    return gui_node();
}
uint32_t gui_model_data_sys::row(const gui_node node) const noexcept
{
    const auto it = m_data.find(node.index);
    if (it != m_data.end())
    {
        if (const auto parent = it->value->parent)
        {
            const auto jt = parent->children.find(it->value.get());
            if (jt != parent->children.end())
                return jt->value.row;
        }
    }
    return 0u;
}
uint32_t gui_model_data_sys::col(const gui_node node) const noexcept
{
    const auto it = m_data.find(node.index);
    if (it != m_data.end())
    {
        if (const auto parent = it->value->parent)
        {
            const auto jt = parent->children.find(it->value.get());
            if (jt != parent->children.end())
                return jt->value.col;
        }
    }
    return 0u;
}
gui_variant gui_model_data_sys::query(const gui_node node, const gui_node_prop prop) const noexcept
{
    auto it = m_data.find(node.index);
    if (it != m_data.end())
    {
        switch (prop)
        {
        case gui_node_prop::enabled:
            return (it->value->state & gui_node_state::enabled) != 0;
        case gui_node_prop::visible:
            return (it->value->state & gui_node_state::visible) != 0;
        case gui_node_prop::checked:
            return (it->value->state & gui_node_state::checked) != 0;
        case gui_node_prop::checkable:
            return (it->value->state & gui_node_state::checkable) != 0;
        case gui_node_prop::data:
            return it->value->data;
        case gui_node_prop::user:
            return it->value->user;
        }
    }
    return gui_variant();
}
#include "icy_gui_model.hpp"
#include "icy_gui_system.hpp"

using namespace icy;

error_type gui_node_data::insert(map<uint32_t, unique_ptr<gui_node_data>>& data, const uint32_t parent, const child_type& args) noexcept
{
    auto it = data.find(parent);
    auto& children = it->value->children;
    
    unique_ptr<gui_node_data> new_node;
    ICY_ERROR(make_unique(gui_node_data(it->value.get()), new_node));
    ICY_ERROR(children.reserve(children.size() + 1));
    ICY_ERROR(data.reserve(data.size() + 1));
    ICY_ERROR(children.insert(new_node.get(), args));
    ICY_ERROR(data.insert(args.index, std::move(new_node)));
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
            erase(data, pair.value.index);
        
        auto& parent_children = ptr->parent->children;
        auto jt = parent_children.find(ptr.get());
        if (jt != parent_children.end())
            parent_children.erase(jt);
    }
}
error_type gui_node_data::modify(const gui_node_prop prop, const gui_variant& value) noexcept
{
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
            node.index = child.value.index;
            return error_type();
        }
    }

    gui_node_data::child_type args;
    args.col = col;
    args.row = row;
    args.index = m_index;
    
    gui_model_event_type* new_event = nullptr;
    ICY_ERROR(gui_model_event_type::make(new_event, gui_model_event_type::create));
    ICY_SCOPE_EXIT{ gui_model_event_type::clear(new_event); };

    ICY_ERROR(gui_node_data::insert(m_data, parent.index, args));

    new_event->index = parent.index;
    new_event->val = std::move(args);
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
    case gui_model_query_type::row:
    case gui_model_query_type::col:
    case gui_model_query_type::index:
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
                    return it->value.index;
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
        /*switch (query.prop)
        {
        default:
            break;
        }*/
    }
    }
    return gui_variant();
}

error_type gui_model_data_sys::process(const gui_model_event_type& event) noexcept 
{
    return error_type();
}
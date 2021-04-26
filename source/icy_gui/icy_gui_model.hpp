#pragma once

#include <icy_engine/core/icy_map.hpp>
#include <icy_engine/core/icy_queue.hpp>
#include <icy_engine/core/icy_set.hpp>
#include <icy_engine/core/icy_thread.hpp>
#include <icy_gui/icy_gui.hpp>

struct gui_node_state_enum
{
    enum : uint32_t
    {
        none        =   0x00,
        enabled     =   0x01,
        visible     =   0x02,
        checked     =   0x04,
        checkable   =   0x08,
        _default    =   enabled | visible,
    };
};
using gui_node_state = decltype(gui_node_state_enum::none);

struct gui_node_data
{
public:
    struct child_type
    {
        uint32_t row;
        uint32_t col;
    };
public:
    static icy::error_type insert(icy::map<uint32_t, icy::unique_ptr<gui_node_data>>& map, const uint32_t parent, const uint32_t index, const child_type& args) noexcept;
    static void erase(icy::map<uint32_t, icy::unique_ptr<gui_node_data>>& map, const uint32_t index) noexcept;
    gui_node_data(const gui_node_data* const parent, const uint32_t index) noexcept : parent(parent), index(index)
    {

    }
    icy::error_type modify(const icy::gui_node_prop prop, const icy::gui_variant& value) noexcept;
    const gui_node_data* parent;
    uint32_t index;
    mutable icy::map<gui_node_data*, child_type> children;
    uint32_t state = gui_node_state::_default;
    icy::gui_variant data;
    icy::gui_variant user;
    icy::gui_variant row_header;
    icy::gui_variant col_header;
};
struct gui_model_event_type
{
    void* _unused = nullptr;
    enum{ none, create, modify, destroy } type = none;
    uint32_t index = 0;
    icy::gui_node_prop prop = icy::gui_node_prop::none;
    icy::gui_variant val;
    static icy::error_type make(gui_model_event_type*& new_event, const decltype(none) type) noexcept
    {
        new_event = icy::allocator_type::allocate<gui_model_event_type>(1);
        if (!new_event)
            return icy::make_stdlib_error(std::errc::not_enough_memory);
        icy::allocator_type::construct(new_event);
        new_event->type = type;
        return icy::error_type();
    }
    static void clear(gui_model_event_type*& new_event) noexcept
    {
        if (new_event)
        {
            icy::allocator_type::destroy(new_event);
            icy::allocator_type::deallocate(new_event);
        }
        new_event = nullptr;
    }
};
struct gui_model_query_type
{
    enum { none, index, row, col, qprop } type = none;
    const gui_node_data* node = nullptr;
    icy::gui_node_prop prop = icy::gui_node_prop::none;
};
class gui_model_data_usr : public icy::gui_data_write_model
{
public:
    gui_model_data_usr(const icy::weak_ptr<icy::gui_system> system) noexcept : m_system(system)
    {

    }
    ~gui_model_data_usr() noexcept;
    icy::error_type initialize() noexcept
    {
        //icy::unique_ptr<gui_node_data> root;
        //ICY_ERROR(icy::make_unique(gui_node_data(nullptr, 0), root));
        //ICY_ERROR(m_data.insert(0u, std::move(root)));
        return icy::error_type();
    }
    gui_model_event_type* next() noexcept
    {
        return static_cast<gui_model_event_type*>(m_events_sys.pop());
    }
private:
    //icy::error_type update() noexcept override;
    //icy::gui_node parent(const icy::gui_node node) const noexcept override;
    //uint32_t row(const icy::gui_node node) const noexcept override;
    //uint32_t col(const icy::gui_node node) const noexcept override;
    //icy::gui_variant query(const icy::gui_node node, const icy::gui_node_prop prop) const noexcept override;
    icy::error_type modify(const icy::gui_node node, const icy::gui_node_prop prop, const icy::gui_variant& value) noexcept override;
    icy::error_type insert(const icy::gui_node parent, const uint32_t row, const uint32_t col, icy::gui_node& node) noexcept override;
    icy::error_type destroy(const icy::gui_node node) noexcept override;
    void notify(gui_model_event_type*& event) noexcept;
    icy::gui_variant process(const gui_model_query_type& query) const noexcept;
private:
    const icy::weak_ptr<icy::gui_system> m_system;
    //icy::detail::intrusive_mpsc_queue m_events_usr;
    icy::detail::intrusive_mpsc_queue m_events_sys;
    //icy::map<uint32_t, icy::unique_ptr<gui_node_data>> m_data;
    uint32_t m_index = 1;
};

struct gui_widget_data;
class gui_window_data_sys;
class gui_model_data_sys
{
public:
    icy::error_type initialize() noexcept
    {
        icy::unique_ptr<gui_node_data> root;
        ICY_ERROR(icy::make_unique(gui_node_data(nullptr, 0), root));
        ICY_ERROR(m_data.insert(0u, std::move(root)));
        return icy::error_type();
    }
    icy::error_type process(const gui_model_event_type& event, icy::set<uint32_t>& output) noexcept;
    icy::error_type send_data(gui_window_data_sys& window, const icy::gui_widget widget, const icy::gui_node node, const icy::gui_data_bind& func, bool& erase) const noexcept;
    icy::error_type recv_data(const gui_widget_data& widget, const icy::gui_node node, const icy::gui_data_bind& func, bool& erase) noexcept;
    icy::gui_node parent(const icy::gui_node node) const noexcept;
    uint32_t row(const icy::gui_node node) const noexcept;
    uint32_t col(const icy::gui_node node) const noexcept;
    icy::gui_variant query(const icy::gui_node node, const icy::gui_node_prop prop) const noexcept;
    const gui_node_data* data(const icy::gui_node node) const noexcept
    {
        auto it = m_data.find(node.index);
        if (it != m_data.end())
            return it->value.get();
        return nullptr;
    }
private:
    icy::map<uint32_t, icy::unique_ptr<gui_node_data>> m_data;
};


class gui_model_proxy_read : public icy::gui_data_read_model
{
public:
    gui_model_proxy_read(const gui_model_data_sys& system) noexcept : m_system(system)
    {

    }
    const gui_node_data* data(const icy::gui_node node) const noexcept
    {
        return m_system.data(node);
    }
    uint32_t row(const icy::gui_node node) const noexcept override
    {
        return m_system.row(node);
    }
    uint32_t col(const icy::gui_node node) const noexcept override
    {
        return m_system.col(node);
    }
private:
    icy::gui_variant query(const icy::gui_node node, const icy::gui_node_prop prop) const noexcept override
    {
        return m_system.query(node, prop);
    }
    icy::gui_node parent(const icy::gui_node node) const noexcept override
    {
        return m_system.parent(node);
    }
private:
    const gui_model_data_sys& m_system;
};
class gui_model_proxy_write : public icy::gui_data_write_model 
{
public:
    gui_model_proxy_write(gui_model_data_sys& system) noexcept : m_system(system)
    {

    }
    /*icy::error_type modify(const icy::gui_node node, const icy::gui_node_prop prop, const icy::gui_variant& value) noexcept override
    {
        return m_system.modify(node, prop, value);
    }
    icy::error_type insert(const icy::gui_node parent, const uint32_t row, const uint32_t col, icy::gui_node& node) noexcept override
    {
        return m_system.insert(parent, row, col, node);
    }
    icy::error_type destroy(const icy::gui_node node) noexcept override
    {
        return m_system.destroy(node);
    }*/
private:
    gui_model_data_sys& m_system;
};
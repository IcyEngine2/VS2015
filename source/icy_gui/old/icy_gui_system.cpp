#define NOMINMAX
#include <icy_gui/icy_gui.hpp>
#include <icy_engine/core/icy_map.hpp>
#include <icy_engine/core/icy_thread.hpp>
#include <icy_engine/core/icy_array.hpp>
#include <icy_engine/core/icy_set.hpp>
#include <icy_engine/core/icy_color.hpp>
#include <icy_engine/graphics/icy_window.hpp>
#include <icy_engine/graphics/icy_render.hpp>
#include "icy_gui_render.hpp"
#define AM_STATIC_API
#define AM_USE_FLOAT
#include "amoeba.h"
#define snscanf _snscanf_s

using namespace icy;

ICY_STATIC_NAMESPACE_BEG
constexpr uint64_t gui_widget_flag(const gui_widget_prop prop) noexcept
{
    return prop != gui_widget_prop::none ? (1 << (uint64_t(prop) - 1)) : 0;
}
constexpr uint32_t gui_node_flag(const gui_node_prop prop) noexcept
{
    return prop != gui_node_prop::none ? (1 << (uint32_t(prop) - 1)) : 0;
}
enum gui_widget_style : uint32_t
{
    gui_widget_style_disabled,
    gui_widget_style_focused,
    gui_widget_style_hovered,
    gui_widget_style_default,
    gui_widget_style_count,
};
enum gui_widget_cursor : uint32_t
{
    gui_widget_cursor_default,
    gui_widget_cursor_move,
    gui_widget_cursor_resize_x,
    gui_widget_cursor_resize_y,
    gui_widget_cursor_resize_diag0,
    gui_widget_cursor_resize_diag1,
    gui_widget_cursor_count,
};
enum gui_widget_state : uint32_t
{
    gui_widget_state_none       =   0x00,
    gui_widget_state_visible    =   0x01,
    gui_widget_state_enabled    =   0x02,
    gui_widget_state_vlayout    =   0x04,
};
enum gui_intersect_type : uint32_t
{
    gui_intersect_none      =   0x00,
    gui_intersect_canvas    =   0x01,
    gui_intersect_border    =   0x02,
    gui_intersect_border_x0 =   0x04 | gui_intersect_border,
    gui_intersect_border_y0 =   0x08 | gui_intersect_border,
    gui_intersect_border_x1 =   0x10 | gui_intersect_border,
    gui_intersect_border_y1 =   0x20 | gui_intersect_border,
    
    gui_intersect_border_x0y0 = gui_intersect_border_x0 | gui_intersect_border_y0,
    gui_intersect_border_x0y1 = gui_intersect_border_x0 | gui_intersect_border_y1,
    gui_intersect_border_x1y0 = gui_intersect_border_x1 | gui_intersect_border_y0,
    gui_intersect_border_x1y1 = gui_intersect_border_x1 | gui_intersect_border_y1,
};
enum gui_node_state
{
    gui_node_state_none     =   0x00,
    gui_node_state_visible  =   0x01,
    gui_node_state_enabled  =   0x02,
};
enum gui_system_change : uint32_t
{
    gui_system_change_none  =   0x00,
    gui_system_change_hover =   0x01,
    gui_system_change_widget=   0x02,
    gui_system_change_style =   0x04,
    gui_system_change_node  =   0x08,
};
struct internal_command
{
    enum class type
    {
        none,
        create_widget,
        create_style,
        destroy_widget,
        destroy_style,
        destroy_node,
        modify_widget,
        modify_style,
        modify_node,
        send_input,
        render,
        query_widget,
        query_style,
        query_node,
        bind,
    };
    type type = type::none;
    uint32_t index = 0;
    uint32_t oper = 0;
    uint32_t prop = 0;
    gui_variant var;
    input_message input;
    render_flags render_flags;
    window_size render_size;
    shared_ptr<gui_node> node;
};
using internal_message = array<internal_command>;

struct gui_style_data
{
    gui_style_data(array<uint32_t>&& parents) noexcept : parents(std::move(parents))
    {

    }
    const array<uint32_t> parents;
    uint64_t flags = 0;
    color bkcolor;
    set<uint32_t> children;
    gui_font_data font;
    string font_name;
    struct border_type
    {
        gui_length lhs;
        gui_length top;
        gui_length rhs;
        gui_length bot;
        color bkcolor;
    } border;
    uint32_t cursor[gui_widget_cursor_count] = {};
};
struct gui_style_view
{
    uint64_t flags = 0;
    color bkcolor;
    gui_font_data font_data;
    string_view font_name;
    gui_style_data::border_type border;
    uint32_t cursor[gui_widget_cursor_count] = {};
    uint32_t state = 0u;
};
struct gui_widget_data
{
    gui_widget_data(const gui_widget_type type, const uint32_t parent) noexcept : 
        type(type), parent(parent), level(0), layer(0), state(0)
    {

    }
    const gui_widget_type type;
    const uint32_t parent;
    struct
    {
        uint32_t level : 0x08;
        uint32_t layer : 0x08;
        uint32_t state : 0x10;
    };
    uint64_t flags = 0;    
    set<uint32_t> children;
    struct size_type
    {
        gui_length value;
        gui_length min;
        gui_length max;
    } size_x, size_y;
    gui_length offset_x;
    gui_length offset_y;
    gui_length anchor_x;
    gui_length anchor_y;
    uint32_t style[gui_widget_style_count] = {};
    color bkcolor;
    float weight = 0;
};
class gui_node_data1;
struct gui_solver_data
{
public:
    gui_solver_data(const uint32_t index) noexcept : data(nullptr), index(index)
    {

    }
    gui_solver_data(const uint32_t index, gui_widget_data& data) noexcept : data(&data), index(index)
    {

    }
    bool operator<(const gui_solver_data& rhs) const noexcept
    {
        return index < rhs.index;
    }
    bool operator==(const gui_solver_data& rhs) const noexcept
    {
        return index == rhs.index;
    }
public:
    const gui_widget_data* data;
    const uint32_t index = 0;
    am_Var* offset_x = nullptr;
    am_Var* offset_y = nullptr;
    am_Var* anchor_x = nullptr;
    am_Var* anchor_y = nullptr;
    am_Var* size_x = nullptr;
    am_Var* size_y = nullptr;
    uint32_t state = 0u;
    struct border_type
    {
        am_Var* lhs = nullptr;
        am_Var* top = nullptr;
        am_Var* rhs = nullptr;
        am_Var* bot = nullptr;
    } border;
    const gui_node_data1* node = nullptr;
};
struct gui_render_item
{
    com_ptr<IUnknown> text;
    color color;
    gui_texture::rect_type rect;
};
struct gui_render_data
{
public:
    bool operator==(const gui_render_data& rhs) const noexcept
    {
        return index == rhs.index;
    }
    bool operator<(const gui_render_data& rhs) const noexcept
    {
        if (level > rhs.level)
            return false;
        else if (level < rhs.level)
            return true;
        else
            return index < rhs.index;
    }
    gui_intersect_type intersect(const int32_t x, const int32_t y) const noexcept
    {
        const auto px = float(x);
        const auto py = float(y);
        if (px < border.min_x || px > border.max_x || py < border.min_y || py > border.max_y)
            return gui_intersect_none;

        const auto lhs = px < canvas.min_x;
        const auto top = py < canvas.min_y;
        const auto rhs = px >= canvas.max_x;
        const auto bot = py >= canvas.max_y;

        auto value = 0u;
        if (lhs) value |= gui_intersect_border_x0; else if (rhs) value |= gui_intersect_border_x1;
        if (top) value |= gui_intersect_border_y0; else if (bot) value |= gui_intersect_border_y1;
        if ((value & gui_intersect_border) == 0)
            return gui_intersect_canvas;
        return gui_intersect_type(value);
    }
public:
    uint32_t index = 0;
    gui_widget_type type = gui_widget_type::none;
    uint32_t level = 0;
    uint32_t state = 0;
    gui_style_view style;
    gui_texture::rect_type canvas, border;
    array<gui_render_item> items;
};
class gui_model_data;
class gui_node_data0 : public gui_node
{
public:
    gui_node_data0(weak_ptr<gui_model_data> model, const shared_ptr<gui_node_data0> parent, const uint32_t row, const uint32_t col) noexcept :
        m_model(model), m_parent(parent), m_row(row), m_col(col)
    {

    }
    shared_ptr<gui_model> model() const noexcept override;
    shared_ptr<gui_node> parent() const noexcept override
    {
        return shared_ptr<gui_node_data0>(m_parent);
    }
    uint32_t row() const noexcept override
    {
        return m_col;
    }
    uint32_t col() const noexcept override
    {
        return m_col;
    }
    shared_ptr<gui_node> child(const uint32_t row, const uint32_t col) noexcept override
    {
        shared_ptr<gui_node_data0> new_node;
        make_shared(new_node, m_model, make_shared_from_this(this), row, col);
        return new_node;
    }
private:
    weak_ptr<gui_model_data> m_model;
    weak_ptr<gui_node_data0> m_parent;
    uint32_t m_row = 0;
    uint32_t m_col = 0;
};
class gui_node_data1
{
public:
    const gui_node_data1* find(const uint32_t row, const uint32_t col) const noexcept;
    gui_node_data1* find(const uint32_t row, const uint32_t col) noexcept 
    {
        return const_cast<gui_node_data1*>(static_cast<const gui_node_data1*>(this)->find(row, col));
    }
    gui_node_data1* insert(const uint32_t row, const uint32_t col) noexcept;
    void remove(const uint32_t row, const uint32_t col) noexcept;
    error_type modify(const gui_node_prop prop, const gui_variant& var) noexcept;
    gui_variant query(const gui_node_prop prop) const noexcept;
private:
    map<uint64_t, gui_node_data1> m_children;
    uint32_t m_state = 0;
    uint32_t m_flags = 0;
    gui_variant m_data;
    gui_variant m_user;
};
class gui_request_data : public gui_request
{
public:
    gui_request_data(gui_system& system) noexcept : m_system(system)
    {

    }
private:
    error_type exec() noexcept override;
    error_type create_widget(uint32_t& widget, const gui_widget_type type, const uint32_t parent) noexcept override;
    error_type create_style(uint32_t& style, const const_array_view<uint32_t> parents) noexcept override;
    error_type destroy_widget(const uint32_t widget) noexcept override;
    error_type destroy_style(const uint32_t style) noexcept override;
    error_type destroy_node(const gui_node& node) noexcept override;
    error_type modify_widget(const uint32_t widget, const gui_widget_prop prop, const gui_variant& var) noexcept override;
    error_type modify_style(const uint32_t style, const gui_style_prop prop, const gui_variant& var) noexcept override;
    error_type modify_node(const gui_node& node, const gui_node_prop prop, const gui_variant& var) noexcept override;
    error_type send_input(const input_message& message) noexcept override;
    error_type render(uint32_t& oper, const window_size size, const render_flags flags = render_flags::none) noexcept override;
    error_type query_widget(uint32_t& oper, const uint32_t widget, const gui_widget_prop prop) noexcept override;
    error_type query_style(uint32_t& oper, const uint32_t style, const gui_style_prop prop) noexcept override;
    error_type query_node(uint32_t& oper, const gui_node& node, const gui_node_prop prop) noexcept override;
    error_type bind(const uint32_t widget, const shared_ptr<gui_node> node) noexcept override;
private:
    gui_system& m_system;
    internal_message m_data;
};
class gui_model_data : public gui_model
{
public:
    error_type initialize() noexcept
    {
        ICY_ERROR(make_shared(m_root0, make_shared_from_this(this), shared_ptr<gui_node_data0>(), 0u, 0u));
        return error_type();
    }
    const gui_node_data1* find(const gui_node& node) const noexcept;
    gui_node_data1* find(const gui_node& node) noexcept
    {
        return const_cast<gui_node_data1*>(static_cast<const gui_model_data*>(this)->find(node));
    }
    void destroy(const gui_node& node) noexcept;
    error_type modify(const gui_node& node, const gui_node_prop prop, const gui_variant& var) noexcept;
    gui_variant query(const gui_node& node, const gui_node_prop prop) const noexcept;
    shared_ptr<gui_node> root() const noexcept override
    {
        return m_root0;
    }
private:
    shared_ptr<gui_node_data0> m_root0;
    gui_node_data1 m_root1;
};
class gui_thread : public thread
{
public:
    gui_system* system = nullptr;
    void cancel() noexcept override
    {
        system->post(nullptr, event_type::global_quit);
    }
    error_type run() noexcept override
    {
        if (auto error = system->exec())
            return event::post(system, event_type::system_error, std::move(error));
        return error_type();
    }
};
class gui_system_data : public gui_system
{
public:
    gui_system_data(const shared_ptr<gui_render_system> render, const shared_ptr<window> window) noexcept : 
        m_render_system(render), m_window(window)
    {

    }
    ~gui_system_data() noexcept;
    error_type initialize() noexcept;
    uint32_t next_oper() noexcept
    {
        return m_oper.fetch_add(1, std::memory_order_acq_rel) + 1;
    }
    uint32_t next_widget() noexcept
    {
        return m_widget.fetch_add(1, std::memory_order_acq_rel) + 1;
    }
    uint32_t next_style() noexcept
    {
        return m_style.fetch_add(1, std::memory_order_acq_rel) + 1;
    }
private:
    const icy::thread& thread() const noexcept override
    {
        return *m_thread;
    }
    error_type exec() noexcept override;
    error_type signal(const event_data& event) noexcept override
    {
        m_cvar.wake();
        return error_type();
    }
    error_type enum_font_names(array<string>& fonts) const noexcept override;
    error_type enum_font_sizes(const string_view font, array<uint32_t>& sizes) const noexcept override;
    error_type create_request(shared_ptr<gui_request>& data) noexcept override;
    error_type create_model(shared_ptr<gui_model>& model) noexcept override;
private:
    void reset(const gui_system_change change) noexcept
    {
        m_change |= change;
    }
    error_type update() noexcept;
    bool calc_style(const uint32_t style_index, const gui_style_prop style_prop, gui_style_view& style) const noexcept;
    gui_style_view calc_style(const uint32_t widget_index, const uint32_t state) const noexcept;
    gui_solver_data& find(const uint32_t index) noexcept
    {
        gui_solver_data tmp(index);
        auto it = binary_search(m_solver_data.begin(), m_solver_data.end(), tmp);
        return *it;
    }
    error_type resize(const window_size size) noexcept;
    error_type input(const input_message& msg) noexcept;
    error_type input_key_press(const key key, const key_mod mods) noexcept;
    error_type input_key_hold(const key key, const key_mod mods) noexcept;
    error_type input_key_release(const key key, const key_mod mods) noexcept;
    error_type input_mouse_move(const int32_t px, const int32_t py, const key_mod mods) noexcept;
    error_type input_mouse_wheel(const int32_t px, const int32_t py, const key_mod mods, const int32_t wheel) noexcept;
    error_type input_mouse_release(const key key, const int32_t px, const int32_t py, const key_mod mods) noexcept;
    error_type input_mouse_press(const key key, const int32_t px, const int32_t py, const key_mod mods) noexcept;
    error_type input_mouse_hold(const key key, const int32_t px, const int32_t py, const key_mod mods) noexcept;
    error_type input_mouse_double(const key key, const int32_t px, const int32_t py, const key_mod mods) noexcept;
    error_type input_text(const string_view text) noexcept;
    error_type render(const uint32_t action, const window_size size, const render_flags flags, shared_ptr<texture>& texture) noexcept;
    error_type bind(const uint32_t widget, const shared_ptr<gui_node> node) noexcept;
    error_type create_widget(const uint32_t widget, const gui_widget_type type, const uint32_t parent) noexcept;
    error_type create_style(const uint32_t style, const const_array_view<uint32_t> parents) noexcept;
    void destroy_widget(const uint32_t widget) noexcept;
    void destroy_style(const uint32_t style) noexcept;
    void destroy_node(const gui_node& node) noexcept;
    error_type query_widget(const uint32_t widget, const gui_widget_prop prop, gui_variant& var) noexcept;
    error_type query_style(const uint32_t style, const gui_style_prop prop, gui_variant& var) noexcept;
    error_type query_node(const gui_node& node, const gui_node_prop prop, gui_variant& var) noexcept;
    error_type modify_widget(const uint32_t widget, const gui_widget_prop prop, const gui_variant& var) noexcept;
    error_type modify_style(const uint32_t style, const gui_style_prop prop, const gui_variant& var) noexcept;
    error_type modify_node(const gui_node& node, const gui_node_prop prop, const gui_variant& var) noexcept;
private:
    shared_ptr<window> m_window;
    shared_ptr<gui_render_system> m_render_system;
    mutex m_lock;
    cvar m_cvar;
    std::atomic<uint32_t> m_oper = 0;
    std::atomic<uint32_t> m_widget = 0;
    std::atomic<uint32_t> m_style = 0;
    map<uint32_t, unique_ptr<gui_widget_data>> m_widgets;
    map<uint32_t, unique_ptr<gui_style_data>> m_styles;
    array<gui_solver_data> m_solver_data;
    array<gui_render_data> m_render_data;
    shared_ptr<gui_thread> m_thread;
    float m_dpi = 96.0f / 72.0f;
    float m_size_x = 0;
    float m_size_y = 0;
    struct hover_type 
    {
        uint32_t index = 0;
        gui_intersect_type type = gui_intersect_none;
    } m_hover;
    struct focus_type
    {
        uint32_t index = 0;
        //uint32_t count = 0;
    } m_focus;
    shared_ptr<window_cursor> m_cursor;
    array<shared_ptr<window_cursor>> m_cursors;
    map<uint32_t, shared_ptr<gui_node_data0>> m_binds;
    uint32_t m_change = 0;
};
ICY_STATIC_NAMESPACE_END

shared_ptr<gui_model> gui_node_data0::model() const noexcept
{
    return shared_ptr<gui_model_data>(m_model);
}

const gui_node_data1* gui_node_data1::find(const uint32_t row, const uint32_t col) const noexcept
{
    const auto index = uint64_t(row) << 0x20 | col;
    const auto it = m_children.find(index);
    return it == m_children.end() ? nullptr : &it->value;
}
gui_node_data1* gui_node_data1::insert(const uint32_t row, const uint32_t col) noexcept
{
    const auto index = uint64_t(row) << 0x20 | col;
    auto it = m_children.find(index);
    if (it == m_children.end())
        m_children.insert(index, gui_node_data1(), &it);
    
    return it == m_children.end() ? nullptr : &it->value;
}
void gui_node_data1::remove(const uint32_t row, const uint32_t col) noexcept
{
    const auto index = uint64_t(row) << 0x20 | col;
    const auto it = m_children.find(index);
    if (it != m_children.end())
        m_children.erase(it);
}
error_type gui_node_data1::modify(const gui_node_prop prop, const gui_variant& var) noexcept
{
    if (var.type() == 0)
    {
        m_flags &= ~gui_node_flag(prop);
        return error_type();
    }

    auto state = 0u;

    switch (prop)
    {
    case gui_node_prop::enabled:
        state = gui_node_state_enabled;
        break;
    case gui_node_prop::visible:
        state = gui_node_state_visible;
        break;

    case gui_node_prop::data:
        m_data = var;
        break;

    case gui_node_prop::user:
        m_user = var;
        break;

    default:
        break;
    }

    if (state)
    {
        m_state &= ~state;
        if (var.as<bool>()) m_state |= state;
    }
    m_flags |= gui_node_flag(prop);
    return error_type();
}
gui_variant gui_node_data1::query(const gui_node_prop prop) const noexcept
{
    if ((m_flags & gui_node_flag(prop)) == 0)
        return gui_variant();
    
    switch (prop)
    {
    case gui_node_prop::enabled:
        return (m_state & gui_node_state_enabled) != 0;
    case gui_node_prop::visible:
        return (m_state & gui_node_state_visible) != 0;
    case gui_node_prop::data:
        return m_data;
    case gui_node_prop::user:
        return m_user;
    }
    return gui_variant();
}

error_type gui_request_data::exec() noexcept
{
    return m_system.post(nullptr, event_type::system_internal, internal_message(std::move(m_data)));
}
error_type gui_request_data::create_widget(uint32_t& widget, const gui_widget_type type, const uint32_t parent) noexcept
{
    internal_command cmd;
    cmd.type = internal_command::type::create_widget;
    cmd.index = widget = static_cast<gui_system_data&>(m_system).next_widget();
    cmd.prop = uint32_t(type);
    cmd.var = parent;
    return m_data.push_back(std::move(cmd));
}
error_type gui_request_data::create_style(uint32_t& style, const const_array_view<uint32_t> parents) noexcept
{
    internal_command cmd;
    cmd.type = internal_command::type::create_style;
    cmd.index = style = static_cast<gui_system_data&>(m_system).next_style();
    cmd.var = parents;
    return m_data.push_back(std::move(cmd));
}
error_type gui_request_data::destroy_widget(const uint32_t widget) noexcept
{
    internal_command cmd;
    cmd.type = internal_command::type::destroy_widget;
    cmd.index = widget;
    return m_data.push_back(std::move(cmd));
}
error_type gui_request_data::destroy_style(const uint32_t style) noexcept
{
    internal_command cmd;
    cmd.type = internal_command::type::destroy_style;
    cmd.index = style;
    return m_data.push_back(std::move(cmd));
}
error_type gui_request_data::destroy_node(const gui_node& node) noexcept
{
    internal_command cmd;
    cmd.type = internal_command::type::destroy_node;
    cmd.node = make_shared_from_this(&node);    
    return m_data.push_back(std::move(cmd));
}
error_type gui_request_data::modify_widget(const uint32_t widget, const gui_widget_prop prop, const gui_variant& var) noexcept
{
    internal_command cmd;
    cmd.type = internal_command::type::modify_widget;
    cmd.index = widget;
    cmd.prop = uint32_t(prop);
    cmd.var = var;
    return m_data.push_back(std::move(cmd));
}
error_type gui_request_data::modify_style(const uint32_t style, const gui_style_prop prop, const gui_variant& var) noexcept
{
    internal_command cmd;
    cmd.type = internal_command::type::modify_style;
    cmd.index = style;
    cmd.prop = uint32_t(prop);
    cmd.var = var;
    return m_data.push_back(std::move(cmd));
}
error_type gui_request_data::modify_node(const gui_node& node, const gui_node_prop prop, const gui_variant& var) noexcept
{
    internal_command cmd;
    cmd.type = internal_command::type::modify_node;
    cmd.prop = uint32_t(prop);
    cmd.var = var;
    cmd.node = make_shared_from_this(&node);
    return m_data.push_back(std::move(cmd));}
error_type gui_request_data::send_input(const input_message& message) noexcept
{
    internal_command cmd;
    cmd.type = internal_command::type::send_input;
    cmd.var = message;
    return m_data.push_back(std::move(cmd));
}
error_type gui_request_data::render(uint32_t& oper, const window_size size, const render_flags flags) noexcept
{
    internal_command cmd;
    cmd.oper = oper = static_cast<gui_system_data&>(m_system).next_oper();
    cmd.type = internal_command::type::render;
    cmd.prop = uint32_t(flags);
    cmd.var = size;
    return m_data.push_back(std::move(cmd));
}
error_type gui_request_data::query_widget(uint32_t& oper, const uint32_t widget, const gui_widget_prop prop) noexcept
{
    internal_command cmd;
    cmd.oper = oper = static_cast<gui_system_data&>(m_system).next_oper();
    cmd.type = internal_command::type::query_widget;
    cmd.index = widget;
    cmd.prop = uint32_t(prop);
    return m_data.push_back(std::move(cmd));
}
error_type gui_request_data::query_style(uint32_t& oper, const uint32_t style, const gui_style_prop prop) noexcept
{
    internal_command cmd;
    cmd.oper = oper = static_cast<gui_system_data&>(m_system).next_oper();
    cmd.type = internal_command::type::query_style;
    cmd.index = style;
    cmd.prop = uint32_t(prop);
    return m_data.push_back(std::move(cmd));
}
error_type gui_request_data::query_node(uint32_t& oper, const gui_node& node, const gui_node_prop prop) noexcept
{
    internal_command cmd;
    cmd.oper = oper = static_cast<gui_system_data&>(m_system).next_oper();
    cmd.type = internal_command::type::query_node;
    cmd.node = make_shared_from_this(&node);
    cmd.prop = uint32_t(prop);
    return m_data.push_back(std::move(cmd));
}
error_type gui_request_data::bind(const uint32_t widget, const shared_ptr<gui_node> node) noexcept
{
    internal_command cmd;
    cmd.type = internal_command::type::bind;
    cmd.node = node;
    cmd.index = widget;
    return m_data.push_back(std::move(cmd));
}

const gui_node_data1* gui_model_data::find(const gui_node& node) const noexcept
{
    if (const auto parent = node.parent())
    {
        if (const auto parent_node = find(*parent))
            return parent_node->find(node.row(), node.col());        
    }
    else
    {
        return &m_root1;
    }
    return nullptr;
}
void gui_model_data::destroy(const gui_node& node) noexcept
{
    if (const auto parent = node.parent())
    {
        if (const auto parent_node = find(*parent))
            parent_node->remove(node.row(), node.col());
    }    
}
error_type gui_model_data::modify(const gui_node& node, const gui_node_prop prop, const gui_variant& var) noexcept
{
    if (const auto parent = node.parent())
    {
        if (const auto parent_node = find(*parent))
        {
            if (const auto find_node = parent_node->insert(node.row(), node.col()))
                return find_node->modify(prop, var);
            else
                return make_stdlib_error(std::errc::not_enough_memory);
        }
    }
    else
    {
        ICY_ERROR(m_root1.modify(prop, var));
    }
    return error_type();
}
gui_variant gui_model_data::query(const gui_node& node, const gui_node_prop prop) const noexcept
{
    if (const auto find_node = find(node))
        return find_node->query(prop);
    return gui_variant();
}

gui_system_data::~gui_system_data() noexcept
{
    if (m_thread)
        m_thread->wait();
    filter(0);
}
error_type gui_system_data::initialize() noexcept
{
    ICY_ERROR(m_lock.initialize());
    
    auto root_widget = make_unique<gui_widget_data>(gui_widget_type::none, 0u);
    auto root_style = make_unique<gui_style_data>(array<uint32_t>());
    if (!root_widget || !root_style)
        return make_stdlib_error(std::errc::not_enough_memory);

    auto dpi = 96u;
    if (const auto hwnd = static_cast<HWND__*>(m_window->handle()))
    {
        auto lib = "user32"_lib;
        ICY_ERROR(lib.initialize());
        if (const auto func = ICY_FIND_FUNC(lib, GetClientRect))
        {
            RECT rect;
            if (func(hwnd, &rect))
            {
                m_size_x = float(rect.right - rect.left);
                m_size_y = float(rect.bottom - rect.top);
            }
            else
                return last_system_error();
        }
        else
        {
            return make_stdlib_error(std::errc::function_not_supported);
        }
    }
    m_dpi = m_render_system->dpi / 72.0f;
    ICY_ERROR(m_widgets.insert(0u, std::move(root_widget)));
    ICY_ERROR(m_styles.insert(0u, std::move(root_style)));
    ICY_ERROR(make_shared(m_thread));
    m_thread->system = this;

    for (auto k = 0u; k < uint32_t(window_cursor::type::_count); ++k)
    {
        shared_ptr<window_cursor> new_cursor;
        if (auto system = m_window->system())
        {
            ICY_ERROR(system->create(new_cursor, window_cursor::type(k)));
        }
        ICY_ERROR(m_cursors.push_back(std::move(new_cursor)));
    }

    uint64_t event_types = event_type::system_internal;
    if (m_window) event_types |= event_type::window_resize | event_type::window_input;
    filter(event_types);
    return error_type();
}
error_type gui_system_data::exec() noexcept
{
    while (true)
    {
        while (auto event = pop())
        {
            if (event->type == event_type::global_quit)
                return error_type();
            
            if (event->type == event_type::window_resize || event->type == event_type::window_input)
            {
                const auto& event_data = event->data<window_message>();
                if (shared_ptr<window>(event_data.window).get() == m_window.get())
                {
                    if (event->type == event_type::window_resize)
                    {
                        ICY_ERROR(resize(event_data.size));
                    }
                    else
                    {
                        ICY_ERROR(input(event_data.input));
                    }
                }
            }
            else if (event->type == event_type::system_internal)
            {
                const auto& event_data = event->data<internal_message>();
                for (auto&& cmd : event_data)
                {
                    switch (cmd.type)
                    {
                    case internal_command::type::create_widget:
                        ICY_ERROR(create_widget(cmd.index, gui_widget_type(cmd.prop), cmd.var.as<uint32_t>()));
                        break;
                    case internal_command::type::create_style:
                        ICY_ERROR(create_style(cmd.index, cmd.var.as<const_array_view<uint32_t>>()));
                        break;
                    case internal_command::type::destroy_widget:
                        destroy_widget(cmd.index);
                        break;
                    case internal_command::type::destroy_style:
                        destroy_style(cmd.index);
                        break;
                    case internal_command::type::destroy_node:
                        destroy_node(*cmd.node);
                        break;
                    case internal_command::type::modify_widget:
                        ICY_ERROR(modify_widget(cmd.index, gui_widget_prop(cmd.prop), cmd.var));
                        break;
                    case internal_command::type::modify_style:
                        ICY_ERROR(modify_style(cmd.index, gui_style_prop(cmd.prop), cmd.var));
                        break;
                    case internal_command::type::modify_node:
                        ICY_ERROR(modify_node(*cmd.node, gui_node_prop(cmd.prop), cmd.var));
                        break;
                    case internal_command::type::query_widget:
                    case internal_command::type::query_style:
                    case internal_command::type::query_node:
                    {
                        gui_message msg;
                        msg.index = cmd.index;
                        msg.oper = cmd.oper;
                        msg.prop = cmd.prop;
                        switch (cmd.type)
                        {
                        case internal_command::type::query_widget:
                            ICY_ERROR(query_widget(cmd.index, gui_widget_prop(cmd.prop), msg.var));
                            break;
                        case internal_command::type::query_style:
                            ICY_ERROR(query_style(cmd.index, gui_style_prop(cmd.prop), msg.var));
                            break;
                        case internal_command::type::query_node:
                            ICY_ERROR(query_node(*cmd.node, gui_node_prop(cmd.prop), msg.var));
                            break;
                        }
                        ICY_ERROR(event::post(this, event_type::gui_query, std::move(msg)));
                        break;
                    }
                    case internal_command::type::send_input:
                        ICY_ERROR(input(cmd.var.as<input_message>()));
                        break;
                    case internal_command::type::render:
                    {
                        gui_message msg;
                        msg.index = cmd.index;
                        msg.oper = cmd.oper;
                        ICY_ERROR(render(cmd.oper, cmd.var.as<window_size>(), render_flags(cmd.prop), msg.texture));
                        ICY_ERROR(event::post(this, event_type::gui_render, std::move(msg)));
                        break;
                    }
                    case internal_command::type::bind:
                    {
                        ICY_ERROR(bind(cmd.index, cmd.node));
                        break;
                    }
                    }
                }
            }
        }
        ICY_ERROR(m_cvar.wait(m_lock));
    }
    return error_type();
}
error_type gui_system_data::enum_font_names(array<string>& fonts) const noexcept
{
    return m_render_system->enum_font_names(fonts);
}
error_type gui_system_data::enum_font_sizes(const string_view font, array<uint32_t>& sizes) const noexcept
{
    return m_render_system->enum_font_sizes(font, sizes);
}
error_type gui_system_data::create_request(shared_ptr<gui_request>& data) noexcept
{
    shared_ptr<gui_request_data> ptr;
    ICY_ERROR(make_shared(ptr, *this));
    data = std::move(ptr);
    return error_type();
}
error_type gui_system_data::create_model(shared_ptr<gui_model>& data) noexcept
{
    shared_ptr<gui_model_data> ptr;
    ICY_ERROR(make_shared(ptr));
    ICY_ERROR(ptr->initialize());
    data = std::move(ptr);
    return error_type();
}
error_type gui_system_data::update() noexcept
{
    if (m_change == 0)
        return error_type();

    ICY_SCOPE_EXIT{ m_change = 0; };

    am_Solver* solver = nullptr;
    struct error_jmp { jmp_buf buf; } jmp;
    if (setjmp(jmp.buf))
    {
        if (solver) am_delsolver(solver);
        return make_stdlib_error(std::errc::not_enough_memory);
    }
    const auto alloc = [](void* user, void* ptr, size_t nsize, size_t)
    {
        ptr = global_realloc(ptr, nsize, user);
        if (nsize && !ptr)
            longjmp(static_cast<error_jmp*>(user)->buf, 1);
        return ptr;
    };
    solver = am_newsolver(alloc, &jmp);
    if (!solver) return make_stdlib_error(std::errc::not_enough_memory);
    ICY_SCOPE_EXIT{ am_delsolver(solver); };

    ICY_ERROR(m_solver_data.reserve(m_widgets.size()));
    m_solver_data.clear();

    for (auto&& pair : m_widgets)
    {
        ICY_ERROR(m_solver_data.emplace_back(pair.key, *pair.value.get()));
    }
    auto& root = m_solver_data.front();
    root.state |= gui_widget_state_visible;
    root.state |= gui_widget_state_enabled;

    for (auto&& widget : m_solver_data)
    {
        const auto& data = *widget.data;
        const auto& parent = find(data.parent);

        if ((parent.state & gui_widget_state_visible) == 0)
            continue;

        if (data.flags & gui_widget_flag(gui_widget_prop::visible))
        {
            if ((data.state & gui_widget_state_visible) == 0)
                continue;
        }
        const auto add_state = [&data, &widget, &parent](const gui_widget_prop prop, const gui_widget_state state)
        {
            if (data.flags & gui_widget_flag(prop))
            {
                if (data.state & state)
                    widget.state |= state;
            }
        };
        if (data.flags & gui_widget_flag(gui_widget_prop::enabled))
        {
            if (data.state & gui_widget_state::gui_widget_state_enabled)
                widget.state |= gui_widget_state::gui_widget_state_enabled;
        }
        else
        {
            widget.state |= gui_widget_state::gui_widget_state_enabled;
        }

        widget.state |= gui_widget_state_visible;
        widget.offset_x = am_newvariable(solver);
        widget.offset_y = am_newvariable(solver);
        widget.anchor_x = am_newvariable(solver);
        widget.anchor_y = am_newvariable(solver);
        widget.size_x = am_newvariable(solver);
        widget.size_y = am_newvariable(solver);

        const auto add_size = [this, &solver, &data](const gui_widget_prop prop, const gui_length length, am_Var* const var, am_Var* const parent)
        {
            if (!(data.flags & gui_widget_flag(prop)))
                return;

            if ((prop == gui_widget_prop::size_x || prop == gui_widget_prop::size_y) && length.type != gui_length_type::relative)
            {
                am_suggest(var, float(length.value) * (length.type == gui_length_type::point ? m_dpi : 1.0f));
                return;
            }

            auto con = am_newconstraint(solver, AM_STRONG);
            am_addterm(con, var, 1.0);

            switch (prop)
            {
            case gui_widget_prop::size_min_x:
            case gui_widget_prop::size_min_y:
                am_setrelation(con, AM_GREATEQUAL);
                break;
            case gui_widget_prop::size_max_x:
            case gui_widget_prop::size_max_y:
                am_setrelation(con, AM_LESSEQUAL);
                break;
            default:
                am_setrelation(con, AM_EQUAL);
            }

            switch (length.type)
            {
            case gui_length_type::point:
                am_addconstant(con, float(length.value) * m_dpi);
                break;
            case gui_length_type::relative:
                am_addterm(con, parent, float(length.value));
                break;
            default:
                am_addconstant(con, float(length.value));
                break;
            }
            am_add(con);
        };

        add_size(gui_widget_prop::size_x, data.size_x.value, widget.size_x, parent.size_x);
        add_size(gui_widget_prop::size_min_x, data.size_x.min, widget.size_x, parent.size_x);
        add_size(gui_widget_prop::size_max_x, data.size_x.max, widget.size_x, parent.size_x);
        add_size(gui_widget_prop::size_y, data.size_y.value, widget.size_y, parent.size_y);
        add_size(gui_widget_prop::size_min_y, data.size_y.min, widget.size_y, parent.size_y);
        add_size(gui_widget_prop::size_max_y, data.size_y.max, widget.size_y, parent.size_y);

        if (widget.index == 0)
        {
            if ((data.flags & gui_widget_flag(gui_widget_prop::size_x)) == 0)
                am_suggest(widget.size_x, m_size_x);
            if ((data.flags & gui_widget_flag(gui_widget_prop::size_y)) == 0)
                am_suggest(widget.size_y, m_size_y);
        }

        if (widget.index && !data.children.empty())
        {
            const auto has_vlayout = data.state & gui_widget_state_vlayout;
            auto con = am_newconstraint(solver, AM_MEDIUM);
            auto mul_weight = 1.0f;

            for (auto&& child_index : data.children)
            {
                auto& child = find(child_index);
                auto weight = 1.0f;

                if (child.data->flags & gui_widget_flag(gui_widget_prop::weight))
                    weight = child.data->weight;

                if (weight)
                    mul_weight *= weight;
            }
            for (auto&& child_index : data.children)
            {
                auto& child = find(child_index);
                auto weight = 1.0f;

                if (child.data->flags & gui_widget_flag(gui_widget_prop::weight))
                    weight = child.data->weight;

                if (weight)
                    am_addterm(con, has_vlayout ? child.size_y : child.size_x, mul_weight / weight);
            }
            am_addconstant(con, 0);
            am_setrelation(con, AM_EQUAL);
            am_addterm(con, has_vlayout ? widget.size_y : widget.size_x, mul_weight);
            am_add(con);
        }

        const auto add_anchor = [&](const gui_widget_prop prop, am_Var* const var, am_Var* const size, const gui_length length)
        {
            if (data.flags & gui_widget_flag(prop))
            {
                switch (length.type)
                {
                case gui_length_type::point:
                    am_suggest(var, float(length.value) * m_dpi);
                    break;
                case gui_length_type::relative:
                {
                    auto con = am_newconstraint(solver, AM_STRONG);
                    am_addterm(con, var, 1.0);
                    am_setrelation(con, AM_EQUAL);
                    am_addterm(con, size, float(length.value));
                    break;
                }
                default:
                    am_suggest(var, float(length.value));
                    break;
                };
            }
            else
            {
                am_suggest(var, 0);
            }
        };
        add_anchor(gui_widget_prop::anchor_x, widget.anchor_x, widget.size_x, data.anchor_x);
        add_anchor(gui_widget_prop::anchor_y, widget.anchor_y, widget.size_y, data.anchor_y);

        const auto add_offset = [&](const gui_widget_prop prop, const bool clip,
            am_Var* const var, am_Var* const size, am_Var* const anchor, const gui_length length,
            am_Var* const parent_size, am_Var* const parent_anchor)
        {
            /*if (clip) //  offset >= 0
            {
                auto con = am_newconstraint(solver, AM_WEAK);
                am_addterm(con, var, 1);
                am_setrelation(con, AM_GREATEQUAL);
                am_addconstant(con, 0);
                am_add(con);
            }
            if (clip) //  offset + size <= parent_size
            {
                auto con = am_newconstraint(solver, AM_STRONG);
                am_addterm(con, var, 1);
                am_addterm(con, size, 1);
                am_setrelation(con, AM_LESSEQUAL);
                am_addterm(con, parent_size, 1);
                am_add(con);
            }*/

            auto con = am_newconstraint(solver, AM_WEAK);
            am_addterm(con, var, 1.0);
            am_setrelation(con, AM_EQUAL);
            am_addterm(con, parent_anchor, 1);
            am_addterm(con, anchor, -1);
            if (data.flags & gui_widget_flag(prop))
            {
                switch (length.type)
                {
                case gui_length_type::point:
                    am_addconstant(con, float(length.value) * m_dpi);
                    break;
                case gui_length_type::relative:
                    am_addterm(con, parent_size, float(length.value));
                    break;
                default:
                    am_addconstant(con, float(length.value));
                    break;
                }
            }
            am_add(con);
        };
        if (widget.index)
        {
            add_offset(gui_widget_prop::offset_x, false,
                widget.offset_x, widget.size_x, widget.anchor_x,
                data.offset_x, parent.size_x, parent.anchor_x);

            add_offset(gui_widget_prop::offset_y, false,
                widget.offset_y, widget.size_y, widget.anchor_y,
                data.offset_y, parent.size_y, parent.anchor_y);
        }
    }

    for (auto it = m_binds.begin(); it != m_binds.end();)
    {
        auto& widget = find(it->key);
        if ((widget.state & gui_widget_state_visible) == 0)
        {
            ++it;
            continue;
        }
        auto model = it->value->model();
        auto model_ptr = static_cast<gui_model_data*>(model.get());
        const gui_node_data1* node = model_ptr ? model_ptr->find(*it->value) : nullptr;
        if (!node)
        {
            it = m_binds.erase(it);
            continue;
        }
        widget.node = node;
        ++it;
    }
    am_updatevars(solver);

    m_render_data.clear();

    for (auto&& widget : m_solver_data)
    {
        const auto pred = [](const gui_render_data& lhs, const gui_render_data& rhs)
        {
            return lhs.index < rhs.index;
        };

        if ((widget.state & gui_widget_state_visible) == 0)
            continue;

        const auto size_x = am_value(widget.size_x);
        const auto size_y = am_value(widget.size_y);
        if (size_x <= 0 || size_y <= 0)
            continue;

        const auto& data = *widget.data;

        gui_render_data new_render = {};
        auto& canvas = new_render.canvas;
        auto& border = new_render.border;

        new_render.index = widget.index;
        new_render.state = widget.state;
        new_render.type = widget.data->type;

        canvas.min_x = am_value(widget.offset_x);
        canvas.min_y = am_value(widget.offset_y);
        gui_render_data parent_data = {};
        parent_data.index = data.parent;
        auto parent = binary_search(m_render_data.begin(), m_render_data.end(), parent_data, pred);
        auto layer = 0u;
        if (widget.index)
        {
            canvas.min_x += parent->canvas.min_x;
            canvas.min_y += parent->canvas.min_y;
            if (widget.data->flags & gui_widget_flag(gui_widget_prop::layer))
                layer = widget.data->layer;
            else
                layer = parent_data.level / 256;
        }
        new_render.level = layer * 256 + widget.data->level;

        canvas.max_x = canvas.min_x + size_x;
        canvas.max_y = canvas.min_y + size_y;
        border = canvas;

        new_render.style = calc_style(widget.index, new_render.state);

        if ((new_render.style.flags & gui_style_flag(gui_style_prop::cursor)) == 0)
        {
            if (data.type == gui_widget_type::line_edit ||
                data.type == gui_widget_type::text_edit)
            {
                new_render.style.cursor[gui_widget_cursor_default] =
                    uint32_t(window_cursor::type::ibeam);
            }
        }

        if (data.flags & gui_widget_flag(gui_widget_prop::bkcolor))
            new_render.style.bkcolor = data.bkcolor;

        if (widget.node)
        {
            switch (data.type)
            {
            case gui_widget_type::window:
            case gui_widget_type::line_edit:
            case gui_widget_type::text_edit:
            {
                const auto var = widget.node->query(gui_node_prop::data);
                string_view str;
                if (var.type() == gui_variant_type<string_view>())
                {
                    str = var.to_string();
                }
                else
                {

                }
                gui_render_item item;
                item.rect = new_render.canvas;
                item.color = new_render.style.font_data.color;
                ICY_ERROR(m_render_system->create_text(item.text, new_render.canvas,
                    new_render.style.font_data, new_render.style.font_name, str, new_render.style.flags));
                new_render.items.push_back(std::move(item));
                break;
            }
            }
        }

        //new_render.items = widget.node;
        ICY_ERROR(m_render_data.push_back(std::move(new_render)));
    }
    std::stable_sort(m_render_data.begin(), m_render_data.end());

    shared_ptr<window_cursor> new_cursor;
    if (m_hover.index)
    {
        for (auto&& widget : m_render_data)
        {
            if (widget.index == m_hover.index)
            {
                auto cursor_offset = gui_widget_cursor_default;
                auto cursor_index = widget.style.cursor[cursor_offset];
                new_cursor = m_cursors[cursor_index];
                break;
            }
        }
    }
    if (new_cursor != m_cursor)
    {
        ICY_ERROR(m_window->cursor(0, m_cursor = new_cursor));
    }

    return error_type();
}
bool gui_system_data::calc_style(const uint32_t style_index, const gui_style_prop prop, gui_style_view& style) const noexcept
{
    auto style_ptr = m_styles.try_find(style_index)->get();
    if (style_ptr->flags & gui_style_flag(prop))
    {
        style.state |= gui_style_flag(prop);
        switch (prop)
        {
        case gui_style_prop::bkcolor:
            style.bkcolor = style_ptr->bkcolor;
            break;

        case gui_style_prop::border_lhs:
            style.border.lhs = style_ptr->border.lhs;
            break;

        case gui_style_prop::border_rhs:
            style.border.rhs = style_ptr->border.rhs;
            break;

        case gui_style_prop::border_top:
            style.border.top = style_ptr->border.top;
            break;

        case gui_style_prop::border_bot:
            style.border.bot = style_ptr->border.bot;
            break;

        case gui_style_prop::border_bkcolor:
            style.border.bkcolor = style_ptr->border.bkcolor;
            break;

        case gui_style_prop::font_stretch:
            style.font_data.stretch = style_ptr->font.stretch;
            break;

        case gui_style_prop::font_weight:
            style.font_data.weight = style_ptr->font.weight;
            break;

        case gui_style_prop::font_size:
            style.font_data.size = style_ptr->font.size;
            break;

        case gui_style_prop::font_italic:
            style.font_data.italic = style_ptr->font.italic;
            break;

        case gui_style_prop::font_oblique:
            style.font_data.oblique = style_ptr->font.oblique;
            break;

        case gui_style_prop::font_strike:
            style.font_data.strike = style_ptr->font.strike;
            break;

        case gui_style_prop::font_underline:
            style.font_data.underline = style_ptr->font.underline;
            break;

        case gui_style_prop::font_spacing:
            style.font_data.spacing = style_ptr->font.spacing;
            break;

        case gui_style_prop::font_align_text:
            style.font_data.align_text = style_ptr->font.align_text;
            break;

        case gui_style_prop::font_align_par:
            style.font_data.align_par = style_ptr->font.align_par;
            break;

        case gui_style_prop::font_trimming:
            style.font_data.trimming = style_ptr->font.trimming;
            break;

        case gui_style_prop::font_name:
            style.font_name = style_ptr->font_name;
            break;

        case gui_style_prop::cursor:
        case gui_style_prop::cursor_move:
        case gui_style_prop::cursor_resize_x:
        case gui_style_prop::cursor_resize_y:
        case gui_style_prop::cursor_resize_diag0:
        case gui_style_prop::cursor_resize_diag1:
        {
            auto cursor_offset = gui_widget_cursor_default;
            switch (prop)
            {
            case gui_style_prop::cursor_move:
                cursor_offset = gui_widget_cursor_move;
                break;
            case gui_style_prop::cursor_resize_x:
                cursor_offset = gui_widget_cursor_resize_x;
                break;
            case gui_style_prop::cursor_resize_y:
                cursor_offset = gui_widget_cursor_resize_y;
                break;
            case gui_style_prop::cursor_resize_diag0:
                cursor_offset = gui_widget_cursor_resize_diag0;
                break;
            case gui_style_prop::cursor_resize_diag1:
                cursor_offset = gui_widget_cursor_resize_diag1;
                break;
            }
            style.cursor[cursor_offset] = style_ptr->cursor[cursor_offset];
            break;
        }
        }
        return true;
    }
    else
    {
        for (auto&& parent : style_ptr->parents)
        {
            if (calc_style(parent, prop, style))
                return true;
        }
    }
    return false;
}
gui_style_view gui_system_data::calc_style(const uint32_t index, const uint32_t state) const noexcept
{
    if (index == 0)
        return gui_style_view();

    gui_widget_style flags[gui_widget_style_count] = {};
    gui_widget_prop props[gui_widget_style_count] = {};

    auto flags_count = 0u;
    
    if ((state & gui_widget_state::gui_widget_state_enabled) == 0)
    {
        flags[flags_count] = gui_widget_style_disabled;
        props[flags_count] = gui_widget_prop::style_disabled;
        flags_count++;
    }
    if (m_focus.index == index)
    {
        flags[flags_count] = gui_widget_style_focused;
        props[flags_count] = gui_widget_prop::style_focused;
        flags_count++;
    }
    if (m_hover.index == index)
    {
        flags[flags_count] = gui_widget_style_hovered;
        props[flags_count] = gui_widget_prop::style_hovered;
        flags_count++;
    }
    flags[flags_count] = gui_widget_style_default;
    props[flags_count] = gui_widget_prop::style_default;
    flags_count++;

    gui_style_view style;
    auto& system_font = m_render_system->system_fonts[gui_system_font_type_default];
    style.font_data = system_font;
    style.font_name = system_font.name;
    style.flags |= gui_style_flag(gui_style_prop::font_name);
    style.flags |= gui_style_flag(gui_style_prop::font_size);
    style.flags |= gui_style_flag(gui_style_prop::font_color);
    style.flags |= gui_style_flag(gui_style_prop::font_weight);
    style.flags |= gui_style_flag(gui_style_prop::font_stretch);
    style.flags |= gui_style_flag(gui_style_prop::font_italic);
    style.flags |= gui_style_flag(gui_style_prop::font_oblique);
    style.flags |= gui_style_flag(gui_style_prop::font_underline);
    style.flags |= gui_style_flag(gui_style_prop::font_strike);

    
    const auto add_prop = [&](const gui_style_prop prop)
    {
        for (auto k = 0u; k < flags_count; ++k)
        {
            auto widget_ptr = m_widgets.find(index)->value.get();
            while (true)
            {
                if (widget_ptr->flags & gui_widget_flag(props[k]))
                {
                    const auto style_index = widget_ptr->style[flags[k]];
                    if (calc_style(style_index, prop, style))
                        return;
                    else
                        break;
                }
                else
                {
                    //auto next_ptr = m_widgets.find(widget_ptr->parent)->value.get();
                    //if (next_ptr == widget_ptr)
                        break;
                    //widget_ptr = next_ptr;
                }
            }
        }
    };
    for (auto k = 1u; k < uint32_t(gui_style_prop::_count); ++k)
        add_prop(gui_style_prop(k)); 

    if ((style.state & gui_style_flag(gui_style_prop::cursor_move)) == 0)
        style.cursor[gui_widget_cursor_move] = style.cursor[gui_widget_cursor_default];
    
    if ((style.state & gui_style_flag(gui_style_prop::cursor_resize_x)) == 0)
        style.cursor[gui_widget_cursor_resize_x] = style.cursor[gui_widget_cursor_default];

    if ((style.state & gui_style_flag(gui_style_prop::cursor_resize_y)) == 0)
        style.cursor[gui_widget_cursor_resize_y] = style.cursor[gui_widget_cursor_default];

    if ((style.state & gui_style_flag(gui_style_prop::cursor_resize_diag0)) == 0)
        style.cursor[gui_widget_cursor_resize_diag0] = style.cursor[gui_widget_cursor_default];

    if ((style.state & gui_style_flag(gui_style_prop::cursor_resize_diag1)) == 0)
        style.cursor[gui_widget_cursor_resize_diag1] = style.cursor[gui_widget_cursor_default];

    return style;
}
error_type gui_system_data::resize(const window_size size) noexcept
{
    m_size_x = float(size.x);
    m_size_y = float(size.y);
    reset(gui_system_change_widget);
    return update();
}
error_type gui_system_data::input(const input_message& msg) noexcept
{
    const auto mods = key_mod(msg.mods);
    ICY_ERROR(update());
    switch (msg.type)
    {
    case input_type::key_release:
        ICY_ERROR(input_key_release(msg.key, mods));
        break;

    case input_type::key_hold:
        ICY_ERROR(input_key_hold(msg.key, mods));
        break;

    case input_type::key_press:
        ICY_ERROR(input_key_press(msg.key, mods));
        break;

    case input_type::text:
    {
        const const_array_view<wchar_t> buf(msg.text, wcsnlen(msg.text, 2));
        string str;
        ICY_ERROR(to_string(buf, str));
        ICY_ERROR(input_text(str));
        break;
    }

    case input_type::mouse_move:
        ICY_ERROR(input_mouse_move(msg.point_x, msg.point_y, mods));
        break;

    case input_type::mouse_wheel:
        ICY_ERROR(input_mouse_wheel(msg.point_x, msg.point_y, mods, msg.wheel));
        break;

    case input_type::mouse_release:
        ICY_ERROR(input_mouse_release(msg.key, msg.point_x, msg.point_y, mods));
        break;

    case input_type::mouse_press:
        ICY_ERROR(input_mouse_press(msg.key, msg.point_x, msg.point_y, mods));
        break;

    case input_type::mouse_hold:
        ICY_ERROR(input_mouse_hold(msg.key, msg.point_x, msg.point_y, mods));
        break;

    case input_type::mouse_double:
        ICY_ERROR(input_mouse_double(msg.key, msg.point_x, msg.point_y, mods));
        break;
    }
    return error_type();
}
error_type gui_system_data::input_key_press(const key key, const key_mod mods) noexcept
{
    return error_type();
}
error_type gui_system_data::input_key_hold(const key key, const key_mod mods) noexcept
{
    return error_type();
}
error_type gui_system_data::input_key_release(const key key, const key_mod mods) noexcept 
{
    return error_type();
}
error_type gui_system_data::input_mouse_move(const int32_t px, const int32_t py, const key_mod mods) noexcept
{
    hover_type new_hover;
    for (auto it = m_render_data.rbegin(); it != m_render_data.rend() && it->index; ++it)
    {
        if (const auto type = it->intersect(px, py))
        {
            new_hover.type = type;
            new_hover.index = it->index;
            break;
        }
    }
    const auto old_hover = m_hover;
    const auto change = new_hover.index != old_hover.index || new_hover.type != old_hover.type;
    m_hover = new_hover;
    if (change)
    {
        reset(gui_system_change_hover);
        ICY_ERROR(update());
    }  
    return error_type();
}
error_type gui_system_data::input_mouse_wheel(const int32_t px, const int32_t py, const key_mod mods, const int32_t wheel) noexcept
{
    return error_type();
}
error_type gui_system_data::input_mouse_release(const key key, const int32_t px, const int32_t py, const key_mod mods) noexcept
{
    return error_type();
}
error_type gui_system_data::input_mouse_press(const key key, const int32_t px, const int32_t py, const key_mod mods) noexcept
{
    return error_type();
}
error_type gui_system_data::input_mouse_hold(const key key, const int32_t px, const int32_t py, const key_mod mods) noexcept
{
    return error_type();
}
error_type gui_system_data::input_mouse_double(const key key, const int32_t px, const int32_t py, const key_mod mods) noexcept 
{
    return error_type();
}
error_type gui_system_data::input_text(const string_view text) noexcept
{
    return error_type();
}
error_type gui_system_data::render(const uint32_t oper, const window_size size, const render_flags flags, shared_ptr<texture>& texture) noexcept
{
    ICY_ERROR(update());
    
    shared_ptr<gui_texture> surface;
    ICY_ERROR(m_render_system->create_texture(surface, size, flags));

    if (!m_render_data.empty())
    {
        const auto& root = m_render_data.front();
        surface->draw_begin(root.style.bkcolor);

        for (auto&& widget : m_render_data)
        {
            if (!widget.index)
                continue;

            ICY_ERROR(surface->fill_rect(widget.canvas, widget.style.bkcolor));

            for (auto&& item : widget.items)
            {
                surface->draw_text(item.rect, item.color, item.text);

            }            
        }
        ICY_ERROR(surface->draw_end());
        /*auto offset = 0u;
        array<render_vertex::index> index;
        array<render_vertex::vec4> coord;
        array<render_vertex::vec4> color;
        array<render_vertex::vec2> texuv;

        for (auto k = m_render_data.size(); k > 1; --k)
        {
            const auto& widget = m_render_data[k - 1];
            const auto lhs = kx * widget.canvas.rect[0] - 1;
            const auto top = 1 - ky * widget.canvas.rect[1];
            const auto rhs = kx * widget.canvas.rect[2] - 1;
            const auto bot = 1 - ky * widget.canvas.rect[3];
            const auto z = 1 - 1.0f / float(widget.level);

            render_vertex::vec4 rgba;
            widget.canvas.color.to_rgbaf(rgba.vec);

            ICY_ERROR(coord.push_back({ lhs, top, z, 1 }));
            ICY_ERROR(coord.push_back({ rhs, top, z, 1 }));
            ICY_ERROR(coord.push_back({ rhs, bot, z, 1 }));
            ICY_ERROR(coord.push_back({ lhs, bot, z, 1 }));

            const auto ioffset = uint32_t(index.size());
            ICY_ERROR(query->draw_primitive(ioffset, ioffset + 6));

            ICY_ERROR(index.push_back(offset + 0));
            ICY_ERROR(index.push_back(offset + 1));
            ICY_ERROR(index.push_back(offset + 2));

            ICY_ERROR(index.push_back(offset + 0));
            ICY_ERROR(index.push_back(offset + 2));
            ICY_ERROR(index.push_back(offset + 3));

            offset += 4;

            for (auto n = 0u; n < 4u; ++n)
            {
                ICY_ERROR(color.push_back(rgba));
            }

            
        }
        query->set_coord_buffer(std::move(coord));
        query->set_color_buffer(std::move(color));
        query->set_index_buffer(std::move(index));*/
    }

    
 /*   render_message msg;
    ICY_ERROR(query->exec(msg.oper));
    msg.texture = texture;
    ICY_ERROR(m_render_wait.insert(oper, std::move(msg)));*/

    texture = std::move(surface);
    return error_type();
}
error_type gui_system_data::bind(const uint32_t widget, const shared_ptr<gui_node> node) noexcept
{
    if (!widget || m_widgets.try_find(widget) == nullptr)
        return error_type();

    auto it = m_binds.find(widget);
    auto ptr = make_shared_from_this(static_cast<const gui_node_data0*>(node.get()));
    if (it == m_binds.end())
    {
        if (widget && ptr)
        {
            ICY_ERROR(m_binds.insert(widget, ptr));
        }
        else
        {
            return error_type();
        }
    }
    else if (ptr)
    {
        it->value = ptr;
    }
    else
    {
        m_binds.erase(it);
    }
    reset(gui_system_change_node); 
    return error_type();
}
error_type gui_system_data::create_widget(const uint32_t widget, const gui_widget_type type, uint32_t parent) noexcept
{
    const auto it = m_widgets.find(widget);
    if (it != m_widgets.end() || widget == 0 || type == gui_widget_type::none)
        return make_stdlib_error(std::errc::invalid_argument);

    auto pwidget = m_widgets.try_find(parent);
    if (!pwidget)
        return make_stdlib_error(std::errc::invalid_argument);

    auto new_data = make_unique<gui_widget_data>(type, parent);
    if (!new_data)
        return make_stdlib_error(std::errc::not_enough_memory);

    new_data->level = 1;
    ICY_ERROR(pwidget->get()->children.insert(widget));
    while (parent = pwidget->get()->parent) 
        pwidget = m_widgets.try_find(parent);
    
    ICY_ERROR(m_widgets.insert(widget, std::move(new_data)));
    return error_type();
}
error_type gui_system_data::create_style(const uint32_t style, const const_array_view<uint32_t> parents) noexcept
{
    const auto it = m_styles.find(style);
    if (it != m_styles.end() || style == 0)
        return make_stdlib_error(std::errc::invalid_argument);

    array<uint32_t> indices; 
    ICY_ERROR(indices.assign(parents));
    if (parents.empty())
    {
        ICY_ERROR(indices.push_back(0));
    }

    for (auto&& index : parents)
    {
        auto pstyle = m_styles.try_find(index);
        if (!pstyle)
            return make_stdlib_error(std::errc::invalid_argument);
    
        ICY_ERROR(pstyle->get()->children.insert(style));        
    }
    auto new_data = make_unique<gui_style_data>(std::move(indices));
    if (!new_data)
        return make_stdlib_error(std::errc::not_enough_memory);

    ICY_ERROR(m_styles.insert(style, std::move(new_data)));
    return error_type();
}
void gui_system_data::destroy_widget(const uint32_t widget) noexcept
{
    if (!widget)
        return;
    
    auto it = m_widgets.find(widget);
    if (it == m_widgets.end())
        return;
    
    auto ptr = std::move(it->value);
    m_widgets.erase(it);

    if (const auto parent = m_widgets.try_find(ptr->parent))
        parent->get()->children.erase(widget);
    
    for (auto&& child : ptr->children)
        destroy_widget(child);

    if (m_hover.index == widget) m_hover = {};
    if (m_focus.index == widget) m_focus = {};
    
    reset(gui_system_change_widget);
}
void gui_system_data::destroy_style(const uint32_t style) noexcept
{
    if (!style)
        return;

    auto it = m_styles.find(style);
    if (it == m_styles.end())
        return;

    auto ptr = std::move(it->value);
    m_styles.erase(it);

    for (auto&& parent : ptr->parents)
    {
        if (const auto sparent = m_styles.try_find(parent))
            sparent->get()->children.erase(style);
    }
    for (auto&& child : ptr->children)
        destroy_style(child);

    for (auto&& widget : m_widgets)
    {
        for (auto k = 0u; k < gui_widget_style_count; ++k)
        {
            auto& index = widget.value->style[k];
            if (index == style)
            {
                index = 0;
                auto prop = gui_widget_prop::none;
                switch (k)
                {
                case gui_widget_style_disabled:
                    prop = gui_widget_prop::style_disabled;
                    break;
                case gui_widget_style_focused:
                    prop = gui_widget_prop::style_focused;
                    break;
                case gui_widget_style_hovered:
                    prop = gui_widget_prop::style_hovered;
                    break;
                case gui_widget_style_default:
                    prop = gui_widget_prop::style_default;
                    break;
                }
                widget.value->flags &= ~gui_widget_flag(prop);
            }
        }
    }

    reset(gui_system_change_style);
}
void gui_system_data::destroy_node(const gui_node& node) noexcept
{
    if (auto model = static_cast<gui_model_data*>(node.model().get()))
    {
        model->destroy(node);
        for (auto it = m_binds.begin(); it != m_binds.end(); )
        {
            if (it->value->model().get() == model)
            {
                if (model->find(*it->value) == nullptr)
                {
                    it = m_binds.erase(it);
                    reset(gui_system_change_node);
                    continue;
                }
            }
            ++it;
        }
    }
}
error_type gui_system_data::query_widget(const uint32_t widget, const gui_widget_prop prop, gui_variant& var) noexcept
{
    return error_type();
}
error_type gui_system_data::query_style(const uint32_t style, const gui_style_prop prop, gui_variant& var) noexcept
{
    return error_type();
}
error_type gui_system_data::query_node(const gui_node& node, const gui_node_prop prop, gui_variant& var) noexcept
{
    if (auto model = node.model())
    {
        var = static_cast<gui_model_data*>(model.get())->query(node, prop);
    }
    return error_type();
}
error_type gui_system_data::modify_widget(const uint32_t widget, const gui_widget_prop prop, const gui_variant& var) noexcept
{
    const auto it = m_widgets.find(widget);
    if (it == m_widgets.end())
        return error_type();

    auto& ref = *it->value;
    if (var.type() == 0)
    {
        ref.flags &= ~gui_widget_flag(prop);
        reset(gui_system_change_widget);
        return error_type();
    }

    auto state = 0u;
    switch (prop)
    {
    case gui_widget_prop::enabled:
        state = gui_widget_state_enabled;
        break;

    case gui_widget_prop::visible:
        state = gui_widget_state_visible;
        break;

    case gui_widget_prop::layout:
        state = gui_widget_state_vlayout;
        break;

    case gui_widget_prop::style_default:
    case gui_widget_prop::style_disabled:
    case gui_widget_prop::style_focused:
    case gui_widget_prop::style_hovered:
    {
        auto style_index = 0u;
        ICY_ERROR(var.get(style_index));
        if (!m_styles.try_find(style_index))
            return make_stdlib_error(std::errc::invalid_argument);
        
        auto style_offset = gui_widget_style_default;
        switch (prop)
        {
        case gui_widget_prop::style_disabled:
            style_offset = gui_widget_style_disabled;
            break;
        case gui_widget_prop::style_focused:
            style_offset = gui_widget_style_focused;
            break;
        case gui_widget_prop::style_hovered:
            style_offset = gui_widget_style_hovered;
            break;
        }
        ref.style[style_offset] = style_index;
        break;
    }

    case gui_widget_prop::bkcolor:
        ICY_ERROR(var.get(ref.bkcolor));
        break;

    case gui_widget_prop::size_x:
        ICY_ERROR(var.get(ref.size_x.value));
        break;
    
    case gui_widget_prop::size_min_x:
        ICY_ERROR(var.get(ref.size_x.min));
        break;

    case gui_widget_prop::size_max_x:
        ICY_ERROR(var.get(ref.size_x.max));
        break;

    case gui_widget_prop::size_y:
        ICY_ERROR(var.get(ref.size_y.value));
        break;

    case gui_widget_prop::size_min_y:
        ICY_ERROR(var.get(ref.size_y.min));
        break;

    case gui_widget_prop::size_max_y:
        ICY_ERROR(var.get(ref.size_y.max));
        break;

    case gui_widget_prop::offset_x:
        ICY_ERROR(var.get(ref.offset_x));
        break;

    case gui_widget_prop::offset_y:
        ICY_ERROR(var.get(ref.offset_y));
        break;

    case gui_widget_prop::anchor_x:
        ICY_ERROR(var.get(ref.anchor_x));
        break;

    case gui_widget_prop::anchor_y:
        ICY_ERROR(var.get(ref.anchor_y));
        break;

    case gui_widget_prop::weight:
        ICY_ERROR(var.get(ref.weight));
        ref.weight = std::max(ref.weight, 0.0f);
        break;

    case gui_widget_prop::layer:
    {
        auto layer = 0u;
        ICY_ERROR(var.get(layer));
        ref.layer = std::min(layer, 255u);
        break;
    }
    }
    if (state)
    {
        ref.state &= ~state;
        if (var.as<bool>()) ref.state |= state;
    }
    ref.flags |= gui_widget_flag(prop);
    reset(gui_system_change_widget);
    
    return error_type();
}
error_type gui_system_data::modify_style(const uint32_t style, const gui_style_prop prop, const gui_variant& var) noexcept
{
    const auto it = m_styles.find(style);
    if (it == m_styles.end())
        return error_type();

    auto& ref = *it->value;
    if (var.type() == 0)
    {
        ref.flags &= ~gui_style_flag(prop);
        reset(gui_system_change_style);
        return error_type();
    }
    auto state = 0u;
    switch (prop)
    {
    case gui_style_prop::cursor:
    case gui_style_prop::cursor_move:
    case gui_style_prop::cursor_resize_x:
    case gui_style_prop::cursor_resize_y:
    case gui_style_prop::cursor_resize_diag0:
    case gui_style_prop::cursor_resize_diag1:
    {
        const auto type = var.as<window_cursor::type>();
        auto cursor_offset = gui_widget_cursor_default;
        switch (prop)
        {
        case gui_style_prop::cursor_move:
            cursor_offset = gui_widget_cursor_move;
            break;
        case gui_style_prop::cursor_resize_x:
            cursor_offset = gui_widget_cursor_resize_x;
            break;
        case gui_style_prop::cursor_resize_y:
            cursor_offset = gui_widget_cursor_resize_y;
            break;
        case gui_style_prop::cursor_resize_diag0:
            cursor_offset = gui_widget_cursor_resize_diag0;
            break;
        case gui_style_prop::cursor_resize_diag1:
            cursor_offset = gui_widget_cursor_resize_diag1;
            break;
        }
        ref.cursor[cursor_offset] = uint32_t(type);
        break;
    }

    }
    ref.flags |= gui_style_flag(prop);
    reset(gui_system_change_style);
    return error_type();
}
error_type gui_system_data::modify_node(const gui_node& node, const gui_node_prop prop, const gui_variant& var) noexcept
{
    if (auto model = node.model())
    {
        static_cast<gui_model_data*>(model.get())->modify(node, prop, var);
        reset(gui_system_change_node);
    }
    return error_type();
}

error_type icy::create_gui_system(shared_ptr<gui_system>& gui, const adapter adapter, const shared_ptr<window> window) noexcept
{
    if (!adapter || !window || !window->handle())
        return make_stdlib_error(std::errc::invalid_argument);

    shared_ptr<gui_render_system> render;
    ICY_ERROR(make_shared(render, adapter));
    ICY_ERROR(render->initialize(static_cast<HWND__*>(window->handle())));

    shared_ptr<gui_system_data> ptr;
    ICY_ERROR(make_shared(ptr, render, window));
    ICY_ERROR(ptr->initialize());
    gui = std::move(ptr);
    return error_type();
}

error_type icy::detail::convert(const uint32_t type, const void* const data, const size_t size, gui_length& length) noexcept
{
    if (type == gui_variant_type<string_view>())
    {
        const auto str = static_cast<const char*>(data);
        auto count = 0;
        if (snscanf(str, size, "%lf%n", &length.value, &count) != 1)
            return make_stdlib_error(std::errc::invalid_argument);

        const auto substr = string_view(str + count, str + size);
        switch (hash(substr))
        {
        case "%"_hash:
            length.value /= 100;
            length.type = gui_length_type::relative;
            break;
        case "pt"_hash:
            length.type = gui_length_type::point;
            break;

        case "mm"_hash:
            length.value *= mm_to_point;
            length.type = gui_length_type::point;
            break;

        case "cm"_hash:
            length.value *= mm_to_point * 10;
            length.type = gui_length_type::point;
            break;

        case "in"_hash:
            length.value *= 72;
            length.type = gui_length_type::point;
            break;
        }
        return error_type();        
    }
    return convert(type, data, size, length.value);
}
error_type icy::detail::convert(const uint32_t type, const void* const data, const size_t size, color& color) noexcept
{
    if (type == gui_variant_type<icy::colors>())
    {
        color = icy::color(*static_cast<const colors*>(data));
        return error_type();
    }
    else if (type == gui_variant_type<string_view>())
    {
        const auto str = string_view(static_cast<const char*>(data), size);
        for (auto k = 1u; k < uint32_t(colors::_count); ++k)
        {
            if (to_string(colors(k)) == str)
            {
                color = icy::color(colors(k));
                return error_type();
            }
        }
    }
    return make_stdlib_error(std::errc::invalid_argument);

}
#define AM_IMPLEMENTATION
#include "amoeba.h"
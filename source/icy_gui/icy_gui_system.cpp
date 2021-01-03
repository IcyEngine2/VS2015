#include <icy_gui/icy_gui.hpp>
#include <icy_engine/core/icy_map.hpp>
#include <icy_engine/core/icy_thread.hpp>
#include <icy_engine/core/icy_array.hpp>
#include <icy_engine/core/icy_set.hpp>
#include <icy_engine/core/icy_color.hpp>
#include <icy_engine/graphics/icy_window.hpp>
#include "icy_gui_text.hpp"
#define AM_STATIC_API
#define AM_USE_FLOAT
#include "amoeba.h"
#define snscanf _snscanf_s

using namespace icy;

ICY_STATIC_NAMESPACE_BEG
class gui_system_data;
constexpr uint64_t gui_widget_flag(const gui_widget_prop prop) noexcept
{
    return prop != gui_widget_prop::none ? (1 << (uint64_t(prop) - 1)) : 0;
}
enum gui_link_type
{
    gui_link_type_child,
    gui_link_type_font,
    gui_link_type_count,
};
struct gui_link_data
{
    set<uint32_t> refs[gui_link_type_count];
};
struct gui_size_data
{
    struct size_type
    {
        gui_length value;
        gui_length min;
        gui_length max;
    } size_x, size_y;
    gui_length offset_x;
    gui_length offset_y;
};
struct gui_border_data
{
    gui_length lhs;
    gui_length top;
    gui_length rhs;
    gui_length bot;
    color bkcolor;
};
struct gui_font_data
{
    enum max
    {
        max_stretch =   9,
        max_weight  =   1000,
        max_size    =   72,
    };
    enum bits
    {
        bits_stretch    =   detail::constexpr_log2(max_stretch) + 1,
        bits_weight     =   detail::constexpr_log2(max_weight) + 1,
        bits_size       =   detail::constexpr_log2(max_size) + 1,
        bits_align_text =   2,
        bits_align_par  =   2,
        bits_trimming   =   2,
    };
    uint32_t index = 0;
    struct
    {
        int64_t stretch     : bits_stretch;
        int64_t weight      : bits_weight;
        int64_t size        : bits_size;
        int64_t italic      : 1;
        int64_t oblique     : 1;
        int64_t strike      : 1;
        int64_t underline   : 1;
        int64_t spacing     : bits_size;
        int64_t align_text  : 2;
        int64_t align_par   : 2;
        int64_t trimming    : 2;
    };
    string name;
};
struct gui_event
{
    const uint32_t source = thread::this_index();
    std::atomic<gui_event*> next = nullptr;
    array<gui_action::data_type> data;
};
struct gui_widget_data
{
public:
    gui_widget_data(const gui_widget_type type = gui_widget_type::none) noexcept : type(type)
    {

    }
    error_type modify(const uint32_t index, map<uint32_t, gui_widget_data>& data, const gui_widget_prop prop, const gui_variant& var) noexcept;
    gui_variant query(const gui_widget_prop prop) const noexcept;
    error_type erase(const gui_link_type type, const uint32_t index) noexcept;
    error_type insert(const gui_link_type type, const uint32_t index) noexcept;
public:
    gui_widget_type type;
    uint64_t flags = gui_widget_flag(gui_widget_prop::enabled) | gui_widget_flag(gui_widget_prop::visible);
    uint32_t parent = 0u;
    unique_ptr<gui_link_data> links;
    unique_ptr<gui_font_data> font;
    unique_ptr<gui_size_data> size;
    unique_ptr<gui_border_data> border;
    color bkcolor;
};
struct gui_widget_list_data
{
    gui_widget_list_data() : index(0), level(0)
    {

    }
    bool operator<(const gui_widget_list_data& rhs) const noexcept
    {
        return index < rhs.index;
    }
    bool operator==(const gui_widget_list_data& rhs) const noexcept
    {
        return index == rhs.index;
    }
    gui_widget_data* data = nullptr;
    struct
    {
        uint32_t index : 0x18;
        uint32_t level : 0x08;
    };
    color bkcolor;
    am_Var* offset_x = nullptr;
    am_Var* offset_y = nullptr;
    am_Var* size_x = nullptr;
    am_Var* size_y = nullptr;
    struct border_type
    {
        am_Var* lhs = nullptr;
        am_Var* top = nullptr;
        am_Var* rhs = nullptr;
        am_Var* bot = nullptr;
        color bkcolor;
    } border;
};
struct gui_widget_render_data
{
    bool operator<(const gui_widget_render_data& rhs) const noexcept
    {
        if (level > rhs.level)
            return false;
        else if (level < rhs.level)
            return true;
        else
            return index < rhs.index;
    }
    struct
    {
        uint32_t index : 0x18;
        uint32_t level : 0x08;
    };
    struct
    {
        color color;
        float rect[4];
    } canvas, border;
};
class gui_thread_data
{
    friend gui_system_data;
public:
    gui_thread_data(gui_system_data& system) noexcept : m_system(system)
    {

    }
    error_type update() noexcept
    {
        while (auto next = m_event->next.load(std::memory_order_acquire))
        {
            m_event = next;
            ICY_ERROR(process(*next));
        }
        return error_type();
    }
    error_type process(const gui_event& event) noexcept;
    error_type append(array<gui_action::data_type>&& data) noexcept;
    error_type sort() noexcept;
    error_type render(render_texture& canvas) noexcept;
    void recalc(am_Solver& solver, gui_widget_list_data& widget) noexcept;
private:
    gui_system_data& m_system;
    gui_thread_data* m_prev = nullptr;
    map<uint32_t, gui_widget_data> m_data;
    gui_event* m_event = nullptr;
    shared_ptr<render_texture> m_texture;
    gui_widget_data m_root;
    array<gui_widget_list_data> m_list;
    array<gui_widget_render_data> m_render;
    float m_kxy = 96.0f / 72.0f;  //  dpi / 72
};
class gui_system_data : public gui_system
{
public:
    gui_system_data(const render_flags flags, void* const handle) noexcept :
        m_flags(flags), m_handle(handle), m_tls_data(*this)
    {

    }
    ~gui_system_data() noexcept;
    error_type initialize(const adapter adapter) noexcept;
    error_type exec() noexcept override
    {
        return make_stdlib_error(std::errc::function_not_supported);
    }
    error_type signal(const event_data& event) noexcept override;
    render_flags flags() const noexcept override
    {
        return m_flags;
    }
    error_type enum_font_names(array<string>& fonts) const noexcept override;
    error_type enum_font_sizes(const string_view font, array<uint32_t>& sizes) const noexcept override;
    error_type update() noexcept override;
    gui_action action() noexcept override
    {
        return gui_action(*this);
    }
    gui_widget_type type(const uint32_t index) const noexcept override;
    gui_variant query(const uint32_t index, const gui_widget_prop prop) const noexcept override;
    error_type render(render_texture& texture) noexcept override;
    error_type process(const event& event) noexcept override;
    error_type append(array<gui_action::data_type>&& data) noexcept;
    uint32_t next() noexcept
    {
        return m_widget.fetch_add(1, std::memory_order_acq_rel) + 1;
    }
private:
    const render_flags m_flags;
    void* const m_handle;
    library m_lib = "user32"_lib;
    gui_text_system m_text;
    shared_ptr<texture_system> m_texture;
    mutex m_lock;
    thread_local_data m_tls_root;
    gui_thread_data m_tls_data;
    gui_event m_event;
    std::atomic<uint32_t> m_widget = 0;
};
ICY_STATIC_NAMESPACE_END

error_type gui_action::exec() noexcept
{
    return static_cast<gui_system_data&>(m_system).append(std::move(m_data));
}
error_type gui_action::create(const gui_widget_type type, uint32_t& index) noexcept
{
    gui_action::data_type new_data = {};
    new_data.var = gui_variant(type);
    index = new_data.widget = static_cast<gui_system_data&>(m_system).next();
    return m_data.push_back(std::move(new_data));
}
error_type gui_action::destroy(const uint32_t index) noexcept
{
    if (!index)
        return make_stdlib_error(std::errc::invalid_argument);

    gui_action::data_type new_data;
    new_data.widget = index;
    return m_data.push_back(std::move(new_data));
}
error_type gui_action::modify(const uint32_t index, const gui_widget_prop prop, const gui_variant& var) noexcept
{
    gui_action::data_type new_data;
    new_data.widget = index;
    new_data.prop = prop;
    new_data.var = var;
    return m_data.push_back(std::move(new_data));
}

error_type gui_widget_data::modify(const uint32_t index, map<uint32_t, gui_widget_data>& data, const gui_widget_prop prop, const gui_variant& var) noexcept
{
    if (var.type() == 0)
        flags &= ~gui_widget_flag(prop);
    else
        flags |= gui_widget_flag(prop);
    
    switch (prop)
    {
    case gui_widget_prop::enabled:
    case gui_widget_prop::visible:
        flags &= ~gui_widget_flag(prop);
        if (var.as<bool>())
            flags |= gui_widget_flag(prop);
        break;

    case gui_widget_prop::size_width:
    case gui_widget_prop::size_min_width:
    case gui_widget_prop::size_max_width:
    case gui_widget_prop::size_height:
    case gui_widget_prop::size_min_height:
    case gui_widget_prop::size_max_height:
    case gui_widget_prop::offset_x:
    case gui_widget_prop::offset_y:
    {
        if (!size)
            ICY_ERROR(make_unique(gui_size_data(), size));
        
        gui_length* ptr = nullptr;
        switch (prop)
        {
        case gui_widget_prop::size_width: ptr = &size->size_x.value; break;
        case gui_widget_prop::size_min_width: ptr = &size->size_x.min; break;
        case gui_widget_prop::size_max_width: ptr = &size->size_x.max; break;
        case gui_widget_prop::size_height: ptr = &size->size_y.value; break;
        case gui_widget_prop::size_min_height: ptr = &size->size_y.min; break;
        case gui_widget_prop::size_max_height: ptr = &size->size_y.max; break;
        case gui_widget_prop::offset_x: ptr = &size->offset_x; break;
        case gui_widget_prop::offset_y: ptr = &size->offset_y; break;
        }
        if (var.type())
            ICY_ERROR(var.get(*ptr));
        break;
    }
    case gui_widget_prop::bkcolor:
    {
        ICY_ERROR(var.get(bkcolor));
        break;
    }

    case gui_widget_prop::parent:
    {
        const auto pindex = var.as<uint32_t>();
        if (pindex == index || index == 0)
            return make_stdlib_error(std::errc::invalid_argument);

        if (pindex == parent)
            break;

        auto next = data.try_find(pindex);
        while (next)
        {
            if (next == this)
                return make_stdlib_error(std::errc::invalid_argument);
            next = data.try_find(next->parent);
        }
        if (parent)
        {
            const auto parent_ptr = data.try_find(parent);
            if (!parent_ptr)
                return make_stdlib_error(std::errc::invalid_argument);
            ICY_ERROR(parent_ptr->erase(gui_link_type_child, index));
        }
        if (pindex)
        {
            const auto parent_ptr = data.try_find(pindex);
            if (!parent_ptr)
                return make_stdlib_error(std::errc::invalid_argument);
            ICY_ERROR(parent_ptr->insert(gui_link_type_child, index));
        }
        parent = pindex;
        break;
    }
    default:
        break;
    }
    return error_type();
}
gui_variant gui_widget_data::query(const gui_widget_prop prop) const noexcept
{
    switch (prop)
    {
    case gui_widget_prop::enabled:
    case gui_widget_prop::visible:
        return gui_variant((flags & gui_widget_flag(prop)) != 0);
    default:
        break;
    }
    return gui_variant();
}
error_type gui_widget_data::erase(const gui_link_type type, const uint32_t index) noexcept
{
    if (!links)
        return make_stdlib_error(std::errc::invalid_argument);

    auto& set = links->refs[type];
    auto it = set.find(index);
    if (it == set.end())
        return make_stdlib_error(std::errc::invalid_argument);

    set.erase(it);
    return error_type();
}
error_type gui_widget_data::insert(const gui_link_type type, const uint32_t index) noexcept
{
    if (!links)
        links = make_unique<gui_link_data>();
    if (!links)
        return make_stdlib_error(std::errc::not_enough_memory);
    
    auto& set = links->refs[type];
    return set.insert(index);
}

error_type gui_thread_data::process(const gui_event& event) noexcept
{
    for (auto&& action : event.data)
    {
        const auto it = m_data.find(action.widget);
        if (action.prop == gui_widget_prop::none)
        {
            if (it == m_data.end() && action.var.type() == gui_variant_type<gui_widget_type>())
            {
                ICY_ERROR(m_data.insert(action.widget, action.var.as<gui_widget_type>()));
            }
            else if (action.widget && it != m_data.end() && action.var.type() == 0)
            {
                m_data.erase(it);
            }
        }
        else if (it != m_data.end())
        {
            ICY_ERROR(it->value.modify(action.widget, m_data, action.prop, action.var));
        }
        else if (action.widget == 0)
        {
            ICY_ERROR(m_root.modify(0u, m_data, action.prop, action.var));
        }
    }
    return error_type();
}
error_type gui_thread_data::append(array<gui_action::data_type>&& data) noexcept
{
    auto new_event = allocator_type::allocate<gui_event>(1);
    if (!new_event)
        return make_stdlib_error(std::errc::not_enough_memory);
    allocator_type::construct(new_event);
    new_event->data = std::move(data);
    ICY_SCOPE_EXIT
    {
        if (new_event)
        {
            allocator_type::destroy(new_event);
            allocator_type::deallocate(new_event);
        }
    };

    while (true)
    {
        gui_event* next = nullptr;
        if (m_event->next.compare_exchange_strong(next, new_event))
            break;
        ICY_ERROR(update());        
    }
    new_event = nullptr;
    return event::post(&m_system, event_type::gui_update);
}
void gui_thread_data::recalc(am_Solver& solver, gui_widget_list_data& widget) noexcept
{
    const auto find = [this](const uint32_t index) -> gui_widget_list_data&
    {
        gui_widget_list_data search;
        search.index = index;
        const auto it = icy::binary_search(m_list.begin(), m_list.end(), search);
        ICY_ASSERT(it != m_list.end(), "CORRUPTED DATA");
        return *it;        
    };
    auto& parent = find(widget.data->parent);
    widget.level = 1;
    if (widget.data->parent && parent.level == 0)
    {
        recalc(solver, parent);
        widget.level = parent.level + 1;
    }
    
    auto& data = *widget.data;

    const auto add_offset = [this, &solver, &data](const gui_widget_prop prop, am_Var* const var, am_Var* const parent_offset, am_Var* const parent_length)
    {
        auto offset_clevel = AM_REQUIRED;
        auto con = am_newconstraint(&solver, offset_clevel);
        am_addterm(con, var, 1.0);
        am_setrelation(con, AM_EQUAL);
        am_addterm(con, parent_offset, 1.0);
        if (data.size && (data.flags & gui_widget_flag(prop)))
        {
            const auto& length = prop == gui_widget_prop::offset_x ? data.size->offset_x : data.size->offset_y;
            switch (length.type)
            {
            case gui_length_type::point:
                am_addconstant(con, float(length.value) * m_kxy);
                break;
            case gui_length_type::relative:
                am_addterm(con, parent_length, float(length.value));
                break;
            default:
                am_addconstant(con, float(length.value));
                break;
            }
        }
        am_add(con);
        return error_type();
    };
    const auto add_size = [this, &solver, &data](const gui_widget_prop (&prop)[3], am_Var* const var,am_Var* const parent) -> error_type
    {
        gui_size_data::size_type* length_s = nullptr;
        if (data.size)
            length_s = prop[0] == gui_widget_prop::size_width ? &data.size->size_x : &data.size->size_y;
        
        auto has_con = false;
        for (auto k = 0u; k < 3; ++k)
        {
            if (!(data.flags & gui_widget_flag(prop[k])))
                continue;

            const auto level = k == 0 ? AM_STRONG : AM_REQUIRED;
            
            has_con = true;
            auto con = am_newconstraint(&solver, level);
            am_addterm(con, var, 1.0);

            gui_length* length = &length_s->value;
            switch (k)
            {
            case 1:
                length = &length_s->min;
                am_setrelation(con, AM_GREATEQUAL);
                break;
            case 2:
                length = &length_s->max;
                am_setrelation(con, AM_LESSEQUAL);
                break;
            default:
                am_setrelation(con, AM_EQUAL);
            }

            switch (length->type)
            {
            case gui_length_type::point:
                am_addconstant(con, float(length->value) * m_kxy);
                break;
            case gui_length_type::relative:
                am_addterm(con, parent, float(length->value));
                break;
            default:
                am_addconstant(con, float(length->value));
                break;
            }
            am_add(con);
        }
        if (!has_con)
        {
            auto con = am_newconstraint(&solver, AM_WEAK);
            am_addterm(con, var, 1.0);
            am_setrelation(con, AM_EQUAL);
            am_addterm(con, parent, 1.0);
            am_add(con);
        }
        return error_type();
    };
    const auto add_border = [this, &solver, &data](const gui_widget_prop prop, const gui_length length, am_Var*& var, am_Var* const parent)
    {
        const auto border_clevel = AM_REQUIRED;
        if (data.flags & gui_widget_flag(prop))
        {
            var = am_newvariable(&solver);
            switch (length.type)
            {
            case gui_length_type::point:
                am_suggest(var, float(length.value) * m_kxy);
                break;
            case gui_length_type::relative:
            {
                auto con = am_newconstraint(&solver, border_clevel);
                am_addterm(con, var, 1.0);
                am_setrelation(con, AM_EQUAL);
                am_addterm(con, parent, float(length.value));
                am_add(con);
                break;
            }
            default:
                am_suggest(var, float(length.value));
                break;
            }
        }
    };
    const auto add_child_size = [&solver, &data, &find](const gui_widget_prop prop, am_Var* const var)
    {
        if ((data.flags & gui_widget_flag(prop)) == 0 && data.links)
        {
            auto& children = data.links->refs[gui_link_type_child];
            if (!children.empty())
            {
                auto con = am_newconstraint(&solver, AM_MEDIUM);
                for (auto&& child : children)
                {
                    auto& child_widget = find(child);
                    am_addterm(con, child_widget.size_x, 1.0);
                }
                am_setrelation(con, AM_EQUAL);
                am_addterm(con, var, 1.0);
                am_add(con);
            }
        }
    };
    const gui_widget_prop prop_w[] =
    {
         gui_widget_prop::size_width,
         gui_widget_prop::size_min_width,
         gui_widget_prop::size_max_width,
    };
    const gui_widget_prop prop_h[] =
    {
         gui_widget_prop::size_height,
         gui_widget_prop::size_min_height,
         gui_widget_prop::size_max_height,
    };
    add_offset(gui_widget_prop::offset_x, widget.offset_x, parent.offset_x, parent.size_x);
    add_offset(gui_widget_prop::offset_y, widget.offset_y, parent.offset_y, parent.size_y);
    add_size(prop_w, widget.size_x, parent.size_x);
    add_size(prop_h, widget.size_y, parent.size_y);

    widget.border.bkcolor = parent.border.bkcolor;
    widget.bkcolor = parent.bkcolor;

    if (auto border = data.border.get())
    {
        if (data.flags & gui_widget_flag(gui_widget_prop::border_color))
            widget.border.bkcolor = border->bkcolor;
        
        add_border(gui_widget_prop::border_lhs, border->lhs, widget.border.lhs, parent.size_x);
        add_border(gui_widget_prop::border_rhs, border->lhs, widget.border.rhs, parent.size_x);
        add_border(gui_widget_prop::border_top, border->top, widget.border.top, parent.size_y);
        add_border(gui_widget_prop::border_bot, border->bot, widget.border.bot, parent.size_y);
    }

    if (data.flags & gui_widget_flag(gui_widget_prop::bkcolor))
        widget.bkcolor = data.bkcolor;

    add_child_size(gui_widget_prop::size_width, widget.size_x);
    add_child_size(gui_widget_prop::size_height, widget.size_y);    
}
error_type gui_thread_data::sort() noexcept
{
    const auto alloc = [](void* user, void* ptr, size_t nsize, size_t)
    {
        return global_realloc(ptr, nsize, user);   
    };
    auto solver = am_newsolver(alloc, nullptr);
    if (!solver)
        return make_stdlib_error(std::errc::not_enough_memory);
    ICY_SCOPE_EXIT{ am_delsolver(solver); };

    m_list.clear();
    
    gui_widget_list_data root = {};    
    const auto make_vars = [solver](gui_widget_list_data& widget)
    {
        widget.size_x = am_newvariable(solver);
        widget.size_y = am_newvariable(solver);
        widget.offset_x = am_newvariable(solver);
        widget.offset_y = am_newvariable(solver);
        return error_type();
    };    
    ICY_ERROR(make_vars(root));
    am_suggest(root.size_x, float(m_root.size->size_x.value.value));
    am_suggest(root.size_y, float(m_root.size->size_y.value.value));
    ICY_ERROR(m_list.push_back(root));
    
    
    for (auto it = m_data.begin(); it != m_data.end(); ++it)
    {
        const auto& widget = it->value;
        gui_widget_list_data new_data = {};
        new_data.index = it->key;
        new_data.data = &it->value;
        ICY_ERROR(make_vars(new_data));
        ICY_ERROR(m_list.push_back(new_data));
    }
    for (auto&& widget : m_list)
    {
        if (widget.level || widget.index == 0)
            continue;
        recalc(*solver, widget);
    }
    am_updatevars(solver);
    m_render.clear();
    for (auto&& widget : m_list)
    {
        if (widget.index == 0)
            continue;

        const auto size_x = am_value(widget.size_x);
        const auto size_y = am_value(widget.size_y);
        if (size_x <= 0 || size_y <= 0)
            continue;

        if ((widget.data->flags & gui_widget_flag(gui_widget_prop::visible)) == 0)
            continue;

        gui_widget_render_data new_render = {};
        new_render.index = widget.index;
        new_render.level = widget.level;

        auto& border = new_render.border;
        auto& canvas = new_render.canvas;

        border.color = widget.border.bkcolor;
        canvas.color = widget.bkcolor;

        canvas.rect[0] = am_value(widget.offset_x);
        canvas.rect[1] = am_value(widget.offset_y);
        canvas.rect[2] = canvas.rect[0] + size_x;
        canvas.rect[3] = canvas.rect[1] + size_y;

        border.rect[0] = canvas.rect[0] - am_value(widget.border.lhs);
        border.rect[1] = canvas.rect[1] - am_value(widget.border.top);
        border.rect[2] = canvas.rect[2] + am_value(widget.border.rhs);
        border.rect[3] = canvas.rect[3] + am_value(widget.border.bot);

        ICY_ERROR(m_render.push_back(new_render));
    }
    std::sort(m_render.begin(), m_render.end());
    return error_type();
}
error_type gui_thread_data::render(render_texture& texture) noexcept
{
    ICY_ERROR(sort());
    
    ICY_ERROR(texture.clear(m_root.bkcolor));
    const auto size = texture.size();
    const auto kx = 1.0f / size.x;
    const auto ky = 1.0f / size.y;

    render_list render;
    ICY_ERROR(render.primitives.reserve(m_render.size() * 2));

    for (auto it = m_render.rbegin(); it != m_render.rend(); ++it)
    {
        const auto& widget = *it;

        render_primitive buf[2];
        for (auto&& primitive : buf)
        {
            for (auto&& v : primitive.vertices)
            {
                widget.canvas.color.to_rgbaf(v.colors);
                v.wcoord[2] = 1.0f / float(widget.level);
                v.wcoord[3] = 1;
            }
        }
        auto lhs = kx * widget.canvas.rect[0] - 1;
        auto top = 1 - ky * widget.canvas.rect[1];
        auto rhs = kx * widget.canvas.rect[2] - 1;
        auto bot = 1 - ky * widget.canvas.rect[3];

        auto& buf0 = buf[0];
        buf0[0].wcoord[0] = lhs;
        buf0[0].wcoord[1] = top;
        buf0[1].wcoord[0] = rhs;
        buf0[1].wcoord[1] = top;
        buf0[2].wcoord[0] = lhs;
        buf0[2].wcoord[1] = bot;

        auto& buf1 = buf[1];
        buf1[0].wcoord[0] = rhs;
        buf1[0].wcoord[1] = top;
        buf1[1].wcoord[0] = rhs;
        buf1[1].wcoord[1] = bot;
        buf1[2].wcoord[0] = lhs;
        buf1[2].wcoord[1] = bot;

        ICY_ERROR(render.primitives.append(buf));
    }

    /*render_primitive prim = {};
    prim[0].wcoord[0] = -0.75;
    prim[0].wcoord[1] = +0.75;

    prim[1].wcoord[0] = -0.25;
    prim[1].wcoord[1] = +0.75;

    prim[2].wcoord[0] = -0.75;
    prim[2].wcoord[1] = 0.25;

    prim[0].wcoord[2] = prim[1].wcoord[2] = prim[2].wcoord[2] = 0.8;
    prim[0].wcoord[3] = prim[1].wcoord[3] = prim[2].wcoord[3] = 1.0;
    
    prim[0].colors[0] = 1;

    render.primitives.push_back(prim);*/

    ICY_ERROR(texture.render(render));
    return error_type();
}

gui_system_data::~gui_system_data() noexcept
{
    auto event = m_event.next.load(std::memory_order_acquire);
    while (event)
    {
        auto next = event->next.load(std::memory_order_acquire);
        allocator_type::destroy(event);
        allocator_type::deallocate(event);
        event = next;
    }
    auto tls = m_tls_data.m_prev;
    while (tls)
    {
        auto prev = tls->m_prev;
        allocator_type::destroy(tls);
        allocator_type::deallocate(tls);
        tls = prev;
    }
    filter(0);
}
error_type gui_system_data::initialize(const adapter adapter) noexcept
{
    ICY_ERROR(m_lib.initialize());
    ICY_ERROR(m_text.initialize());
    ICY_ERROR(create_texture_system(adapter, m_texture));
    ICY_ERROR(m_lock.initialize());
    ICY_ERROR(m_tls_root.initialize());
    m_tls_data.m_event = &m_event;
    filter(event_type::global_quit);
    return error_type();
}
error_type gui_system_data::signal(const event_data&) noexcept
{
    return error_type();   
}
error_type gui_system_data::enum_font_names(array<string>& fonts) const noexcept
{
    return m_text.enum_font_names(fonts);
}
error_type gui_system_data::enum_font_sizes(const string_view font, array<uint32_t>& sizes) const noexcept
{
    return m_text.enum_font_sizes(font, sizes);
}
error_type gui_system_data::update() noexcept
{
    auto ptr = static_cast<gui_thread_data*>(m_tls_root.query());
    if (!ptr)
    {
        auto size_x = 0.0f;
        auto size_y = 0.0f;
        auto dpi = 0u;
        if (const auto hwnd = static_cast<HWND__*>(m_handle))
        {
            if (const auto func = ICY_FIND_FUNC(m_lib, GetClientRect))
            {
                RECT rect;
                if (func(hwnd, &rect))
                {
                    size_x = float(rect.right - rect.left);
                    size_y = float(rect.bottom - rect.top);
                }
            }
            if (const auto func = ICY_FIND_FUNC(m_lib, GetDpiForWindow))
            {
                dpi = func(hwnd);
            }
        }
        if (!dpi) dpi = 96u;
        
        unique_ptr<gui_size_data> size;
        ICY_ERROR(make_unique(gui_size_data(), size));
        size->size_x.value.value = size_x;
        size->size_y.value.value = size_y;

        ptr = allocator_type::allocate<gui_thread_data>(1);
        if (!ptr)
            return make_stdlib_error(std::errc::not_enough_memory);
        allocator_type::construct(ptr, *this);
        ptr->m_event = &m_event;

        ptr->m_kxy = dpi / 72.0f;
        ptr->m_root.size = std::move(size);
        
        ICY_LOCK_GUARD(m_lock);
        ptr->m_prev = m_tls_data.m_prev;
        m_tls_data.m_prev = ptr;
        ICY_ERROR(m_tls_root.assign(ptr));
    }
    return ptr->update();
}
gui_widget_type gui_system_data::type(const uint32_t index) const noexcept
{
    if (const auto ptr = static_cast<gui_thread_data*>(m_tls_root.query()))
    {
        if (const auto widget = ptr->m_data.try_find(index))
            return widget->type;
    }
    return gui_widget_type::none;
}
gui_variant gui_system_data::query(const uint32_t index, const gui_widget_prop prop) const noexcept
{
    if (const auto ptr = static_cast<gui_thread_data*>(m_tls_root.query()))
    {
        if (const auto widget = ptr->m_data.try_find(index))
            return widget->query(prop);
    }
    return gui_variant();
}
error_type gui_system_data::process(const event& event) noexcept
{
    if (!event)
        return error_type();

    if (event->type == event_type::window_resize)
    {
        auto& msg = event->data<window_message>();
        if (msg.handle != m_handle)
            return error_type();

        array<gui_action::data_type> vec;
        gui_action::data_type new_action = {};
        
        new_action.prop = gui_widget_prop::size_width;
        new_action.var = gui_length(msg.size.x, gui_length_type::pixel);
        ICY_ERROR(vec.push_back(std::move(new_action)));
        new_action.prop = gui_widget_prop::size_height;
        new_action.var = gui_length(msg.size.y, gui_length_type::pixel);
        ICY_ERROR(vec.push_back(std::move(new_action)));        
        ICY_ERROR(append(std::move(vec)));
    }
    else if (event->type == event_type::window_input)
    {
        //m_tls_data.append();
    }
    return error_type();
}
error_type gui_system_data::render(render_texture& texture) noexcept
{
    ICY_ERROR(update());
    if (const auto ptr = static_cast<gui_thread_data*>(m_tls_root.query()))
        return ptr->render(texture);
    return error_type();
}
error_type gui_system_data::append(array<gui_action::data_type>&& data) noexcept
{
    ICY_ERROR(update());
    auto ptr = static_cast<gui_thread_data*>(m_tls_root.query());
    return ptr->append(std::move(data));
}

error_type icy::create_gui_system(shared_ptr<gui_system>& gui, const render_flags flags, const adapter adapter, void* const handle) noexcept
{
    shared_ptr<gui_system_data> ptr;
    ICY_ERROR(make_shared(ptr, flags, handle));
    ICY_ERROR(ptr->initialize(adapter));
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
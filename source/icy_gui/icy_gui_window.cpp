#include <icy_engine/core/icy_json.hpp>
#include "icy_gui_window.hpp"
#include "icy_gui_system.hpp"
#include "icy_gui_render.hpp"
#include "icy_gui_system.hpp"
#include "icy_gui_attr.hpp"
#include <dwrite.h>
#include <d2d1.h>
#define AM_USE_FLOAT
#define AM_STATIC_API
#include "amoeba.h"
using namespace icy;

ICY_STATIC_NAMESPACE_BEG

enum gui_window_state
{
    gui_window_state_none       =   0x00,
    gui_window_state_updated    =   0x01,
};
ICY_STATIC_NAMESPACE_END

gui_window_data_sys::solver_type::~solver_type() noexcept
{
	if (system)
		am_delsolver(system);
}
error_type gui_widget_data::initialize(map<uint32_t, unique_ptr<gui_widget_data>>& data, const uint32_t index, const json& json)
{
    if (json.type() != json_type::object)
        return error_type();

    auto it = data.find(index);
    auto& widget = *it->value;

    for (auto k = 0u; k < json.size(); ++k)
    {
        const auto& key = json.keys()[k];
        if (hash(key) != "type"_hash)
            continue;

        const auto& val = json.vals()[k];
        widget.type = gui_str_to_type(val.get());

        switch (widget.type)
        {
        case gui_widget_type::edit_line:
        case gui_widget_type::edit_text:
        case gui_widget_type::view_combo:
        {
            gui_widget_item new_item;
            new_item.type = gui_widget_item_type::text;
            ICY_ERROR(widget.items.push_back(std::move(new_item)));
            break;
        }
        break;

        default:
            break;
        }        
    }
    for (auto k = 0u; k < json.size(); ++k)
    {
        const auto& key = json.keys()[k];
        if (hash(key) == "type"_hash)
            continue;

        const auto& val = json.vals()[k];

        switch (hash(key))
        {
        case "layout"_hash:
        case "Layout"_hash:
        {
            it->value->layout = gui_str_to_layout(val.get());
            break;
        }

        case "Widgets"_hash:
        case "widgets"_hash:
        {
            if (val.type() != json_type::array)
                break;

            for (auto n = 0u; n < val.size(); ++n)
            {
                insert_args args;
                args.index = uint32_t(data.size());
                args.offset = UINT32_MAX;
                args.parent = index;
                ICY_ERROR(insert(data, args));
                ICY_ERROR(initialize(data, args.index, val.vals()[n]));
            }
            break;
        }

        default:
        {
            modify_args args;
            args.index = index;
            ICY_ERROR(copy(key, args.key));
            switch (val.type())
            {
            case json_type::boolean:
            {
                json_type_boolean value = false;
                if (val.get(value) == error_type())
                    args.val = value;
                break;
            }

            case json_type::floating:
            {
                json_type_double value = 0.0f;
                if (val.get(value) == error_type())
                    args.val = value;
                break;
            }

            case json_type::integer:
            {
                json_type_integer value = 0;
                if (val.get(value) == error_type())
                    args.val = value;
                break;
            }

            case json_type::string:
            {
                args.val = val.get();
                if (!args.val && !val.get().empty())
                    return make_stdlib_error(std::errc::not_enough_memory);
                break;
            }
            }
            ICY_ERROR(modify(data, args));
            break;
        }
        }
    }

    return error_type();
}
error_type gui_widget_data::insert(map<uint32_t, unique_ptr<gui_widget_data>>& data, const insert_args& args) noexcept
{
    auto it = data.find(args.parent);
    auto& children = it->value->children;
    const auto offset = std::min(size_t(args.offset), children.size());
        
    unique_ptr<gui_widget_data> new_widget;
    ICY_ERROR(make_unique(gui_widget_data(it->value.get(), args.index), new_widget));
    ICY_ERROR(children.reserve(children.size() + 1));
    ICY_ERROR(data.reserve(data.size() + 1));

    ICY_ERROR(children.push_back(nullptr));
    for (auto k = offset + 1; k < children.size(); ++k)
        children[k] = std::move(children[k - 1]);
    children[offset] = new_widget.get();
    new_widget->type = args.type;
    new_widget->layout = args.layout;
    ICY_ERROR(data.insert(args.index, std::move(new_widget)));
    //ICY_ERROR(modify(data, args));
    return error_type();
}
error_type gui_widget_data::modify(map<uint32_t, unique_ptr<gui_widget_data>>& data, const modify_args& args) noexcept
{
    auto it = data.find(args.index);
    if (it == data.end())
        return make_unexpected_error();
    
    auto& widget = *it->value;
    //ICY_ERROR(copy(args.type, it->value->type));
    //it->value->attr.clear();
    string str_key;
    ICY_ERROR(copy(args.key, str_key));

    auto copy_val = args.val;
    if (copy_val.type() != args.val.type())
        return make_stdlib_error(std::errc::not_enough_memory);

    if (str_key.empty() || str_key == "text"_s || str_key == "Text"_s)
    {
        switch (widget.type)
        {
        case gui_widget_type::edit_line:
        case gui_widget_type::edit_text:
        {
            for (auto&& item : widget.items)
            {
                if (item.type == gui_widget_item_type::text)
                {
                    item.value = std::move(copy_val);
                    item.text = gui_text();
                    break;
                }
            }
            break;
        }
        default:
            break;
        }
    }
    else
    {
        auto state = gui_widget_state::none;
        switch (hash(args.key))
        {
        case "visible"_hash:
        case "Visible"_hash:
            state = gui_widget_state::visible;
            break;

        case "enabled"_hash:
        case "Enabled"_hash:
            state = gui_widget_state::enabled;
            break;  
        
        case "vscroll"_hash:
        case "vScroll"_hash:
        case "VScroll"_hash:
            state = gui_widget_state::vscroll;
            break;

        case "vscroll_auto"_hash:
        case "vScrollAuto"_hash:
        case "VScrollAuto"_hash:
        case "auto_vscroll"_hash:
        case "autoVScroll"_hash:
        case "AutoVScroll"_hash:
            state = gui_widget_state::vscroll_auto;
            break;

        case "vscroll_inv"_hash:
        case "vScrollInv"_hash:
        case "VScrollInv"_hash:
        case "inv_vscroll"_hash:
        case "invVScroll"_hash:
        case "InvVScroll"_hash:
            state = gui_widget_state::vscroll_inv;
            break;

        case "hscroll"_hash:
        case "hScroll"_hash:
        case "HScroll"_hash:
            state = gui_widget_state::hscroll;
            break;

        case "hscroll_auto"_hash:
        case "hScrollAuto"_hash:
        case "HScrollAuto"_hash:
        case "auto_hscroll"_hash:
        case "autoHScroll"_hash:
        case "AutoHScroll"_hash:
            state = gui_widget_state::hscroll_auto;
            break;

        case "hscroll_inv"_hash:
        case "hScrollInv"_hash:
        case "HScrollInv"_hash:
        case "inv_hscroll"_hash:
        case "invHScroll"_hash:
        case "InvHScroll"_hash:
            state = gui_widget_state::hscroll_inv;
            break;
        }
        if (state)
        {
            bool value = false;
            if (args.val.get(value))
            {
                it->value->state &= ~state;
                if (value)
                    it->value->state |= state;
            }
        }
        else if (const auto attr = gui_str_to_attr(str_key))
        {           
            auto jt = widget.attr.find(attr);
            if (jt == widget.attr.end())
            {
                if (copy_val)
                    ICY_ERROR(widget.attr.insert(attr, std::move(copy_val)));
            }
            else if (copy_val)
            {
                jt->value = std::move(copy_val);
            }
            else
            {
                widget.attr.erase(jt);
            }
        }
    }
    return error_type();
}
bool gui_widget_data::erase(map<uint32_t, unique_ptr<gui_widget_data>>& data, const uint32_t index) noexcept
{
    auto it = data.find(index);
    if (it != data.end())
    {
        auto ptr = std::move(it->value);
        data.erase(it);
        auto children = std::move(ptr->children);
        for (auto&& child : children)
            erase(data, child->index);
        
        auto& pchildren = ptr->parent->children;
        auto k = 0u;
        for (; k < pchildren.size(); ++k)
        {
            if (pchildren[k] == ptr.get())
                break;
        }
        for (auto n = k + 1; n < pchildren.size(); ++n)
            pchildren[n - 1] = pchildren[n];
        if (k < pchildren.size())
            pchildren.pop_back();
        return true;
    }
    return false;
}
gui_window_data_usr::~gui_window_data_usr() noexcept
{
    while (auto event = static_cast<gui_window_event_type*>(m_events.pop()))
        gui_window_event_type::clear(event);
    if (auto system = shared_ptr<gui_system>(m_system))
        static_cast<gui_system_data*>(system.get())->wake();
}
void gui_window_data_usr::notify(gui_window_event_type*& event) const noexcept
{
    if (auto system = shared_ptr<gui_system>(m_system))
    {
        m_events.push(event);
        static_cast<gui_system_data*>(system.get())->wake();
    }
    event = nullptr;
}
error_type gui_window_data_usr::update() noexcept
{
    return error_type();
}
gui_widget gui_window_data_usr::parent(const gui_widget widget) const noexcept
{
    auto it = m_data.find(widget.index);
    if (it != m_data.end() && it->value->parent)
    {
        return { it->value->parent->index };
    }
    return gui_widget();
}
gui_widget gui_window_data_usr::child(const gui_widget parent, const size_t offset) const noexcept
{
    auto it = m_data.find(parent.index);
    if (it != m_data.end())
    {
        auto& children = it->value->children;
        if (offset < children.size())
            return { children[offset]->index };
    }
    return gui_widget();
}
size_t gui_window_data_usr::offset(const gui_widget widget) const noexcept
{
    auto it = m_data.find(widget.index);
    if (it != m_data.end() && it->value->parent)
    {
        const auto& children = it->value->parent->children;
        for (auto k = 0u; k < children.size(); ++k)
        {
            if (children[k]->index == widget.index)
            {
                return k;
            }
        }
    }
    return SIZE_MAX;
}
gui_variant gui_window_data_usr::query(const gui_widget widget, const string_view prop) const noexcept
{
    auto it = m_data.find(widget.index);
    if (it != m_data.end())
    {
        if (const auto prop_attr = gui_str_to_attr(prop))
        {
            if (auto ptr = it->value->attr.try_find(prop_attr))
                return *ptr;
        }
    }
    return gui_variant();
}
gui_widget_type gui_window_data_usr::type(const gui_widget widget) const noexcept
{
    auto it = m_data.find(widget.index);
    if (it != m_data.end())
        return it->value->type;
    return gui_widget_type::none;
}
gui_widget_layout gui_window_data_usr::layout(const gui_widget widget) const noexcept
{
    auto it = m_data.find(widget.index);
    if (it != m_data.end())
        return it->value->layout;
    return gui_widget_layout::none;
}
gui_widget_state gui_window_data_usr::state(const gui_widget widget) const noexcept
{
    auto it = m_data.find(widget.index);
    if (it != m_data.end())
        return gui_widget_state(it->value->state);
    return gui_widget_state::none;
}
error_type gui_window_data_usr::modify(const gui_widget widget, const string_view prop, const gui_variant& value) noexcept
{
    gui_widget_data::modify_args args;
    args.index = widget.index;
    ICY_ERROR(copy(prop, args.key));
    args.val = value;
    if (args.val.type() != value.type())
        return make_stdlib_error(std::errc::not_enough_memory);
    
    gui_window_event_type* new_event = nullptr;
    ICY_ERROR(gui_window_event_type::make(new_event, gui_window_event_type::modify));
    ICY_SCOPE_EXIT{ gui_window_event_type::clear(new_event); };
    ICY_ERROR(gui_widget_data::modify(m_data, args));
    new_event->index = widget.index;
    new_event->val = std::move(args);
    notify(new_event);
    return error_type();
}
error_type gui_window_data_usr::insert(const gui_widget parent, const size_t offset, const gui_widget_type type, gui_widget_layout layout, gui_widget& widget) noexcept
{
    gui_widget_data::insert_args args;
    args.parent = parent.index;
    args.index = m_index;
    args.offset = uint32_t(offset);
    args.type = type;
    args.layout = layout;
    
    gui_window_event_type* new_event = nullptr;
    ICY_ERROR(gui_window_event_type::make(new_event, gui_window_event_type::create));
    ICY_SCOPE_EXIT{ gui_window_event_type::clear(new_event); };
    ICY_ERROR(gui_widget_data::insert(m_data, args));
    new_event->index = widget.index = m_index++;
    new_event->val = std::move(args);
    notify(new_event);
    return error_type();
}
error_type gui_window_data_usr::destroy(const gui_widget widget) noexcept
{
    gui_window_event_type* new_event = nullptr;
    ICY_ERROR(gui_window_event_type::make(new_event, gui_window_event_type::destroy));
    new_event->index = widget.index;
    if (gui_widget_data::erase(m_data, widget.index))
        notify(new_event);
    return error_type();
}
error_type gui_window_data_usr::find(const icy::string_view prop, const icy::gui_variant value, icy::array<icy::gui_widget>& list) const noexcept
{
    list.clear();
    const auto prop_key = gui_str_to_attr(prop);
    if (prop_key == gui_widget_attr::none)
        return make_stdlib_error(std::errc::invalid_argument);

    string val_str;
    ICY_ERROR(to_string(value, val_str));
    
    for (auto&& pair : m_data)
    {
        const auto ptr = pair.value->attr.try_find(prop_key);
        if (ptr)
        {
            string str;
            ICY_ERROR(to_string(*ptr, str));

            if (ptr->type() == gui_variant_type<string_view>())
            {
                auto jt = str.find(val_str);
                if (jt != str.end())
                    ICY_ERROR(list.push_back(gui_widget{ pair.key }));
            }
            else if (str == val_str)
            {
                ICY_ERROR(list.push_back(gui_widget{ pair.key }));                
            }
        }
    }
    return error_type();
}
error_type gui_window_data_usr::render(const gui_widget widget, uint32_t& query) const noexcept
{
    auto system = shared_ptr<gui_system>(m_system);
    if (system && m_data.try_find(widget.index))
    {
        gui_window_event_type* new_event = nullptr;
        ICY_ERROR(gui_window_event_type::make(new_event, gui_window_event_type::render));
        new_event->index = widget.index;
        new_event->val = query = static_cast<gui_system_data*>(system.get())->next_query();
        notify(new_event);
    }
    return error_type();
}
error_type gui_window_data_usr::resize(const window_size size) noexcept
{
	if (auto system = shared_ptr<gui_system>(m_system))
	{
		gui_window_event_type* new_event = nullptr;
		ICY_ERROR(gui_window_event_type::make(new_event, gui_window_event_type::resize));
		new_event->val = size;
		notify(new_event);
	}
	return error_type();
}

error_type gui_window_data_sys::initialize(const shared_ptr<icy::window> window, const json& json) noexcept
{
    m_window = window;
    m_window_index = window->index();

    m_dpi = 1.0f * window->dpi();
	
    ICY_ERROR(m_system->render().create_font(m_font, window, gui_system_font_type_default));

    ICY_ERROR(make_unique(jmp_type(), m_jmp));

    unique_ptr<gui_widget_data> root;
    ICY_ERROR(make_unique(gui_widget_data(), root));
	//root->bkcolor = colors::black;
    ICY_ERROR(m_data.insert(0u, std::move(root)));
    ICY_ERROR(resize(window->size()));
    ICY_ERROR(gui_widget_data::initialize(m_data, 0u, json));
    m_timer_scroll = make_unique<icy::timer>();
    if (!m_timer_scroll)
        return make_stdlib_error(std::errc::not_enough_memory);

    return error_type();
}
error_type gui_window_data_sys::input(const input_message& msg, array<gui_widget>& output) noexcept
{
    ICY_ERROR(update());
    switch (msg.type)
    {
    case input_type::mouse_move:
        ICY_ERROR(input_mouse_move(msg.point_x, msg.point_y, key_mod(msg.mods), output));
        break;
    case input_type::mouse_press:
        ICY_ERROR(input_mouse_press(msg.key, msg.point_x, msg.point_y, key_mod(msg.mods), output));
        break;
    case input_type::mouse_release:
        ICY_ERROR(input_mouse_release(msg.key, msg.point_x, msg.point_y, key_mod(msg.mods), output));
        break;
    case input_type::mouse_wheel:
        ICY_ERROR(input_mouse_wheel(msg.point_x, msg.point_y, key_mod(msg.mods), msg.wheel, output));
        break;
    case input_type::mouse_double:
        ICY_ERROR(input_mouse_double(msg.key, msg.point_x, msg.point_y, key_mod(msg.mods), output));
        break;
    case input_type::mouse_hold:
        ICY_ERROR(input_mouse_hold(msg.key, msg.point_x, msg.point_y, key_mod(msg.mods), output));
        break;
    case input_type::key_press:
        ICY_ERROR(input_key_press(msg.key, key_mod(msg.mods), output));
        break;
    case input_type::key_release:
        ICY_ERROR(input_key_release(msg.key, key_mod(msg.mods), output));
        break;
    case input_type::key_hold:
        ICY_ERROR(input_key_hold(msg.key, key_mod(msg.mods), output));
        break;
    case input_type::text:
        ICY_ERROR(input_text(string_view(msg.text, strnlen(msg.text, input_message::u8_max)), output));
        break;
    default:
        break;
    }

    return error_type();
}
error_type gui_window_data_sys::resize(const window_size size) noexcept
{
    auto& root = *m_data.front()->value.get();
    m_size = size;
    ICY_ERROR(reset());
    return error_type();
}
error_type gui_window_data_sys::update() noexcept
{
    if (m_state & gui_window_state_updated)
        return error_type();
    
    const auto alloc = [](void* user, void* ptr, size_t nsize, size_t)
    {
        ptr = global_realloc(ptr, nsize, user);
        if (nsize && !ptr)
            longjmp(static_cast<jmp_type*>(user)->buf, 1);
        return ptr;
    };
    if (_setjmp(m_jmp->buf))
        return make_stdlib_error(std::errc::not_enough_memory);

    m_solver = solver_type();
    m_solver.system = am_newsolver(alloc, m_jmp.get());
    if (!m_solver.system)
        return make_stdlib_error(std::errc::not_enough_memory);

    am_autoupdate(m_solver, 1);

    auto& root = *m_data.front().value.get();
    root.min_x = am_newvariable(m_solver);
    root.min_y = am_newvariable(m_solver);
    root.max_x = am_newvariable(m_solver);
    root.max_y = am_newvariable(m_solver);

    for (auto&& pair : m_data)
    {
        auto& widget = *pair.value;
        widget.size_x = 0;
        widget.size_y = 0;
        for (auto&& item : widget.items)
        {
            ICY_ERROR(solve_item(widget, item, m_font));
            switch (item.type)
            {
            case gui_widget_item_type::text:
            {
                widget.size_x = std::max(widget.size_x, item.x + item.w);
                widget.size_y = std::max(widget.size_y, item.y + item.h);
                break;
            }
            }
        }
    }
    am_suggest(root.min_x, 0, AM_REQUIRED);
    am_suggest(root.min_y, 0, AM_REQUIRED);
    am_suggest(root.max_x, root.size_x = float(m_size.x), AM_REQUIRED);
    am_suggest(root.max_y, root.size_y = float(m_size.y), AM_REQUIRED);
    
    ICY_ERROR(update(root));
    m_state |= gui_window_state_updated;

    //if (m_focus && !m_data.try_find(m_focus.widget)) m_focus = focus_type();
    //if (m_press && !m_data.try_find(m_press.widget)) m_press = press_type();
    //if (m_hover && !m_data.try_find(m_hover.widget)) m_hover = hover_type();

    return error_type();
}
error_type gui_window_data_sys::update(gui_widget_data& widget) noexcept
{
    auto count = 0u;
    auto wx = 0.0f;
    auto wy = 0.0f;
    for (auto&& ptr : widget.children)
    {
        if (!(ptr->state & gui_widget_state::visible))
            continue;

        ptr->min_x = am_newvariable(m_solver);
        ptr->min_y = am_newvariable(m_solver);
        ptr->max_x = am_newvariable(m_solver);
        ptr->max_y = am_newvariable(m_solver);


        ptr->weight_x = ptr->size_x;
        ptr->weight_x += ptr->margins[0].size;
        ptr->weight_x += ptr->borders[0].size;
        ptr->weight_x += ptr->padding[0].size;
        ptr->weight_x += ptr->margins[2].size;
        ptr->weight_x += ptr->borders[2].size;
        ptr->weight_x += ptr->padding[2].size;


        ptr->weight_y = ptr->size_y;
        ptr->weight_y += ptr->margins[1].size;
        ptr->weight_y += ptr->borders[1].size;
        ptr->weight_y += ptr->padding[1].size;
        ptr->weight_y += ptr->margins[3].size;
        ptr->weight_y += ptr->borders[3].size;
        ptr->weight_y += ptr->padding[3].size;

        wx += ptr->weight_x;
        wy += ptr->weight_y;

        ++count;
    }
    for (auto&& ptr : widget.children)
    {
        if (!(ptr->state & gui_widget_state::visible))
            continue;
    
        ptr->weight_x /= wx;
        ptr->weight_y /= wy;
    }

    wx = 0;
    wy = 0;
    for (auto&& ptr : widget.children)
    {
        if (!(ptr->state & gui_widget_state::visible))
            continue;

        const auto attr_wx = ptr->attr.try_find(gui_widget_attr::weight_x);
        const auto attr_wy = ptr->attr.try_find(gui_widget_attr::weight_y);
        const auto attr_w = ptr->attr.try_find(gui_widget_attr::weight);
        
        auto new_wx = ptr->weight_x;
        auto new_wy = ptr->weight_y;

        if (attr_wx)
            to_value(*attr_wx, new_wx);
        else if (attr_w)
            to_value(*attr_w, new_wx);

        if (attr_wy)
            to_value(*attr_wy, new_wy);
        else if (attr_w)
            to_value(*attr_w, new_wy);

        ptr->weight_x = new_wx;
        ptr->weight_y = new_wy;
        wx += new_wx;
        wy += new_wy;
    }
    for (auto&& ptr : widget.children)
    {
        if (!(ptr->state & gui_widget_state::visible))
            continue;

        ptr->weight_x /= wx;
        ptr->weight_y /= wy;
    }




    const auto size_x = am_value(widget.max_x) - am_value(widget.min_x);
    const auto size_y = am_value(widget.max_y) - am_value(widget.min_y);
    
    if (widget.layout == gui_widget_layout::vbox)
    {
        gui_widget_data* prev = nullptr;

        auto n = 0u;
        for (auto&& ptr : widget.children)
        {
            if (!(ptr->state & gui_widget_state::visible))
                continue;

            ++n;
            {
                auto min_x = am_newconstraint(m_solver, AM_STRONG);
                am_addterm(min_x, ptr->min_x, 1.0);
                am_setrelation(min_x, AM_EQUAL);
                am_addterm(min_x, widget.min_x, 1.0);
                am_addconstant(min_x, ptr->margins[0].size);
                am_addconstant(min_x, ptr->borders[0].size);
                am_addconstant(min_x, ptr->padding[0].size);
                am_add(min_x);
            }
            {
                auto max_x = am_newconstraint(m_solver, AM_STRONG);
                am_addterm(max_x, ptr->max_x, 1.0);
                am_setrelation(max_x, AM_EQUAL);
                am_addterm(max_x, widget.max_x, 1.0);
                am_addconstant(max_x, -ptr->margins[2].size);
                am_addconstant(max_x, -ptr->borders[2].size);
                am_addconstant(max_x, -ptr->padding[2].size);
                am_add(max_x);
            }
            {
                auto min_y = am_newconstraint(m_solver, AM_STRONG);
                am_addterm(min_y, ptr->min_y, 1.0);
                am_setrelation(min_y, AM_EQUAL);
                am_addterm(min_y, prev ? prev->max_y : widget.min_y, 1.0);
                if (prev)
                {
                    am_addconstant(min_y, prev->margins[3].size);
                    am_addconstant(min_y, prev->borders[3].size);
                    am_addconstant(min_y, prev->padding[3].size);
                }
                am_addconstant(min_y, ptr->margins[1].size);
                am_addconstant(min_y, ptr->borders[1].size);
                am_addconstant(min_y, ptr->padding[1].size);
                am_add(min_y);
            }
            if (n == count)
            {
                auto max_y = am_newconstraint(m_solver, AM_STRONG);
                am_addterm(max_y, ptr->max_y, 1.0);
                am_setrelation(max_y, AM_EQUAL);
                am_addterm(max_y, widget.max_y, 1.0);
                am_addconstant(max_y, -ptr->margins[3].size);
                am_addconstant(max_y, -ptr->borders[3].size);
                am_addconstant(max_y, -ptr->padding[3].size);
                am_add(max_y);
            }
            else
            {
                auto max_y = am_newconstraint(m_solver, AM_STRONG);
                am_addterm(max_y, ptr->max_y, 1.0);
                am_setrelation(max_y, AM_EQUAL);
                am_addterm(max_y, ptr->min_y, 1.0);
                am_addconstant(max_y, size_y * ptr->weight_y);
                am_add(max_y);
            }
            prev = ptr;
        }
    }
    else if (widget.layout == gui_widget_layout::hbox)
    {
        gui_widget_data* prev = nullptr;
        auto n = 0u;
        for (auto&& ptr : widget.children)
        {
            if (!(ptr->state & gui_widget_state::visible))
                continue;
            ++n;
            {
                auto min_y = am_newconstraint(m_solver, AM_STRONG);
                am_addterm(min_y, ptr->min_y, 1.0);
                am_setrelation(min_y, AM_EQUAL);
                am_addterm(min_y, widget.min_y, 1.0);
                am_addconstant(min_y, ptr->margins[1].size);
                am_addconstant(min_y, ptr->borders[1].size);
                am_addconstant(min_y, ptr->padding[1].size);
                am_add(min_y);
            }
            {
                auto max_y = am_newconstraint(m_solver, AM_STRONG);
                am_addterm(max_y, ptr->max_y, 1.0);
                am_setrelation(max_y, AM_EQUAL);
                am_addterm(max_y, widget.max_y, 1.0);
                am_addconstant(max_y, -ptr->margins[3].size);
                am_addconstant(max_y, -ptr->borders[3].size);
                am_addconstant(max_y, -ptr->padding[3].size);
                am_add(max_y);
            }
            {
                auto min_x = am_newconstraint(m_solver, AM_STRONG);
                am_addterm(min_x, ptr->min_x, 1.0);
                am_setrelation(min_x, AM_EQUAL);
                am_addterm(min_x, prev ? prev->max_x : widget.min_x, 1.0);
                if (prev)
                {
                    am_addconstant(min_x, prev->margins[2].size);
                    am_addconstant(min_x, prev->borders[2].size);
                    am_addconstant(min_x, prev->padding[2].size);
                }
                am_addconstant(min_x, ptr->margins[0].size);
                am_addconstant(min_x, ptr->borders[0].size);
                am_addconstant(min_x, ptr->padding[0].size);
                am_add(min_x);
            }
            if (n == count)
            {
                auto max_x = am_newconstraint(m_solver, AM_STRONG);
                am_addterm(max_x, ptr->max_x, 1.0);
                am_setrelation(max_x, AM_EQUAL);
                am_addterm(max_x, widget.max_y, 1.0);
                am_addconstant(max_x, -ptr->margins[2].size);
                am_addconstant(max_x, -ptr->borders[2].size);
                am_addconstant(max_x, -ptr->padding[2].size);
                am_add(max_x);
            }
            else
            {
                auto max_x = am_newconstraint(m_solver, AM_STRONG);
                am_addterm(max_x, ptr->max_x, 1.0);
                am_setrelation(max_x, AM_EQUAL);
                am_addterm(max_x, ptr->min_x, 1.0);
                am_addconstant(max_x, size_x * ptr->weight_x);
                am_add(max_x);
            }
            prev = ptr;
        }
    }

    for (auto&& child : widget.children)
    {
        if (!(child->state & gui_widget_state::visible))
            continue;

        ICY_ERROR(update(*child));

        auto min_x = am_value(child->min_x);
        auto min_y = am_value(child->min_y);
        auto max_x = am_value(child->max_x);
        auto max_y = am_value(child->max_y);

        auto hscroll = 0;
        auto vscroll = 0;

        auto& sx = child->scroll_x.view_size = max_x - min_x;
        auto& sy = child->scroll_y.view_size = max_y - min_y;

        const auto calc_hscroll = [&]
        {
            if (child->state & gui_widget_state::hscroll)
            {
                hscroll = 1;
            }
            else if (child->state & gui_widget_state::hscroll_auto)
            {
                for (auto&& item : child->items)
                {
                    if (item.x + item.w > sx)
                    {
                        hscroll = 1;
                        break;
                    }
                }
            }
        };
        const auto calc_vscroll = [&]
        {
            if (child->state & gui_widget_state::vscroll)
            {
                vscroll = 1;
            }
            else if (child->state & gui_widget_state::vscroll_auto)
            {
                for (auto&& item : child->items)
                {
                    if (item.y + item.h > sy)
                    {
                        vscroll = 1;
                        break;
                    }
                }
            }
        };

        calc_hscroll();
        calc_vscroll();
        if (hscroll) sy -= m_system->hscroll().size;
        if (vscroll) sx -= m_system->vscroll().size;

        if (!hscroll && vscroll) calc_hscroll();
        if (!vscroll && hscroll) calc_vscroll();

        if (child->state & gui_widget_state::hscroll_inv) hscroll *= -1;
        if (child->state & gui_widget_state::vscroll_inv) vscroll *= -1;

        sx = max_x - min_x;
        sy = max_y - min_y;
        if (hscroll) sy -= m_system->hscroll().size;
        if (vscroll) sx -= m_system->vscroll().size;

        using scroll_items_type = std::pair<uint32_t, gui_widget_item_type>[4];

        scroll_items_type vscroll_items =
        {
            std::make_pair(0u, gui_widget_item_type::vscroll_bk),
            std::make_pair(0u, gui_widget_item_type::vscroll_max),
            std::make_pair(0u, gui_widget_item_type::vscroll_min),
            std::make_pair(0u, gui_widget_item_type::vscroll_val),
        };
        scroll_items_type hscroll_items =
        {
            std::make_pair(0u, gui_widget_item_type::hscroll_bk),
            std::make_pair(0u, gui_widget_item_type::hscroll_max),
            std::make_pair(0u, gui_widget_item_type::hscroll_min),
            std::make_pair(0u, gui_widget_item_type::hscroll_val),
        };
        
        for (auto&& item : child->items)
        {
            const auto index = [&] { return 1u + uint32_t(std::distance(child->items.data(), &item)); };
            switch (item.type)
            {
            case gui_widget_item_type::text:
            {
                item.state &= ~gui_widget_item_state::visible;
                child->scroll_x.max = std::max(child->scroll_x.max, item.x + item.w);
                child->scroll_y.max = std::max(child->scroll_y.max, item.y + item.h);
                break;
            }

            case gui_widget_item_type::vscroll_bk: vscroll_items[0].first = index(); break;
            case gui_widget_item_type::vscroll_max: vscroll_items[1].first = index(); break;
            case gui_widget_item_type::vscroll_min: vscroll_items[2].first = index(); break;
            case gui_widget_item_type::vscroll_val: vscroll_items[3].first = index(); break;
            case gui_widget_item_type::hscroll_bk: hscroll_items[0].first = index(); break;
            case gui_widget_item_type::hscroll_max: hscroll_items[1].first = index(); break;
            case gui_widget_item_type::hscroll_min: hscroll_items[2].first = index(); break;
            case gui_widget_item_type::hscroll_val: hscroll_items[3].first = index(); break;
            }

        }

        const auto func = [&](const bool is_vscroll)
        {
            const auto scroll = is_vscroll ? vscroll : hscroll;
            const auto& sys_scroll = is_vscroll ? m_system->vscroll() : m_system->hscroll();
            auto& items = is_vscroll ? vscroll_items : hscroll_items;
            if (scroll)
            {
                for (auto&& pair : items)
                {
                    if (!pair.first)
                    {
                        ICY_ERROR(child->items.push_back(pair.second));
                        pair.first = uint32_t(child->items.size());
                    }
                    child->items[pair.first - 1].state |= gui_widget_item_state::visible;
                }
                auto& bk = child->items[items[0].first - 1];
                auto& max = child->items[items[1].first - 1];
                auto& min = child->items[items[2].first - 1];
                auto& val = child->items[items[3].first - 1];

                bk.image = sys_scroll.background;

                if (m_press.widget == child->index && m_press.item == items[1].first)
                    max.image = sys_scroll.max_focused;
                else if (m_hover.widget == child->index && m_hover.item == items[1].first && !m_press)
                    max.image = sys_scroll.max_hovered;
                else
                    max.image = sys_scroll.max_default;

                if (m_press.widget == child->index && m_press.item == items[2].first)
                    min.image = sys_scroll.min_focused;
                else if (m_hover.widget == child->index && m_hover.item == items[2].first && !m_press)
                    min.image = sys_scroll.min_hovered;
                else
                    min.image = sys_scroll.min_default;

                if (m_press.widget == child->index && m_press.item == items[3].first)
                    val.image = sys_scroll.val_focused;
                else if (m_hover.widget == child->index && m_hover.item == items[3].first && !m_press)
                    val.image = sys_scroll.val_hovered;
                else
                    val.image = sys_scroll.val_default;

                if (is_vscroll)
                {
                    for (auto&& pair : items)
                    {
                        auto& item = child->items[pair.first - 1];
                        item.x = sx;
                        item.w = sys_scroll.size;
                        item.y = 0;
                        item.h = 0;                    
                    }
                    min.h = float(min.image.size().y);
                    max.h = float(max.image.size().y);
                    max.y = sy - max.h;
                    const auto canvas = child->scroll_y.area_size = max.y - min.h;

                    val.h = std::min(canvas, std::max(sys_scroll.size, canvas * sy / std::max(sy, child->scroll_y.max)));
                    val.y = min.h;

                    child->scroll_y.clamp();
                    
                    if (child->scroll_y.max > sy)
                        val.y += (canvas - val.h) * child->scroll_y.val / (child->scroll_y.max - sy);
                    
                    bk.y = min.h;
                    bk.h = canvas;
                }
                else
                {
                    for (auto&& pair : items)
                    {
                        auto& item = child->items[pair.first - 1];
                        item.y = sy;
                        item.h = sys_scroll.size;
                        item.x = 0;
                        item.w = 0;
                    }
                    min.w = float(min.image.size().x);
                    max.w = float(max.image.size().x);
                    max.x = sx - max.w;
                    const auto canvas = child->scroll_x.area_size = max.x - min.w;

                    val.w = std::min(canvas, std::max(sys_scroll.size, canvas * sx / std::max(sx, child->scroll_x.max)));
                    val.x = min.w;
                    
                    child->scroll_x.clamp();

                    if (child->scroll_x.max > sx)
                        val.x += (canvas - val.w) * child->scroll_x.val / (child->scroll_x.max - sx);

                    bk.x = min.w;
                    bk.w = canvas;
                }
            }
            else
            {
                for (auto&& pair : items)
                {
                    if (pair.first) child->items[pair.first - 1].state &= gui_widget_item_state::visible;
                }
            }
            return error_type();
        };
        ICY_ERROR(func(1));
        ICY_ERROR(func(0));

        child->state &= ~gui_widget_state::has_hscroll;
        child->state &= ~gui_widget_state::has_vscroll;
        if (hscroll) child->state |= gui_widget_state::has_hscroll;
        if (vscroll) child->state |= gui_widget_state::has_vscroll;

        for (auto&& item : child->items)
        {
            auto item_min_x = item.x;
            auto item_min_y = item.y;
            if (hscroll) item_min_x -= child->scroll_x.val;
            if (vscroll) item_min_y -= child->scroll_y.val;

            const auto item_max_x = item_min_x + item.w;
            const auto item_max_y = item_min_y + item.h;

            if (item_max_y < 0.0f || item_max_x < 0.0f || item_min_x >= sx || item_min_y >= sy)
                continue;

            item.state |= gui_widget_item_state::visible;
            /*
            if (item.type == gui_widget_item_type::text && item.text)
            {
                ICY_COM_ERROR(item.text->SetMaxWidth(sx - item.x));
                ICY_COM_ERROR(item.text->SetMaxHeight(sy - item.y));
            }*/
        }


    }

    return error_type();
}
error_type gui_window_data_sys::input_key_press(const key key, const key_mod mods, array<gui_widget>& output) noexcept
{
    return error_type();
}
error_type gui_window_data_sys::input_key_hold(const key key, const key_mod mods, array<gui_widget>& output) noexcept
{

    return error_type();
}
error_type gui_window_data_sys::input_key_release(const key key, const key_mod mods, array<gui_widget>& output) noexcept
{

    return error_type();
}
error_type gui_window_data_sys::input_mouse_move(const int32_t px, const int32_t py, const key_mod mods, array<gui_widget>& output) noexcept
{
    const auto& root = *m_data.front().value;

    hover_type new_hover;
    ICY_ERROR(hover_widget(root, px, py, new_hover));
    if (new_hover)
    {
        auto it = m_data.find(new_hover.widget);
        if (it != m_data.end())
            ICY_ERROR(hover_item(*it->value, px, py, new_hover));
    }

    if (m_focus)
    {
        
    }
    if (m_press)
    {
        auto it = m_data.find(m_press.widget);
        if (it == m_data.end())
        {
            m_press = press_type();
        }
        else if (m_press.item)
        {
            auto& widget = *it->value.get();
            auto& item = widget.items[m_press.item - 1];
            if (item.type == gui_widget_item_type::vscroll_val)
            {
                auto flex_scroll = false;
                if (const auto ptr = widget.attr.try_find(gui_widget_attr::flex_scroll))
                    to_value(*ptr, flex_scroll);
                
                const auto local = py - (m_press.dy + m_system->vscroll().min_default.size().y + am_value(widget.min_y));
                const auto num = (widget.scroll_y.max - widget.scroll_y.view_size) * local;
                const auto den = (widget.scroll_y.area_size - item.h);
                
                if (den <= 0 || num <= 0)
                {
                    (flex_scroll ? widget.scroll_y.exp : widget.scroll_y.val) = 0;
                }
                else
                {
                    (flex_scroll ? widget.scroll_y.exp : widget.scroll_y.val) = num / den;
                    widget.scroll_y.clamp();
                }
                if (flex_scroll)
                {
                    m_timer_point = clock_type::now();
                    ICY_ERROR(m_timer_scroll->initialize(SIZE_MAX, std::chrono::milliseconds(10)));
                }
                ICY_ERROR(reset());
            }
            

        }
    }

    if (new_hover.widget != m_hover.widget || new_hover.item != m_hover.item)
    {
        ICY_ERROR(reset());
        m_hover = new_hover;
    }

    return error_type();
}
error_type gui_window_data_sys::input_mouse_wheel(const int32_t px, const int32_t py, const key_mod mods, const int32_t wheel, array<gui_widget>& output) noexcept
{

    return error_type();
}
error_type gui_window_data_sys::input_mouse_release(const key key, const int32_t px, const int32_t py, const key_mod mods, array<gui_widget>& output) noexcept
{
    if (key == key::mouse_left)
    {
        if (m_press)
        {
            m_timer_scroll->cancel();
            static_cast<state_type&>(m_focus) = m_press;
            m_press = press_type();
            ICY_ERROR(reset());
        }
    }
    return error_type();
}
error_type gui_window_data_sys::input_mouse_press(const key key, const int32_t, const int32_t, const key_mod mods, array<gui_widget>& output) noexcept
{
    if (key == key::mouse_left)
    {
        if (m_hover)
        {
            static_cast<state_type&>(m_press) = m_hover;
            ICY_ERROR(reset());
        }
        if (m_focus)
        {

        }        
    }
    return error_type();
}
error_type gui_window_data_sys::input_mouse_hold(const key key, const int32_t px, const int32_t py, const key_mod mods, array<gui_widget>& output) noexcept
{

    return error_type();
}
error_type gui_window_data_sys::input_mouse_double(const key key, const int32_t px, const int32_t py, const key_mod mods, array<gui_widget>& output) noexcept
{

    return error_type();
}
error_type gui_window_data_sys::input_text(const string_view text, array<gui_widget>& output) noexcept
{

    return error_type();
}
error_type gui_window_data_sys::solve_item(gui_widget_data& widget, gui_widget_item& item, const gui_font& font) noexcept
{
    if (item.type == gui_widget_item_type::text)
    {
        if (item.text)
            return error_type();

        const auto& render = m_system->render();
        string str;
        if (const auto error = to_string(item.value, str))
        {
            if (error == make_stdlib_error(std::errc::invalid_argument))
                return error_type();
            return error;
        }
        ICY_ERROR(render.create_text(item.text, font, str));
        DWRITE_TEXT_METRICS metrics;
        ICY_COM_ERROR(item.text->GetMetrics(&metrics));
        item.w = metrics.widthIncludingTrailingWhitespace;
        item.h = metrics.height;

        if (widget.scroll_y.step == 0)
            widget.scroll_y.step = metrics.height / metrics.lineCount;
    }
    else
    {
        item.w = 0;
        item.h = 0;
    }
    return error_type();
}
error_type gui_window_data_sys::make_view(const gui_data_model& proxy, const gui_node_data& node, const gui_data_bind& func, gui_widget_data& widget) noexcept
{
    array<const gui_node_data*> nodes;
    for (auto&& child : node.children)
    {
        if (child->value.col != 0)
            continue;

        if (!(child->key->state & gui_node_state::visible))
            continue;

        if (!func.filter(proxy, gui_node{ child->key->index }))
            continue;

        ICY_ERROR(nodes.push_back(child.key));
    }
    const auto pred = [&proxy, &func](const gui_node_data*& lhs, const gui_node_data*& rhs)
    {
        return func.compare(proxy, gui_node{ lhs->index }, gui_node{ rhs->index }) < 0;
    };
    std::sort(nodes.begin(), nodes.end(), pred);

    auto offset = 10.0f;    

    /*if (const auto attr_offset = widget.attr.try_find("offset"_s))
    {

    }*/

    for (auto&& child : nodes)
    {
        gui_widget_item new_item;
        new_item.type = gui_widget_item_type::text;
        new_item.value = child->data;
        ICY_ERROR(solve_item(widget, new_item, m_font));
        if (new_item.text)
        {
            new_item.x = widget.size_x;
            new_item.y = widget.size_y;
            
            ICY_ERROR(widget.items.push_back(std::move(new_item)));
            widget.size_y += new_item.h;
            if (widget.type == gui_widget_type::view_tree)
            {
                widget.size_x += offset;
                ICY_ERROR(make_view(proxy, *child, func, widget));
                widget.size_x -= offset;
            }
        }
    }
    return error_type();
}
error_type gui_window_data_sys::hover_widget(const gui_widget_data& widget, const int32_t px, const int32_t py, hover_type& new_hover) const noexcept
{
    for (auto&& child : widget.children)
    {
        if (!(child->state & (gui_widget_state::visible | gui_widget_state::enabled)))
            continue;

        const auto min_x = lround(am_value(child->min_x));
        const auto min_y = lround(am_value(child->min_y));
        const auto max_x = lround(am_value(child->max_x));
        const auto max_y = lround(am_value(child->max_y));
        if (px < min_x || py < min_y || px >= max_x || py >= max_y)
            continue;

        new_hover.widget = child->index;
        ICY_ERROR(hover_widget(*child, px, py, new_hover));
        if (new_hover)
            break;
    }
    return error_type();
}
error_type gui_window_data_sys::hover_item(const gui_widget_data& widget, const int32_t px, const int32_t py, hover_type& new_hover) const noexcept
{
    new_hover.item = 0;
    for (auto&& item : widget.items)
    {
        if (!(item.state & (gui_widget_item_state::enabled | gui_widget_item_state::visible)))
            continue;

        auto item_x = item.x;
        auto item_y = item.y;
        switch (item.type)
        {
        case gui_widget_item_type::text:
        {
            if (widget.state & gui_widget_state::has_hscroll) item_x -= widget.scroll_x.val;
            if (widget.state & gui_widget_state::has_vscroll) item_y -= widget.scroll_y.val;
            break;
        }
        }
        const auto min_x = am_value(widget.min_x) + item_x;
        const auto min_y = am_value(widget.min_y) + item_y;
        const auto max_x = min_x + item.w;
        const auto max_y = min_y + item.h;

        if (px < min_x || px >= max_x || py < min_y || py >= max_y)
            continue;

        new_hover.item = uint32_t(std::distance(widget.items.data(), &item) + 1);
        new_hover.dx = uint32_t(px - min_x);
        new_hover.dy = uint32_t(py - min_y);
    }
    return error_type();
}
error_type gui_window_data_sys::process(const gui_window_event_type& event, array<gui_widget>& output) noexcept
{
    switch (event.type)
    {
    case gui_window_event_type::create:
    {
        const auto args = event.val.get<gui_widget_data::insert_args>();
        ICY_ASSERT(args, "");
        ICY_ERROR(gui_widget_data::insert(m_data, *args));
        break;
    }
    case gui_window_event_type::modify:
    {
        const auto args = event.val.get<gui_widget_data::modify_args>();
        ICY_ASSERT(args, "");
        ICY_ERROR(gui_widget_data::modify(m_data, *args));
        break;
    }
    case gui_window_event_type::destroy:
    {
        auto it = m_data.find(event.index);
        gui_widget_data::erase(m_data, event.index);
        break;
    }
    default:
        return error_type();
    }
    ICY_ERROR(reset());
    return error_type();
}
window_size gui_window_data_sys::size(const uint32_t widget) const noexcept
{
	if (widget == 0)
		return m_size;
	
	return window_size();
}    
error_type gui_window_data_sys::send_data(gui_model_data_sys& model, const gui_widget widget, const gui_node node, const gui_data_bind& func, bool& erase) const noexcept 
{
    auto it = m_data.find(widget.index);
    if (it == m_data.end())
    {
        erase = true;
        return error_type();
    }
    ICY_ERROR(model.recv_data(*it->value, node, func, erase));
    return error_type();
}
error_type gui_window_data_sys::recv_data(const gui_data_model& proxy, const gui_node_data& node, const gui_widget index, const gui_data_bind& func, bool& erase) noexcept
{
    auto it = m_data.find(index.index);
    if (it == m_data.end())
    {
        erase = true;
        return error_type();
    }
    auto& widget = *it->value;
    switch (widget.type)
    {
    case gui_widget_type::edit_line:
    case gui_widget_type::edit_text:
    {
        for (auto&& item : widget.items)
        {
            if (item.type == gui_widget_item_type::text)
            {
                item.text = gui_text();
                if ((node.state & gui_node_state::visible) && func.filter(proxy, gui_node{ node.index }))
                {
                    item.value = node.data;
                }
                else
                {
                    item.value = gui_variant();
                }
                break;
            }
        }
        break;
    }
    case gui_widget_type::view_list:
    case gui_widget_type::view_table:
    case gui_widget_type::view_tree:
    {
        if (m_hover.widget == it->key) m_hover.item = 0;
        if (m_press.widget == it->key) m_press.item = 0;
        if (m_focus.widget == it->key) m_focus.item = 0;

        widget.items.clear();
        widget.size_x = 0;
        widget.size_y = 0;
        if ((node.state & gui_node_state::visible) && func.filter(proxy, gui_node{ node.index }))
        {
            ICY_ERROR(make_view(proxy, node, func, widget));
        }
        break;
    }
    default:
        return error_type();
    }
    ICY_ERROR(reset());


    return error_type();
}
error_type gui_window_data_sys::timer(timer::pair& pair) noexcept
{
    if (pair.timer == m_timer_scroll.get())
    {
        if (!m_press)
        {
            pair.timer->cancel();
            return error_type();
        }
        else
        {
            auto it = m_data.find(m_press.widget);
            if (it == m_data.end())
            {
                m_press = press_type();
                pair.timer->cancel();
                return error_type();
            }
            auto& widget = *it->value.get();
            auto done = 0;
            const auto delta_y = widget.scroll_y.exp - widget.scroll_y.val;
            const auto delta_t = 1e-6 * (std::chrono::duration_cast<std::chrono::microseconds>(clock_type::now() - m_timer_point)).count();
            if (fabsf(delta_y) <= widget.scroll_y.step)
            {
                widget.scroll_y.val = widget.scroll_y.exp;
                widget.scroll_y.clamp();
                ++done;
            }
            else
            {                
                widget.scroll_y.val += delta_y * delta_t * 0.5f;
                widget.scroll_y.clamp();
            }

            if (done == 1)
            {
                pair.timer->cancel();
            }
            ICY_ERROR(reset());
        }
    }
    return error_type();
}
error_type gui_window_data_sys::reset() noexcept
{
    ICY_ERROR(m_system->post_update(*this));
    m_state &= ~gui_window_state_updated;
    return error_type();
}
error_type gui_window_data_sys::render(gui_texture& texture) noexcept
{
    const auto& root = *m_data.front().value;
    color bkcolor;
    gui_attr_default(gui_widget_attr::bkcolor).get(bkcolor);
    if (const auto ptr = root.attr.try_find(gui_widget_attr::bkcolor))
        to_value(*ptr, bkcolor);
    
    texture.draw_begin(bkcolor);
    ICY_ERROR(render(texture, root));
    ICY_ERROR(texture.draw_end());
    return error_type();
}
error_type gui_window_data_sys::render(gui_texture& texture, const gui_widget_data& widget) noexcept
{
    for (auto&& child : widget.children)
    {
        if (!(child->state & gui_widget_state::visible))
            continue;

        const auto x0 = am_value(child->min_x);
        const auto y0 = am_value(child->min_y);
        const auto x1 = am_value(child->max_x);
        const auto y1 = am_value(child->max_y);

        color bkcolor;
        if (const auto ptr = child->attr.try_find(gui_widget_attr::bkcolor))
            to_value(*ptr, bkcolor);

        texture.fill_rect({ x0, y0, x1, y1 }, bkcolor);
        
        auto clip = child->state & (gui_widget_state::has_hscroll | gui_widget_state::has_vscroll);
        if (clip)
        {
            auto clip_rect = gui_texture::rect_type{ x0, y0, x1, y1 };
            //if (child->state & gui_widget_state::has_vscroll) clip_rect.max_x -= m_system->vscroll().size;
            //if (child->state & gui_widget_state::has_hscroll) clip_rect.max_y -= m_system->hscroll().size;
            texture.push_clip(clip_rect);
        }
        for (auto&& item : child->items)
        {
            if (!(item.state & gui_widget_item_state::visible))
                continue;

            switch (item.type)
            {
            case gui_widget_item_type::text:
            {
                auto item_x = item.x;
                auto item_y = item.y;
                if (child->state & gui_widget_state::has_hscroll) item_x -= child->scroll_x.val;
                if (child->state & gui_widget_state::has_vscroll) item_y -= child->scroll_y.val;
                ICY_ERROR(texture.draw_text(x0 + item_x, y0 + item_y, item.color, item.text.value));
                break;
            }
            case gui_widget_item_type::vscroll_bk:
            case gui_widget_item_type::vscroll_min:
            case gui_widget_item_type::vscroll_max:
            case gui_widget_item_type::vscroll_val:
            case gui_widget_item_type::hscroll_bk:
            case gui_widget_item_type::hscroll_min:
            case gui_widget_item_type::hscroll_max:
            case gui_widget_item_type::hscroll_val:
            {
                gui_texture::rect_type rect;
                rect.max_x = rect.min_x = x0 + item.x;
                rect.max_y = rect.min_y = y0 + item.y;
                rect.max_x += item.w;
                rect.max_y += item.h;
                texture.draw_image(rect, item.image.value);
                break;
            }

            default:
                break;
            }
        }
        if (clip)
        {
            texture.pop_clip();
        }
        ICY_ERROR(render(texture, *child));
    }
	return error_type();
}

#define AM_IMPLEMENTATION
#include "amoeba.h"
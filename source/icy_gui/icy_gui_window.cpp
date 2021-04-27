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

static error_type gui_widget_init(gui_widget_data& widget) noexcept
{
    switch (widget.type)
    {
    case gui_widget_type::edit_line:
    case gui_widget_type::edit_text:
    case gui_widget_type::view_combo:
    {
        widget.items.clear();
        gui_widget_item new_item;
        new_item.type = gui_widget_item_type::text;
        gui_node_state_set(new_item.state, gui_node_state::enabled);
        gui_node_state_set(new_item.state, gui_node_state::editable);
        ICY_ERROR(widget.items.push_back(std::move(new_item)));
        break;
    }
    }
    return error_type();
}

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
    }
    ICY_ERROR(gui_widget_init(widget));

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
    ICY_ERROR(gui_widget_init(*new_widget));
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
                if (item.type == gui_widget_item_type::text && gui_node_state_isset(item.state, gui_node_state::editable))
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
        if (state != gui_widget_state::none)
        {
            bool value = false;
            if (args.val.get(value))
            {
                if (value)
                    gui_widget_state_set(it->value->state, state);
                else
                    gui_widget_state_unset(it->value->state, state);
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
    ICY_ERROR(m_caret_timer.reset(m_system->caret_blink_time()));

    return error_type();
}
error_type gui_window_data_sys::input(const input_message& msg) noexcept
{
    ICY_ERROR(update());
    switch (msg.type)
    {
    case input_type::mouse_move:
        ICY_ERROR(input_mouse_move(msg.point_x, msg.point_y, key_mod(msg.mods)));
        break;
    case input_type::mouse_press:
        ICY_ERROR(input_mouse_press(msg.key, msg.point_x, msg.point_y, key_mod(msg.mods)));
        break;
    case input_type::mouse_release:
        ICY_ERROR(input_mouse_release(msg.key, msg.point_x, msg.point_y, key_mod(msg.mods)));
        break;
    case input_type::mouse_wheel:
        ICY_ERROR(input_mouse_wheel(msg.point_x, msg.point_y, key_mod(msg.mods), msg.wheel));
        break;
    case input_type::mouse_double:
        ICY_ERROR(input_mouse_double(msg.key, msg.point_x, msg.point_y, key_mod(msg.mods)));
        break;
    //case input_type::mouse_hold:
    //    ICY_ERROR(input_mouse_hold(msg.key, msg.point_x, msg.point_y, key_mod(msg.mods), output));
        break;
    case input_type::key_press:
        ICY_ERROR(input_key_press(msg.key, key_mod(msg.mods)));
        break;
    case input_type::key_release:
        ICY_ERROR(input_key_release(msg.key, key_mod(msg.mods)));
        break;
    case input_type::key_hold:
        ICY_ERROR(input_key_hold(msg.key, key_mod(msg.mods)));
        break;
    case input_type::text:
        ICY_ERROR(input_text(string_view(msg.text, strnlen(msg.text, input_message::u8_max))));
        break;
    default:
        break;
    }

    return error_type();
}
error_type gui_window_data_sys::action(const window_action& msg) noexcept
{
    if (m_focus)
    {
        auto it = m_data.find(m_focus.widget);
        if (it == m_data.end())
        {
            m_focus = focus_type();
            return make_stdlib_error(std::errc::invalid_argument);
        }
        auto& widget = *it->value.get();
        if (widget.type == gui_widget_type::edit_text ||
            widget.type == gui_widget_type::edit_line)
        {
            const auto item = 0u;
            switch (msg.type)
            {
            case window_action_type::undo:
            {
                ICY_ERROR(text_action(widget, item, gui_text_keybind::undo));
                break;
            }
            case window_action_type::redo:
            {
                ICY_ERROR(text_action(widget, item, gui_text_keybind::redo));
                break;
            }
            case window_action_type::cut:
            {
                ICY_ERROR(text_action(widget, item, gui_text_keybind::cut));
                break;
            }
            case window_action_type::copy:
            {
                ICY_ERROR(text_action(widget, item, gui_text_keybind::copy));
                break;
            }
            case window_action_type::paste:
            {
                if (!msg.clipboard_text.empty())
                    ICY_ERROR(text_action(widget, item, gui_text_keybind::paste, msg.clipboard_text));
                break;
            }
            case window_action_type::select_all:
            {
                ICY_ERROR(text_action(widget, item, gui_text_keybind::select_all));
                break;
            }
            }
        }
    }
    return error_type();
}
error_type gui_window_data_sys::resize(const window_size size) noexcept
{
    auto& root = *m_data.front()->value.get();
    m_size = size;
    ICY_ERROR(reset(gui_reset_reason::update_render_list));
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
                if (!item.text && (
                    widget.type == gui_widget_type::edit_line || 
                    widget.type == gui_widget_type::edit_text))
                {
                    item.value = ""_s;
                    ICY_ERROR(solve_item(widget, item, m_font));
                }

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
    
    size_t level = 0;
    ICY_ERROR(update(root, level));
    const auto pred = [](const render_widget& lhs, const render_widget& rhs) 
    {
        return lhs.level < rhs.level;
    };
    std::stable_sort(m_render_list.begin(), m_render_list.end(), pred);
    m_state |= gui_window_state_updated;

    return error_type();
}
error_type gui_window_data_sys::update(gui_widget_data& widget, size_t& level) noexcept
{
    auto space = 0.0f;
    if (widget.type == gui_widget_type::splitter)
    {
        gui_attr_default(gui_widget_attr::splitter_size).get(space);
        widget.items.clear();
    }
    auto count = 0u;
    auto wx = 0.0f;
    auto wy = 0.0f;
    for (auto&& ptr : widget.children)
    {
        ptr->min_x = am_newvariable(m_solver);
        ptr->min_y = am_newvariable(m_solver);
        ptr->max_x = am_newvariable(m_solver);
        ptr->max_y = am_newvariable(m_solver);

        if (!gui_widget_state_isset(ptr->state, gui_widget_state::visible))
            continue;

        ptr->weight_x = ptr->size_x;
        if (const auto width = ptr->attr.try_find(gui_widget_attr::width))
            to_value(*width, ptr->weight_x);
        
        ptr->weight_x += ptr->margins[0].size;
        ptr->weight_x += ptr->borders[0].size;
        ptr->weight_x += ptr->padding[0].size;
        ptr->weight_x += ptr->margins[2].size;
        ptr->weight_x += ptr->borders[2].size;
        ptr->weight_x += ptr->padding[2].size;


        ptr->weight_y = ptr->size_y;
        if (const auto height = ptr->attr.try_find(gui_widget_attr::height))
            to_value(*height, ptr->weight_y);

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
        if (!gui_widget_state_isset(ptr->state, gui_widget_state::visible))
            continue;
    
        if (wx) ptr->weight_x /= wx;
        if (wy) ptr->weight_y /= wy;
    }

    wx = 0;
    wy = 0;
    for (auto&& ptr : widget.children)
    {
        if (!gui_widget_state_isset(ptr->state, gui_widget_state::visible))
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
        if (!gui_widget_state_isset(ptr->state, gui_widget_state::visible))
            continue;

        if (wx) ptr->weight_x /= wx;
        if (wy) ptr->weight_y /= wy;
    }

    const auto size_x = am_value(widget.max_x) - am_value(widget.min_x);
    const auto size_y = am_value(widget.max_y) - am_value(widget.min_y);
    
    const auto level_k = pow(2.0, -double(level));
    const auto dyn_level = am_Num(level_k * 1e3);
    const auto usr_level = am_Num(level_k * 1e5);
    const auto sys_level = am_Num(level_k * 1e7);

    if (widget.type == gui_widget_type::view_tabs)
    {
        auto item_index = 0u;
        for (auto&& item : widget.items)
        {
            if (item.type != gui_widget_item_type::text)
            {
                break;
            }
            if (gui_node_state_isset(item.state, gui_node_state::selected))
            {
                item_index = uint32_t(std::distance(widget.items.data(), &item)) + 1;
                break;
            }
        }
        for (auto&& ptr : widget.children)
            gui_widget_state_unset(ptr->state, gui_widget_state::visible);
        
        if (item_index && item_index <= widget.children.size())
        {
            auto ptr = widget.children[item_index - 1];
            gui_widget_state_set(ptr->state, gui_widget_state::visible);


            const auto min_x = 0
                + am_value(widget.min_x)
                + ptr->margins[0].size
                + ptr->borders[0].size
                + ptr->padding[0].size;

            const auto min_y = 0
                + am_value(widget.min_y)
                + ptr->margins[1].size
                + ptr->borders[1].size
                + ptr->padding[1].size 
                + widget.size_y;

            const auto max_x = 0
                + am_value(widget.max_x)
                - ptr->margins[2].size
                - ptr->borders[2].size
                - ptr->padding[2].size;

            const auto max_y = 0
                + am_value(widget.max_y)
                - ptr->margins[3].size
                - ptr->borders[3].size
                - ptr->padding[3].size;

            am_suggest(ptr->min_x, min_x, sys_level);
            am_suggest(ptr->min_y, min_y, sys_level);
            am_suggest(ptr->max_x, max_x, sys_level);
            am_suggest(ptr->max_y, max_y, sys_level);
        }
    }
    else if (widget.layout == gui_widget_layout::vbox)
    {
        gui_widget_data* prev = nullptr;

        auto n = 0u;
        for (auto&& ptr : widget.children)
        {
            if (!(gui_widget_state_isset(ptr->state, gui_widget_state::visible)))
                continue;

            ++n;
            {
                auto min_x = am_newconstraint(m_solver, sys_level);
                am_addterm(min_x, ptr->min_x, 1.0);
                am_setrelation(min_x, AM_EQUAL);
                am_addterm(min_x, widget.min_x, 1.0);
                am_addconstant(min_x, ptr->margins[0].size);
                am_addconstant(min_x, ptr->borders[0].size);
                am_addconstant(min_x, ptr->padding[0].size);
                am_add(min_x);
            }
            {
                auto max_x = am_newconstraint(m_solver, sys_level);
                am_addterm(max_x, ptr->max_x, 1.0);
                am_setrelation(max_x, AM_EQUAL);
                am_addterm(max_x, widget.max_x, 1.0);
                am_addconstant(max_x, -ptr->margins[2].size);
                am_addconstant(max_x, -ptr->borders[2].size);
                am_addconstant(max_x, -ptr->padding[2].size);
                am_add(max_x);
            }
            {
                auto min_y = am_newconstraint(m_solver, sys_level);
                am_addterm(min_y, ptr->min_y, 1.0);
                am_setrelation(min_y, AM_EQUAL);
                am_addterm(min_y, prev ? prev->max_y : widget.min_y, 1.0);
                if (prev)
                {
                    am_addconstant(min_y, prev->margins[3].size);
                    am_addconstant(min_y, prev->borders[3].size);
                    am_addconstant(min_y, prev->padding[3].size);
                    am_addconstant(min_y, space);
                    if (widget.type == gui_widget_type::splitter)
                    {
                        gui_widget_item new_item;
                        new_item.type = gui_widget_item_type::vsplitter;
                        gui_node_state_set(new_item.state, gui_node_state::enabled);
                        new_item.value = std::make_pair(prev->index, ptr->index);
                        ICY_ERROR(widget.items.push_back(std::move(new_item)));
                    }
                }
                am_addconstant(min_y, ptr->margins[1].size);
                am_addconstant(min_y, ptr->borders[1].size);
                am_addconstant(min_y, ptr->padding[1].size);
                am_add(min_y);
            }
            if (n == count)
            {
                auto max_y = am_newconstraint(m_solver, sys_level);
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
                auto max_y = am_newconstraint(m_solver, dyn_level);
                am_addterm(max_y, ptr->max_y, 1.0);
                am_setrelation(max_y, AM_EQUAL);
                am_addterm(max_y, ptr->min_y, 1.0);
                am_addconstant(max_y, size_y * ptr->weight_y);
                am_add(max_y);
            }
            if (const auto height = ptr->attr.try_find(gui_widget_attr::height))
            {
                auto value = 0.0f;
                if (to_value(*height, value))
                {
                    auto max_y = am_newconstraint(m_solver, usr_level);
                    am_addterm(max_y, ptr->max_y, 1.0);
                    am_setrelation(max_y, AM_EQUAL);
                    am_addterm(max_y, ptr->min_y, 1.0);
                    am_addconstant(max_y, value);
                    am_add(max_y);
                }
            }
            auto min_height = 0.0f;
            switch (ptr->type)
            {
            case gui_widget_type::view_list:
            case gui_widget_type::view_table:
            case gui_widget_type::view_tree:
            case gui_widget_type::edit_text:
            {
                min_height = m_system->vscroll().size * 3;
                break;
            }
            case gui_widget_type::edit_line:
            {
                min_height = ptr->scroll_y.step;
                break;
            }
            }

            auto max_y = am_newconstraint(m_solver, sys_level);
            am_addterm(max_y, ptr->max_y, 1.0);
            am_setrelation(max_y, AM_GREATEQUAL);
            am_addterm(max_y, ptr->min_y, 1.0);
            am_addconstant(max_y, min_height);
            am_add(max_y);

            prev = ptr;
        }
    }
    else if (widget.layout == gui_widget_layout::hbox)
    {
        gui_widget_data* prev = nullptr;
        auto n = 0u;
        for (auto&& ptr : widget.children)
        {
            if (!(gui_widget_state_isset(ptr->state, gui_widget_state::visible)))
                continue;
            ++n;
            {
                auto min_y = am_newconstraint(m_solver, sys_level);
                am_addterm(min_y, ptr->min_y, 1.0);
                am_setrelation(min_y, AM_EQUAL);
                am_addterm(min_y, widget.min_y, 1.0);
                am_addconstant(min_y, ptr->margins[1].size);
                am_addconstant(min_y, ptr->borders[1].size);
                am_addconstant(min_y, ptr->padding[1].size);
                am_add(min_y);
            }
            {
                auto max_y = am_newconstraint(m_solver, sys_level);
                am_addterm(max_y, ptr->max_y, 1.0);
                am_setrelation(max_y, AM_EQUAL);
                am_addterm(max_y, widget.max_y, 1.0);
                am_addconstant(max_y, -ptr->margins[3].size);
                am_addconstant(max_y, -ptr->borders[3].size);
                am_addconstant(max_y, -ptr->padding[3].size);
                am_add(max_y);
            }
            {
                auto min_x = am_newconstraint(m_solver, sys_level);
                am_addterm(min_x, ptr->min_x, 1.0);
                am_setrelation(min_x, AM_EQUAL);
                am_addterm(min_x, prev ? prev->max_x : widget.min_x, 1.0);
                if (prev)
                {
                    am_addconstant(min_x, prev->margins[2].size);
                    am_addconstant(min_x, prev->borders[2].size);
                    am_addconstant(min_x, prev->padding[2].size);
                    am_addconstant(min_x, space);

                    if (widget.type == gui_widget_type::splitter)
                    {
                        gui_widget_item new_item;
                        new_item.type = gui_widget_item_type::hsplitter;
                        gui_node_state_set(new_item.state, gui_node_state::enabled);
                        new_item.value = std::make_pair(prev->index, ptr->index);
                        ICY_ERROR(widget.items.push_back(std::move(new_item)));
                    }
                }
                am_addconstant(min_x, ptr->margins[0].size);
                am_addconstant(min_x, ptr->borders[0].size);
                am_addconstant(min_x, ptr->padding[0].size);
                am_add(min_x);
            }

            if (n == count)
            {
                auto max_x = am_newconstraint(m_solver, sys_level);
                am_addterm(max_x, ptr->max_x, 1.0);
                am_setrelation(max_x, AM_EQUAL);
                am_addterm(max_x, widget.max_x, 1.0);
                am_addconstant(max_x, -ptr->margins[2].size);
                am_addconstant(max_x, -ptr->borders[2].size);
                am_addconstant(max_x, -ptr->padding[2].size);
                am_add(max_x);
            }
            else
            {
                auto max_x = am_newconstraint(m_solver, dyn_level);
                am_addterm(max_x, ptr->max_x, 1.0);
                am_setrelation(max_x, AM_EQUAL);
                am_addterm(max_x, ptr->min_x, 1.0);
                am_addconstant(max_x, size_x * ptr->weight_x);
                am_add(max_x);
            }
            if (const auto width = ptr->attr.try_find(gui_widget_attr::width))
            {
                auto value = 0.0f;
                if (to_value(*width, value))
                {
                    auto max_x = am_newconstraint(m_solver, usr_level);
                    am_addterm(max_x, ptr->max_x, 1.0);
                    am_setrelation(max_x, AM_EQUAL);
                    am_addterm(max_x, ptr->min_x, 1.0);
                    am_addconstant(max_x, value);
                    am_add(max_x);
                }
            }
            auto min_width = 0.0f;
            switch (ptr->type)
            {
            case gui_widget_type::view_list:
            case gui_widget_type::view_table:
            case gui_widget_type::view_tree:
            case gui_widget_type::edit_text:
            {
                min_width = m_system->hscroll().size * 3 + m_system->vscroll().size;
                break;
            }
            case gui_widget_type::edit_line:
            {
                min_width = m_system->hscroll().size * 3;
                break;
            }
            }

            auto max_x = am_newconstraint(m_solver, sys_level);
            am_addterm(max_x, ptr->max_x, 1.0);
            am_setrelation(max_x, AM_GREATEQUAL);
            am_addterm(max_x, ptr->min_x, 1.0);
            am_addconstant(max_x, min_width);
            am_add(max_x);

            prev = ptr;
        }
    }


    for (auto&& child : widget.children)
    {
        if (!gui_widget_state_isset(child->state, gui_widget_state::visible))
            continue;

        render_widget rwidget;
        rwidget.widget = child;

        level += 1;
        ICY_ERROR(update(*child, level));
        level -= 1;

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
            if (gui_widget_state_isset(child->state, gui_widget_state::hscroll))
            {
                hscroll = 1;
            }
            else if (gui_widget_state_isset(child->state, gui_widget_state::hscroll_auto))
            {
                for (auto&& item : child->items)
                {
                    switch (item.type)
                    {
                    case gui_widget_item_type::text:
                    case gui_widget_item_type::col_header:
                    {
                        if (item.x + item.w > sx)
                        {
                            hscroll = 1;
                            return;
                        }
                    }
                    }
                }
            }
        };
        const auto calc_vscroll = [&]
        {
            if (gui_widget_state_isset(child->state, gui_widget_state::vscroll))
            {
                vscroll = 1;
            }
            else if (gui_widget_state_isset(child->state, gui_widget_state::vscroll_auto))
            {
                for (auto&& item : child->items)
                {
                    switch (item.type)
                    {
                    case gui_widget_item_type::text:
                    case gui_widget_item_type::row_header:
                    {
                        if (item.y + item.h > sy)
                        {
                            vscroll = 1;
                            return;
                        }
                        break;
                    }
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

        if (gui_widget_state_isset(child->state, gui_widget_state::hscroll_inv)) hscroll *= -1;
        if (gui_widget_state_isset(child->state, gui_widget_state::vscroll_inv)) vscroll *= -1;

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
        
        for (auto k = uint32_t(child->items.size()); k; --k)
        {
            auto& item = child->items[k - 1];
            switch (item.type)
            {
            case gui_widget_item_type::text:
            {
                auto max_item_x = item.x + item.w;
                auto max_item_y = item.y + item.h;
                if (gui_widget_state_isset(child->state, gui_widget_state::has_row_header)) max_item_x += child->row_header;
                if (gui_widget_state_isset(child->state, gui_widget_state::has_col_header)) max_item_y += child->col_header;
                child->scroll_x.max = std::max(child->scroll_x.max, max_item_x);
                child->scroll_y.max = std::max(child->scroll_y.max, max_item_y);
                break;
            }
            case gui_widget_item_type::row_header:
            case gui_widget_item_type::col_header:
            {
                break;
            }

            case gui_widget_item_type::vscroll_bk: vscroll_items[0].first = k; break;
            case gui_widget_item_type::vscroll_max: vscroll_items[1].first = k; break;
            case gui_widget_item_type::vscroll_min: vscroll_items[2].first = k; break;
            case gui_widget_item_type::vscroll_val: vscroll_items[3].first = k; break;
            case gui_widget_item_type::hscroll_bk: hscroll_items[0].first = k; break;
            case gui_widget_item_type::hscroll_max: hscroll_items[1].first = k; break;
            case gui_widget_item_type::hscroll_min: hscroll_items[2].first = k; break;
            case gui_widget_item_type::hscroll_val: hscroll_items[3].first = k; break;
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
                    gui_node_state_set(child->items[pair.first - 1].state, gui_node_state::visible);
                    gui_node_state_set(child->items[pair.first - 1].state, gui_node_state::enabled);
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
                    if (hscroll)
                        max.y += m_system->hscroll().size;

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
                    if (pair.first) 
                        gui_node_state_unset(child->items[pair.first - 1].state, gui_node_state::visible);
                }
            }
            return error_type();
        };
        ICY_ERROR(func(1));
        ICY_ERROR(func(0));

        if (hscroll) 
            gui_widget_state_set(child->state, gui_widget_state::has_hscroll);
        else
            gui_widget_state_unset(child->state, gui_widget_state::has_hscroll);
        
        if (vscroll) 
            gui_widget_state_set(child->state, gui_widget_state::has_vscroll);
        else
            gui_widget_state_unset(child->state, gui_widget_state::has_vscroll);

        for (auto&& item : child->items)
        {
            gui_node_state_unset(item.state, gui_node_state::visible);

            auto item_x = item.x;
            auto item_y = item.y;

            auto max_x = sx;
            auto max_y = sy;
            
            if (item.type == gui_widget_item_type::text)
            {
                if (gui_widget_state_isset(child->state, gui_widget_state::has_hscroll))
                    item_x -= child->scroll_x.val;
                if (gui_widget_state_isset(child->state, gui_widget_state::has_vscroll))
                    item_y -= child->scroll_y.val;
                if (gui_widget_state_isset(child->state, gui_widget_state::has_row_header))
                    item_x += child->row_header;
                if (gui_widget_state_isset(child->state, gui_widget_state::has_col_header))
                    item_y += child->col_header;
            }
            else if (item.type == gui_widget_item_type::row_header)
            {
                if (gui_widget_state_isset(child->state, gui_widget_state::has_vscroll))
                    item_y -= child->scroll_y.val;
                if (gui_widget_state_isset(child->state, gui_widget_state::has_col_header))
                    item_y += child->col_header;
            }
            else if (item.type == gui_widget_item_type::col_header)
            {
                if (gui_widget_state_isset(child->state, gui_widget_state::has_hscroll))
                    item_x -= child->scroll_x.val;
                if (gui_widget_state_isset(child->state, gui_widget_state::has_row_header))
                    item_x += child->row_header;
            }
            else if (
                item.type >= gui_widget_item_type::_scroll_beg &&
                item.type < gui_widget_item_type::_scroll_end)
            {
                max_x = FLT_MAX;
                max_y = FLT_MAX;
            }
            else if (item.type == gui_widget_item_type::vsplitter)
            {
                std::pair<uint32_t, uint32_t> pair;
                if (!item.value.get(pair))
                    continue;

                auto lhs = m_data.try_find(pair.first);
                auto rhs = m_data.try_find(pair.second);
                if (!lhs || !rhs)
                    continue;

                item.x = item_x = 0;
                item.y = item_y = am_value(lhs->get()->max_y) - am_value(child->min_y);
                item.w = am_value(child->max_x) - am_value(child->min_x);
                item.h = am_value(rhs->get()->min_y) - am_value(lhs->get()->max_y);
            }
            else if (item.type == gui_widget_item_type::hsplitter)
            {
                std::pair<uint32_t, uint32_t> pair;
                if (!item.value.get(pair))
                    continue;

                auto lhs = m_data.try_find(pair.first);
                auto rhs = m_data.try_find(pair.second);
                if (!lhs || !rhs)
                    continue;

                item.x = item_x = am_value(lhs->get()->max_x) - am_value(child->min_x);
                item.y = item_y = 0;
                item.w = am_value(rhs->get()->min_x) - am_value(lhs->get()->max_x);
                item.h = am_value(child->max_y) - am_value(child->min_y);
            }
            else
            {
                continue;
            }
            
            const auto item_max_x = item_x + item.w;
            const auto item_max_y = item_y + item.h;

            if (item_max_y < 0.0f || item_max_x < 0.0f || item_x >= max_x || item_y >= max_y)
                continue;

            gui_node_state_set(item.state, gui_node_state::visible);

            render_item ritem;
            ritem.x = item_x + am_value(child->min_x);
            ritem.y = item_y + am_value(child->min_y);
            ritem.item = &item;
            ICY_ERROR(rwidget.items.push_back(ritem));
        }
        rwidget.level = level;
        ICY_ERROR(m_render_list.push_back(std::move(rwidget)));
    }

    return error_type();
}
error_type gui_window_data_sys::input_key_press(const key key, const key_mod mods) noexcept
{
    if (!m_focus)
        return error_type();

    auto it = m_data.find(m_focus.widget);
    if (it == m_data.end())
    {
        m_focus = focus_type();
        return make_stdlib_error(std::errc::invalid_argument);
    }
    auto& widget = *it->value.get();
    if (widget.type == gui_widget_type::edit_text ||
        widget.type == gui_widget_type::edit_line)
    {
        return input_key_hold(key, mods);
    }


    return error_type();
}
error_type gui_window_data_sys::input_key_hold(const key key, const key_mod mods) noexcept
{
    if (!m_focus)
        return error_type();

    auto it = m_data.find(m_focus.widget);
    if (it == m_data.end())
    {
        m_focus = focus_type();
        return make_stdlib_error(std::errc::invalid_argument);
    }
    auto& widget = *it->value.get();
    if (widget.type == gui_widget_type::edit_text ||
        widget.type == gui_widget_type::edit_line)
    {
        const auto item = 0u;
        const auto has_ctrl = key_mod_and(key_mod::lctrl | key_mod::rctrl, mods);
        switch (key)
        {
        case key::back:
        {
            ICY_ERROR(text_action(widget, item, has_ctrl ? gui_text_keybind::ctrl_back : gui_text_keybind::back));
            break;
        }
        case key::del:
        {
            ICY_ERROR(text_action(widget, item, has_ctrl ? gui_text_keybind::ctrl_del: gui_text_keybind::del));
            break;
        }
        case key::left:
        {
            ICY_ERROR(text_action(widget, item, has_ctrl ? gui_text_keybind::ctrl_left : gui_text_keybind::left));
            break;
        }
        case key::right:
        {
            ICY_ERROR(text_action(widget, item, has_ctrl ? gui_text_keybind::ctrl_right: gui_text_keybind::right));
            break;
        }
        case key::up:
        {
            ICY_ERROR(text_action(widget, item, has_ctrl ? gui_text_keybind::ctrl_up : gui_text_keybind::up));
            break;
        }
        case key::down:
        {
            ICY_ERROR(text_action(widget, item, has_ctrl ? gui_text_keybind::ctrl_down: gui_text_keybind::down));
            break;
        }
        }
    }

    return error_type();
}
error_type gui_window_data_sys::input_key_release(const key key, const key_mod mods) noexcept
{

    return error_type();
}
error_type gui_window_data_sys::input_mouse_move(const int32_t px, const int32_t py, const key_mod mods) noexcept
{
    ICY_SCOPE_EXIT{ m_last_x = px; m_last_y = py; };
    const auto& root = *m_data.front().value;

    hover_type new_hover;
    for (auto it = m_render_list.rbegin(); it != m_render_list.rend(); ++it)
    {
        if (!(gui_widget_state_isset(it->widget->state, gui_widget_state::enabled)))
            continue;

        const auto& widget = *it->widget;

        const auto min_x = lround(am_value(widget.min_x));
        const auto min_y = lround(am_value(widget.min_y));
        const auto max_x = lround(am_value(widget.max_x));
        const auto max_y = lround(am_value(widget.max_y));

        if (px < min_x || py < min_y || px >= max_x || py >= max_y)
            continue;

        new_hover.widget = it->widget->index;
        new_hover.dx = px - min_x;
        new_hover.dy = py - min_y;

        const auto& items = it->items;
        for (auto jt = it->items.rbegin(); jt != it->items.rend(); ++jt)
        {
            if (!(gui_node_state_isset(jt->item->state, gui_node_state::enabled)))
                continue;

            const auto item_min_x = lround(jt->x);
            const auto item_min_y = lround(jt->y);
            if (px < item_min_x || py < item_min_y || px >= item_min_x + jt->item->w || py >= item_min_y + jt->item->h)
                continue;

            new_hover.item = uint32_t(std::distance(widget.items.data(), static_cast<const gui_widget_item*>(jt->item)) + 1);
            new_hover.dx = uint32_t(px - item_min_x);
            new_hover.dy = uint32_t(py - item_min_y);
            break;
        }
        
        if (new_hover.widget != m_hover.widget || new_hover.item != m_hover.item)
        {
            auto type = window_cursor::type::none;
            if (gui_widget_state_isset(widget.state, gui_widget_state::enabled))
            {
                if (new_hover.item)
                {
                    const auto& item = widget.items[new_hover.item - 1];
                    if (gui_node_state_isset(item.state, gui_node_state::enabled))
                    {
                        switch (item.type)
                        {
                        case gui_widget_item_type::vsplitter:
                            type = window_cursor::type::size_y;
                            break;

                        case gui_widget_item_type::hsplitter:
                            type = window_cursor::type::size_x;
                            break;
                        
                        case gui_widget_item_type::text:
                        {
                            if (gui_node_state_isset(item.state, gui_node_state::editable))
                                type = window_cursor::type::ibeam;
                            break;
                        }

                        case gui_widget_item_type::hscroll_bk:
                        case gui_widget_item_type::hscroll_max:
                        case gui_widget_item_type::hscroll_min:
                        case gui_widget_item_type::hscroll_val:
                        case gui_widget_item_type::vscroll_bk:
                        case gui_widget_item_type::vscroll_max:
                        case gui_widget_item_type::vscroll_min:
                        case gui_widget_item_type::vscroll_val:
                            ICY_ERROR(reset(gui_reset_reason::update_render_list));
                            break;
                        }
                    }
                }
                else if (widget.type == gui_widget_type::edit_line || widget.type == gui_widget_type::edit_text)
                {
                    type = window_cursor::type::ibeam;
                }
            }
            if (auto hwnd = shared_ptr<icy::window>(m_window))
            {
                ICY_ERROR(hwnd->cursor(0, m_system->cursor(type)));
            }

            if (m_hover && m_hover.item)
            {
                auto jt = m_data.find(m_hover.widget);
                if (jt == m_data.end())
                    return make_stdlib_error(std::errc::invalid_argument);
                
                if (gui_widget_state_isset(jt->value->state, gui_widget_state::enabled))
                {
                    const auto& item = jt->value->items[m_hover.item - 1];
                    if (gui_node_state_isset(item.state, gui_node_state::enabled))
                    {
                        switch (item.type)
                        {
                        case gui_widget_item_type::hscroll_bk:
                        case gui_widget_item_type::hscroll_max:
                        case gui_widget_item_type::hscroll_min:
                        case gui_widget_item_type::hscroll_val:
                        case gui_widget_item_type::vscroll_bk:
                        case gui_widget_item_type::vscroll_max:
                        case gui_widget_item_type::vscroll_min:
                        case gui_widget_item_type::vscroll_val:
                            ICY_ERROR(reset(gui_reset_reason::update_render_list));
                            break;
                        }
                    }
                }

            }
        }
        break;
    }

    if (m_focus)
    {
        
    }
    if (m_press)
    {
        auto it = m_data.find(m_press.widget);
        if (it == m_data.end())
        {
            return make_stdlib_error(std::errc::invalid_argument);
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
                    widget.scroll_y.exp = 0;
                }
                else
                {
                    widget.scroll_y.exp = num / den;
                }
                if (flex_scroll)
                {
                    ICY_ERROR(m_vscroll_val.reset(std::chrono::milliseconds(20)));
                }
                else
                {
                    widget.scroll_y.val = widget.scroll_y.exp;
                }
                widget.scroll_y.clamp();
                ICY_ERROR(reset(gui_reset_reason::update_render_list));
            }
            else if (item.type == gui_widget_item_type::hscroll_val)
            {
                auto flex_scroll = false;
                if (const auto ptr = widget.attr.try_find(gui_widget_attr::flex_scroll))
                    to_value(*ptr, flex_scroll);

                const auto local = px - (m_press.dx + m_system->hscroll().min_default.size().x + am_value(widget.min_x));
                const auto num = (widget.scroll_x.max - widget.scroll_x.view_size) * local;
                const auto den = (widget.scroll_x.area_size - item.w);

                if (den <= 0 || num <= 0)
                {
                    widget.scroll_x.exp = 0;
                }
                else
                {
                    widget.scroll_x.exp = num / den;
                }
                if (flex_scroll)
                {
                    ICY_ERROR(m_hscroll_val.reset(std::chrono::milliseconds(20)));
                }
                else
                {
                    widget.scroll_x.val = widget.scroll_x.exp;
                }
                widget.scroll_x.clamp();
                ICY_ERROR(reset(gui_reset_reason::update_render_list));
            }
            else if (item.type == gui_widget_item_type::vsplitter || item.type == gui_widget_item_type::hsplitter)
            {
                ICY_ERROR(reset(gui_reset_reason::update_splitter));
            }
        }
    }
    m_hover = new_hover;

    return error_type();
}
error_type gui_window_data_sys::input_mouse_wheel(const int32_t px, const int32_t py, const key_mod mods, const int32_t wheel) noexcept
{

    return error_type();
}
error_type gui_window_data_sys::input_mouse_release(const key key, const int32_t px, const int32_t py, const key_mod mods) noexcept
{
    if (key == key::mouse_left)
    {
        if (m_press)
        {
            if (auto hwnd = shared_ptr<icy::window>(m_window))
            {
                ICY_ERROR(hwnd->cursor(1, nullptr));
            }

            auto it = m_data.find(m_press.widget);
            if (it == m_data.end())
            {
                return make_stdlib_error(std::errc::invalid_argument);
            }
            auto& widget = *it->value.get();
            
            if (m_hover.widget == m_press.widget && m_hover.item == m_press.item)
            {               
                if (m_press.item)
                {
                    auto& item = widget.items[m_press.item - 1];
                    ICY_ERROR(click_widget_lmb(widget, &item));
                }
                else
                {
                    ICY_ERROR(click_widget_lmb(widget, nullptr));
                }
            }
            if (m_press.item)
            {
                auto& item = widget.items[m_press.item - 1];
                switch (item.type)
                {
                case gui_widget_item_type::vsplitter:
                {
                    auto pair = std::make_pair(0u, 0u);
                    if (item.value.get(pair))
                    {
                        auto lhs = m_data.find(pair.first);
                        auto rhs = m_data.find(pair.second);
                        if (lhs != m_data.end() && rhs != m_data.end())
                        {
                            const auto delta = float(py) - m_splitter;

                            const auto lhs_size = am_value(lhs->value->max_y) - am_value(lhs->value->min_y);
                            auto lhs_h = lhs->value->attr.find(gui_widget_attr::height);
                            if (lhs_h == lhs->value->attr.end())
                                ICY_ERROR(lhs->value->attr.insert(gui_widget_attr::height, 0.0f, &lhs_h));
                            lhs_h->value = std::max(0.0f, lhs_size + delta);

                            const auto rhs_size = am_value(rhs->value->max_y) - am_value(rhs->value->min_y);
                            auto rhs_h = rhs->value->attr.find(gui_widget_attr::height);
                            if (rhs_h == rhs->value->attr.end())
                                ICY_ERROR(rhs->value->attr.insert(gui_widget_attr::height, 0.0f, &rhs_h));
                            rhs_h->value = std::max(0.0f, rhs_size - delta);

                            ICY_ERROR(reset(gui_reset_reason::update_render_list));
                        }
                    }
                    break;
                }
                case gui_widget_item_type::hsplitter:
                {
                    auto pair = std::make_pair(0u, 0u);
                    if (item.value.get(pair))
                    {
                        auto lhs = m_data.find(pair.first);
                        auto rhs = m_data.find(pair.second);
                        if (lhs != m_data.end() && rhs != m_data.end())
                        {
                            const auto delta = float(px) - m_splitter;

                            const auto lhs_size = am_value(lhs->value->max_x) - am_value(lhs->value->min_x);
                            auto lhs_w = lhs->value->attr.find(gui_widget_attr::width);
                            if (lhs_w == lhs->value->attr.end())
                                ICY_ERROR(lhs->value->attr.insert(gui_widget_attr::width, 0.0f, &lhs_w));
                            lhs_w->value = std::max(0.0f, lhs_size + delta);

                            const auto rhs_size = am_value(rhs->value->max_x) - am_value(rhs->value->min_x);
                            auto rhs_w = rhs->value->attr.find(gui_widget_attr::width);
                            if (rhs_w == rhs->value->attr.end())
                                ICY_ERROR(rhs->value->attr.insert(gui_widget_attr::width, 0.0f, &rhs_w));
                            rhs_w->value = std::max(0.0f, rhs_size - delta);

                            ICY_ERROR(reset(gui_reset_reason::update_render_list));
                        }
                    }
                    break;
                }

                case gui_widget_item_type::vscroll_min:
                    m_vscroll_min.cancel();
                    ICY_ERROR(reset(gui_reset_reason::update_render_list));
                    break;
                case gui_widget_item_type::vscroll_max:
                    m_vscroll_max.cancel();
                    ICY_ERROR(reset(gui_reset_reason::update_render_list));
                    break;
                case gui_widget_item_type::vscroll_val:
                    m_vscroll_val.cancel();
                    ICY_ERROR(reset(gui_reset_reason::update_render_list));
                    break;
                case gui_widget_item_type::hscroll_min:
                    m_hscroll_min.cancel();
                    ICY_ERROR(reset(gui_reset_reason::update_render_list));
                    break;
                case gui_widget_item_type::hscroll_max:
                    m_hscroll_max.cancel();
                    ICY_ERROR(reset(gui_reset_reason::update_render_list));
                    break;
                case gui_widget_item_type::hscroll_val:
                    m_hscroll_val.cancel();
                    ICY_ERROR(reset(gui_reset_reason::update_render_list));
                    break;
                }
            }
            m_press = press_type();
        }
    }
    return error_type();
}
error_type gui_window_data_sys::input_mouse_press(const key key, const int32_t px, const int32_t py, const key_mod mods) noexcept
{
    if (key == key::mouse_left)
    {
        m_focus = focus_type();
        m_press = m_hover;
        if (!m_hover)
            return error_type();// reset(gui_reset_reason::update_render_list);
        
        auto it = m_data.find(m_press.widget);
        if (it == m_data.end())
            return make_stdlib_error(std::errc::invalid_argument);
        
        auto& widget = *it->value.get();
        m_focus.widget = m_press.widget;
        m_focus.item = m_press.item;

        if (m_press.item)
        {
            const auto& item = widget.items[m_press.item - 1];
            switch (item.type)
            {
            case gui_widget_item_type::vsplitter:
            {
                m_splitter = float(py);
                if (auto hwnd = shared_ptr<icy::window>(m_window))
                {
                    ICY_ERROR(hwnd->cursor(1, m_system->cursor(window_cursor::type::size_y)));
                }
                break;
            }
            case gui_widget_item_type::hsplitter:
            {
                m_splitter = float(px);
                if (auto hwnd = shared_ptr<icy::window>(m_window))
                {
                    ICY_ERROR(hwnd->cursor(1, m_system->cursor(window_cursor::type::size_x)));
                }
                break;
            }

            case gui_widget_item_type::vscroll_min:
            {
                widget.scroll_y.val -= widget.scroll_y.step;
                widget.scroll_y.exp = widget.scroll_y.val;
                widget.scroll_y.clamp();
                ICY_ERROR(m_vscroll_min.reset(m_system->scroll_update_freq()));
                return reset(gui_reset_reason::update_render_list);
            }
            case gui_widget_item_type::vscroll_max:
            {
                widget.scroll_y.val += widget.scroll_y.step;
                widget.scroll_y.exp = widget.scroll_y.val;
                widget.scroll_y.clamp();
                ICY_ERROR(m_vscroll_max.reset(m_system->scroll_update_freq()));
                return reset(gui_reset_reason::update_render_list);
            }
            case gui_widget_item_type::hscroll_min:
            {
                widget.scroll_x.val -= widget.scroll_x.step;
                widget.scroll_x.exp = widget.scroll_x.val;
                widget.scroll_x.clamp();
                ICY_ERROR(m_hscroll_min.reset(m_system->scroll_update_freq()));
                return reset(gui_reset_reason::update_render_list);
            }
            case gui_widget_item_type::hscroll_max:
            {
                widget.scroll_x.val += widget.scroll_x.step;
                widget.scroll_x.exp = widget.scroll_x.val;
                widget.scroll_x.clamp();
                ICY_ERROR(m_hscroll_max.reset(m_system->scroll_update_freq()));
                return reset(gui_reset_reason::update_render_list);
            }
            case gui_widget_item_type::vscroll_bk:
            case gui_widget_item_type::hscroll_bk:
            case gui_widget_item_type::vscroll_val:
            case gui_widget_item_type::hscroll_val:
                return error_type();
            }
        }

        if (widget.type == gui_widget_type::edit_line ||
            widget.type == gui_widget_type::edit_text)
        {
            gui_widget_item* item = nullptr;
            for (auto&& ptr : widget.items)
            {
                if (ptr.type == gui_widget_item_type::text && gui_node_state_isset(ptr.state, gui_node_state::editable))
                {
                    item = &ptr;
                    break;
                }
            }
            if (!item)
                return error_type();

            DWRITE_HIT_TEST_METRICS metrics;
            auto trailing = FALSE;
            auto inside = FALSE;
            ICY_COM_ERROR(item->text->HitTestPoint(float(m_press.dx), float(m_press.dy), &trailing, &inside, &metrics));
            string str;
            ICY_ERROR(to_string(item->value, str));
            auto pos = 0u;
            auto posw = 0u;
            for (auto it = str.begin(); it != str.end() && posw < metrics.textPosition; ++it, ++pos)
            {
                wchar_t wchr[2] = {};
                const auto wlen = it.to_utf16(wchr);
                if (!wlen)
                    return make_stdlib_error(std::errc::invalid_argument);
                posw += uint32_t(wcslen(wchr));
            }
            m_focus.text_select_beg = m_focus.text_select_end = pos;
            m_focus.text_trail_beg = m_focus.text_trail_end = !!trailing;
            m_focus.item = uint32_t(std::distance(widget.items.data(), item)) + 1;
            return reset(gui_reset_reason::update_text_caret);
        }
        else if (widget.type == gui_widget_type::view_tabs)
        {
            if (m_press.item)
            {
                auto& item = widget.items[m_press.item - 1];
                if (item.type == gui_widget_item_type::text && gui_node_state_isset(item.state, gui_node_state::enabled))
                {
                    for (auto&& other_item : widget.items)
                        gui_node_state_unset(other_item.state, gui_node_state::selected);
                    gui_node_state_set(item.state, gui_node_state::selected); 
                    ICY_ERROR(m_system->post_change(*this, { widget.index }));
                    return reset(gui_reset_reason::update_render_list);
                }
            }
        }
    }
    return error_type();
}
error_type gui_window_data_sys::input_mouse_double(const key key, const int32_t px, const int32_t py, const key_mod mods) noexcept
{

    return error_type();
}
error_type gui_window_data_sys::input_text(const string_view text) noexcept
{
    if (!m_focus)
        return error_type();

    auto it = m_data.find(m_focus.widget);
    if (it == m_data.end())
    {
        m_focus = focus_type();
        return make_stdlib_error(std::errc::invalid_argument);
    }
    auto& widget = *it->value.get();
    if (widget.type == gui_widget_type::edit_text ||
        widget.type == gui_widget_type::edit_line)
    {
        ICY_ERROR(text_action(widget, 0, gui_text_keybind::text, text));
    }

    return error_type();
}
error_type gui_window_data_sys::solve_item(gui_widget_data& widget, gui_widget_item& item, const gui_font& font) noexcept
{
    if (false
        || item.type == gui_widget_item_type::text
        || item.type == gui_widget_item_type::row_header
        || item.type == gui_widget_item_type::col_header)
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

        if (item.type == gui_widget_item_type::text)
        {
            if (widget.scroll_y.step == 0) widget.scroll_y.step = metrics.height / metrics.lineCount;
            if (widget.scroll_x.step == 0) widget.scroll_x.step = widget.scroll_y.step;
        }
    }
    else
    {
        item.w = 0;
        item.h = 0;
    }
    return error_type();
}
error_type gui_window_data_sys::make_view(const gui_model_proxy_read& proxy, const gui_node_data& node, const gui_data_bind& func, gui_widget_data& widget, const size_t level) noexcept
{
    array<const gui_node_data*> nodes;
    for (auto&& child : node.children)
    {
        if (child->value.col != 0)
            continue;

        if (!(gui_node_state_isset(child->key->state, gui_node_state::visible)))
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
            new_item.node = child->index;
            new_item.state = child->state;

            ICY_ERROR(widget.items.push_back(std::move(new_item)));
            if (widget.type == gui_widget_type::view_tabs)
            {
                widget.size_x += new_item.w;                
            }
            else
            {
                widget.size_y += new_item.h;
                if (widget.type == gui_widget_type::view_tree)
                {
                    widget.size_x += offset;
                    ICY_ERROR(make_view(proxy, *child, func, widget, level + 1));
                    widget.size_x -= offset;
                }
            }
        }
    }

    if (level == 0 && (widget.type == gui_widget_type::view_tree || widget.type == gui_widget_type::view_table))
    {
        gui_widget_state_unset(widget.state, gui_widget_state::has_row_header);
        gui_widget_state_unset(widget.state, gui_widget_state::has_col_header);
        widget.row_header = 0;
        widget.col_header = 0;

        map<uint32_t, gui_widget_item> row_headers;
        map<uint32_t, gui_widget_item> col_headers;
        map<uint32_t, map<uint32_t, gui_widget_item>> cols;

        for (auto&& item : widget.items)
        {
            const auto node = proxy.data(gui_node{ item.node });
            if (!node)
                continue;

            const auto item_index = uint32_t(std::distance(widget.items.data(), &item));
            if (node->row_header)
            {
                gui_widget_item new_item;
                new_item.type = gui_widget_item_type::row_header;
                new_item.value = node->row_header;
                new_item.state = gui_node_state::enabled;
                ICY_ERROR(solve_item(widget, new_item, m_font));
                if (new_item.text)
                {
                    gui_widget_state_set(widget.state, gui_widget_state::has_row_header);
                    widget.row_header = std::max(widget.row_header, new_item.w);
                    new_item.y = item.y;
                    new_item.node = item.node;
                    ICY_ERROR(row_headers.insert(item_index, std::move(new_item)));
                }
            }
            const auto add_col_header = [this, &col_headers, &widget](const uint32_t col, const gui_node_data& node)
            {
                if (node.col_header && col_headers.try_find(col) == nullptr)
                {
                    gui_widget_item new_item;
                    new_item.type = gui_widget_item_type::col_header;
                    new_item.value = node.col_header;
                    gui_node_state_set(new_item.state, gui_node_state::enabled);
                    ICY_ERROR(solve_item(widget, new_item, m_font));
                    if (new_item.text)
                    {
                        gui_widget_state_set(widget.state, gui_widget_state::has_col_header);
                        widget.col_header = std::max(widget.col_header, new_item.h);
                        new_item.node = node.index;
                        ICY_ERROR(col_headers.insert(col, std::move(new_item)));
                    }
                }
                return error_type();
            };
            ICY_ERROR(add_col_header(0u, *node));

            if (node->parent)
            {
                const auto row = proxy.row(gui_node{ item.node });
                for (auto&& pair : node->parent->children)
                {
                    if (pair.value.row == row && pair.value.col != 0)
                    {
                        ICY_ERROR(add_col_header(pair.value.col, *pair.key));
                        if (!gui_node_state_isset(pair.key->state, gui_node_state::visible))
                            continue;
                        
                        gui_widget_item new_item;
                        new_item.type = gui_widget_item_type::text;
                        new_item.value = pair.key->data;
                        new_item.state = pair.key->state;
                        ICY_ERROR(solve_item(widget, new_item, m_font));
                        if (new_item.text)
                        {
                            new_item.y = item.y;
                            new_item.node = item.node;
                            auto jt = cols.find(pair.value.col);
                            if (jt == cols.end())
                            {
                                ICY_ERROR(cols.insert(pair.value.col, map<uint32_t, gui_widget_item>(), &jt));
                            }
                            ICY_ERROR(jt->value.insert(item_index, std::move(new_item)));
                        }
                    }
                }
            }
        }
        auto max_w = 0.0f;
        for (auto&& item : widget.items)
            max_w = std::max(item.x + item.w, max_w);
        if (auto ptr = col_headers.try_find(0u))
            max_w = std::max(ptr->w, max_w);
        
        for (auto&& col : cols)
        {
            auto new_w = 0.0f;
            for (auto&& pair : col.value)
            {
                pair.value.x += max_w;
                new_w = std::max(new_w, pair.value.w);
                ICY_ERROR(widget.items.push_back(std::move(pair.value)));
            }
            auto jt = col_headers.find(col.key);
            if (jt != col_headers.end())
            {
                jt->value.x += max_w;
                new_w = std::max(new_w, jt->value.w);
            }

            max_w += new_w;
        }

        ICY_ERROR(widget.items.reserve(widget.items.size() + row_headers.size()));
        for (auto&& pair : row_headers)
            ICY_ERROR(widget.items.push_back(std::move(pair.value)));

        ICY_ERROR(widget.items.reserve(widget.items.size() + col_headers.size()));
        for (auto&& pair : col_headers)
            ICY_ERROR(widget.items.push_back(std::move(pair.value)));
    }

    return error_type();
}
error_type gui_window_data_sys::click_widget_lmb(gui_widget_data& widget, gui_widget_item* const item) noexcept
{
    if (item)
    {
        
    }
    return error_type();
}
error_type gui_window_data_sys::push_action(gui_widget_data& widget, gui_text_action& action) noexcept
{
    ICY_ASSERT(widget.action <= widget.actions.size(), "CORRUPTED ACTION LIST");
    ICY_ERROR(widget.actions.resize(widget.action));
    ICY_ERROR(exec_action(widget, action));
    ICY_ERROR(widget.actions.push_back(std::move(action)));
    widget.action++;
    return error_type();
}
error_type gui_window_data_sys::exec_action(gui_widget_data& widget, gui_text_action& action) noexcept
{
    ICY_ASSERT(action.item < widget.items.size(), "CORRUPTED ACTION");

    auto& item = widget.items[action.item];
    ICY_ASSERT(item.type == gui_widget_item_type::text, "CORRUPTED ACTION ITEM");

    string str;
    ICY_ERROR(to_string(item.value, str));

    string new_mid_str;
    if (const auto error = to_string(action.value, new_mid_str))
    {
        if (error != make_stdlib_error(std::errc::invalid_argument))
            return error;
    }
    string old_mid_str;
    ICY_ERROR(copy(string_view(str.begin() + action.offset, str.begin() + action.offset + action.length), old_mid_str));

    string swap_str;
    ICY_ERROR(swap_str.append(string_view(str.begin(), str.begin() + action.offset)));
    ICY_ERROR(swap_str.append(new_mid_str));
    ICY_ERROR(swap_str.append(string_view(str.begin() + action.offset + action.length, str.end())));
    action.value = std::move(old_mid_str);
    action.length = uint32_t(std::distance(new_mid_str.begin(), new_mid_str.end()));
    item.text = gui_text();
    item.value = swap_str;
    return error_type();
}
error_type gui_window_data_sys::text_action(gui_widget_data& widget, const uint32_t item, const gui_text_keybind type, string_view insert) noexcept
{
    ICY_ASSERT(widget.action <= widget.actions.size(), "CORRUPTED ACTION LIST");
    const auto reset_to_value = [this](const uint32_t select, const bool trail)
    {
        m_focus.text_select_beg = m_focus.text_select_end = select;
        m_focus.text_trail_beg = m_focus.text_trail_end = trail;
    };
    const auto pos_0 = [this]
    {
        return m_focus.text_select_beg + (m_focus.text_trail_beg ? 1 : 0);
    };
    const auto pos_1 = [this]
    {
        return m_focus.text_select_end + (m_focus.text_trail_end ? 1 : 0);
    };

    if (type == gui_text_keybind::undo || type == gui_text_keybind::redo)
    {
        gui_text_action* action = nullptr;
        if (type == gui_text_keybind::undo)
        {
            if (widget.action == 0)
                return error_type();
            action = &widget.actions[--widget.action];
        }
        else
        {
            if (widget.action == widget.actions.size())
                return error_type();
            action = &widget.actions[widget.action++];
        }
        ICY_ERROR(exec_action(widget, *action));
        reset_to_value(action->offset + action->length, false);
        ICY_ERROR(m_system->post_change(*this, { widget.index }));
        return reset(gui_reset_reason::update_render_list);
    }

    string insert_str;
    switch (type)
    {
    case gui_text_keybind::text:
    case gui_text_keybind::paste:
    {
        for (auto it = insert.begin(); it != insert.end(); ++it)
        {
            char32_t chr = 0;
            ICY_ERROR(it.to_char(chr));
            if (chr == 0x7F)
            {
                continue;
            }
            else if (chr < 0x20)
            {
                if (widget.type == gui_widget_type::edit_line)
                    continue;
                
                if (chr == '\r' || chr == '\n' || chr == '\t')
                    ;
                else
                    continue;
            }
            ICY_ERROR(insert_str.append(string_view(it, it + 1)));
        }
        if (insert_str.empty())
            return error_type();
        break;
    }
    case gui_text_keybind::left:
    {
        if (pos_0() < pos_1())
        {
            reset_to_value(m_focus.text_select_beg, m_focus.text_trail_beg);
            return reset(gui_reset_reason::update_text_select);           
        }
        else if (pos_0() > pos_1())
        {
            ;
        }
        else
        {
            if (m_focus.text_trail_end)
            {
                m_focus.text_trail_end = false;
            }
            else if (m_focus.text_select_end)
            {
                m_focus.text_select_end -= 1;
            }
            else
            {
                return error_type();
            }
        }
        reset_to_value(m_focus.text_select_end, m_focus.text_trail_end);
        return reset(gui_reset_reason::update_text_select);
    }
    case gui_text_keybind::right:
    {
        return error_type();
    }
    case gui_text_keybind::ctrl_left:
    case gui_text_keybind::ctrl_right:
    case gui_text_keybind::up:
    case gui_text_keybind::ctrl_up:
    case gui_text_keybind::down:
    case gui_text_keybind::ctrl_down:
    {
        return error_type();
    }

    case gui_text_keybind::del:
    {
        break;
    }
    case gui_text_keybind::ctrl_del:
    {
        break;
    }
    case gui_text_keybind::back:
    {
        break;
    }
    case gui_text_keybind::ctrl_back:
    {
        break;
    }
    default:
        return error_type();
    }

    //const auto pos_0 = m_focus.text_select_beg + (m_focus.text_trail_beg ? 1 : 0);
    //const auto pos_1 = m_focus.text_select_end + (m_focus.text_trail_end ? 1 : 0);

    const auto len = uint32_t(std::distance(insert_str.begin(), insert_str.end()));
    gui_text_action action;
    action.item = item;
    action.offset = std::min(pos_0(), pos_1());
    action.length = std::max(pos_0(), pos_1()) - action.offset;
    action.value = std::move(insert_str);

    ICY_ERROR(widget.actions.resize(widget.action));
    ICY_ERROR(exec_action(widget, action));
    ICY_ERROR(widget.actions.push_back(std::move(action)));
    widget.action++;
    ICY_ERROR(m_system->post_change(*this, { widget.index }));

    reset_to_value(m_focus.text_select_end + len, m_focus.text_trail_end);
    return reset(gui_reset_reason::update_render_list);
}
error_type gui_window_data_sys::process(const gui_window_event_type& event) noexcept
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
    ICY_ERROR(reset(gui_reset_reason::update_render_list));
    return error_type();
}
window_size gui_window_data_sys::size(const uint32_t widget) const noexcept
{
	if (widget == 0)
		return m_size;
	
	return window_size();
}    
error_type gui_window_data_sys::send_data(gui_model_data_sys& model, const gui_widget widget, const gui_node node, const gui_data_bind& func, bool& erase) noexcept 
{
    auto it = m_data.find(widget.index);
    if (it == m_data.end())
    {
        erase = true;
        return error_type();
    }
    ICY_ERROR(model.recv_data(*this, *it->value, node, func, erase));
    return error_type();
}
error_type gui_window_data_sys::recv_data(const gui_model_proxy_read& proxy, const gui_node_data& node, const gui_widget index, const gui_data_bind& func, bool& erase) noexcept
{
    auto it = m_data.find(index.index);
    if (it == m_data.end())
    {
        erase = true;
        return error_type();
    }
    auto& widget = *it->value;
    widget.actions.clear();
    widget.action = 0;

    if (m_focus && m_focus.widget == index.index)
        m_focus = focus_type();

    const auto reset_items = [&]
    {
        if (m_hover.widget == it->key) m_hover.item = 0;
        if (m_press.widget == it->key) m_press.item = 0;
        if (m_focus.widget == it->key) m_focus.item = 0;
        widget.items.clear();
        widget.size_x = 0;
        widget.size_y = 0;
    };

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
                if (gui_node_state_isset(node.state, gui_node_state::visible) && func.filter(proxy, gui_node{ node.index }))
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
    case gui_widget_type::view_tabs:
    {
        reset_items();
        if (gui_node_state_isset(node.state, gui_node_state::visible) && func.filter(proxy, gui_node{ node.index }))
        {
            ICY_ERROR(make_view(proxy, node, func, widget));
        }
        break;
    }

    default:
        return error_type();
    }
    ICY_ERROR(reset(gui_reset_reason::update_render_list));


    return error_type();
}
error_type gui_window_data_sys::timer(timer::pair& pair) noexcept
{
    auto dt = 0.0f;

    gui_widget_data* widget = nullptr;
    const auto func = [&]() -> error_type
    {
        if (!m_press)
        {
            pair.timer->cancel();
            return error_type();
        }
        auto it = m_data.find(m_press.widget);
        if (it == m_data.end())
        {
            m_press = press_type();
            pair.timer->cancel();
            return make_stdlib_error(std::errc::invalid_argument);
        }
        widget = it->value.get();
        return error_type();
    };
    const auto step_per_sec = 15;

    if (m_vscroll_val.check(pair, dt))
    {
        ICY_ERROR(func());
        if (widget)
        {
            const auto delta_y = widget->scroll_y.exp - widget->scroll_y.val;
            if (fabsf(delta_y) <= widget->scroll_y.step)
            {
                widget->scroll_y.val = widget->scroll_y.exp;
                widget->scroll_y.clamp();
                pair.timer->cancel();
            }
            else
            {
                widget->scroll_y.val += delta_y * dt * 0.5f;
                widget->scroll_y.clamp();
            }
        }
    }
    else if (m_hscroll_val.check(pair, dt))
    {
        ICY_ERROR(func());
        if (widget)
        {
            const auto delta_x = widget->scroll_x.exp - widget->scroll_x.val;
            if (fabsf(delta_x) <= widget->scroll_x.step)
            {
                widget->scroll_x.val = widget->scroll_x.exp;
                widget->scroll_x.clamp();
                pair.timer->cancel();
            }
            else
            {
                widget->scroll_x.val += delta_x * dt * 0.5f;
                widget->scroll_x.clamp();
            }
        }
    }
    else if (m_vscroll_min.check(pair, dt))
    {
        ICY_ERROR(func());
        if (widget && m_hover.widget == m_press.widget && m_hover.item == m_press.item)
        {
            widget->scroll_y.val -= widget->scroll_y.step * dt * step_per_sec;
            widget->scroll_y.clamp();
            if (widget->scroll_y.val == 0)
                pair.timer->cancel();
        }
    }
    else if (m_hscroll_min.check(pair, dt))
    {
        ICY_ERROR(func());
        if (widget && m_hover.widget == m_press.widget && m_hover.item == m_press.item)
        {
            widget->scroll_x.val -= widget->scroll_x.step * dt * step_per_sec;
            widget->scroll_x.clamp();
            if (widget->scroll_x.val <= 0)
                pair.timer->cancel();
        }
    }
    else if (m_vscroll_max.check(pair, dt))
    {
        ICY_ERROR(func());
        if (widget && m_hover.widget == m_press.widget && m_hover.item == m_press.item)
        {
            widget->scroll_y.val += widget->scroll_y.step * dt * step_per_sec;
            widget->scroll_y.clamp();
            if (widget->scroll_y.val + widget->scroll_y.view_size >= widget->scroll_y.max)
                pair.timer->cancel();
        }
    }
    else if (m_hscroll_max.check(pair, dt))
    {
        ICY_ERROR(func());
        if (widget && m_hover.widget == m_press.widget && m_hover.item == m_press.item)
        {
            widget->scroll_x.val += widget->scroll_x.step * dt * step_per_sec;
            widget->scroll_x.clamp();
            if (widget->scroll_x.val + widget->scroll_x.view_size >= widget->scroll_x.max)
                pair.timer->cancel();
        }
    }
    else if (m_caret_timer.check(pair, dt))
    {
        m_caret_visible = !m_caret_visible;
        if (!m_focus)
            return error_type();
        auto jt = m_data.find(m_focus.widget);
        if (jt == m_data.end())
        {
            m_focus = focus_type();
            return make_stdlib_error(std::errc::invalid_argument);
        }
        if (jt->value->type == gui_widget_type::edit_line ||
            jt->value->type == gui_widget_type::edit_text)
        {
            ;
        }
        else
        {
            return error_type();
        }
        return reset(gui_reset_reason::update_text_caret);

    }
    else
    {
        return error_type();
    }
    ICY_ERROR(reset(gui_reset_reason::update_render_list));
    return error_type();
}
error_type gui_window_data_sys::reset(const gui_reset_reason reason) noexcept
{
    ICY_ERROR(m_system->post_update(*this));
    if (reason == gui_reset_reason::update_render_list)
    {
        m_state &= ~gui_window_state_updated;
        m_render_list.clear();
    }
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
    
    const auto draw_splitter = [this, &texture](const render_item& item)
    {
        gui_texture::rect_type rect;
        rect.min_x = item.x;
        rect.min_y = item.y;
        rect.max_x = rect.min_x + item.item->w;
        rect.max_y = rect.min_y + item.item->h;
        texture.fill_rect(rect, colors::light_gray);
    };

    render_item* splitter_item = nullptr;

    for (auto&& pair : m_render_list)
    {
        const auto widget = pair.widget;
        const auto x0 = am_value(widget->min_x);
        const auto y0 = am_value(widget->min_y);
        const auto x1 = am_value(widget->max_x);
        const auto y1 = am_value(widget->max_y);

        color bkcolor;
        if (const auto ptr = widget->attr.try_find(gui_widget_attr::bkcolor))
            to_value(*ptr, bkcolor);

        ICY_ERROR(texture.fill_rect({ x0, y0, x1, y1 }, bkcolor));

        auto clip = false;
        if (false
            || gui_widget_state_isset(widget->state, gui_widget_state::has_hscroll)
            || gui_widget_state_isset(widget->state, gui_widget_state::has_vscroll)
            || gui_widget_state_isset(widget->state, gui_widget_state::has_row_header)
            || gui_widget_state_isset(widget->state, gui_widget_state::has_col_header))
        {
            auto clip_rect = gui_texture::rect_type{ x0, y0, x1, y1 };
            
            if (gui_widget_state_isset(widget->state, gui_widget_state::has_vscroll))
                clip_rect.max_x -= m_system->vscroll().size;
            
            if (gui_widget_state_isset(widget->state, gui_widget_state::has_hscroll))
                clip_rect.max_y -= m_system->hscroll().size;
            
            if (gui_widget_state_isset(widget->state, gui_widget_state::has_row_header))
                clip_rect.min_x += widget->row_header;

            if (gui_widget_state_isset(widget->state, gui_widget_state::has_col_header))
                clip_rect.min_y += widget->col_header;

            texture.push_clip(clip_rect);
            clip = true;
        }
        auto it = pair.items.begin();
        for (; it != pair.items.end(); ++it)
        {
            if (it->item->type == gui_widget_item_type::text)
            {
                const auto item_index = uint32_t(std::distance(widget->items.data(), it->item));
                ICY_ERROR(texture.draw_text(it->x, it->y, it->item->color, it->item->text.value));
                if (m_focus && m_focus.widget == widget->index && m_focus.item == item_index + 1 && it->item->text)
                {
                    if (m_caret_visible && gui_node_state_isset(it->item->state, gui_node_state::editable))
                    {
                        DWRITE_HIT_TEST_METRICS metrics;
                        auto px = 0.0f;
                        auto py = 0.0f;
                        string str;
                        ICY_ERROR(to_string(it->item->value, str));
                        auto pos = 0u;
                        auto posw = 0u;
                        for (auto jt = str.begin(); jt != str.end() && pos < m_focus.text_select_end; ++jt, ++pos)
                        {
                            wchar_t wchr[2] = {};
                            const auto wlen = jt.to_utf16(wchr);
                            if (!wlen)
                                return make_stdlib_error(std::errc::invalid_argument);
                            posw += uint32_t(wlen);
                        }
                        ICY_COM_ERROR(it->item->text->HitTestTextPosition(posw, m_focus.text_trail_end, &px, &py, &metrics));

                        gui_texture::rect_type caret_rect;
                        caret_rect.min_x = it->x + px;
                        caret_rect.min_y = it->y + py;
                        caret_rect.max_x = caret_rect.min_x + 2;
                        caret_rect.max_y = caret_rect.min_y + metrics.height;
                        ICY_ERROR(texture.fill_rect(caret_rect, colors::white));
                    }
                }
            }
            else
            {
                break;
            }
        }
        if (clip)
        {
            texture.pop_clip();
            clip = false;
        }

        if (gui_widget_state_isset(widget->state, gui_widget_state::has_row_header))
        {
            auto clip_rect = gui_texture::rect_type{ x0, y0, x0 + widget->row_header, y1 };
            if (gui_widget_state_isset(widget->state, gui_widget_state::has_col_header))
                clip_rect.min_y += widget->col_header;

            if (gui_widget_state_isset(widget->state, gui_widget_state::has_hscroll))
                clip_rect.max_y -= m_system->hscroll().size;

            texture.push_clip(clip_rect);
            for (; it != pair.items.end(); ++it)
            {
                if (it->item->type == gui_widget_item_type::row_header)
                {
                    ICY_ERROR(texture.draw_text(it->x, it->y, it->item->color, it->item->text.value));
                }
                else
                {
                    break;
                }
            }
            texture.pop_clip();
        }
        if (gui_widget_state_isset(widget->state, gui_widget_state::has_col_header))
        {
            auto clip_rect = gui_texture::rect_type{ x0, y0, x1, y0 + widget->col_header };
            if (gui_widget_state_isset(widget->state, gui_widget_state::has_row_header))
                clip_rect.min_x += widget->row_header;

            if (gui_widget_state_isset(widget->state, gui_widget_state::has_vscroll))
                clip_rect.max_x -= m_system->vscroll().size;

            texture.push_clip(clip_rect);
            for (; it != pair.items.end(); ++it)
            {
                if (it->item->type == gui_widget_item_type::col_header)
                {
                    ICY_ERROR(texture.draw_text(it->x, it->y, it->item->color, it->item->text.value));
                }
                else
                {
                    break;
                }
            }
            texture.pop_clip();
        }
        if (widget->type == gui_widget_type::splitter)
        {
            for (; it != pair.items.end(); ++it)
            {
                const auto item_index = uint32_t(std::distance(widget->items.data(), it->item));
                if (it->item->type == gui_widget_item_type::vsplitter ||
                    it->item->type == gui_widget_item_type::hsplitter)
                {
                    draw_splitter(*it);
                    if (m_press && m_press.widget == widget->index && m_press.item == item_index + 1)
                        splitter_item = &*it;
                }
                else
                {
                    break;
                }
            }
        }
        if (gui_widget_state_isset(widget->state, gui_widget_state::has_vscroll))
        {
            for (; it != pair.items.end(); ++it)
            {
                if (it->item->type == gui_widget_item_type::vscroll_bk ||
                    it->item->type == gui_widget_item_type::vscroll_min ||
                    it->item->type == gui_widget_item_type::vscroll_max ||
                    it->item->type == gui_widget_item_type::vscroll_val)
                {
                    gui_texture::rect_type rect;
                    rect.min_x = rect.max_x = it->x;
                    rect.min_y = rect.max_y = it->y;
                    rect.max_x += it->item->w;
                    rect.max_y += it->item->h;
                    texture.draw_image(rect, it->item->image.value);
                }
                else
                {
                    break;
                }
            }
        }
        if (gui_widget_state_isset(widget->state, gui_widget_state::has_hscroll))
        {
            for (; it != pair.items.end(); ++it)
            {
                if (it->item->type == gui_widget_item_type::hscroll_bk ||
                    it->item->type == gui_widget_item_type::hscroll_min ||
                    it->item->type == gui_widget_item_type::hscroll_max ||
                    it->item->type == gui_widget_item_type::hscroll_val)
                {
                    gui_texture::rect_type rect;
                    rect.min_x = rect.max_x = it->x;
                    rect.min_y = rect.max_y = it->y;
                    rect.max_x += it->item->w;
                    rect.max_y += it->item->h;
                    texture.draw_image(rect, it->item->image.value);
                }
                else
                {
                    break;
                }
            }
        }
    }
    if (splitter_item)
    {
        if (splitter_item->item->type == gui_widget_item_type::vsplitter)
        {
            const auto dy = m_last_y - m_splitter;
            splitter_item->y += dy;
            draw_splitter(*splitter_item);
            splitter_item->y -= dy;
        }
        else if (splitter_item->item->type == gui_widget_item_type::hsplitter)
        {
            const auto dx = m_last_x - m_splitter;
            splitter_item->x += dx;
            draw_splitter(*splitter_item);
            splitter_item->x -= dx;
        }
    }

    ICY_ERROR(texture.draw_end());
    return error_type();
}

#define AM_IMPLEMENTATION
#include "amoeba.h"
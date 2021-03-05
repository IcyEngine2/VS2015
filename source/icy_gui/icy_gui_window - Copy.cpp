#include "icy_gui_window.hpp"
#include "icy_gui_system.hpp"
#include "icy_gui_render.hpp"
#include "icy_gui_style.hpp"
#include "icy_gui_system.hpp"
#include "default_style.hpp"
#define AM_USE_FLOAT
#define AM_STATIC_API
#include "amoeba.h"
#include "../libs/css/libcss/include/libcss/libcss.h"
#include <dwrite.h>
using namespace icy;

ICY_STATIC_NAMESPACE_BEG

enum gui_window_state
{
    gui_window_state_none       =   0x00,
    gui_window_state_has_list   =   0x01,
};
enum gui_widget_box_var
{
	gui_widget_box_var_left,
	gui_widget_box_var_top,
	gui_widget_box_var_width,
	gui_widget_box_var_height,
	gui_widget_box_var_min_width,
	gui_widget_box_var_max_width,
	gui_widget_box_var_min_height,
	gui_widget_box_var_max_height,
	gui_widget_box_var_margin_left,
	gui_widget_box_var_margin_top,
	gui_widget_box_var_margin_right,
	gui_widget_box_var_margin_bottom,
	gui_widget_box_var_border_left,
	gui_widget_box_var_border_top,
	gui_widget_box_var_border_right,
	gui_widget_box_var_border_bottom,
	gui_widget_box_var_padding_left,
	gui_widget_box_var_padding_top,
	gui_widget_box_var_padding_right,
	gui_widget_box_var_padding_bottom,

	gui_widget_box_con_left,
	gui_widget_box_con_top,
	gui_widget_box_con_block_width,
	gui_widget_box_con_block_height,
	gui_widget_box_con_inline_width,
	gui_widget_box_con_inline_height,

	gui_widget_box_con_max_width,
	gui_widget_box_con_min_width,
	gui_widget_box_con_max_height,
	gui_widget_box_con_min_height,

	gui_widget_box_var_count,
};
struct gui_widget_box
{
public:
	am_Solver* solver = nullptr;
	gui_widget_box* parent = nullptr;
	gui_widget_box* anchor = nullptr;
	gui_widget_data* widget = nullptr;
	gui_widget_box* prev = nullptr;
	gui_widget_box* next = nullptr;
	gui_widget_box* child = nullptr;
	am_Var* var(const gui_widget_box_var index)
	{
		if (!m_vars[index])
			m_vars[index] = am_newvariable(solver);
		return m_vars[index];
	}
	am_Var* cvar(const gui_widget_box_var index) const
	{
		return m_vars[index];
	}
	am_Constraint* cons[gui_widget_box_var_count] = {};
public:
private:
	am_Var* m_vars[gui_widget_box_var_count] = {};
};

ICY_STATIC_NAMESPACE_END

error_type gui_widget_data::insert(map<uint32_t, unique_ptr<gui_widget_data>>& data, const insert_args& args) noexcept
{
    auto it = data.find(args.parent);
    auto& children = it->value->children;
    const auto offset = std::max(size_t(args.offset), children.size());
        
    unique_ptr<gui_widget_data> new_widget;
    ICY_ERROR(make_unique(gui_widget_data(it->value.get(), args.index), new_widget));
    ICY_ERROR(children.reserve(children.size() + 1));
    ICY_ERROR(data.reserve(data.size() + 1));

    ICY_ERROR(children.push_back(nullptr));
    for (auto k = offset + 1; k < children.size(); ++k)
        children[k] = std::move(children[k - 1]);
    children[offset] = new_widget.get();
    ICY_ERROR(data.insert(args.index, std::move(new_widget)));
    ICY_ERROR(modify(data, args));
    return error_type();
}
error_type gui_widget_data::modify(map<uint32_t, unique_ptr<gui_widget_data>>& data, const insert_args& args) noexcept
{
    auto it = data.find(args.index);
    ICY_ERROR(copy(args.type, it->value->type));
    it->value->attr.clear();
    for (auto&& pair : args.attr)
    {
        string str_key;
        string str_val;
        ICY_ERROR(copy(pair.key, str_key));
        ICY_ERROR(copy(pair.value, str_val));
        ICY_ERROR(it->value->attr.insert(std::move(str_key), std::move(str_val)));
    }
    if (args.text.empty())
    {
        it->value->text.value = gui_variant();
    }
    else
    {
		string new_text;
		ICY_ERROR(new_text.reserve(args.text.bytes().size()));
		char32_t last_chr = 0;
		for (auto it = args.text.begin(); it != args.text.end(); ++it)
		{
			char32_t chr = 0;
			ICY_ERROR(it.to_char(chr));
			auto substr = string_view(it, it + 1);
			if (chr == '\r' || chr == '\n' || chr == '\t' || chr == '\f')
			{
				chr = ' ';
				substr = " "_s;
			}
			if (last_chr == ' ' && chr == ' ')
				continue;	//	do not append multispace
				
			ICY_ERROR(new_text.append(substr));
			last_chr = chr;
		}
		if (new_text.empty())
		{
			it->value->text.value = gui_variant();
		}
		else
		{
			it->value->text.value = gui_variant(string_view(new_text));
			if (!it->value->text.value)
				return make_stdlib_error(std::errc::not_enough_memory);
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
gui_widget gui_window_data_usr::child(const gui_widget parent, const size_t offset) noexcept
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
error_type gui_window_data_usr::insert(const gui_widget parent, const size_t offset, const string_view type, gui_widget& widget) noexcept
{
    gui_widget_data::insert_args args;
    args.parent = parent.index;
    args.index = m_index;
    args.offset = uint32_t(offset);
    ICY_ERROR(copy(type, args.type));

    gui_window_event_type* new_event = nullptr;
    ICY_ERROR(gui_window_event_type::make(new_event, gui_window_event_type::create));
    ICY_SCOPE_EXIT{ gui_window_event_type::clear(new_event); };
    ICY_ERROR(gui_widget_data::insert(m_data, args));
    new_event->index = widget.index = m_index++;
    new_event->val = std::move(args);
    notify(new_event);
    return error_type();
}
error_type gui_window_data_usr::erase(const gui_widget widget) noexcept
{
    gui_window_event_type* new_event = nullptr;
    ICY_ERROR(gui_window_event_type::make(new_event, gui_window_event_type::destroy));
    new_event->index = widget.index;
    if (gui_widget_data::erase(m_data, widget.index))
        notify(new_event);
    return error_type();
}
error_type gui_window_data_usr::layout(const gui_widget widget, gui_layout& value) noexcept
{
    auto it = m_data.find(widget.index);
    if (it == m_data.end())
        return error_type();

    gui_widget_data::insert_args args;
    ICY_ERROR(copy(value.text, args.text));
    ICY_ERROR(copy(value.type, args.type));
    for (auto&& pair : value.attributes)
    {
        string str_key;
        string str_val;
        ICY_ERROR(copy(pair.key, str_key));
        ICY_ERROR(copy(pair.value, str_val));
        ICY_ERROR(args.attr.insert(std::move(str_key), std::move(str_val)));
    }
    if (value.widget == 0)
    {
        auto& wdata = *it->value;
        auto children = std::move(wdata.children);
        for (auto&& child : children)
        {
            ICY_ERROR(erase(gui_widget{ child->index }));
        }
    }
    gui_window_event_type* new_event = nullptr;
    ICY_ERROR(gui_window_event_type::make(new_event, gui_window_event_type::modify));
    ICY_SCOPE_EXIT{ gui_window_event_type::clear(new_event); };
    args.index = new_event->index = value.widget = widget.index; 
    new_event->val = std::move(args);
    notify(new_event);
    
    for (auto&& child : value.children)
    {
        gui_widget new_widget;
        ICY_ERROR(insert(widget, args.offset++, child.type, new_widget));
        child.widget = new_widget.index;
        ICY_ERROR(layout(new_widget, child));
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
error_type gui_window_data_sys::initialize(const shared_ptr<window> window) noexcept
{
    m_window = window;
    m_dpi = 1.0f * window->dpi();
	
    ICY_ERROR(m_system->render().create_font(m_font, 
        static_cast<HWND__*>(window->handle()), gui_system_font_type_default));

    unique_ptr<gui_widget_data> root;
    ICY_ERROR(make_unique(gui_widget_data(), root));
	root->bkcolor = colors::black;
    ICY_ERROR(m_data.insert(0u, std::move(root)));
    ICY_ERROR(resize(window->size()));
    return error_type();
}
error_type gui_window_data_sys::input(const input_message& msg) noexcept
{
    return error_type();
}
error_type gui_window_data_sys::resize(const window_size size) noexcept
{
    auto& root = *m_data.front()->value.get();
    m_size = size;
	m_state &= ~gui_window_state_has_list;
    return error_type();
}
error_type gui_window_data_sys::update() noexcept
{
    if (m_state & gui_window_state_has_list)
        return error_type();
    
    m_list.clear();
    gui_select_system css;
    ICY_ERROR(m_system->create_css(css));
    ICY_ERROR(update(css, *m_data.front().value));

	struct jmp_buf_type { jmp_buf buf; } jmp;
	if (setjmp(jmp.buf))
		return make_stdlib_error(std::errc::not_enough_memory);

	const auto alloc = [](void* user, void* ptr, size_t nsize, size_t)
	{
		ptr = global_realloc(ptr, nsize, user);
		if (nsize && !ptr)
			longjmp(static_cast<jmp_buf_type*>(user)->buf, 1);
		return ptr;
	}; 
	auto solver = am_newsolver(alloc, &jmp); 
	ICY_SCOPE_EXIT{ am_delsolver(solver); };
#if _DEBUG
	am_autoupdate(solver, 1);
#endif

	array<gui_widget_box> box_list;
	ICY_ERROR(box_list.reserve(m_list.size()));

	const auto find = [this, &box_list](const gui_widget_data* ptr) -> gui_widget_box*
	{
		const auto jt = std::find(m_list.begin(), m_list.end(), ptr);
		if (jt != m_list.end())
		{
			const auto dn = std::distance(m_list.begin(), jt);
			return &box_list[dn];
		}
		return nullptr;
	};

	for (auto it = m_list.begin(); it != m_list.end(); ++it)
	{
		auto& widget = **it;
		const auto style = widget.css.style();
		
		gui_widget_box new_box;
		new_box.solver = solver;
		new_box.parent = find(widget.parent);
		new_box.widget = &widget;

		if (!widget.parent)
		{
			am_suggest(new_box.var(gui_widget_box_var_left), 0, AM_REQUIRED);
			am_suggest(new_box.var(gui_widget_box_var_top), 0, AM_REQUIRED);
			am_suggest(new_box.var(gui_widget_box_var_width), am_Num(m_size.x), AM_REQUIRED);
			am_suggest(new_box.var(gui_widget_box_var_height), am_Num(m_size.y), AM_REQUIRED);
		}

		switch (widget.position)
		{
		case CSS_POSITION_RELATIVE:
		{
			new_box.anchor = new_box.parent;
			break;
		}
		case CSS_POSITION_ABSOLUTE:
		case CSS_POSITION_FIXED:
		{
			const gui_widget_data* next = &widget;
			while (next = next->parent)
			{
				if (widget.position == CSS_POSITION_ABSOLUTE && next->position != CSS_POSITION_STATIC)
					break;
				if (next->type == "html"_s)
					break;
			}
			new_box.anchor = find(next);
			break;
		}
		}
		if (widget.text.layout)
		{
			DWRITE_TEXT_METRICS metrics;
			ICY_COM_ERROR(widget.text.layout->GetMetrics(&metrics));
			am_suggest(new_box.var(gui_widget_box_var_height), metrics.height, AM_STRONG);
			am_suggest(new_box.var(gui_widget_box_var_width), metrics.widthIncludingTrailingWhitespace, AM_STRONG);
		}

		const auto calc_prop = [&](const gui_widget_box_var var)
		{
			uint8_t(*func)(const css_computed_style * style, css_fixed * len, css_unit * unit) = nullptr;
			auto rel_var = gui_widget_box_var_count;
			auto set_val = 1;

			switch (var)
			{
			case gui_widget_box_var_border_left:
			case gui_widget_box_var_border_right:
				set_val = CSS_BORDER_WIDTH_WIDTH;
			case gui_widget_box_var_max_width:
			case gui_widget_box_var_min_width:
			case gui_widget_box_var_margin_left:
			case gui_widget_box_var_margin_right:
			case gui_widget_box_var_padding_left:
			case gui_widget_box_var_padding_right:
				rel_var = gui_widget_box_var_width;
				break;

			case gui_widget_box_var_border_top:
			case gui_widget_box_var_border_bottom:
				set_val = CSS_BORDER_WIDTH_WIDTH;
			case gui_widget_box_var_max_height:
			case gui_widget_box_var_min_height:
			case gui_widget_box_var_margin_top:
			case gui_widget_box_var_margin_bottom:
			case gui_widget_box_var_padding_top:
			case gui_widget_box_var_padding_bottom:
				rel_var = gui_widget_box_var_height;
				break;
			default:
				return;
			}

			switch (var)
			{
			case gui_widget_box_var_max_width: func = &css_computed_max_width; break;
			case gui_widget_box_var_min_width: func = &css_computed_min_width; break;
			case gui_widget_box_var_max_height: func = &css_computed_max_height; break;
			case gui_widget_box_var_min_height: func = &css_computed_min_height; break;

			case gui_widget_box_var_border_left: func = &css_computed_border_left_width; break;
			case gui_widget_box_var_border_top: func = &css_computed_border_top_width; break;
			case gui_widget_box_var_border_right: func = &css_computed_border_right_width; break;
			case gui_widget_box_var_border_bottom: func = &css_computed_border_bottom_width; break;

			case gui_widget_box_var_margin_left: func = &css_computed_margin_left; break;
			case gui_widget_box_var_margin_top: func = &css_computed_margin_top; break;
			case gui_widget_box_var_margin_right: func = &css_computed_margin_right; break;
			case gui_widget_box_var_margin_bottom: func = &css_computed_margin_bottom; break;

			case gui_widget_box_var_padding_left: func = &css_computed_padding_left; break;
			case gui_widget_box_var_padding_top: func = &css_computed_padding_top; break;
			case gui_widget_box_var_padding_right: func = &css_computed_padding_right; break;
			case gui_widget_box_var_padding_bottom: func = &css_computed_padding_bottom; break;

			default:
				return;
			}

			css_fixed len = 0;
			css_unit unit = CSS_UNIT_PX;
			const auto val = func(style, &len, &unit);

			if (val == 0 && new_box.parent)
			{
				auto& con = new_box.cons[var];
				con = am_newconstraint(solver, AM_MEDIUM);
				am_setrelation(con, AM_EQUAL);
				am_addterm(con, new_box.var(var), 1.0);
				am_addterm(con, new_box.parent->var(var), -1.0);
				am_add(con);
			}
			else if (val == set_val)
			{
				if (unit == CSS_UNIT_PCT)
				{
					auto& con = new_box.cons[var];
					con = am_newconstraint(solver, AM_MEDIUM);
					am_setrelation(con, AM_EQUAL);
					am_addterm(con, new_box.var(var), 1.0);
					am_addterm(con, new_box.parent->var(rel_var), -FIXTOFLT(len) * 0.01f);
					am_add(con);
				}
				else
				{
					auto calc = 0.0f;
					switch (unit)
					{
					case CSS_UNIT_EM:
						calc = FIXTOFLT(len) * new_box.widget->font_size;
						break;
					case CSS_UNIT_REM:
					{
						calc = FIXTOFLT(len) * m_data.front()->value->font_size;
						break;
					}
					case CSS_UNIT_PT:
						calc = FIXTOFLT(len) / 72.0f * m_dpi;
						break;
					default:
						calc = FIXTOFLT(len);
						break;
					}

					am_suggest(new_box.var(var), calc, AM_MEDIUM);
				}

				am_Constraint** con_ptr = nullptr;
				int con_rel = 0;

				switch (var)
				{
				case gui_widget_box_var_max_width:
				{
					con_ptr = &new_box.cons[gui_widget_box_con_max_width];
					con_rel = AM_LESSEQUAL;
					break;
				}
				case gui_widget_box_var_min_width:
				{
					con_ptr = &new_box.cons[gui_widget_box_con_min_width];
					con_rel = AM_GREATEQUAL;
					break;
				}
				case gui_widget_box_var_max_height:
				{
					con_ptr = &new_box.cons[gui_widget_box_con_max_height];
					con_rel = AM_LESSEQUAL;
					break;
				}
				case gui_widget_box_var_min_height:
				{
					con_ptr = &new_box.cons[gui_widget_box_con_min_height];
					con_rel = AM_GREATEQUAL;
					break;
				}
				}
				if (con_ptr)
				{
					auto& con = *con_ptr;
					con = am_newconstraint(solver, AM_MEDIUM);
					am_addterm(con, new_box.var(rel_var), 1.0);
					am_setrelation(con, con_rel);
					am_addterm(con, new_box.var(var), 1.0);
					am_add(con);
				}
			}
		};

		calc_prop(gui_widget_box_var_max_width);
		calc_prop(gui_widget_box_var_max_height);
		calc_prop(gui_widget_box_var_min_width);
		calc_prop(gui_widget_box_var_min_height);

		calc_prop(gui_widget_box_var_margin_left);
		calc_prop(gui_widget_box_var_margin_top);
		calc_prop(gui_widget_box_var_margin_right);
		calc_prop(gui_widget_box_var_margin_bottom);

		calc_prop(gui_widget_box_var_border_left);
		calc_prop(gui_widget_box_var_border_top);
		calc_prop(gui_widget_box_var_border_right);
		calc_prop(gui_widget_box_var_border_bottom);

		calc_prop(gui_widget_box_var_padding_left);
		calc_prop(gui_widget_box_var_padding_top);
		calc_prop(gui_widget_box_var_padding_right);
		calc_prop(gui_widget_box_var_padding_bottom);


		ICY_ERROR(box_list.push_back(std::move(new_box)));
	}
	for (auto&& box : box_list)
	{
		auto& children = box.widget->children;
		for (auto n = 0u; n < children.size(); ++n)
		{
			auto ptr = find(children[n]);
			if (!ptr)
				continue;

			if (!box.child)
				box.child = ptr;
			
			for (auto k = n; k; --k)
			{
				if (const auto prev = find(children[k - 1]))
				{
					ptr->prev = prev;
					break;
				}
			}
			for (auto k = n + 1; k < children.size(); ++k)
			{
				if (const auto next = find(children[k]))
				{
					ptr->next = next;
					break;
				}				
			}
		}
	}

	auto step = 0;
	const auto print = [&]
	{
		string str;
		str.appendf("\r\n#%1: "_s, step);
		for (auto&& ptr : box_list)
		{
			string type;
			const gui_widget_data* next = ptr.widget;
			while (next)
			{
				type.appendf(".[%1]%2"_s, next->display == CSS_DISPLAY_BLOCK ? "blk"_s : "inl"_s, string_view(next->type));
				next = next->parent;
			}
			str.appendf("\r\nBox[%1]: %2"_s, ptr.widget->index, string_view(type));
			str.appendf("[%1,%2,%3,%4]"_s
				, lround(am_value(ptr.cvar(gui_widget_box_var_left)))
				, lround(am_value(ptr.cvar(gui_widget_box_var_top)))
				, lround(am_value(ptr.cvar(gui_widget_box_var_width)))
				, lround(am_value(ptr.cvar(gui_widget_box_var_height))));
		}
		icy::win32_debug_print(str);
	};
	am_Num margins_array[256];
	size_t margins_count = 0u;

	const auto collapse = [&margins_array, &margins_count]() -> am_Num
	{
		auto max_positive = 0.0f;
		auto min_negative = -FLT_MAX;
		for (auto k = 0u; k < margins_count; ++k)
		{
			const auto val = margins_array[k];
			if (val >= 0)
				max_positive = std::max(max_positive, val);
			else
				min_negative = std::max(min_negative, val);
		}
		const auto val = max_positive + (min_negative == -FLT_MAX ? 0 : min_negative);
		margins_count = 0;
		return val;
	};
	const auto margin_append = [&margins_array, &margins_count](const gui_widget_box& box, const gui_widget_box_var var)
	{
		if (margins_count == std::size(margins_array))
			return;
		margins_array[margins_count++] = am_value(box.cvar(var));
	};


	
	for (auto&& box : box_list)
	{
		box.widget->borders[0].size = lround(am_value(box.cvar(gui_widget_box_var_border_left)));
		box.widget->borders[1].size = lround(am_value(box.cvar(gui_widget_box_var_border_top)));
		box.widget->borders[2].size = lround(am_value(box.cvar(gui_widget_box_var_border_right)));
		box.widget->borders[3].size = lround(am_value(box.cvar(gui_widget_box_var_border_bottom)));

		box.widget->content_box[0] = lround(am_value(box.cvar(gui_widget_box_var_left)));
		box.widget->content_box[1] = lround(am_value(box.cvar(gui_widget_box_var_top)));
		box.widget->content_box[2] = lround(am_value(box.cvar(gui_widget_box_var_width)));
		box.widget->content_box[3] = lround(am_value(box.cvar(gui_widget_box_var_height)));		
	}

    m_state |= gui_window_state_has_list;
    return error_type();
}
error_type gui_window_data_sys::update(gui_select_system& css, gui_widget_data& widget) noexcept
{
	if (widget.type == "text"_s && widget.parent && widget.parent->type == "style"_s)
	{
		ICY_ERROR(widget.inline_style.initialize(css.ctx(), ""_s, widget.text.value.str()));
		ICY_ERROR(css.append(widget.inline_style));
		return error_type();
	}

	ICY_ERROR(css.calc(widget));
	const auto style = widget.css.style();
	const auto is_root = widget.type == "html"_s;
	const auto visible = css_computed_visibility(style);

	widget.display = css_computed_display(style, is_root);
	widget.position = css_computed_position(style);

	if (widget.parent)
	{
		widget.state &= ~gui_widget_state_visible;
		widget.state &= ~gui_widget_state_collapse;

		if (widget.display == CSS_DISPLAY_INHERIT)
			widget.display = widget.parent->display;
		if (widget.position == CSS_POSITION_INHERIT)
			widget.position = widget.parent->position;		
	}

	switch (visible)
	{
	case CSS_VISIBILITY_INHERIT:
		if (widget.parent && widget.display != CSS_DISPLAY_NONE)
		{
			if (widget.parent->state & gui_widget_state_visible) 
				widget.state |= gui_widget_state_visible;
			else if (widget.parent->state & gui_widget_state_collapse) 
				widget.state |= gui_widget_state_collapse;
		}
		break;

	case CSS_VISIBILITY_VISIBLE:
		widget.state |= gui_widget_state_visible;
		break;

	case CSS_VISIBILITY_COLLAPSE:
		widget.state |= gui_widget_state_collapse;
		break;

	case CSS_VISIBILITY_HIDDEN:
		break;
	}

	if (widget.state & gui_widget_state_visible)
	{
		css_fixed len = 0;
		css_unit unit = css_unit::CSS_UNIT_PX;

		const auto calc = [&](float& ref)
		{
			switch (unit)
			{
			case CSS_UNIT_EM:
				if (widget.parent)
					ref = FIXTOFLT(len) * widget.parent->font_size;
				break;

			case CSS_UNIT_PX:
				ref = FIXTOFLT(len);
				break;

			case CSS_UNIT_PT:
				ref = FIXTOFLT(len) / 72.0f * m_dpi;
				break;

			default:
				ref = 0;
			}
		};

		switch (css_computed_font_size(style, &len, &unit))
		{
		case CSS_FONT_SIZE_INHERIT:
			if (widget.parent)
				widget.font_size = widget.parent->font_size;
			break;
		case CSS_FONT_SIZE_XX_SMALL:
			break;
		case CSS_FONT_SIZE_X_SMALL:
			break;
		case CSS_FONT_SIZE_SMALL:
			break;
		case CSS_FONT_SIZE_MEDIUM:
			break;
		case CSS_FONT_SIZE_LARGE:
			break;
		case CSS_FONT_SIZE_X_LARGE:
			break;
		case CSS_FONT_SIZE_XX_LARGE:
			break;
		case CSS_FONT_SIZE_LARGER:
			break;
		case CSS_FONT_SIZE_SMALLER:
			break;
		case CSS_FONT_SIZE_DIMENSION:
			calc(widget.font_size);
			break;
		}


		switch (css_computed_line_height(style, &len, &unit))
		{
		case CSS_LINE_HEIGHT_INHERIT:
			if (widget.parent)
				widget.line_size = widget.parent->line_size;
			break;
		case CSS_LINE_HEIGHT_NUMBER:
			widget.line_size = widget.font_size * FIXTOFLT(len);
			break;
		case CSS_LINE_HEIGHT_DIMENSION:
			calc(widget.line_size);
			break;
		default:
			widget.line_size = widget.font_size * 1.2f;
			break;
		}

		css_color color;
		if (!widget.text.value.str().empty())
		{
			const auto& render = m_system->render();
			ICY_ERROR(render.create_text(widget.text.layout, m_font, widget.text.value.str()));
			const auto layout = static_cast<IDWriteTextLayout*>(widget.text.layout.value.get());
			const auto range = DWRITE_TEXT_RANGE{ 0, UINT32_MAX };
			ICY_COM_ERROR(layout->SetFontSize(widget.font_size, range));
		}
		else
		{
			widget.text.layout = gui_text();
		}

		switch (css_computed_color(style, &color))
		{
		case CSS_COLOR_INHERIT:
			if (widget.parent)
				widget.text.color = widget.parent->text.color;
			break;
		case CSS_COLOR_COLOR:
			widget.text.color = color::from_bgra(color);
			break;
		};

		switch (css_computed_background_color(style, &color))
		{
		case CSS_COLOR_INHERIT:
			if (widget.parent)
				widget.bkcolor = widget.parent->bkcolor;
			break;
		case CSS_COLOR_COLOR:
			widget.bkcolor = color::from_bgra(color);
			break;
		}
		ICY_ERROR(m_list.push_back(&widget));
	}

	for (auto&& ptr : widget.children)
		ICY_ERROR(update(css, *ptr));

	return error_type();
}
error_type gui_window_data_sys::input_key_press(const key key, const key_mod mods) noexcept
{
    return error_type();
}
error_type gui_window_data_sys::input_key_hold(const key key, const key_mod mods) noexcept
{

    return error_type();
}
error_type gui_window_data_sys::input_key_release(const key key, const key_mod mods) noexcept
{

    return error_type();
}
error_type gui_window_data_sys::input_mouse_move(const int32_t px, const int32_t py, const key_mod mods) noexcept
{

    return error_type();
}
error_type gui_window_data_sys::input_mouse_wheel(const int32_t px, const int32_t py, const key_mod mods, const int32_t wheel) noexcept
{

    return error_type();
}
error_type gui_window_data_sys::input_mouse_release(const key key, const int32_t px, const int32_t py, const key_mod mods) noexcept
{

    return error_type();
}
error_type gui_window_data_sys::input_mouse_press(const key key, const int32_t px, const int32_t py, const key_mod mods) noexcept
{

    return error_type();
}
error_type gui_window_data_sys::input_mouse_hold(const key key, const int32_t px, const int32_t py, const key_mod mods) noexcept
{

    return error_type();
}
error_type gui_window_data_sys::input_mouse_double(const key key, const int32_t px, const int32_t py, const key_mod mods) noexcept
{

    return error_type();
}
error_type gui_window_data_sys::input_text(const string_view text) noexcept
{

    return error_type();
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
        const auto args = event.val.get<gui_widget_data::insert_args>();
        ICY_ASSERT(args, "");
        ICY_ERROR(gui_widget_data::modify(m_data, *args));
        break;
    }
    case gui_window_event_type::destroy:
    {
        gui_widget_data::erase(m_data, event.index);
        break;
    }
    default:
        return error_type();
    }
    m_state &= ~gui_window_state_has_list;
    return error_type();
}
window_size gui_window_data_sys::size(const uint32_t widget) const noexcept
{
	window_size wsize;
	if (const auto ptr = m_data.try_find(widget))
	{
		wsize.x = ptr->get()->content_box[2];
		wsize.y = ptr->get()->content_box[3];
	}
	return wsize;
}
error_type gui_window_data_sys::render(gui_texture& texture) noexcept
{
	texture.draw_begin(m_data.front().value->bkcolor);
    for (auto k = 1u; k < m_list.size(); ++k)
    {
		auto widget = m_list[k];
		if (!(widget->state & gui_widget_state_visible))
			continue;

		const auto x = float(widget->content_box[0]);
		const auto y = float(widget->content_box[1]);
		const auto w = widget->content_box[2];
		const auto h = widget->content_box[3];

		if (widget->bkcolor.a)
			texture.fill_rect({ x, y, x + w, y + h }, widget->bkcolor);

		if (widget->text.layout.value)
        {
            ICY_ERROR(texture.draw_text(x, y, widget->text.color, widget->text.layout.value));
        }
    }

    ICY_ERROR(texture.draw_end());
    return error_type();
}

/*
error_type gui_select_output::preproc(const gui_window_data_sys& window, gui_widget_data& widget) const noexcept
{
	auto style = m_value->styles[CSS_PSEUDO_ELEMENT_NONE];
	auto len = 0;
	auto unit = css_unit::CSS_UNIT_PX;


	


	const auto func = [&](const size_t index)
	{
		uint8_t(*func_calc_border)(const css_computed_style * style, css_fixed * len, css_unit * unit);
		uint8_t(*func_calc_margin)(const css_computed_style * style, css_fixed * len, css_unit * unit);
		uint8_t(*func_calc_padding)(const css_computed_style * style, css_fixed * len, css_unit * unit);

		switch (index)
		{
		case 0:
			func_calc_border = &css_computed_border_left_width;
			func_calc_margin = &css_computed_margin_left;
			func_calc_padding = &css_computed_padding_left;
			break;
		case 1:
			func_calc_border = &css_computed_border_top_width;
			func_calc_margin = &css_computed_margin_top;
			func_calc_padding = &css_computed_padding_top;
			break;
		case 2:
			func_calc_border = &css_computed_border_right_width;
			func_calc_margin = &css_computed_margin_right;
			func_calc_padding = &css_computed_padding_right;
			break;
		case 3:
			func_calc_border = &css_computed_border_bottom_width;
			func_calc_margin = &css_computed_margin_bottom;
			func_calc_padding = &css_computed_padding_bottom;
			break;
		default:
			return;
		}

	

		widget.size[index].margin = 0;
		switch (func_calc_margin(style, &len, &unit))
		{
		case CSS_MARGIN_SET:
			widget.size[index].margin = gui_size_calc(window, widget, unit, len);
			break;
		case CSS_MARGIN_INHERIT:
			if (widget.parent)
				widget.size[index].margin = widget.parent->size[index].margin;
			break;
		}

		widget.size[index].padding = 0;
		switch (func_calc_padding(style, &len, &unit))
		{
		case CSS_PADDING_SET:
			widget.size[index].padding = gui_size_calc(window, widget, unit, len);
			break;
		case CSS_PADDING_INHERIT:
			if (widget.parent)
				widget.size[index].padding = widget.parent->size[index].padding;
			break;
		}
	};

	func(0);
	func(1);
	func(2);
	func(3);

	switch (widget.position)
	{
	case CSS_POSITION_STATIC:
	case CSS_POSITION_RELATIVE:
	{
		//	relative: or offset by [left/top/right/bot]
		break;
	}
	case CSS_POSITION_ABSOLUTE:

	default:
		break;
	}
	if (widget.text.value)
	{
		const auto box = window.box(widget.index, gui_widget_box_type::inner);
		const auto& render = window.system().render();
		ICY_ERROR(render.create_text(widget.text.layout, window.font(), widget.text.value.str()));
		const auto layout = static_cast<IDWriteTextLayout*>(widget.text.layout.value.get());
		const auto range = DWRITE_TEXT_RANGE{ 0, UINT32_MAX };
		ICY_COM_ERROR(layout->SetFontSize(widget.font_size, range));

		css_color color = 0;
		switch (css_computed_color(style, &color))
		{
		case CSS_COLOR_INHERIT:
			if (widget.parent)
				widget.text.color = widget.parent->text.color;
			break;
		case CSS_COLOR_COLOR:
			widget.text.color = color::from_rgba(color);
			break;
		};
	}
	else
	{
		widget.text.layout = gui_text();
		widget.text.color = colors::black;
	}

	return error_type();
}
error_type gui_select_output::postproc(const gui_window_data_sys& window, gui_widget_data& widget) const noexcept
{
	auto style = m_value->styles[CSS_PSEUDO_ELEMENT_NONE];
	auto len = 0;
	auto unit = css_unit::CSS_UNIT_PX;

	/*const auto func_wh = [&](const uint32_t index)
	{
		uint8_t(*func_calc)(const css_computed_style * style, css_fixed * len, css_unit * unit) = nullptr;
		am_Var* var = nullptr;
		am_Var* var_parent = nullptr;
		am_Constraint* con = nullptr;
		switch (index)
		{
		case 0:
			func_calc = css_computed_width;
			var = widget.dx.var_val;
			con = widget.dx.con_val;
			var_parent = widget.parent ? widget.parent->dx.var_val : nullptr;
			break;
		case 1:
			func_calc = css_computed_min_width;
			var = widget.dx.var_min;
			con = widget.dx.con_min;
			var_parent = widget.parent ? widget.parent->dx.var_min : nullptr;
			break;
		case 2:
			func_calc = css_computed_max_width;
			var = widget.dx.var_max;
			con = widget.dx.con_max;
			var_parent = widget.parent ? widget.parent->dx.var_max : nullptr;
			break;
		case 3:
			func_calc = css_computed_height;
			var = widget.dy.var_val;
			con = widget.dy.con_val;
			var_parent = widget.parent ? widget.parent->dy.var_val : nullptr;
			break;
		case 4:
			func_calc = css_computed_min_height;
			var = widget.dy.var_min;
			con = widget.dy.con_min;
			var_parent = widget.parent ? widget.parent->dy.var_min : nullptr;
			break;
		case 5:
			func_calc = css_computed_max_height;
			var = widget.dy.var_max;
			con = widget.dy.con_max;
			var_parent = widget.parent ? widget.parent->dy.var_max : nullptr;
			break;
		default:
			return;
		}

		switch (func_calc_offset(style, &len, &unit))
		{
		case CSS_LEFT_SET:
			am_suggest(widget.size[index].var, calc());
			break;
		case CSS_LEFT_INHERIT:
		{
			auto con = widget.size[index].con;
			am_resetconstraint(con);
			am_addterm(con, widget.size[index].var, 1);
			am_setrelation(con, AM_EQUAL);
			am_addterm(con, widget.parent->size[index].var, 1);
			break;
		}
		case CSS_LEFT_AUTO:
			break;
		}
		switch (func_calc(style, &len, &unit))
		{
		case CSS_WIDTH_SET:
			static_assert(CSS_MIN_WIDTH_SET == CSS_WIDTH_SET, "");
			static_assert(CSS_MAX_WIDTH_SET == CSS_WIDTH_SET, "");
			static_assert(CSS_MIN_HEIGHT_SET == CSS_WIDTH_SET, "");
			static_assert(CSS_MAX_HEIGHT_SET == CSS_WIDTH_SET, "");
			static_assert(CSS_HEIGHT_SET == CSS_WIDTH_SET, "");
			am_suggest(var, calc());
			break;
		case CSS_WIDTH_INHERIT:
			static_assert(CSS_MIN_WIDTH_INHERIT == CSS_WIDTH_INHERIT, "");
			static_assert(CSS_MAX_WIDTH_INHERIT == CSS_WIDTH_INHERIT, "");
			static_assert(CSS_MIN_HEIGHT_INHERIT == CSS_WIDTH_INHERIT, "");
			static_assert(CSS_MAX_HEIGHT_INHERIT == CSS_WIDTH_INHERIT, "");
			static_assert(CSS_HEIGHT_INHERIT == CSS_WIDTH_INHERIT, "");
			if (widget.parent)
			{
				am_resetconstraint(con);
				am_addterm(con, var, 1);
				am_setrelation(con, AM_EQUAL);
				am_addterm(con, var_parent, 1);
			}
			break;
		}
	};


	func_wh(0);
	func_wh(1);
	func_wh(2);
	func_wh(3);
	func_wh(4);
	func_wh(5);
	


	switch (widget.display)
	{
	case CSS_DISPLAY_BLOCK:
	{
		return error_type();
		break;
	}
	case CSS_DISPLAY_INLINE:
	{
		if (const auto parent = widget.parent)
		{
			const auto it = std::find(parent->children.begin(), parent->children.end(), &widget);
			const auto index = std::distance(parent->children.begin(), it);
			const auto prev = index ? parent->children[index - 1] : parent;

			const auto lhs = am_value(prev->size[2].var);
			const auto top = am_value(parent->size[1].var);
			auto rhs = lhs;
			auto bot = top;

			if (widget.text.layout)
			{
				DWRITE_TEXT_METRICS metrics = {};
				ICY_COM_ERROR(widget.text.layout->GetMetrics(&metrics));
				rhs += metrics.width;
				bot += metrics.height;
			}
			am_suggest(widget.size[0].var, lhs);
			am_suggest(widget.size[1].var, top);
			am_suggest(widget.size[2].var, rhs);
			am_suggest(widget.size[3].var, bot);
		}
		break;
	}
	case CSS_DISPLAY_NONE:
	{
		break;
	}

	default:
		return error_type();
	}

	return error_type();
}


*/



/*
for (auto pass = 0u; pass < 2u; ++pass)
	{
		step = 0;
		const auto postproc = pass > 0;
		if (postproc)
		{
			for (auto&& box : box_list)
			{
				if (box.widget->text.layout)
				{
					const auto max_width = am_value(box.var(gui_widget_box_var_width));
					ICY_COM_ERROR(box.widget->text.layout->SetMaxWidth(max_width));

					DWRITE_TEXT_METRICS metrics;
					ICY_COM_ERROR(box.widget->text.layout->GetMetrics(&metrics));
					am_suggest(box.var(gui_widget_box_var_width), metrics.widthIncludingTrailingWhitespace, AM_WEAK);
				}
			}
		}
		for (auto&& box : box_list)
		{
			++step;

			auto child = box.child;
			if (!child)
				continue;

			auto is_block = false;
			while (child)
			{
				switch (child->widget->display)
				{
				case CSS_DISPLAY_BLOCK:
					is_block = true;
					break;
				default:
					break;
				}
				child = child->next;
			}
			child = box.child;

			if (is_block == false)
			{
				auto& conw = box.cons[gui_widget_box_con_inline_width];
				if (!conw)
					conw = am_newconstraint(solver, AM_WEAK);
				else
					am_resetconstraint(conw);

				am_addterm(conw, box.var(gui_widget_box_var_width), 1.0);
				am_setrelation(conw, AM_GREATEQUAL);

				while (child)
				{
					if (child->prev && postproc)
					{
						margin_append(*child, gui_widget_box_var_margin_left);
						margin_append(*child->prev, gui_widget_box_var_margin_right);
						am_addconstant(conw, collapse() * 0.5f);
					}
					else if (child->prev)
					{
						am_addterm(conw, child->cvar(gui_widget_box_var_margin_left), 0.5);
					}
					else // first
					{
						am_addterm(conw, child->cvar(gui_widget_box_var_margin_left), 1.0);
					}
					am_addterm(conw, child->cvar(gui_widget_box_var_border_left), 1.0);
					am_addterm(conw, child->cvar(gui_widget_box_var_padding_left), 1.0);
					am_addterm(conw, child->var(gui_widget_box_var_width), 1.0);
					am_addterm(conw, child->cvar(gui_widget_box_var_padding_right), 1.0);
					am_addterm(conw, child->cvar(gui_widget_box_var_border_right), 1.0);

					if (child->next && postproc)
					{
						margin_append(*child, gui_widget_box_var_margin_right);
						margin_append(*child->next, gui_widget_box_var_margin_left);
						am_addconstant(conw, collapse() * 0.5f);
					}
					else if (child->next)
					{
						am_addterm(conw, child->cvar(gui_widget_box_var_margin_right), 0.5);
					}
					else // last
					{
						am_addterm(conw, child->cvar(gui_widget_box_var_margin_right), 1.0);
					}

					if (true) // set child top
					{
						auto& con = child->cons[gui_widget_box_con_top];
						if (!con)
							con = am_newconstraint(solver, AM_STRONG);
						else
							am_resetconstraint(con);
						am_addterm(con, child->var(gui_widget_box_var_top), 1.0);
						am_setrelation(con, AM_LESSEQUAL);
						am_addterm(con, child->cvar(gui_widget_box_var_margin_top), 1.0);
						am_addterm(con, child->cvar(gui_widget_box_var_border_top), 1.0);
						am_addterm(con, child->cvar(gui_widget_box_var_padding_top), 1.0);
						am_addterm(con, box.var(gui_widget_box_var_top), 1.0);
						am_add(con);
					}
					if (true) // set child left
					{
						auto& con = child->cons[gui_widget_box_con_left];
						if (!con)
							con = am_newconstraint(solver, AM_STRONG);
						else
							am_resetconstraint(con);
						am_addterm(con, child->var(gui_widget_box_var_left), 1.0);
						am_setrelation(con, AM_GREATEQUAL);
						if (const auto prev = child->prev)
						{
							am_addterm(con, prev->var(gui_widget_box_var_left), 1.0);
							am_addterm(con, prev->var(gui_widget_box_var_width), 1.0);
							am_addterm(con, prev->cvar(gui_widget_box_var_padding_right), 1.0);
							am_addterm(con, prev->cvar(gui_widget_box_var_border_right), 1.0);

							if (postproc)
							{
								margin_append(*prev, gui_widget_box_var_margin_right);
								margin_append(*child, gui_widget_box_var_margin_left);
								am_addconstant(con, collapse());
							}
							else
							{
								am_addterm(con, prev->cvar(gui_widget_box_var_margin_right), 0.5);
								am_addterm(con, box.cvar(gui_widget_box_var_margin_left), 0.5);
							}
						}
						else
						{
							am_addterm(con, box.var(gui_widget_box_var_left), 1.0);
							am_addterm(con, child->cvar(gui_widget_box_var_margin_left), 1.0);
						}
						am_addterm(con, child->cvar(gui_widget_box_var_border_left), 1.0);
						am_addterm(con, child->cvar(gui_widget_box_var_padding_left), 1.0);
						am_add(con);
					}
					if (true) // set child height
					{
						auto& con = child->cons[gui_widget_box_con_inline_height];
						if (!con)
							con = am_newconstraint(solver, AM_STRONG);
						else
							am_resetconstraint(con);
						am_addterm(con, child->cvar(gui_widget_box_var_margin_top), 1.0);
						am_addterm(con, child->cvar(gui_widget_box_var_border_top), 1.0);
						am_addterm(con, child->cvar(gui_widget_box_var_padding_top), 1.0);
						am_addterm(con, child->var(gui_widget_box_var_height), 1.0);
						am_addterm(con, child->cvar(gui_widget_box_var_margin_bottom), 1.0);
						am_addterm(con, child->cvar(gui_widget_box_var_border_bottom), 1.0);
						am_addterm(con, child->cvar(gui_widget_box_var_padding_bottom), 1.0);
						am_setrelation(con, AM_LESSEQUAL);
						am_addterm(con, box.var(gui_widget_box_var_height), 1.0);
						am_add(con);
					}
					child = child->next;
				}
				am_add(conw);
			}
			else
			{
				auto& conh = box.cons[gui_widget_box_con_block_height];
				//if (!conh)
				//	conh = am_newconstraint(solver, AM_WEAK);
				//else
				//	am_resetconstraint(conh);

				am_addterm(conh, box.var(gui_widget_box_var_height), 1.0);
				am_setrelation(conh, AM_LESSEQUAL);

				while (child)
				{
					if (child->prev && postproc)	// try collapse
					{
						margin_append(*child, gui_widget_box_var_margin_top);
						margin_append(*child->prev, gui_widget_box_var_margin_bottom);
						am_addconstant(conh, collapse() * 0.5f);
					}
					else if (child->prev)
					{
						am_addterm(conh, child->cvar(gui_widget_box_var_margin_top), 0.5);
					}
					else // first
					{
						am_addterm(conh, child->cvar(gui_widget_box_var_margin_top), 1.0);
					}
					am_addterm(conh, child->cvar(gui_widget_box_var_border_top), 1.0);
					am_addterm(conh, child->cvar(gui_widget_box_var_padding_top), 1.0);
					am_addterm(conh, child->var(gui_widget_box_var_height), 1.0);
					am_addterm(conh, child->cvar(gui_widget_box_var_padding_bottom), 1.0);
					am_addterm(conh, child->cvar(gui_widget_box_var_border_bottom), 1.0);

					if (child->next && postproc)	// try collapse
					{
						margin_append(*child, gui_widget_box_var_margin_bottom);
						margin_append(*child->next, gui_widget_box_var_margin_top);
						am_addconstant(conh, collapse() * 0.5f);
					}
					else if (child->next)
					{
						am_addterm(conh, child->cvar(gui_widget_box_var_margin_bottom), 0.5);
					}
					else // last
					{
						am_addterm(conh, child->cvar(gui_widget_box_var_margin_bottom), 1.0);
					}

					//	child left
					if (true)
					{
						auto& con = child->cons[gui_widget_box_con_left];
						if (!con)
							con = am_newconstraint(solver, AM_STRONG);
						else
							am_resetconstraint(con);
						am_addterm(con, child->var(gui_widget_box_var_left), 1.0);
						am_setrelation(con, AM_EQUAL);
						am_addterm(con, child->cvar(gui_widget_box_var_margin_left), 1.0);
						am_addterm(con, child->cvar(gui_widget_box_var_border_left), 1.0);
						am_addterm(con, child->cvar(gui_widget_box_var_padding_left), 1.0);
						am_addterm(con, box.var(gui_widget_box_var_left), 1.0);
						am_add(con);
					}
					//	child top
					if (true)
					{
						auto& con = child->cons[gui_widget_box_con_top];
						if (!con)
							con = am_newconstraint(solver, AM_STRONG);
						else
							am_resetconstraint(con);
						am_addterm(con, child->var(gui_widget_box_var_top), 1.0);
						am_setrelation(con, AM_EQUAL);
						if (const auto prev = child->prev)
						{
							am_addterm(con, prev->var(gui_widget_box_var_top), 1.0);
							am_addterm(con, prev->var(gui_widget_box_var_height), 1.0);
							am_addterm(con, prev->cvar(gui_widget_box_var_padding_bottom), 1.0);
							am_addterm(con, prev->cvar(gui_widget_box_var_border_bottom), 1.0);

							//	margin collapse
							if (postproc)
							{
								margin_append(*prev, gui_widget_box_var_margin_bottom);
								margin_append(*child, gui_widget_box_var_margin_top);
								am_addconstant(con, collapse());
							}
							else
							{
								am_addterm(con, prev->cvar(gui_widget_box_var_margin_bottom), 1.0);
								am_addterm(con, box.cvar(gui_widget_box_var_margin_top), 1.0);
							}
						}
						else // is first
						{
							am_addterm(con, box.var(gui_widget_box_var_top), 1.0);
							am_addterm(con, child->cvar(gui_widget_box_var_margin_top), 1.0);
						}
						am_addterm(con, child->cvar(gui_widget_box_var_border_top), 1.0);
						am_addterm(con, child->cvar(gui_widget_box_var_padding_top), 1.0);
						am_add(con);
					}
					//	child width
					if (true)
					{
						auto& con = child->cons[gui_widget_box_con_block_width];
						if (!con)
							con = am_newconstraint(solver, AM_MEDIUM);
						else
							am_resetconstraint(con);
						am_addterm(con, child->var(gui_widget_box_var_width), 1.0);
						am_addterm(con, child->cvar(gui_widget_box_var_margin_left), 1.0);
						am_addterm(con, child->cvar(gui_widget_box_var_border_left), 1.0);
						am_addterm(con, child->cvar(gui_widget_box_var_padding_left), 1.0);
						am_addterm(con, child->cvar(gui_widget_box_var_margin_right), 1.0);
						am_addterm(con, child->cvar(gui_widget_box_var_border_right), 1.0);
						am_addterm(con, child->cvar(gui_widget_box_var_padding_right), 1.0);
						am_setrelation(con, AM_LESSEQUAL);
						am_addterm(con, box.var(gui_widget_box_var_width), 1.0);
						am_add(con);
					}
					child = child->next;
				}
				am_add(conh);
			}
			print();
		}
		am_updatevars(solver);
	}
*/
#define AM_IMPLEMENTATION
#include "amoeba.h"


/*if (auto ptr = box.child)
{
	auto con_h = box.new_con(AM_EQUAL, AM_MEDIUM, gui_widget_box_var_height, -1.0);
	am_addconstant(con_h, box.widget->line_size * 2);
	while (ptr)
	{
		am_addterm(con_h, ptr->cvar(gui_widget_box_var_margin_top), 1.0);
		am_addterm(con_h, ptr->cvar(gui_widget_box_var_border_top), 1.0);
		am_addterm(con_h, ptr->cvar(gui_widget_box_var_padding_top), 1.0);
		am_addterm(con_h, ptr->var(gui_widget_box_var_height), 1.0);
		am_addterm(con_h, ptr->cvar(gui_widget_box_var_padding_bottom), 1.0);
		am_addterm(con_h, ptr->cvar(gui_widget_box_var_border_bottom), 1.0);
		am_addterm(con_h, ptr->cvar(gui_widget_box_var_margin_bottom), 1.0);
		ptr = ptr->next;
	}
	am_add(con_h);
}
if (!box.next)
{
	auto con_h = box.new_con(AM_EQUAL, AM_MEDIUM, gui_widget_box_var_height, -1.0);
	am_addterm(con_h, box.var(gui_widget_box_var_top), 1.0);
	am_addterm(con_h, box.var(gui_widget_box_var_height), 1.0);
	am_addterm(con_h, box.cvar(gui_widget_box_var_padding_bottom), 1.0);
	am_addterm(con_h, box.cvar(gui_widget_box_var_border_bottom), 1.0);
	am_addterm(con_h, box.cvar(gui_widget_box_var_margin_bottom), 1.0);
	if (pbox)
		am_addconstant(con_h, pbox->widget->line_size);
	am_add(con_h);
}
if (pbox)
{
	auto con_max_h = pbox->new_con(AM_LESSEQUAL, AM_MEDIUM, gui_widget_box_var_height, -1.0);
	am_addterm(con_max_h, box.cvar(gui_widget_box_var_margin_top), 1);
	am_addterm(con_max_h, box.cvar(gui_widget_box_var_border_top), 1);
	am_addterm(con_max_h, box.cvar(gui_widget_box_var_padding_top), 1);
	am_addterm(con_max_h, box.cvar(gui_widget_box_var_height), 1);
	am_addterm(con_max_h, box.cvar(gui_widget_box_var_padding_bottom), 1);
	am_addterm(con_max_h, box.cvar(gui_widget_box_var_margin_bottom), 1);
	am_addterm(con_max_h, box.cvar(gui_widget_box_var_border_bottom), 1);
	am_add(con_max_h);
}*/
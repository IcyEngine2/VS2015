#include "icy_gui_style.hpp"
#include "icy_gui_system.hpp"
#include "icy_gui_window.hpp"
#include "../libs/css/libcss/include/libcss/libcss.h"
#include "../libs/css/libwapcaplet/include/libwapcaplet/libwapcaplet.h"
#if _DEBUG
#pragma comment(lib, "libcssd")
#else
#pragma comment(lib, "libcss")
#endif

//  left | margin | border | padding | content
//  min, max, width
//  padding
//  left, right

using namespace icy;

static error_type make_css_error(const css_error code) noexcept
{
	return error_type(uint32_t(code), error_source_css);
}

static error_type css_error_to_string(const unsigned code, const string_view, string& str) noexcept
{
	const auto ptr = css_error_to_string(css_error(code));
	if (!ptr)
		return make_stdlib_error(std::errc::invalid_argument);
	return icy::to_string(string_view(ptr, strlen(ptr)), str);
}
const error_source icy::error_source_css = register_error_source("css"_s, css_error_to_string);

gui_select_output::~gui_select_output() noexcept
{
	if (m_value)
		css_select_results_destroy(m_value);
}
css_computed_style* gui_select_output::style() const noexcept
{
	return m_value->styles[CSS_PSEUDO_ELEMENT_NONE];
}

gui_select_system::~gui_select_system() noexcept
{
	if (m_value)
		css_select_ctx_destroy(m_value);
}
error_type gui_select_system::initialize(lwc_context_s* ctx) noexcept
{
	if (const auto error = css_select_ctx_create(ctx, &m_value))
		return make_css_error(error);
	m_ctx = ctx;
	return error_type();
}
error_type gui_select_system::append(gui_style_data& sheet) noexcept
{
	if (const auto error = css_select_ctx_append_sheet(m_value, sheet.m_value, CSS_ORIGIN_USER, nullptr))
		return make_css_error(error);
	return error_type();
}
void gui_select_system::remove(gui_style_data& sheet) noexcept
{
	css_select_ctx_remove_sheet(m_value, sheet.m_value);
}
error_type gui_select_system::calc(gui_widget_data& widget) const noexcept
{
	css_media media = {};
	media.type = css_media_type::CSS_MEDIA_SCREEN;
#define NODE_INIT const auto ctx = static_cast<lwc_context*>(pw); auto cnode = static_cast<const gui_widget_data*>(node)
	
	css_select_handler handler = {};
	handler.handler_version = CSS_SELECT_HANDLER_VERSION_1;
	handler.node_name = [](void* pw, void* node, css_qname* qname)
	{
		qname->name = nullptr;
		qname->ns = nullptr;
		NODE_INIT;
		auto error = lwc_intern_string(ctx, cnode->type.bytes().data(), cnode->type.bytes().size(), &qname->name);
		return error ? CSS_NOMEM : CSS_OK;
	};
	handler.node_classes = [](void* pw, void* node, lwc_string*** classes, uint32_t* n_classes)
	{
		*classes = nullptr;
		*n_classes = 0;
		NODE_INIT;
		const auto str = cnode->attr.find("class"_s);
		array<string_view> vec(ctx->realloc, ctx->user);

		if (str != cnode->attr.end())
		{
			if (split(str->value.str(), vec))
				return CSS_NOMEM;
		}
		if (vec.empty())
			return CSS_OK;

		*n_classes = uint32_t(vec.size());
		*classes = static_cast<lwc_string**>(ctx->realloc(nullptr, sizeof(lwc_string*) * vec.size(), ctx->user));
		memset(*classes, 0, sizeof(lwc_string*) * vec.size());
		auto k = 0u;
		for (; k < vec.size(); ++k)
		{
			auto& addr = (*classes)[k];
			if (lwc_intern_string(ctx, vec[k].bytes().data(), vec[k].bytes().size(), &addr))
				break;
		}
		if (k != vec.size())
		{
			for (*n_classes = k; *n_classes; --(*n_classes))
				lwc_string_unref(ctx, (*classes)[k]);

			ctx->realloc(*classes, 0, ctx->user);
			*classes = nullptr;
			return CSS_NOMEM;
		}
		return CSS_OK;
	};
	handler.node_id = [](void* pw, void* node, lwc_string** id)
	{
		*id = nullptr;
		NODE_INIT;

		const auto str = cnode->attr.find("id"_s);
		if (str != cnode->attr.end())
		{
			if (lwc_intern_string(ctx, str->value.str().bytes().data(), str->value.str().bytes().size(), id))
				return CSS_NOMEM;
		}
		return CSS_OK;
	};
	handler.named_ancestor_node = [](void* pw, void* node, const css_qname* qname, void** ancestor)
	{
		*ancestor = nullptr;
		NODE_INIT;
		const auto name = string_view(lwc_string_data(qname->name), strlen(lwc_string_data(qname->name)));

		while (const auto pnode = cnode->parent)
		{
			if (pnode->type == name)
			{
				*ancestor = const_cast<gui_widget_data*>(pnode);
				break;
			}
			cnode = pnode;
		}
		return CSS_OK;
	};
	handler.named_parent_node = [](void* pw, void* node, const css_qname* qname, void** parent)
	{
		*parent = nullptr;
		NODE_INIT;
		const auto name = string_view(lwc_string_data(qname->name), strlen(lwc_string_data(qname->name)));

		if (const auto pnode = cnode->parent)
		{
			if (pnode->type == name)
				*parent = const_cast<gui_widget_data*>(pnode);
		}
		return CSS_OK;
	};
	handler.named_sibling_node = [](void* pw, void* node, const css_qname* qname, void** sibling)
	{
		*sibling = nullptr;
		NODE_INIT;
		const auto name = string_view(lwc_string_data(qname->name), strlen(lwc_string_data(qname->name)));

		if (const auto pnode = cnode->parent)
		{
			const auto& children = pnode->children;
			const auto it = std::find(children.begin(), children.end(), cnode);
			gui_widget_data* next = nullptr;
			if (it + 1 != children.end())
				next = (*it + 1);
			if (next && next->type == name)
			{
				*sibling = next;
				return CSS_OK;
			}
		}
		return CSS_OK;
	};
	handler.named_generic_sibling_node = [](void* pw, void* node, const css_qname* qname, void** sibling)
	{
		*sibling = nullptr;
		NODE_INIT;
		const auto name = string_view(lwc_string_data(qname->name), strlen(lwc_string_data(qname->name)));
		if (const auto pnode = cnode->parent)
		{
			const auto& children = pnode->children;
			auto it = std::find(children.begin(), children.end(), cnode);
			++it;
			while (it != children.end())
			{
				const auto snode = *it;
				if (snode->type == name)
				{
					*sibling = snode;
					break;
				}
				++it;
			}
		}
		return CSS_OK;
	};
	handler.parent_node = [](void* pw, void* node, void** parent)
	{
		*parent = nullptr;
		NODE_INIT;
		if (const auto pnode = cnode->parent)
			*parent = const_cast<gui_widget_data*>(pnode);
		return CSS_OK;
	};
	handler.sibling_node = [](void* pw, void* node, void** sibling)
	{
		*sibling = nullptr;
		NODE_INIT;
		if (const auto pnode = cnode->parent)
		{
			const auto& children = pnode->children;
			auto it = std::find(children.begin(), children.end(), cnode);
			++it;
			if (it != children.end())
				*sibling = *it;
		}
		return CSS_OK;
	};
	handler.node_has_name = [](void* pw, void* node, const css_qname* qname, bool* match)
	{
		*match = false;
		NODE_INIT;
		const auto name = string_view(lwc_string_data(qname->name), strlen(lwc_string_data(qname->name)));
		if (cnode->type == name)
			*match = true;
		return CSS_OK;
	};
	handler.node_has_class = [](void* pw, void* node, lwc_string* name, bool* match)
	{
		*match = false;
		NODE_INIT;
		const auto sname = string_view(lwc_string_data(name), strlen(lwc_string_data(name)));

		const auto str = cnode->attr.find("class"_s);
		array<string_view> vec(ctx->realloc, ctx->user);
		if (str != cnode->attr.end())
		{
			if (split(str->value.str(), vec))
				return CSS_NOMEM;
		}
		const auto it = std::find(vec.begin(), vec.end(), sname);
		if (it != vec.end())
			*match = true;

		return CSS_OK;
	};
	handler.node_has_id = [](void* pw, void* node, lwc_string* name, bool* match)
	{
		*match = false;
		NODE_INIT;
		const auto sname = string_view(lwc_string_data(name), strlen(lwc_string_data(name)));

		const auto id = cnode->attr.find("id"_s);
		if (id != cnode->attr.end())
		{
			if (id->value.str() == sname)
				*match = true;
		}
		return CSS_OK;
	};
	handler.node_has_attribute = [](void* pw, void* node, const css_qname* qname, bool* match)
	{
		*match = false;
		NODE_INIT;
		const auto name = string_view(lwc_string_data(qname->name), strlen(lwc_string_data(qname->name)));

		if (const auto ptr = cnode->attr.try_find(name))
			*match = true;

		return CSS_OK;
	};
	handler.node_has_attribute_equal = [](void* pw, void* node, const css_qname* qname, lwc_string* value, bool* match)
	{
		*match = false;
		NODE_INIT;
		const auto name = string_view(lwc_string_data(qname->name), strlen(lwc_string_data(qname->name)));
		const auto svalue = string_view(lwc_string_data(value), strlen(lwc_string_data(value)));

		if (const auto ptr = cnode->attr.try_find(name))
		{
			if (ptr->str() == svalue)
				*match = true;
		}
		return CSS_OK;
	};
	handler.node_has_attribute_dashmatch = [](void* pw, void* node, const css_qname* qname, lwc_string* value, bool* match)
	{
		*match = false;
		return CSS_OK;
	};
	handler.node_has_attribute_includes = [](void* pw, void* node, const css_qname* qname, lwc_string* value, bool* match)
	{
		*match = false;
		NODE_INIT;
		const auto name = string_view(lwc_string_data(qname->name), strlen(lwc_string_data(qname->name)));
		const auto svalue = string_view(lwc_string_data(value), strlen(lwc_string_data(value)));

		const auto str = cnode->attr.find(name);
		array<string_view> vec(ctx->realloc, ctx->user);
		if (str != cnode->attr.end())
		{
			if (split(str->value.str(), vec))
				return CSS_NOMEM;
		}
		if (std::find(vec.begin(), vec.end(), svalue) != vec.end())
			*match = true;

		return CSS_OK;
	};
	handler.node_has_attribute_prefix = [](void* pw, void* node, const css_qname* qname, lwc_string* value, bool* match)
	{
		*match = false;
		NODE_INIT;
		const auto name = string_view(lwc_string_data(qname->name), strlen(lwc_string_data(qname->name)));
		const auto svalue = string_view(lwc_string_data(value), strlen(lwc_string_data(value)));

		if (const auto str = cnode->attr.try_find(name))
		{
			if (str->str().bytes().size() >= svalue.bytes().size())
			{
				if (memcmp(str->str().bytes().data(), svalue.bytes().data(), svalue.bytes().size()) == 0)
					*match = true;
			}
		}
		return CSS_OK;
	};
	handler.node_has_attribute_suffix = [](void* pw, void* node, const css_qname* qname, lwc_string* value, bool* match)
	{
		*match = false;
		NODE_INIT;
		const auto name = string_view(lwc_string_data(qname->name), strlen(lwc_string_data(qname->name)));
		const auto svalue = string_view(lwc_string_data(value), strlen(lwc_string_data(value)));

		if (const auto str = cnode->attr.try_find(name))
		{
			if (str->str().bytes().size() >= svalue.bytes().size())
			{
				const auto ptr = str->str().bytes().data() + str->str().bytes().size() - svalue.bytes().size();
				if (memcmp(ptr, svalue.bytes().data(), svalue.bytes().size()) == 0)
					*match = true;
			}
		}
		return CSS_OK;
	};
	handler.node_has_attribute_substring = [](void* pw, void* node, const css_qname* qname, lwc_string* value, bool* match)
	{
		*match = false;
		NODE_INIT;
		const auto name = string_view(lwc_string_data(qname->name), strlen(lwc_string_data(qname->name)));
		const auto svalue = string_view(lwc_string_data(value), strlen(lwc_string_data(value)));

		if (const auto str = cnode->attr.try_find(name))
		{
			if (str->str().find(svalue) != str->str().end())
				*match = true;
		}
		return CSS_OK;
	};
	handler.node_is_root = [](void* pw, void* node, bool* match)
	{
		*match = false;
		NODE_INIT;
		if (cnode->type == "html"_s)
			*match = true;
		return CSS_OK;
	};
	handler.node_count_siblings = [](void* pw, void* node, bool same_name, bool after, int32_t* count)
	{
		*count = 0;
		NODE_INIT;
		if (const auto pnode = cnode->parent)
		{
			const auto& children = pnode->children;
			auto it = std::find(children.begin(), children.end(), cnode);
			if (same_name)
			{
				if (after)
				{
					++it;
					while (it != children.end())
					{
						const auto snode = *it;
						if (snode->type == cnode->type)
							(*count)++;
						++it;
					}
				}
				else if (it != children.begin())
				{
					--it;
					while (it != children.begin())
					{
						const auto snode = *it;
						if (snode->type == cnode->type)
							(*count)++;
						--it;
					}
				}
			}
			else
			{
				if (after)
					*count = uint32_t(std::distance(it + 1, children.end()));
				else
					*count = uint32_t(std::distance(children.begin(), it));
			}
		}
		return CSS_OK;
	};
	handler.node_is_empty = [](void* pw, void* node, bool* match)
	{
		*match = false;
		NODE_INIT;
		if (cnode->children.empty())
			*match = true;
		return CSS_OK;
	};
	handler.node_is_link = [](void* pw, void* node, bool* match)
	{
		*match = false;
		NODE_INIT;
		if (cnode->state & gui_widget_state_link)
			*match = true;
		return CSS_OK;
	};
	handler.node_is_visited = [](void* pw, void* node, bool* match)
	{
		*match = false;
		NODE_INIT;
		if (cnode->state & gui_widget_state_visited)
			*match = true;
		return CSS_OK;
	};
	handler.node_is_hover = [](void* pw, void* node, bool* match)
	{
		*match = false;
		NODE_INIT;
		if (cnode->state & gui_widget_state_hovered)
			*match = true;
		return CSS_OK;
	};
	handler.node_is_active = [](void* pw, void* node, bool* match)
	{
		*match = false;
		NODE_INIT;
		if (cnode->state & gui_widget_state_active)
			*match = true;
		return CSS_OK;
	};
	handler.node_is_focus = [](void* pw, void* node, bool* match)
	{
		*match = false;
		NODE_INIT;
		if (cnode->state & gui_widget_state_focused)
			*match = true;
		return CSS_OK;
	};
	handler.node_is_enabled = [](void* pw, void* node, bool* match)
	{
		*match = true;
		NODE_INIT;
		if (cnode->state & gui_widget_state_enabled)
			*match = false;
		return CSS_OK;
	};
	handler.node_is_disabled = [](void* pw, void* node, bool* match)
	{
		*match = false;
		NODE_INIT;
		if ((cnode->state & gui_widget_state_enabled) == 0)
			*match = true;
		return CSS_OK;
	};
	handler.node_is_checked = [](void* pw, void* node, bool* match)
	{
		*match = false;
		NODE_INIT;
		if (cnode->state & gui_widget_state_checked)
			*match = true;
		return CSS_OK;
	};
	handler.node_is_target = [](void* pw, void* node, bool* match)
	{
		*match = false;
		NODE_INIT;
		if (cnode->state & gui_widget_state_target)
			*match = true;
		return CSS_OK;
	};
	handler.node_is_lang = [](void* pw, void* node, lwc_string* lang, bool* match)
	{
		*match = false;
		NODE_INIT;
		const auto slang = string_view(lwc_string_data(lang), strlen(lwc_string_data(lang)));
		if (const auto vlang = cnode->attr.try_find("lang"_s))
		{
			if (vlang->str() == slang)
				*match = true;
		}
		return CSS_OK;
	};
	handler.node_presentational_hint = [](void* pw, void* node, uint32_t* nhints, css_hint** hints)
	{
		*hints = nullptr;
		*nhints = 0;
		return CSS_OK;
	};
	handler.ua_default_for_property = [](void* pw, uint32_t property, css_hint* hint)
	{
		hint->prop = property;
		if (property == CSS_PROP_COLOR)
		{
			hint->data.color = 0x00000000;
			hint->status = CSS_COLOR_COLOR;
		}
		else if (property == CSS_PROP_FONT_FAMILY)
		{
			hint->data.strings = nullptr;
			hint->status = CSS_FONT_FAMILY_SANS_SERIF;
		}
		else if (property == CSS_PROP_QUOTES)
		{
			hint->data.strings = nullptr;
			hint->status = CSS_QUOTES_NONE;
		}
		else if (property == CSS_PROP_VOICE_FAMILY)
		{
			hint->data.strings = nullptr;
			hint->status = 0;
		}
		else
		{
			return CSS_INVALID;
		}
		return CSS_OK;
	};
	handler.compute_font_size = [](void* pw, const css_hint* parent, css_hint* size)
	{
		const css_hint_length sizes[] = 
		{
			{ FLTTOFIX(6.75), CSS_UNIT_PT },
			{ FLTTOFIX(7.50), CSS_UNIT_PT },
			{ FLTTOFIX(9.75), CSS_UNIT_PT },
			{ FLTTOFIX(12.0), CSS_UNIT_PT },
			{ FLTTOFIX(13.5), CSS_UNIT_PT },
			{ FLTTOFIX(18.0), CSS_UNIT_PT },
			{ FLTTOFIX(24.0), CSS_UNIT_PT }
		};
		auto parent_size = &sizes[CSS_FONT_SIZE_MEDIUM - 1];
		if (parent)
		{
			assert(parent->status == CSS_FONT_SIZE_DIMENSION);
			assert(parent->data.length.unit != CSS_UNIT_EM);
			assert(parent->data.length.unit != CSS_UNIT_EX);
			parent_size = &parent->data.length;
		}
		auto parent_index = 0u;
		auto parent_delta = DBL_MAX;
		for (auto k = 0u; k < std::size(sizes); ++k)
		{
			const auto delta = fabs(FIXTOFLT(sizes[k].value) - FIXTOFLT(parent_size->value));
			if (delta < parent_delta)
			{
				parent_delta = delta;
				parent_index = k;
			}
		}

		if (size->status < CSS_FONT_SIZE_LARGER) 
		{
			/* Keyword -- simple */
			size->data.length = sizes[size->status - 1];
		}
		else if (size->status == CSS_FONT_SIZE_LARGER)
		{
			if (parent_index + 1 < std::size(sizes))
			{
				size->data.length = sizes[parent_index + 1];
			}
			else
			{
				size->data.length.value = FMUL(parent_size->value, FLTTOFIX(1.2));
				size->data.length.unit = parent_size->unit;
			}
		}
		else if (size->status == CSS_FONT_SIZE_SMALLER) 
		{
			if (parent_index > 0)
			{
				size->data.length = sizes[parent_index - 1];
			}
			else
			{
				size->data.length.value = FMUL(parent_size->value, FLTTOFIX(0.8));
				size->data.length.unit = parent_size->unit;
			}
		}
		else if (size->data.length.unit == CSS_UNIT_EM || size->data.length.unit == CSS_UNIT_EX) 
		{
			size->data.length.value = FMUL(size->data.length.value, parent_size->value);
			if (size->data.length.unit == CSS_UNIT_EX) 
			{
				size->data.length.value = FMUL(size->data.length.value, FLTTOFIX(0.6));
			}
			size->data.length.unit = parent_size->unit;
		}
		else if (size->data.length.unit == CSS_UNIT_PCT) 
		{
			size->data.length.value = 
				FDIV(FMUL(size->data.length.value, parent_size->value), FLTTOFIX(100));
			size->data.length.unit = parent_size->unit;
		}
		size->status = CSS_FONT_SIZE_DIMENSION;
		return CSS_OK;
	};
	handler.set_libcss_node_data = [](void* pw, void* node, void* libcss_node_data)
	{
		return CSS_OK;
	};
	handler.get_libcss_node_data = [](void* pw, void* node, void** libcss_node_data)
	{
		*libcss_node_data = nullptr;
		return CSS_OK;
	};

	css_select_results* new_style = nullptr;
	if (const auto error = css_select_style(m_value, const_cast<gui_widget_data*>(&widget), &media, 
		widget.inline_style.m_value, &handler, m_ctx, &new_style))
		return make_css_error(error);
	std::swap(widget.css.m_value, new_style);
	if (new_style)
		css_select_results_destroy(new_style);
	return error_type();
}

gui_style_data::gui_style_data(gui_style_data&& rhs) noexcept :
	m_name(std::move(rhs.m_name)), m_value(rhs.m_value)
{
	rhs.m_value = nullptr;
}
gui_style_data::~gui_style_data() noexcept
{
	if (m_value)
		css_stylesheet_destroy(m_value);
}
error_type gui_style_data::initialize(lwc_context* ctx, const string_view name, const string_view css) noexcept
{
	ICY_ERROR(copy(name, m_name));

	css_stylesheet_params params = {};
	params.params_version = CSS_STYLESHEET_PARAMS_VERSION_1;
	params.level = CSS_LEVEL_DEFAULT;
	params.url = m_name.bytes().data();
	params.ctx = ctx;
	params.resolve = [](void* pw, const char* base, lwc_string* rel, lwc_string** abs)
	{
		*abs = lwc_string_ref(rel);
		return CSS_OK;
	};
	
	css_stylesheet* new_sheet = nullptr;
	if (const auto error = css_stylesheet_create(&params, &new_sheet))
		return make_css_error(error);
	ICY_SCOPE_EXIT{ if (new_sheet) css_stylesheet_destroy(new_sheet); };

	if (const auto error = css_stylesheet_append_data(new_sheet, css.ubytes().data(), css.ubytes().size()))
	{
		if (error == CSS_NEEDDATA)
			;
		else
			return make_css_error(error);
	}
	if (const auto error = css_stylesheet_data_done(new_sheet))
		return make_css_error(error);

	if (m_value)
	{
		css_stylesheet_destroy(m_value);
		m_value = nullptr;
	}
	std::swap(m_value, new_sheet);
    return error_type();
}

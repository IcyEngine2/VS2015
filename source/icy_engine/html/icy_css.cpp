#include <icy_engine/html/icy_css.hpp>
#include "../../libs/css/libcss/include/libcss/libcss.h"
#include "../../libs/css/libwapcaplet/include/libwapcaplet/libwapcaplet.h"
#include <icy_engine/core/icy_color.hpp>

using namespace icy;

#if _DEBUG
#pragma comment(lib, "libcssd")
#else
#pragma comment(lib, "libcss")
#endif

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

class css_handler : public css_select_handler
{
public:
	struct pair_type
	{
		lwc_context* ctx = nullptr;
		html_document_css* doc = nullptr;
	};
	css_handler() noexcept;
};
class css_system::data_type
{
public:
	std::atomic<uint32_t> ref = 1;
	lwc_context* ctx = nullptr;
	css_select_ctx* select = nullptr;
};
class css_style::data_type
{
public:
	std::atomic<uint32_t> ref = 1;
	css_stylesheet* sheet = nullptr;
	css_system system;
};

css_handler::css_handler() noexcept
{
	handler_version = CSS_SELECT_HANDLER_VERSION_1;
	node_name = [](void* pw, void* node, css_qname* qname)
	{
		qname->name = nullptr;
		qname->ns = nullptr;

		const auto pair = static_cast<pair_type*>(pw);
		const auto cnode = static_cast<const html_node_css*>(node);
		auto error = lwc_intern_string(pair->ctx, cnode->tag().bytes().data(), cnode->tag().bytes().size(), &qname->name);
		return error ? CSS_NOMEM : CSS_OK;
	};
	node_classes = [](void* pw, void* node, lwc_string*** classes, uint32_t* n_classes)
	{
		*classes = nullptr;
		*n_classes = 0;

		const auto pair = static_cast<pair_type*>(pw);
		const auto cnode = static_cast<const html_node_css*>(node);
		const auto str = cnode->find("class"_s);

		array<string_view> vec(pair->ctx->realloc, pair->ctx->user);
		if (split(str, vec))
			return CSS_NOMEM;

		if (vec.empty())
			return CSS_OK;

		*n_classes = uint32_t(vec.size());
		*classes = static_cast<lwc_string**>(pair->ctx->realloc(nullptr,
			sizeof(lwc_string*) * vec.size(), pair->ctx->user));
		memset(*classes, 0, sizeof(lwc_string*) * vec.size());
		auto k = 0u;
		for (; k < vec.size(); ++k)
		{
			auto& addr = (*classes)[k];
			if (lwc_intern_string(pair->ctx, vec[k].bytes().data(), vec[k].bytes().size(), &addr))
				break;
		}
		if (k != vec.size())
		{
			for (*n_classes = k; *n_classes; --(*n_classes))
				lwc_string_unref(pair->ctx, (*classes)[k]);

			pair->ctx->realloc(*classes, 0, pair->ctx->user);
			*classes = nullptr;
			return CSS_NOMEM;
		}
		return CSS_OK;
	};
	node_id = [](void* pw, void* node, lwc_string** id)
	{
		*id = nullptr;
		const auto pair = static_cast<pair_type*>(pw);
		const auto cnode = static_cast<const html_node_css*>(node);

		const auto str = cnode->find("id"_s);
		if (!str.empty())
		{
			if (lwc_intern_string(pair->ctx, str.bytes().data(), str.bytes().size(), id))
				return CSS_NOMEM;
		}
		return CSS_OK;
	};
	named_ancestor_node = [](void* pw, void* node, const css_qname* qname, void** ancestor)
	{
		*ancestor = nullptr;
		const auto pair = static_cast<pair_type*>(pw);
		auto cnode = static_cast<const html_node_css*>(node);
		const auto name = string_view(lwc_string_data(qname->name), strlen(lwc_string_data(qname->name)));

		while (cnode->index())
		{
			const auto pnode = pair->doc->node(cnode->parent());
			if (pnode->tag() == name)
			{
				*ancestor = pnode;
				break;
			}
			cnode = pnode;
		}

		return CSS_OK;
	};
	named_parent_node = [](void* pw, void* node, const css_qname* qname, void** parent)
	{
		*parent = nullptr;
		const auto pair = static_cast<pair_type*>(pw);
		const auto cnode = static_cast<const html_node_css*>(node);
		const auto name = string_view(lwc_string_data(qname->name), strlen(lwc_string_data(qname->name)));

		if (cnode->index())
		{
			const auto pnode = pair->doc->node(cnode->parent());
			if (pnode->tag() == name)
				*parent = pnode;
		}
		return CSS_OK;
	};
	named_sibling_node = [](void* pw, void* node, const css_qname* qname, void** sibling)
	{
		*sibling = nullptr;
		const auto pair = static_cast<pair_type*>(pw);
		const auto cnode = static_cast<const html_node_css*>(node);
		const auto name = string_view(lwc_string_data(qname->name), strlen(lwc_string_data(qname->name)));

		if (cnode->index())
		{
			const auto pnode = pair->doc->node(cnode->parent());
			const auto children = pnode->children();
			const auto it = std::find(children.begin(), children.end(), cnode->index());
			ICY_ASSERT(it != children.end(), "CORRUPTED DOM TREE");
			html_node_css* next = nullptr;
			if (it + 1 != children.end())
				next = pair->doc->node(*(it + 1));
			if (next && next->tag() == name)
			{
				*sibling = next;
				return CSS_OK;
			}
		}
		return CSS_OK;
	};
	named_generic_sibling_node = [](void* pw, void* node, const css_qname* qname, void** sibling)
	{
		*sibling = nullptr;
		const auto pair = static_cast<pair_type*>(pw);
		const auto cnode = static_cast<const html_node_css*>(node);
		const auto name = string_view(lwc_string_data(qname->name), strlen(lwc_string_data(qname->name)));

		if (cnode->index())
		{
			const auto pnode = pair->doc->node(cnode->parent());
			const auto children = pnode->children();
			auto it = std::find(children.begin(), children.end(), cnode->index());
			ICY_ASSERT(it != children.end(), "CORRUPTED DOM TREE");
			++it;
			while (it != children.end())
			{
				const auto snode = pair->doc->node(*it);
				if (snode->tag() == name)
				{
					*sibling = snode;
					break;
				}
				++it;
			}
		}
		return CSS_OK;
	};
	parent_node = [](void* pw, void* node, void** parent)
	{
		*parent = nullptr;
		const auto pair = static_cast<pair_type*>(pw);
		const auto cnode = static_cast<const html_node_css*>(node);
		if (cnode->index())
			*parent = pair->doc->node(cnode->parent());

		return CSS_OK;
	};
	sibling_node = [](void* pw, void* node, void** sibling)
	{
		*sibling = nullptr;
		const auto pair = static_cast<pair_type*>(pw);
		const auto cnode = static_cast<const html_node_css*>(node);
		if (cnode->index())
		{
			const auto pnode = pair->doc->node(cnode->parent());
			const auto children = pnode->children();
			auto it = std::find(children.begin(), children.end(), cnode->index());
			ICY_ASSERT(it != children.end(), "CORRUPTED DOM TREE");
			++it;
			if (it != children.end())
				*sibling = pair->doc->node(*it);
		}
		return CSS_OK;
	};
	node_has_name = [](void* pw, void* node, const css_qname* qname, bool* match)
	{
		*match = false;
		const auto pair = static_cast<pair_type*>(pw);
		const auto cnode = static_cast<const html_node_css*>(node);
		const auto name = string_view(lwc_string_data(qname->name), strlen(lwc_string_data(qname->name)));
		if (cnode->tag() == name)
			*match = true;
		return CSS_OK;
	};
	node_has_class = [](void* pw, void* node, lwc_string* name, bool* match)
	{
		*match = false;
		const auto pair = static_cast<pair_type*>(pw);
		const auto cnode = static_cast<const html_node_css*>(node);
		const auto sname = string_view(lwc_string_data(name), strlen(lwc_string_data(name)));

		const auto str = cnode->find("class"_s);
		array<string_view> vec(pair->ctx->realloc, pair->ctx->user);
		if (split(str, vec))
			return CSS_NOMEM;

		const auto it = std::find(vec.begin(), vec.end(), sname);
		if (it != vec.end())
			*match = true;

		return CSS_OK;
	};
	node_has_id = [](void* pw, void* node, lwc_string* name, bool* match)
	{
		*match = false;
		const auto pair = static_cast<pair_type*>(pw);
		const auto cnode = static_cast<const html_node_css*>(node);
		const auto sname = string_view(lwc_string_data(name), strlen(lwc_string_data(name)));

		const auto id = cnode->find("id"_s);
		if (id == sname)
			*match = true;

		return CSS_OK;
	};
	node_has_attribute = [](void* pw, void* node, const css_qname* qname, bool* match)
	{
		*match = false;
		const auto pair = static_cast<pair_type*>(pw);
		const auto cnode = static_cast<const html_node_css*>(node);
		const auto name = string_view(lwc_string_data(qname->name), strlen(lwc_string_data(qname->name)));

		if (!cnode->find(name).empty())
			*match = true;

		return CSS_OK;
	};
	node_has_attribute_equal = [](void* pw, void* node, const css_qname* qname, lwc_string* value, bool* match)
	{
		*match = false;
		const auto pair = static_cast<pair_type*>(pw);
		const auto cnode = static_cast<const html_node_css*>(node);
		const auto name = string_view(lwc_string_data(qname->name), strlen(lwc_string_data(qname->name)));
		const auto svalue = string_view(lwc_string_data(value), strlen(lwc_string_data(value)));

		const auto str = cnode->find(name);
		if (str == svalue)
			*match = true;

		return CSS_OK;
	};
	node_has_attribute_dashmatch = [](void* pw, void* node, const css_qname* qname, lwc_string* value, bool* match)
	{
		*match = false;
		return CSS_OK;
	};
	node_has_attribute_includes = [](void* pw, void* node, const css_qname* qname, lwc_string* value, bool* match)
	{
		*match = false;
		const auto pair = static_cast<pair_type*>(pw);
		const auto cnode = static_cast<const html_node_css*>(node);
		const auto name = string_view(lwc_string_data(qname->name), strlen(lwc_string_data(qname->name)));
		const auto svalue = string_view(lwc_string_data(value), strlen(lwc_string_data(value)));

		const auto str = cnode->find(name);
		array<string_view> vec(pair->ctx->realloc, pair->ctx->user);
		if (split(str, vec))
			return CSS_NOMEM;

		if (std::find(vec.begin(), vec.end(), svalue) != vec.end())
			*match = true;

		return CSS_OK;
	};
	node_has_attribute_prefix = [](void* pw, void* node, const css_qname* qname, lwc_string* value, bool* match)
	{
		*match = false;
		const auto pair = static_cast<pair_type*>(pw);
		const auto cnode = static_cast<const html_node_css*>(node);
		const auto name = string_view(lwc_string_data(qname->name), strlen(lwc_string_data(qname->name)));
		const auto svalue = string_view(lwc_string_data(value), strlen(lwc_string_data(value)));

		const auto str = cnode->find(name);
		if (str.bytes().size() >= svalue.bytes().size())
		{
			if (memcmp(str.bytes().data(), svalue.bytes().data(), svalue.bytes().size()) == 0)
				*match = true;
		}
		return CSS_OK;
	};
	node_has_attribute_suffix = [](void* pw, void* node, const css_qname* qname, lwc_string* value, bool* match)
	{
		*match = false;
		const auto pair = static_cast<pair_type*>(pw);
		const auto cnode = static_cast<const html_node_css*>(node);
		const auto name = string_view(lwc_string_data(qname->name), strlen(lwc_string_data(qname->name)));
		const auto svalue = string_view(lwc_string_data(value), strlen(lwc_string_data(value)));

		const auto str = cnode->find(name);
		if (str.bytes().size() >= svalue.bytes().size())
		{
			const auto ptr = str.bytes().data() + str.bytes().size() - svalue.bytes().size();
			if (memcmp(ptr, svalue.bytes().data(), svalue.bytes().size()) == 0)
				*match = true;
		}
		return CSS_OK;
	};
	node_has_attribute_substring = [](void* pw, void* node, const css_qname* qname, lwc_string* value, bool* match)
	{
		*match = false;
		const auto pair = static_cast<pair_type*>(pw);
		const auto cnode = static_cast<const html_node_css*>(node);
		const auto name = string_view(lwc_string_data(qname->name), strlen(lwc_string_data(qname->name)));
		const auto svalue = string_view(lwc_string_data(value), strlen(lwc_string_data(value)));

		const auto str = cnode->find(name);
		if (str.find(svalue) != str.end())
			*match = true;

		return CSS_OK;
	};
	node_is_root = [](void* pw, void* node, bool* match)
	{
		*match = false;
		const auto pair = static_cast<pair_type*>(pw);
		const auto cnode = static_cast<const html_node_css*>(node);
		if (cnode->index() == 0)
			*match = true;

		return CSS_OK;
	};
	node_count_siblings = [](void* pw, void* node, bool same_name, bool after, int32_t* count)
	{
		*count = 0;
		const auto pair = static_cast<pair_type*>(pw);
		const auto cnode = static_cast<const html_node_css*>(node);
		if (cnode->index())
		{
			const auto pnode = pair->doc->node(cnode->parent());
			const auto children = pnode->children();
			auto it = std::find(children.begin(), children.end(), cnode->index());
			ICY_ASSERT(it != children.end(), "CORRUPTED DOM TREE");
			if (same_name)
			{
				if (after)
				{
					++it;
					while (it != children.end())
					{
						const auto snode = pair->doc->node(*it);
						if (snode->tag() == cnode->tag())
							(*count)++;
						++it;
					}
				}
				else if (it != children.begin())
				{
					--it;
					while (it != children.begin())
					{
						const auto snode = pair->doc->node(*it);
						if (snode->tag() == cnode->tag())
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
	node_is_empty = [](void* pw, void* node, bool* match)
	{
		*match = false;
		const auto pair = static_cast<pair_type*>(pw);
		const auto cnode = static_cast<const html_node_css*>(node);
		if (cnode->children().empty())
			*match = true;
		return CSS_OK;
	};
	node_is_link = [](void* pw, void* node, bool* match)
	{
		*match = false;
		const auto pair = static_cast<pair_type*>(pw);
		const auto cnode = static_cast<const html_node_css*>(node);
		if (cnode->flags() & css_flags::link)
			*match = true;
		return CSS_OK;
	};
	node_is_visited = [](void* pw, void* node, bool* match)
	{
		*match = false;
		const auto pair = static_cast<pair_type*>(pw);
		const auto cnode = static_cast<const html_node_css*>(node);
		if (cnode->flags() & css_flags::visited)
			*match = true;
		return CSS_OK;
	};
	node_is_hover = [](void* pw, void* node, bool* match)
	{
		*match = false;
		const auto pair = static_cast<pair_type*>(pw);
		const auto cnode = static_cast<const html_node_css*>(node);
		if (cnode->flags() & css_flags::hover)
			*match = true;
		return CSS_OK;
	};
	node_is_active = [](void* pw, void* node, bool* match)
	{
		*match = false;
		const auto pair = static_cast<pair_type*>(pw);
		const auto cnode = static_cast<const html_node_css*>(node);
		if (cnode->flags() & css_flags::active)
			*match = true;
		return CSS_OK;
	};
	node_is_focus = [](void* pw, void* node, bool* match)
	{
		*match = false;
		const auto pair = static_cast<pair_type*>(pw);
		const auto cnode = static_cast<const html_node_css*>(node);
		if (cnode->flags() & css_flags::focus)
			*match = true;
		return CSS_OK;
	};
	node_is_enabled = [](void* pw, void* node, bool* match)
	{
		*match = true;
		const auto pair = static_cast<pair_type*>(pw);
		const auto cnode = static_cast<const html_node_css*>(node);
		if (cnode->flags() & css_flags::disabled)
			*match = false;
		return CSS_OK;
	};
	node_is_disabled = [](void* pw, void* node, bool* match)
	{
		*match = false;
		const auto pair = static_cast<pair_type*>(pw);
		const auto cnode = static_cast<const html_node_css*>(node);
		if (cnode->flags() & css_flags::disabled)
			*match = true;
		return CSS_OK;
	};
	node_is_checked = [](void* pw, void* node, bool* match)
	{
		*match = false;
		const auto pair = static_cast<pair_type*>(pw);
		const auto cnode = static_cast<const html_node_css*>(node);
		if (cnode->flags() & css_flags::checked)
			*match = true;
		return CSS_OK;
	};
	node_is_target = [](void* pw, void* node, bool* match)
	{
		*match = false;
		const auto pair = static_cast<pair_type*>(pw);
		const auto cnode = static_cast<const html_node_css*>(node);
		if (cnode->flags() & css_flags::target)
			*match = true;
		return CSS_OK;
	};
	node_is_lang = [](void* pw, void* node, lwc_string* lang, bool* match)
	{
		*match = false;
		const auto pair = static_cast<pair_type*>(pw);
		const auto cnode = static_cast<const html_node_css*>(node);
		const auto slang = string_view(lwc_string_data(lang), strlen(lwc_string_data(lang)));
		const auto vlang = cnode->find("lang"_s);
		if (vlang == slang)
			*match = true;

		return CSS_OK;
	};
	node_presentational_hint = [](void* pw, void* node, uint32_t* nhints, css_hint** hints)
	{
		*hints = nullptr;
		*nhints = 0;
		return CSS_OK;
	};
	ua_default_for_property = [](void* pw, uint32_t property, css_hint* hint)
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
	compute_font_size = [](void* pw, const css_hint* parent, css_hint* size)
	{
		size->data.length.unit = CSS_UNIT_PX;
		size->data.length.value = 18;
		size->prop = CSS_PROP_FONT_SIZE;
		size->status = CSS_FONT_SIZE_DIMENSION;
		return CSS_OK;
	};
	set_libcss_node_data = [](void* pw, void* node, void* libcss_node_data)
	{
		return CSS_OK;
	};
	get_libcss_node_data = [](void* pw, void* node, void** libcss_node_data)
	{
		*libcss_node_data = nullptr;
		return CSS_OK;
	};
}
css_style::css_style(const css_style& rhs) noexcept : data(rhs.data)
{
	if (data)
		data->ref.fetch_add(1, std::memory_order_release);
}
css_style::~css_style() noexcept
{
	if (data && data->ref.fetch_sub(1, std::memory_order_acq_rel) == 1)
	{
		const auto ctx = data->system.data->ctx;
		if (data->sheet)
		{
			css_select_ctx_remove_sheet(data->system.data->select, data->sheet);
			css_stylesheet_destroy(data->sheet);
		}
		ctx->realloc(this, 0, ctx->user);
	}
}

css_system::css_system(const css_system& rhs) noexcept : data(rhs.data)
{
	if (data)
		data->ref.fetch_add(1, std::memory_order_release);
}
css_system::~css_system() noexcept
{
	if (data && data->ref.fetch_sub(1, std::memory_order_acq_rel) == 1)
	{
		if (data->select) css_select_ctx_destroy(data->select);
		const auto ctx = data->ctx;
		ctx->realloc(data, 0, ctx->user);
		lwc_shutdown(ctx);
	}
}

error_type css_system::create(const string_view url, const const_array_view<char> parse, css_style& style) noexcept
{
	if (!data)
		return make_stdlib_error(std::errc::invalid_argument);

	string url_str(data->ctx->realloc, data->ctx->user);
	ICY_ERROR(copy(url, url_str));

	css_stylesheet_params params = {};
	params.params_version = CSS_STYLESHEET_PARAMS_VERSION_1;
	params.level = CSS_LEVEL_DEFAULT;
	params.url = url_str.bytes().data();
	params.ctx = data->ctx;
	params.resolve = [](void* pw, const char* base, lwc_string* rel, lwc_string** abs)
	{
		*abs = lwc_string_ref(rel);
		return CSS_OK;
	};
	css_stylesheet* new_sheet = nullptr;
	if (const auto error = css_stylesheet_create(&params, &new_sheet))
		return make_css_error(error);
	ICY_SCOPE_EXIT{ if (new_sheet) css_stylesheet_destroy(new_sheet); };

	if (const auto error = css_stylesheet_append_data(new_sheet,
		reinterpret_cast<const uint8_t*>(parse.data()), parse.size()))
	{
		if (error == CSS_NEEDDATA)
			;
		else
			return make_css_error(error);
	}

	if (const auto error = css_stylesheet_data_done(new_sheet))
		return make_css_error(error);

	auto new_data = static_cast<css_style::data_type*>(data->ctx->realloc(
		nullptr, sizeof(css_style::data_type), data->ctx->user));
	if (!new_data)
		return make_stdlib_error(std::errc::not_enough_memory);
	allocator_type::construct(new_data);

	style = css_style();
	style.data = new_data;
	style.data->system = *this;
	std::swap(style.data->sheet, new_sheet);

	return error_type();
}
error_type css_system::insert(const css_style& style) noexcept
{
	if (!data || !style.data || style.data->system.data != data)
		return make_stdlib_error(std::errc::invalid_argument);

	if (const auto error = css_select_ctx_append_sheet(data->select, style.data->sheet,
		css_origin::CSS_ORIGIN_AUTHOR, nullptr))
		return make_css_error(error);

	return error_type();
}
error_type css_system::erase(const css_style& style) noexcept
{
	if (!data || !style.data || style.data->system.data != data)
		return make_stdlib_error(std::errc::invalid_argument);

	if (const auto error = css_select_ctx_remove_sheet(data->select, style.data->sheet))
		return make_css_error(error);

	return error_type();
}
error_type css_system::apply(html_document_css& doc, const uint32_t index) const noexcept
{
	auto node = doc.node(index);
	if (!data || !node)
		return make_stdlib_error(std::errc::invalid_argument);

	const html_node_css* pnode = nullptr;
	if (index)
	{
		pnode = doc.node(node->parent());
		if (!pnode->m_style)
			return make_stdlib_error(std::errc::invalid_argument);			
	}

	css_media media = {};
	media.type = css_media_type::CSS_MEDIA_SCREEN;

	css_handler handler;
	css_handler::pair_type pair;
	pair.ctx = data->ctx;
	pair.doc = &doc;

	css_select_results* results = nullptr;
	if (const auto error = css_select_style(data->select, node, &media, nullptr, &handler, &pair, &results))
		return make_css_error(error);
	ICY_SCOPE_EXIT{ css_select_results_destroy(data->ctx, results); };

	css_computed_style* style = nullptr;
	if (const auto error = css_computed_style_compose(data->ctx, pnode ? pnode->m_style : nullptr,
		results->styles[CSS_PSEUDO_ELEMENT_NONE], handler.compute_font_size, &pair, &style))
		return make_css_error(error);

	if (node->m_style)
		css_computed_style_destroy(node->m_style);

	node->m_system = *this;
	node->m_style = style;
	return error_type();
}

html_node_css::~html_node_css() noexcept
{
	if (m_style)
		css_computed_style_destroy(m_style);
}
color html_node_css::color() const noexcept
{
	css_color clr = 0;
	if (m_style && css_computed_color(m_style, &clr))
		return icy::color::from_bgra(clr);

	return icy::color();
}

error_type icy::create_css_system(heap* const heap, css_system& system) noexcept
{
	auto realloc = heap ? heap_realloc : global_realloc;

	auto ctx = lwc_initialise(realloc, heap);
	if (!ctx)
		return make_stdlib_error(std::errc::not_enough_memory);
	ICY_SCOPE_EXIT{ if (ctx) lwc_shutdown(ctx); };

	css_select_ctx* select = nullptr;
	if (const auto error = css_select_ctx_create(ctx, &select))
		return make_css_error(error);
	ICY_SCOPE_EXIT{ if (select) css_select_ctx_destroy(select); };

	auto new_data = static_cast<css_system::data_type*>(
		realloc(nullptr, sizeof(css_system::data_type), heap));
	if (!new_data)
		return make_stdlib_error(std::errc::not_enough_memory);
	allocator_type::construct(new_data);
	
	system = css_system();
	system.data = new_data;
	std::swap(system.data->ctx, ctx);
	std::swap(system.data->select, select);

	return error_type();
}


const error_source icy::error_source_css = register_error_source("css"_s, css_error_to_string);

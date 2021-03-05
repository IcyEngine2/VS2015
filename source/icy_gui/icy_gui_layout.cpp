#include <icy_gui/icy_gui.hpp>
#include "../libs/gumbo/gumbo.h"
#if _DEBUG
#pragma comment(lib, "gumbod.lib")
#else
#pragma comment(lib, "gumbo.lib")
#endif
using namespace icy;

static error_type gui_layout_create(const GumboNode& node, gui_layout& output) noexcept
{
    if (node.type == GumboNodeType::GUMBO_NODE_ELEMENT ||
        node.type == GumboNodeType::GUMBO_NODE_TEMPLATE)
    {
        const auto& data = node.v.element;
        for (auto k = 0u; k < data.attributes.length; ++k)
        {
            const auto attr = static_cast<GumboAttribute*>(data.attributes.data[k]);
            const auto key = string_view(attr->name, strlen(attr->name));
            const auto val = string_view(attr->value, strlen(attr->value));
            string key_str;
            string val_str;
            ICY_ERROR(copy(key, key_str));
            ICY_ERROR(copy(val, val_str));
            ICY_ERROR(output.attributes.insert(std::move(key_str), std::move(val_str)));
        }
        const auto tag = gumbo_normalized_tagname(data.tag);
        ICY_ERROR(copy(string_view(tag, strlen(tag)), output.type));
        
        for (auto k = 0u; k < data.children.length; ++k)
        {
            const auto next = static_cast<const GumboNode*>(data.children.data[k]);
            gui_layout child;
            ICY_ERROR(gui_layout_create(*next, child));
            if (false
                || next->type == GUMBO_NODE_ELEMENT
                || next->type == GUMBO_NODE_TEMPLATE
                || next->type == GUMBO_NODE_TEXT
                || next->type == GUMBO_NODE_CDATA)
            {
                ICY_ERROR(output.children.push_back(std::move(child)));
            }
        }
    }
    else if (node.type == GUMBO_NODE_TEXT)
    {
        auto text = node.v.text.text;
        auto tlen = strlen(text);
        ICY_ERROR(copy("text"_s, output.type));
        ICY_ERROR(copy(string_view(text, tlen), output.text));        
    }
    else if (node.type == GUMBO_NODE_CDATA)
    {
        auto text = node.v.text.text;
        auto tlen = strlen(text);
        ICY_ERROR(copy("cdata"_s, output.type));
        ICY_ERROR(copy(string_view(text, tlen), output.text));
    }
    else
    {
        return error_type();
    }
    return error_type();
}

error_type gui_layout::initialize(const string_view html) noexcept
{
    GumboOptions options = kGumboDefaultOptions;
    options.allocator = [](void* user, size_t size) { return icy::realloc(nullptr, size); };
    options.deallocator = [](void* user, void* ptr) { icy::realloc(ptr, 0); };
    auto gumbo = gumbo_parse_with_options(&options, html.bytes().data(), html.bytes().size());
    if (!gumbo)
        return make_stdlib_error(std::errc::not_enough_memory);

    ICY_SCOPE_EXIT{ gumbo_destroy_output(&options, gumbo); };
    ICY_ERROR(gui_layout_create(*gumbo->root, *this));
    return error_type();
}
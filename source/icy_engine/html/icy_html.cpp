#include <icy_engine/html/icy_html.hpp>
#include "../../libs/gumbo/gumbo.h"

#if _DEBUG
#pragma comment(lib, "gumbod")
#else
#pragma comment(lib, "gumbo")
#endif

using namespace icy;

string_view icy::html_tag_normalize(const string_view tag) noexcept
{
    const auto tagn = gumbo_tagn_enum(tag.bytes().data(), uint32_t(tag.bytes().size()));
    if (tagn < GumboTag::GUMBO_TAG_UNKNOWN)
    {
        auto ptr = gumbo_normalized_tagname(tagn);
        return string_view(ptr, strlen(ptr));
    }
    return string_view();
}

error_type html_document_base::initialize(const const_array_view<char> parse) noexcept
{
    const GumboAllocatorFunction heap_alloc = [](void* user, size_t size) { return static_cast<icy::heap*>(user)->realloc(nullptr, size); };
    const GumboDeallocatorFunction heap_free = [](void* user, void* ptr) { static_cast<icy::heap*>(user)->realloc(ptr, 0); };

    GumboOptions options = kGumboDefaultOptions;
    options.allocator = m_heap ? heap_alloc : [](void*, size_t size) { return icy::realloc(nullptr, size); };
    options.deallocator = m_heap ? heap_free : [](void*, void* ptr) { icy::realloc(ptr, 0); };

    auto output = gumbo_parse_with_options(&options, parse.data(), parse.size());
    if (!output)
        return make_stdlib_error(std::errc::not_enough_memory);
    ICY_SCOPE_EXIT{ gumbo_destroy_output(&options, output); };

    if (!output->root)
        return make_stdlib_error(std::errc::invalid_argument);
    
    using func_type = error_type(*)(html_document_base& self, const GumboNode& node, const uint32_t parent);
    static func_type func = [](html_document_base& self, const GumboNode& node, const uint32_t parent)
    {
        const auto index = self.size();
        switch (node.type)
        {
        case GumboNodeType::GUMBO_NODE_ELEMENT:
        {
            const auto& elem = node.v.element;
            const auto tag_str = gumbo_normalized_tagname(elem.tag);
            const auto tag = tag_str ? string_view(tag_str, strlen(tag_str)) : string_view();

            ICY_ERROR(self.insert(html_type::node, tag, parent));
            const auto new_node = self.node(index);
            ICY_ASSERT(new_node, "INVALID NODE");
            if (index)
                ICY_ERROR(new_node->initialize(*self.node(parent)));

            for (auto k = 0u; k < elem.attributes.length; ++k)
            {
                const auto attribute = static_cast<GumboAttribute*>(elem.attributes.data[k]);
                if (!attribute->name)
                    continue;

                const auto str_key = attribute->name ? string_view(attribute->name, strlen(attribute->name)) : string_view();
                const auto str_val = attribute->value ? string_view(attribute->value, strlen(attribute->value)) : string_view();
                string new_key(self.m_heap);
                string new_val(self.m_heap);
                ICY_ERROR(copy(str_key, new_key));
                ICY_ERROR(copy(str_val, new_val));
                ICY_ERROR(new_node->m_attributes.insert(std::move(new_key), std::move(new_val)));
            }
            for (auto k = 0u; k < elem.children.length; ++k)
                ICY_ERROR(func(self, *static_cast<const GumboNode*>(elem.children.data[k]), index));
            
            break;
        }

        case GumboNodeType::GUMBO_NODE_TEXT:
        case GumboNodeType::GUMBO_NODE_CDATA:
        case GumboNodeType::GUMBO_NODE_WHITESPACE:
        {
            ICY_ERROR(self.insert(html_type::text, string_view(), parent));
            const auto new_node = self.node(index);
            ICY_ASSERT(new_node, "INVALID NODE");
            ICY_ERROR(new_node->initialize(*self.node(parent)));
            const auto text = string_view(node.v.text.text, strlen(node.v.text.text));
            ICY_ERROR(copy(text, new_node->m_text));
            break;
        }
        }
        return error_type();
    };
    ICY_ERROR(func(*this, *output->root, 0));    
    return error_type();
}

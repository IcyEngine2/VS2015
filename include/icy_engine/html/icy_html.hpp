#pragma once

#include <icy_engine/core/icy_array.hpp>
#include <icy_engine/core/icy_string.hpp>
#include <icy_engine/core/icy_map.hpp>

namespace icy
{
    class html_document_base;
    enum class html_type : uint32_t
    {
        null,
        node,
        text,
    };
    class html_node
    {
        friend html_document_base;
    public:
        html_node(heap* const heap, const uint32_t index, const html_type type, const string_view tag, const uint32_t parent) noexcept : 
            m_index(index), m_type(type), m_tag(tag), m_parent(parent), m_attributes(heap), m_text(heap), m_children(heap)
        {

        }
        uint32_t index() const noexcept
        {
            return m_index;
        }
        html_type type() const noexcept
        {
            return m_type;
        }
        string_view tag() const noexcept
        {
            return m_tag;
        }
        const_array_view<uint32_t> children() const noexcept
        {
            return m_children;
        }
        uint32_t parent() const noexcept
        {
            return m_parent;
        }
        string_view text() const noexcept
        {
            return m_text;
        }
        const_array_view<string> keys() const noexcept
        {
            return m_attributes.keys();
        }
        const_array_view<string> vals() const noexcept
        {
            return m_attributes.vals();
        }
        string_view find(const string_view attribute) const noexcept
        {
            const auto it = m_attributes.find(attribute);
            if (it == m_attributes.end())
                return string_view();
            else
                return it->value;
        }
    private:
        error_type initialize(html_node& parent) noexcept
        {
            ICY_ASSERT(parent.index() == m_parent, "INVALID PARENT");
            return parent.m_children.push_back(m_index);
        }
    private:
        const uint32_t m_index;
        html_type m_type = html_type::null;
        string_view m_tag;
        array<uint32_t> m_children;
        uint32_t m_parent = 0;
        string m_text;
        map<string, string> m_attributes;
    };
    string_view html_tag_normalize(const string_view tag) noexcept;

    class html_document_base
    {
    public:
        html_document_base(heap* heap = nullptr) noexcept : m_heap(heap)
        {

        }
        error_type initialize(const const_array_view<char> parse) noexcept;
        virtual size_t size() const noexcept = 0;
        virtual const html_node* node(const size_t index) const noexcept = 0;
        virtual html_node* node(const size_t index) noexcept = 0;
        virtual error_type insert(const html_type type, const string_view tag, const uint32_t parent) noexcept = 0;
    protected:
        heap* m_heap = nullptr;
    };
}
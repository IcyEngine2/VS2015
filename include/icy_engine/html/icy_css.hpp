#pragma once

#include "icy_html.hpp"

struct css_computed_style;

namespace icy
{
    class css_system;
    class html_document_css;

    extern const error_source error_source_css;

    enum class css_flags : uint32_t
    {
        none    =   0x00,
        link    =   0x01,
        visited =   0x02,
        hover   =   0x04,
        active  =   0x08,
        focus   =   0x10,
        disabled=   0x20,
        checked =   0x40,
        target  =   0x80,
    };
    inline constexpr bool operator&(const css_flags lhs, const css_flags rhs) noexcept
    {
        return !!(uint32_t(lhs) & uint32_t(rhs));
    }
    inline constexpr css_flags operator|(const css_flags lhs, const css_flags rhs) noexcept
    {
        return css_flags(uint32_t(lhs) | uint32_t(rhs));
    }

    class css_style
    {
    public:
        css_style() noexcept = default;
        css_style(const css_style&) noexcept;
        ICY_DEFAULT_COPY_ASSIGN(css_style);
        ~css_style() noexcept;
    public:
        class data_type;
        data_type* data = nullptr;
    };

    class css_system
    {
    public:
        css_system() noexcept = default;
        css_system(const css_system&) noexcept;
        ICY_DEFAULT_COPY_ASSIGN(css_system);
        ~css_system() noexcept;
    public:
        error_type create(const string_view url, const const_array_view<char> parse, css_style& style) noexcept;
        error_type insert(const css_style& style) noexcept;
        error_type erase(const css_style& style) noexcept;
        error_type apply(html_document_css& doc, const uint32_t index) const noexcept;
    public:
        class data_type;
        data_type* data = nullptr;  
    };

    class html_node_css : public html_node
    {
        friend css_system;
    public:
        using html_node::html_node;
        html_node_css(html_node_css&& rhs) noexcept : html_node(std::move(rhs)),
            m_flags(rhs.m_flags), m_style(rhs.m_style)
        {
            rhs.m_style = nullptr;
        }
        ICY_DEFAULT_MOVE_ASSIGN(html_node_css);
        ~html_node_css() noexcept;
    public:
        css_flags flags() const noexcept
        {
            return m_flags;
        }
        icy::color color() const noexcept;
        
    private:
        css_flags m_flags = css_flags::none;
        css_computed_style* m_style = nullptr;
        css_system m_system;
    };

    class html_document_css : public html_document_base
    {
    public:
        html_document_css(heap* const heap = nullptr) noexcept : html_document_base(heap), m_data(heap)
        {

        }
        size_t size() const noexcept
        {
            return m_data.size();
        }
        const html_node_css* node(const size_t index) const noexcept
        {
            return index < m_data.size() ? &m_data[index] : nullptr;
        }
        html_node_css* node(const size_t index) noexcept override
        {
            return index < m_data.size() ? &m_data[index] : nullptr;
        }
        error_type insert(const html_type type, const string_view tag, const uint32_t parent) noexcept override
        {
            return m_data.emplace_back(m_heap, uint32_t(m_data.size()), type, tag, parent);
        }
    private:
        array<html_node_css> m_data;
    };

    error_type create_css_system(heap* const heap, css_system& system) noexcept;
}
#pragma once

#include "icy_css.hpp"
#include <icy_engine/graphics/icy_graphics.hpp>

namespace icy
{
    class html_node_render
    {
    public:
        const html_node_css* style() const noexcept
        {
            return m_style;
        }
        render_d2d_rectangle box() const noexcept
        {
            return m_box;
        }
    private:
        const html_node_css* m_style;
        render_d2d_rectangle m_box;
    };
    class html_document_render
    {
    public:
        html_document_render(heap* const heap) : data(heap)
        {

        }
        error_type update(const html_document_css& doc) noexcept;
    public:
        array<html_node_render> data;
    };
}
#pragma once

#include <icy_engine/core/icy_color.hpp>
#include <icy_engine/core/icy_map.hpp>
#include "icy_render_math.hpp"
#include "icy_render_svg.hpp"

namespace icy
{
    class render_svg_geometry;
    class adapter;

    enum class render_element_type : uint32_t
    {
        none,
        clear,
        svg,
    };

    struct render_camera
    {
        render_vector pos;
        render_vector dir;
        float min_z = 0;
        float max_z = 0;
        render_d2d_rectangle view;
    };
    struct render_element_svg
    {
        render_d2d_vector offset;
        render_svg_geometry geometry;
        float layer = 0;
    };
    struct render_element
    {
        render_element(const render_element_type type = render_element_type::none) noexcept : type(type)
        {

        }
        render_element_type type;
        color clear;
        render_element_svg svg;
    };

    class render_list
    {
    public:
        error_type clear(const color color) noexcept;
        error_type draw(const render_svg_geometry geometry, const render_d2d_vector offset, const float layer = 0) noexcept;
    public:
        array<render_element> data;
    };
}
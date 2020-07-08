#pragma once

#include <icy_engine/core/icy_color.hpp>
#include <icy_engine/core/icy_event.hpp>
#include "icy_render_math.hpp"
#include "icy_render_svg.hpp"

namespace icy
{
    class render_svg_geometry;
    class window_system;

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
        render_list(const size_t frame) noexcept : frame(frame)
        {

        }
        error_type clear(const color color) noexcept;
        error_type draw(const render_svg_geometry geometry, const render_d2d_vector offset, const float layer = 0) noexcept;
    public:
        const size_t frame;
        array<render_element> data;
    };
    class render_system : public event_system
    {
    public:
        virtual error_type update(render_list&& render) noexcept = 0;
    };
    error_type create_event_system(shared_ptr<render_system>& render, const window_system& window) noexcept;
}
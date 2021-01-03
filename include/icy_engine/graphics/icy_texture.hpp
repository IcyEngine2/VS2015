#pragma once

#include <icy_engine/core/icy_smart_pointer.hpp>
#include <icy_engine/core/icy_color.hpp>
#include <icy_engine/core/icy_matrix.hpp>
#include "icy_adapter.hpp"

namespace icy
{
    struct texture;
    struct render_vertex
    {
        float wcoord[4];
        float colors[4];
        float tcoord[2];
    };
    struct render_primitive
    {
        render_vertex& operator[](const size_t index) noexcept
        {
            return vertices[index];
        }
        const render_vertex& operator[](const size_t index) const noexcept
        {
            return vertices[index];
        }
        render_vertex vertices[3];
    };
    struct render_list
    {
        array<render_primitive> primitives;
        const texture* texture = nullptr;
    };
    struct texture
    {
        virtual ~texture() noexcept = 0
        {

        }
        virtual void* handle() const noexcept = 0;
        virtual window_size size() const noexcept = 0;     
    };
    struct render_texture : public texture
    {
        virtual error_type save(array_view<color> colors) const noexcept = 0;
        virtual error_type clear(const color color) noexcept = 0;
        virtual error_type render(const render_list& cmd) noexcept = 0;
    };
    struct texture_system
    {
        virtual ~texture_system() noexcept = 0
        {

        }
        virtual error_type create(const window_size size, const render_flags flags, shared_ptr<render_texture>& texture) const noexcept = 0;
        virtual error_type create(const const_matrix_view<color> data, shared_ptr<texture>& texture) const noexcept = 0;
    };

    error_type create_texture_system(const adapter adapter, shared_ptr<texture_system>& system) noexcept;
}
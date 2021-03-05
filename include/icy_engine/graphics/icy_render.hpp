#pragma once

#include <icy_engine/core/icy_event.hpp>
#include <icy_engine/core/icy_matrix.hpp>
#include <icy_engine/core/icy_color.hpp>
#include <icy_engine/core/icy_array.hpp>
#include "icy_adapter.hpp"


namespace icy
{
    namespace render_vertex
    {
        using index = uint32_t;
        union vec4
        {
            struct
            {
                float x;
                float y;
                float z; 
                float w;
            };
            float vec[4];
        };
        union vec2
        {
            struct
            {
                float u;
                float v;
            };
            float vec[2];
        };
    }

    struct render_system;
    struct render_query
    {
        virtual ~render_query() noexcept = 0
        {

        }
        virtual error_type exec(uint32_t& oper) noexcept = 0;
        virtual void clear(const color color) noexcept = 0;
        error_type set_index_buffer(const const_array_view<render_vertex::index> data) noexcept
        {
            array<render_vertex::index> vec;
            ICY_ERROR(vec.assign(data));
            set_index_buffer(std::move(vec));
            return error_type();
        }
        error_type set_coord_buffer(const const_array_view<render_vertex::vec4> data) noexcept
        {
            array<render_vertex::vec4> vec;
            ICY_ERROR(vec.assign(data));
            set_coord_buffer(std::move(vec));
            return error_type();
        }
        error_type set_color_buffer(const const_array_view<render_vertex::vec4> data) noexcept
        {
            array<render_vertex::vec4> vec;
            ICY_ERROR(vec.assign(data));
            set_color_buffer(std::move(vec));
            return error_type();
        }
        error_type set_texuv_buffer(const const_array_view<render_vertex::vec2> data) noexcept
        {
            array<render_vertex::vec2> vec;
            ICY_ERROR(vec.assign(data));
            set_texuv_buffer(std::move(vec));
            return error_type();
        }
        virtual void set_index_buffer(array<render_vertex::index>&& data) noexcept = 0;
        virtual void set_coord_buffer(array<render_vertex::vec4>&& data) noexcept = 0;
        virtual void set_color_buffer(array<render_vertex::vec4>&& data) noexcept = 0;
        virtual void set_texuv_buffer(array<render_vertex::vec2>&& data) noexcept = 0;
        virtual void set_texture(const shared_ptr<texture> texture) noexcept = 0;
        virtual error_type draw_primitive(const render_vertex::index offset, const render_vertex::index size) noexcept = 0;
    };
    struct render_texture : public texture
    {
        virtual shared_ptr<render_system> system() const noexcept = 0;
        virtual error_type save(uint32_t& oper, const window_size offset, const window_size size) const noexcept = 0;
        virtual error_type query(shared_ptr<render_query>& data) noexcept = 0;
    };
    struct render_message
    {
        uint32_t oper = 0;
        shared_ptr<texture> texture;
        matrix<color> colors;
    };
    class thread;
    struct render_system : public event_system
    {
        virtual const icy::thread& thread() const noexcept = 0;
        icy::thread& thread() noexcept
        {
            return const_cast<icy::thread&>(static_cast<const render_system*>(this)->thread());
        }
        virtual adapter adapter() const noexcept = 0;
        virtual error_type create(const window_size size, const render_flags flags, shared_ptr<render_texture>& texture) noexcept = 0;
        virtual error_type create(const const_matrix_view<color> data, shared_ptr<texture>& texture) noexcept = 0;
    };

    error_type create_render_system(shared_ptr<render_system>& system, const adapter adapter) noexcept;
}
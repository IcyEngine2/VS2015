#pragma once

#include "icy_render_math.hpp"
#include "icy_array.hpp"

namespace icy
{    
    enum class render_type : uint32_t
    {
        none,
        svg,
        /*camera,
        skybox,
        light_direct,
        light_cone,
        static_mesh,*/
        _count,
    };
    enum class render_flag : uint32_t
    {
        none        =   0x00,
        disabled    =   0x01,
    };
    struct render_object
    {
        uint64_t index : 0x1E;
        uint64_t count : 0x1E;
        uint64_t otype : 0x04;
    };
    static_assert(sizeof(render_object) == sizeof(uint64_t), "INVALID OBJECT SIZE");    
        
    class render_system
    {
    public:
        virtual error_type insert(array<render_d2d_vertex>&& input, render_object& object) noexcept = 0;
        virtual error_type remove(const render_object object) noexcept = 0;
        virtual error_type set_flag(const render_object object, const render_flag flag) noexcept = 0;
    };
}
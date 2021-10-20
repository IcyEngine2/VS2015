#pragma once

#define _USE_MATH_DEFINES 1
#include <corecrt_math_defines.h>
#include <icy_engine/core/icy_array.hpp>
#include <icy_engine/core/icy_color.hpp>
#include <icy_engine/resource/icy_engine_resource.hpp>

namespace icy
{
    enum class render_animation_type : uint32_t
    {
        none,
        first,
        linear,
        repeat,
        _total,
    };
    enum class render_light_type : uint32_t
    {
        none,
        dir,
        point,
        spot,
        ambient,
        area,
        _total,
    };
    enum class render_texture_type : uint32_t
    {
        none,
        diffuse,
        specular,
        ambient,
        emissive,
        height,
        normals,
        shininess,
        opacity,
        displacement,
        lightmap,
        reflection,
        pbr_base_color,
        pbr_normal_camera,
        pbr_emission_color,
        pbr_metalness,
        pbr_diffuse_roughness,
        pbr_ambient_occlusion,
        _total,
    };
    enum class render_shading_model : uint32_t
    {
        none,
        wire,
        flat,
        gouraud,
        phong,
        blinn,
        comic,
        oren_nayar,
        minnaert,
        cook_torrance,
        fresnel,
        _total,
    };
    enum class render_texture_map_type : uint32_t
    {
        none,
        uv,
        sphere,
        cylinder,
        box,
        plane,
        _total,
    };
    enum class render_texture_map_mode : uint32_t
    {
        wrap,
        clamp,
        decal,
        mirror,
        _total,
    };
    enum class render_texture_oper : uint32_t
    {
        multiply,
        add,
        substract,
        divide,
        smooth_add, //  T = (T1 + T2) - (T1 * T2)
        signed_add, //  T = T1 + (T2-0.5)
        _total,
    };
    enum class render_texture_compress : uint32_t
    {
        none,
        dds1,
        dds5,
        _total,
    };
    enum class render_mesh_type : uint32_t
    {
        none,
        point,
        line,
        triangle,
        polygon,
        _total,
    };

    union render_vec4
    {
        render_vec4() noexcept : vec{ 0, 0, 0, 1 }
        {

        }
        render_vec4(const float x, const float y, const float z, const float w) : vec{ x, y, z, w}
        {

        }
        struct
        {
            float x;
            float y;
            float z;
            float w;
        };
        float vec[4];
    };
    union render_vec3
    {
        render_vec3() noexcept : vec{}
        {

        }
        render_vec3(const float x, const float y, const float z) : vec{ x, y, z }
        {

        }
        struct
        {
            float u;
            float v;
            float w;
        };
        struct
        {
            float x;
            float y;
            float z;
        };
        float vec[3];
    };
    union render_vec2
    {
        render_vec2() noexcept : vec{}
        {

        }
        render_vec2(const float x, const float y) : vec{ x, y }
        {

        }
        struct
        {
            float u;
            float v;
        };
        struct
        {
            float x;
            float y;
        };
        float vec[2];
    };
    struct render_transform
    {
        render_vec3 world = render_vec3(0, 0, 0);
        render_vec3 scale = render_vec3(1, 1, 1);
        render_vec4 angle = render_vec4(0, 0, 0, 1);
    };

    struct render_gui_vtx
    {
        render_vec2 pos;
        render_vec2 tex;
        color col;
    };
    struct render_gui_cmd
    {
        render_vec4 clip;
        guid tex;//  ID
        uint32_t vtx = 0;   //  offset
        uint32_t idx = 0;   //  offset
        uint32_t size = 0;  //  Count
    };
    struct render_gui_list
    {
        array<uint32_t> idx;
        array<render_gui_vtx> vtx;
        array<render_gui_cmd> cmd;
    };
    struct render_gui_frame
    {
        array<render_gui_list> data;
        uint32_t vbuffer = 0;
        uint32_t ibuffer = 0;
        render_vec4 viewport;
    };

    struct render_animation_node
    {
        struct step3
        {
            double time = 0;
            render_vec3 value;
        };
        struct step4
        {
            double time = 0;
            render_vec4 value;
        };
        array<step3> world;
        array<step3> scale;
        array<step4> angle;
        render_animation_type prefix = render_animation_type::none;
        render_animation_type suffix = render_animation_type::none;
    };
    struct render_animation
    {
        double frames = 0;
        double fps = 0;
        map<guid, render_animation_node> nodes;
    };
    struct render_texture
    {
        render_texture_type type = render_texture_type::none;
        render_texture_map_type map_type = render_texture_map_type::none;
        render_texture_map_mode map_mode[3] = {};
        render_texture_compress compress = render_texture_compress::none;
        double blend = 0;
        window_size size;
        array<uint8_t> bytes;
    };
    struct render_material
    {
        struct tuple
        {
            guid index;
            render_texture_type type = render_texture_type::none;
            render_texture_oper oper = render_texture_oper::multiply;
        };
        render_shading_model shading = render_shading_model::none;
        bool two_sided = false;
        double opacity = 0;
        double transparency = 0;
        double bump_scaling = 0;
        double reflectivity = 0;
        double shininess = 0;
        double shininess_percent = 0;
        double refract = 0;
        color color_diffuse;
        color color_ambient;
        color color_specular;
        color color_emissive;
        color color_transparent;
        color color_reflective;
        array<tuple> textures;
    };
    struct render_bone
    {
        struct weight
        {
            uint32_t vertex = 0;
            double value = 0;
        };
        string name;
        array<weight> weights;
        render_transform transform;
    };
    struct render_mesh
    {
        struct channel
        {
            array<render_vec3> tex;
            array<color> color;
            uint32_t uv = 0; //  Specifies the number of components for a given UV channel
        };
        render_mesh() noexcept = default;
        render_mesh_type type = render_mesh_type::none;
        array<render_vec3> world;
        array<render_vec4> angle;
        channel channels[8];
        array<uint32_t> indices;
        array<render_bone> bones;
        guid material;
    };
    struct render_light
    {
        render_light_type type = render_light_type::none;
        render_vec2 size;
        render_vec3 world;
        render_vec4 angle;
        double factor_const = 0;
        double factor_linear = 1;
        double factor_quad = 0;
        color diffuse;
        color specular;
        color ambient;
        double angle_inner = 2 * M_PI;
        double angle_outer = 2 * M_PI;
    };
    struct render_camera
    {
        render_vec3 world;
        render_vec4 angle;
        double fov = M_PI / 4;
        double zmin = 0.1f;
        double zmax = 1000.0f;
        double aspect = 0;
    };
    struct render_node
    {
        render_node() noexcept
        {
        
        }
        render_transform transform;
        guid parent;
        array<guid> nodes;
        array<guid> meshes;
        unique_ptr<render_camera> camera;
        unique_ptr<render_light> light;
    };
    struct render_scene
    {
        map<guid, render_texture> textures;
        map<guid, render_mesh> meshes;
        map<guid, render_material> materials;
        map<guid, render_node> nodes;
        render_vec4 viewport;
    };

    inline error_type copy(const render_gui_list& src, render_gui_list& dst) noexcept
    {
        ICY_ERROR(copy(src.idx, dst.idx));
        ICY_ERROR(copy(src.vtx, dst.vtx));
        ICY_ERROR(copy(src.cmd, dst.cmd));
        return error_type();
    }
    inline error_type copy(const render_gui_frame& src, render_gui_frame& dst) noexcept
    {
        dst.vbuffer = src.vbuffer;
        dst.ibuffer = src.ibuffer;
        dst.viewport = src.viewport;
        ICY_ERROR(copy(src.data, dst.data));
        return error_type();
    }

    string_view to_string(const render_animation_type input) noexcept;
    string_view to_string(const render_light_type input) noexcept;
    string_view to_string(const render_texture_type input) noexcept;
    string_view to_string(const render_shading_model input) noexcept;
    string_view to_string(const render_texture_map_type input) noexcept;
    string_view to_string(const render_texture_map_mode input) noexcept;
    string_view to_string(const render_texture_oper input) noexcept;
    string_view to_string(const render_texture_compress input) noexcept;
    string_view to_string(const render_mesh_type input) noexcept;

    error_type to_value(const json& input, render_vec2& output) noexcept;
    error_type to_value(const json& input, render_vec3& output) noexcept;
    error_type to_value(const json& input, render_vec4& output) noexcept;
    error_type to_value(const json& input, render_transform& output) noexcept;
    error_type to_json(const render_vec2& input, json& output) noexcept;
    error_type to_json(const render_vec3& input, json& output) noexcept;
    error_type to_json(const render_vec4& input, json& output) noexcept;
    error_type to_json(const render_transform& input, json& output) noexcept;

    error_type to_value(const json& input, render_animation_node::step3& output) noexcept;
    error_type to_value(const json& input, render_animation_node::step4& output) noexcept;
    error_type to_value(const json& input, render_animation_node& output) noexcept;
    error_type to_value(const json& input, render_animation& output) noexcept;
    error_type to_value(const json& input, render_texture& output) noexcept;
    error_type to_value(const json& input, render_material::tuple& output) noexcept;
    error_type to_value(const json& input, render_material& output) noexcept;
    error_type to_value(const json& input, render_bone::weight& output) noexcept;
    error_type to_value(const json& input, render_bone& output) noexcept;
    error_type to_value(const json& input, render_mesh::channel& output) noexcept;
    error_type to_value(const json& input, render_mesh& output) noexcept;
    error_type to_value(const json& input, render_light& output) noexcept;
    error_type to_value(const json& input, render_camera& output) noexcept;
    error_type to_value(const json& input, render_node& output) noexcept;
    error_type to_json(const render_animation_node::step3& input, json& output) noexcept;
    error_type to_json(const render_animation_node::step4& input, json& output) noexcept;
    error_type to_json(const render_animation_node& input, json& output) noexcept;
    error_type to_json(const render_animation& input, json& output) noexcept;
    error_type to_json(const render_texture& input, json& output) noexcept;
    error_type to_json(const render_material::tuple& input, json& output) noexcept;
    error_type to_json(const render_material& input, json& output) noexcept;
    error_type to_json(const render_bone::weight& input, json& output) noexcept;
    error_type to_json(const render_bone& input, json& output) noexcept;
    error_type to_json(const render_mesh::channel& input, json& output) noexcept;
    error_type to_json(const render_mesh& input, json& output) noexcept;
    error_type to_json(const render_light& input, json& output) noexcept;
    error_type to_json(const render_camera& input, json& output) noexcept;
    error_type to_json(const render_node& input, json& output) noexcept;
}
#pragma once

#include <icy_engine/core/icy_event.hpp>
#include <icy_engine/core/icy_string.hpp>
#include <icy_engine/core/icy_matrix.hpp>
#include <icy_engine/core/icy_color.hpp>
#include <icy_engine/core/icy_map.hpp>
#include <icy_engine/utility/icy_variant.hpp>

namespace icy
{
    class json;
    enum class resource_locale : uint32_t
    {
        none,
        _total,
    };
    enum class resource_type : uint32_t
    {
        none,
        user,
        text,
        image,
        animation,
        texture,
        material,
        mesh,
        node,
        _total,
    };
    struct resource_header
    {
        explicit operator bool() const noexcept
        {
            if (false
                || !index
                || locale >= resource_locale::_total
                || type == resource_type::none
                || type >= resource_type::_total)
                return false;
            return true;
        }
        guid index;
        resource_type type = resource_type::none;
        resource_locale locale = resource_locale::none;
        string name;
    };
    struct resource_binary
    {
        error_type to_json(icy::json& output) const noexcept;
        error_type from_json(const icy::json& input) noexcept;
        clock_type::time_point time_create = {};
        clock_type::time_point time_update = {};
        int64_t hash = 0;
    };
    struct resource_data
    {
        error_type to_json(icy::json& output) const noexcept;
        error_type from_json(const icy::json& input) noexcept;
        resource_type type = resource_type::none;
        string name;
        map<uint32_t, resource_binary> binary;
    };

    struct render_animation;
    struct render_texture;
    struct render_material;
    struct render_mesh;
    struct render_node;

    struct resource_event 
    {
        resource_event() noexcept = default;
        resource_event(resource_event&& rhs) noexcept : 
            header(std::move(rhs.header)), time_create(rhs.time_create), time_update(rhs.time_update),
            hash(rhs.hash), error(rhs.error), bytes(rhs.bytes)
        {
            rhs.bytes = nullptr;
            rhs.header.type = resource_type::none;
        }
        ICY_DEFAULT_MOVE_ASSIGN(resource_event);
        void check(const resource_type type) noexcept
        {
#if _DEBUG
            ICY_ASSERT(header.type == type && bytes, "INVALID TYPE OR NULL BYTES");
#endif
        }
        ~resource_event() noexcept;
    public:
        array<uint8_t>& user() noexcept
        {
            check(resource_type::user);
            return *static_cast<array<uint8_t>*>(bytes);
        }
        string& text() noexcept
        {
            check(resource_type::text);
            return *static_cast<string*>(bytes);
        }
        matrix<color>& image() noexcept
        {
            check(resource_type::image);
            return *static_cast<matrix<color>*>(bytes);
        }
        render_animation& animation() noexcept
        {
            check(resource_type::animation);
            return *static_cast<render_animation*>(bytes);
        }
        render_texture& texture() noexcept
        {
            check(resource_type::texture);
            return *static_cast<render_texture*>(bytes);
        }
        render_material& material() noexcept
        {
            check(resource_type::material);
            return *static_cast<render_material*>(bytes);
        }
        render_mesh& mesh() noexcept
        {
            check(resource_type::mesh);
            return *static_cast<render_mesh*>(bytes);
        }
        render_node& node() noexcept
        {
            check(resource_type::node);
            return *static_cast<render_node*>(bytes);
        }    
    public:
        resource_header header;
        clock_type::time_point time_create = {};
        clock_type::time_point time_update = {};
        uint64_t hash = 0;
        error_type error;
        void* bytes = nullptr;   
    };
    struct resource_system : public event_system
    {
        static shared_ptr<resource_system> global() noexcept;
        virtual const icy::thread& thread() const noexcept = 0;
        virtual icy::thread& thread() noexcept
        {
            return const_cast<icy::thread&>(static_cast<const resource_system*>(this)->thread());
        }
        virtual error_type load(const resource_header& header) noexcept = 0;
        virtual error_type store(const resource_header& header, const string_view path) noexcept = 0;
        virtual error_type store(const resource_header& header, const const_array_view<uint8_t> bytes) noexcept = 0;
        virtual error_type store(const guid& index, const const_matrix_view<color> colors) noexcept = 0;
        virtual error_type list(map<guid, resource_data>& output) const noexcept = 0;
    };

    error_type create_resource_system(shared_ptr<resource_system>& system, const string_view path, const size_t capacity) noexcept;
    
    inline string_view to_string(const resource_type type) noexcept
    {
        switch (type)
        {
        case resource_type::user: return "user"_s;
        case resource_type::text: return "text"_s;
        case resource_type::image: return "image"_s;
        case resource_type::animation: return "animation"_s;
        case resource_type::texture: return "texture"_s;
        case resource_type::material: return "material"_s;
        case resource_type::mesh: return "mesh"_s;
        case resource_type::node: return "node"_s;
        }
        return string_view();
    }
    inline string_view to_string(const resource_locale locale) noexcept
    {
        switch (locale)
        {
        case resource_locale::none: return "enUS"_s;
        }
        return string_view();
    }
}
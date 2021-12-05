#pragma once

#include <icy_engine/core/icy_event.hpp>
#include <icy_engine/core/icy_map.hpp>
#include <icy_engine/utility/icy_variant.hpp>

namespace icy
{
    struct component_type_enum
    {
        enum
        {
            none        =   0,
            user        =   1u  <<  0u,
            transform   =   1u  <<  1u, //  render_transform        
            mesh        =   1u  <<  2u, //  guid[] (ref to render_mesh)
        };
    };
    using component_type = decltype(component_type_enum::none);
    
    
    class entity
    {
    public:
        entity() noexcept = default;
        entity(const uint32_t count, const component_type types) noexcept : m_value((uint64_t(types) << 0x20) | uint64_t(count))
        {
            
        }
        uint64_t index() const noexcept
        {
            return m_value;
        }
        component_type types() const noexcept
        {
            return component_type(m_value >> 0x20);
        }
    private:
        uint64_t m_value = 0;
    };
    enum class entity_event_type : uint32_t
    {
        none,
        create_entity,
        remove_entity,
        modify_component,
    };
    struct entity_event
    {
        entity_event_type type = entity_event_type::none;
        entity entity;
        component_type ctype = component_type::none;
        variant value;
    };
    struct entity_data
    {
        entity parent;
        array<entity> children;
        map<component_type, variant> data;
    };
    struct entity_system : public event_system
    {
        static shared_ptr<entity_system> global() noexcept;
        virtual error_type create_entity(const entity parent, const component_type types, entity& output) noexcept = 0;
        virtual error_type remove_entity(const entity entity) noexcept = 0;
        virtual error_type query_entity(const entity entity, entity_data& data) const noexcept = 0;
        virtual error_type modify_component(const entity entity, const component_type type, const variant& value) noexcept = 0;
    };
    error_type create_entity_system(shared_ptr<entity_system>& system) noexcept;
}
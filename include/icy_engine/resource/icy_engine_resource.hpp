#pragma once

#include <icy_engine/core/icy_event.hpp>
#include <icy_engine/core/icy_string.hpp>
#include <icy_engine/core/icy_matrix.hpp>
#include <icy_engine/core/icy_color.hpp>
#include <icy_engine/core/icy_map.hpp>
#include <icy_engine/utility/icy_variant.hpp>

namespace icy
{
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
        uint64_t hash = 0;
    };
    struct resource_data
    {
        error_type to_json(icy::json& output) const noexcept;
        error_type from_json(const icy::json& input) noexcept;
        resource_type type = resource_type::none;
        string name;
        map<uint32_t, resource_binary> binary;
    };

    struct resource_event 
    {
        resource_header header;
        clock_type::time_point time_create = {};
        clock_type::time_point time_update = {};
        uint64_t hash = 0;
        error_type error;
        struct
        {
            array<uint8_t> user;
            string text;
            matrix<color> image;
        } data;
    };
    struct resource_system : public event_system
    {
        virtual const icy::thread& thread() const noexcept = 0;
        virtual icy::thread& thread() noexcept
        {
            return const_cast<icy::thread&>(static_cast<const resource_system*>(this)->thread());
        }
        virtual error_type load(const resource_header& header) noexcept = 0;
        virtual error_type store(const resource_header& header, const string_view path) noexcept = 0;
        virtual error_type store(const resource_header& header, const const_array_view<uint8_t> bytes) noexcept = 0;
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
#pragma once

#include "icy_parser.hpp"

namespace icy
{
    enum class osm_error_code : uint32_t
    {
        success,
        index_not_found,
        index_invalid_format,
        index_is_not_positive,
        invalid_tag_parent,
        invalid_tag_name,
        tag_key_not_found,
        tag_val_not_found,
        member_type_not_found,
        member_type_unknown,
    };
    extern const error_source error_source_osm_parser;
    inline error_type make_osm_error(const osm_error_code code) noexcept
    {
        return error_type(unsigned(code), error_source_osm_parser);
    }

    struct osm_type_enum
    {
        enum
        {
            null,
            node,
            way,
            rel,
            _count,
        };
    };
    using osm_type = decltype(osm_type_enum::null);
    struct osm_index
    {
        osm_index() : index(0), type(osm_type::null)
        {

        }
        uint64_t index : 0x3E;
        uint64_t type : 0x02;
    };
    struct osm_node : public osm_index
    {
        map<string, string> tags;
    };
    struct osm_way : public osm_node
    {
        array<osm_index> refs;
    };
    struct osm_rel : public osm_way
    {
        array<string> roles;
    };
    class osm_parser : public xml_parser
    {
    public:
        virtual error_type callback(const osm_node& base) noexcept = 0;
    private:
        error_type callback(const xml_node& node) noexcept override final;
    private:
        osm_rel m_base;
        const xml_node* m_root = nullptr;
    };

    inline error_type osm_parser::callback(const xml_node& node) noexcept
    {
        const auto get_type = [](const string_view str)
        {
            switch (hash(str))
            {
            case "node"_hash: return osm_type::node;
            case "way"_hash: return osm_type::way;
            case "relation"_hash:
            case "rel"_hash: return osm_type::rel;
            }
            return osm_type::null;
        };
        const auto check_index = [](const string* ptr, osm_index& value)
        {
            if (!ptr)
                return make_osm_error(osm_error_code::index_not_found);

            auto index = 0i64;
            if (ptr->to_value(index))
                return make_osm_error(osm_error_code::index_invalid_format);

            if (index <= 0)
                return make_osm_error(osm_error_code::index_is_not_positive);

            value.index = uint64_t(index);
            return error_type();
        };
        const auto reset = [this]()
        {
            m_root = nullptr;
            m_base.type = 0;
            m_base.refs.clear();
            m_base.tags.clear();
            m_base.roles.clear();
        };

        if (&node == m_root && node.done)
        {
            ICY_ERROR(callback(m_base));
            reset();
            return {};
        }

        const auto type = get_type(node.name);

        if (type)
        {
            ICY_ERROR(check_index(node.attributes.try_find("id"_s), m_base));
            reset();
            m_root = &node;
            m_base.type = type;
            for (auto&& pair : node.attributes)
                ICY_ERROR(emplace(m_base.tags, pair.key, pair.value));
            if (node.done)
            {
                ICY_ERROR(callback(m_base));
                reset();
                return {};
            }
        }
        else
        {
            if (node.parent != m_root)
                return make_osm_error(osm_error_code::invalid_tag_parent);
        }

        if (node.name == "tag"_s)
        {
            const auto str_key = node.attributes.try_find("k"_s);
            const auto str_val = node.attributes.try_find("v"_s);
            if (!str_key)
                return make_osm_error(osm_error_code::tag_key_not_found);
            if (!str_val)
                return make_osm_error(osm_error_code::tag_val_not_found);
            ICY_ERROR(emplace(m_base.tags, *str_key, *str_val));
        }
        else if (node.name == "nd"_s)
        {
            osm_index new_index;
            new_index.type = osm_type::node;
            ICY_ERROR(check_index(node.attributes.try_find("ref"_s), new_index));
            ICY_ERROR(m_base.refs.push_back(new_index));
        }
        else if (node.name == "member"_s)
        {
            const auto str_type = node.attributes.try_find("type"_s);
            if (!str_type)
                return make_osm_error(osm_error_code::member_type_not_found);

            osm_index new_index;
            new_index.type = get_type(*str_type);
            if (!new_index.type)
                return make_osm_error(osm_error_code::member_type_unknown);

            ICY_ERROR(check_index(node.attributes.try_find("ref"_s), new_index));
            ICY_ERROR(m_base.refs.push_back(new_index));
            const auto str_role = node.attributes.try_find("role"_s);
            if (str_role)
            {
                string new_role;
                ICY_ERROR(to_string(*str_role, new_role));
                ICY_ERROR(m_base.roles.push_back(std::move(new_role)));
            }
        }
        else if (node.name == "osm"_s)
        {
            //  skip
        }
        else if (!type)
        {
            return make_osm_error(osm_error_code::invalid_tag_name);
        }
        return {};
    }

    static error_type osm_error_to_string(const unsigned code, const string_view, string& str) noexcept
    {
        string_view msg;
        switch (static_cast<osm_error_code>(code))
        {
        case osm_error_code::index_not_found:
            msg = "index not found"_s;
            break;

        case osm_error_code::index_invalid_format:
            msg = "index is not a valid integer"_s;
            break;

        case osm_error_code::index_is_not_positive:
            msg = "index is not positive"_s;
            break;

        case osm_error_code::invalid_tag_parent:
            msg = "invalid <tag> parent"_s;
            break;

        case osm_error_code::invalid_tag_name:
            msg = "invalid <tag> name"_s;
            break;

        case osm_error_code::tag_key_not_found:
            msg = "<tag> key 'k' not found"_s;
            break;

        case osm_error_code::tag_val_not_found:
            msg = "<tag> value 'v' not found"_s;
            break;

        case osm_error_code::member_type_not_found:
            msg = "<member> 'type' not found"_s;
            break;

        case osm_error_code::member_type_unknown:
            msg = "member 'type' unknown"_s;
            break;
        }
        return icy::to_string(msg, str);
    }
    //extern const error_source icy::error_source_osm_parser = register_error_source("osm"_s, osm_error_to_string);
}
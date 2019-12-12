#pragma once

#include <icy_engine/utility/icy_json.hpp>
#include <icy_engine/core/icy_file.hpp>

namespace icy
{
    enum class config_error_code : uint32_t
    {
        success,
        invalid_path,
        is_required,
        integer_overflow,
        string_empty,
        string_length,
        string_unknown,
        invalid_value,
        invalid_type,
    };
    extern const error_source error_source_config;
    inline error_type make_config_error(const config_error_code code) noexcept
    {
        return error_type(uint32_t(code), error_source_config);
    }

    class config
    {
    public:
        enum param_type : uint32_t
        {
            required,
            optional
        };
    public:
        config(const char delim = '/') noexcept : m_delim(delim)
        {

        }
        error_type bind(const string_view path, json&& value, const param_type type = param_type::optional) noexcept;
        error_type bind(const string_view path, const json_type_boolean value, const param_type type = param_type::optional) noexcept
        {
            return bind(path, json(value), type);
        }
        error_type bind(const string_view path, const json_type_integer value, const param_type type = param_type::optional) noexcept
        {
            return bind(path, json(value), type);
        }
        error_type bind(const string_view path, const json_type_double value, const param_type type = param_type::optional) noexcept
        {
            return bind(path, json(value), type);
        }
        error_type bind(const string_view path, const string_view value, const param_type type = param_type::optional) noexcept
        {
            string str;
            ICY_ERROR(to_string(value, str));
            return bind(path, json(std::move(str)), type);
        }
        template<typename T>
        error_type operator()(const json& json, const string_view path, T& value) const noexcept
        {
            return operator()(hash64(path), value);
        }
        error_type operator()(const json& json, const uint64_t hash_path, json_type_boolean& value) const noexcept;
        error_type operator()(const json& json, const uint64_t hash_path, json_type_double& value) const noexcept;
        error_type operator()(const json& json, const uint64_t hash_path, std::make_unsigned_t<json_type_integer>& value) noexcept;
        template<typename T, typename = std::enable_if_t<std::is_integral<T>::value>>
        error_type operator()(const json& json, const uint64_t hash_path, T& value) noexcept
        {
            const icy::json* ptr = nullptr;
            auto ptype = param_type::required;
            const auto error = get(json, hash_path, ptr, ptype);
            if (!ptr)
                ICY_ERROR(error);

            json_type_integer integer = 0;
            ICY_ERROR(ptr->get(integer));
            if (integer > json_type_integer(std::numeric_limits<T>::max()) ||
                integer < json_type_integer(std::numeric_limits<T>::min()))
            {
                return make_config_error(config_error_code::integer_overflow);
            }
            ICY_ERROR(filter(hash_path, integer));
            value = T(integer);
            return {};
        }
        error_type operator()(const json& json, const uint64_t hash_path, string& value) const noexcept;
        virtual error_type filter(const uint64_t, const json_type_integer) const noexcept
        {
            return {};
        }
        virtual error_type filter(const uint64_t, const json_type_double) const noexcept
        {
            return {};
        }
        virtual error_type filter(const uint64_t, const string_view) const noexcept
        {
            return {};
        }
        void clear() noexcept
        {
            m_map.clear();
        }
    private:
        struct value_type
        {
            param_type type = param_type::required;
            json value;
            array<string> path;
        };
    private:
        error_type get(const json& root, const uint64_t hash_path, const json*& ptr, config::param_type& ptype) const noexcept;
    private:
        const char m_delim;
        map<uint64_t, value_type> m_map;
    };
    
}
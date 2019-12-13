#include <icy_engine/utility/icy_config.hpp>

using namespace icy;

static error_type config_error_to_string(const uint32_t code, const string_view, string& str)
{
    switch (config_error_code(code))
    {
    case config_error_code::invalid_path:
        return to_string("Invalid parameter path"_s, str);
    case config_error_code::is_required:
        return to_string("Parameter is required"_s, str);
    case config_error_code::integer_overflow:
        return to_string("Integer overflow"_s, str);
    case config_error_code::string_empty:
        return to_string("String is empty"_s, str);
    case config_error_code::string_length:
        return to_string("String has invalid length"_s, str);
    case config_error_code::string_unknown:
        return to_string("Unknown string value", str);
    case config_error_code::invalid_value:
        return to_string("Invalid value"_s, str);
    case config_error_code::invalid_type:
        return to_string("Invalid type"_s, str);
    }
    return make_stdlib_error(std::errc::invalid_argument);
}
error_type config::operator()(const json& json, const uint64_t hash_path, json_type_boolean& value) const noexcept
{
    const icy::json* ptr = nullptr;
    auto ptype = param_type::required;
    const auto error = get(json, hash_path, ptr, ptype);
    if (!ptr)
        return error;

    ICY_ERROR(ptr->get(value));
    return {};
}
error_type config::operator()(const json& json, const uint64_t hash_path, json_type_double& value) const noexcept
{
    const icy::json* ptr = nullptr;
    auto ptype = param_type::required;
    const auto error = get(json, hash_path, ptr, ptype);
    if (!ptr)
        return error;
    
    ICY_ERROR(ptr->get(value));
    ICY_ERROR(filter(hash_path, value));
    return {};
}
error_type config::operator()(const json& json, const uint64_t hash_path, std::make_unsigned_t<json_type_integer>& value) noexcept
{
    const icy::json* ptr = nullptr;
    auto ptype = param_type::required;
    const auto error = get(json, hash_path, ptr, ptype);
    if (!ptr)
        return error;

    json_type_integer integer = 0;
    ICY_ERROR(ptr->get(integer));
    if (integer < 0)
        return make_config_error(config_error_code::integer_overflow);
    
    ICY_ERROR(filter(hash_path, integer));
    value = static_cast<std::make_unsigned_t<json_type_integer>>(integer);
    return {};

}
error_type config::operator()(const json& json, const uint64_t hash_path, string& value) const noexcept
{
    const icy::json* ptr = nullptr;
    auto ptype = param_type::required;
    const auto error = get(json, hash_path, ptr, ptype);
    if (!ptr)
        return error;

    ICY_ERROR(to_string(ptr->get(), value));
    if (value.empty())
    {
        if (ptype == param_type::required)
            return make_config_error(config_error_code::string_empty);
        else
            return {};
    }
    ICY_ERROR(filter(hash_path, value));
    return {};
}
error_type config::bind(const string_view path, json&& value, const param_type type) noexcept
{
    value_type new_val;
    new_val.value = std::move(value);
    new_val.type = type;
    array<string_view> words;
    ICY_ERROR(split(path, words, m_delim));
    for (auto&& word : words)
    {
        string str;
        ICY_ERROR(to_string(word, str));
        ICY_ERROR(new_val.path.push_back(std::move(str)));
    }
    ICY_ERROR(m_map.insert(hash64(path), std::move(new_val)));
    return {};
}
error_type config::get(const json& root, const uint64_t hash_path, const json*& ptr, config::param_type& ptype) const noexcept
{
    const auto it = m_map.find(hash_path);
    if (it == m_map.end())
        return make_config_error(config_error_code::invalid_path);

    ptr = &root;
    for (auto k = 0u; k < it->value.path.size(); ++k)
    {
        const auto next = ptr->find(it->value.path[k]);
        ptr = next;
        if (!ptr)
        {
            ptr = &it->value.value;
            ptype = it->value.type;
            if (it->value.type == param_type::required)
                return make_config_error(config_error_code::is_required);
            break;
        }
    }
    return {};
}

const error_source icy::error_source_config = register_error_source("config"_s, config_error_to_string);
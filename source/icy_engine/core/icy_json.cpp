#include <icy_engine/core/icy_json.hpp>
#include "jsmn.h"
#include <cmath>

using namespace icy;

static json_type get_json_type(const string_view str, const jsmntok_t& token) noexcept
{
	switch (token.type)
	{
	case jsmntype_t::JSMN_ARRAY:
		return json_type::array;
	case jsmntype_t::JSMN_OBJECT:
		return json_type::object;
	case jsmntype_t::JSMN_STRING:
		return json_type::string;
	case jsmntype_t::JSMN_PRIMITIVE:
	{
		auto beg = str.bytes().data() + token.start;
		const auto end = str.bytes().data() + token.end;
		if (*beg == 't' || *beg == 'f')
			return json_type::boolean;
		else if (*beg == 'n')
			return json_type::none;
		for (; beg < end; ++beg)
		{
			if (*beg == '.')
				return json_type::floating;
		}
		return json_type::integer;
	}
	}
	return json_type::none;
};
template<typename T>
static error_type json_to_integer(const json& json, T& value) noexcept
{
    json_type_integer integer = 0;
    ICY_ERROR(json.get(integer));
    const auto max = std::numeric_limits<T>::max();
    const auto min = std::numeric_limits<T>::min();
    if (integer > max)
        value = max;
    else if (integer < min)
        value = min;
    else
        value = T(integer);
    return error_type();
}

json::json(json&& rhs) noexcept : m_type(rhs.m_type)
{
    switch (m_type)
    {
    case json_type::boolean:
        new (&m_boolean) json_type_boolean(rhs.m_boolean);
        break;
    case json_type::integer:
        new (&m_integer) json_type_integer(rhs.m_integer);
        break;
    case json_type::floating:
        new (&m_floating) json_type_double(rhs.m_floating);
        break;
    case json_type::string:
        new (&m_string) json_type_string(std::move(rhs.m_string));
        break;
    case json_type::array:
        new (&m_array) json_type_array(std::move(rhs.m_array));
        break;
    case json_type::object:
        new (&m_object) json_type_object(std::move(rhs.m_object));
        break;
    default:
        memset(&m_object, 0, sizeof(m_object));
        break;
    }
    new (&rhs) json;
}
json::~json() noexcept
{
    switch (m_type)
    {
    case icy::json_type::string:
        m_string.~string();
        break;
    case icy::json_type::array:
        m_array.~array();
        break;
    case icy::json_type::object:
        m_object.~map();
        break;
    }
}
json::json(const json_type_boolean value) noexcept : m_type(json_type::boolean), m_boolean(value)
{

}
json::json(const json_type_integer value) noexcept : m_type(json_type::integer), m_integer(value)
{

}
json::json(const json_type_double value) noexcept : m_type(json_type::floating), m_floating(value)
{

}
json::json(json_type_string&& value) noexcept : m_type(json_type::string), m_string(std::move(value))
{

}
error_type json::create(const string_view str, json& parent, const const_array_view<jsmntok_t> tokens, const int token) noexcept
{
	const auto val = string_view(str.bytes().data() + tokens[token].start, str.bytes().data() + tokens[token].end);

	parent.m_type = get_json_type(str, tokens[token]);
	switch (parent.m_type)
	{
	case json_type::boolean:
		return to_value(val, parent.m_boolean);
	case json_type::integer:
		return to_value(val, parent.m_integer);
	case json_type::floating:
		return to_value(val, parent.m_floating);
	
	case json_type::string:
		new (&parent.m_string) json_type_string;
		return to_string(val, parent.m_string);
	
	case json_type::array:
	{
		new (&parent.m_array) json_type_array;
		ICY_ERROR(parent.m_array.resize(tokens[token].size));
		auto count = 0;
		for (auto k = 0; k < tokens.size() && count < tokens[token].size; ++k)
		{
			if (tokens[k].parent != token)
				continue;
			ICY_ERROR(create(str, parent.m_array[count], tokens, k));
			++count;
		}
		break;
	}
	case json_type::object:
	{
		new (&parent.m_object) json_type_object;
		ICY_ERROR(parent.m_object.reserve(tokens[token].size));
		auto count = 0;
		for (auto k = 0; k < tokens.size() - 1 && count < tokens[token].size; ++k)
		{
			if (tokens[k].parent != token)
				continue;
			if (tokens[k].type != jsmntype_t::JSMN_STRING || tokens[k + 1].parent != k)
				return make_stdlib_error(std::errc::illegal_byte_sequence);

			auto key_str = string_view(str.bytes().data() + tokens[k].start, str.bytes().data() + tokens[k].end);
			string key;
			ICY_ERROR(to_string(key_str, key));
			json new_json;
			ICY_ERROR(create(str, new_json, tokens, k + 1));
			ICY_ERROR(parent.m_object.insert(std::move(key), std::move(new_json)));
			++count;
		}
		break;
	}
	}
	return error_type();
}
error_type json::get(json_type_boolean& value) const noexcept
{
    switch (m_type)
    {
    case json_type::boolean:
        value = m_boolean;
        break;
    case json_type::integer:
        value = !!m_integer;
        break;
    case json_type::floating:
        value = !!m_floating;
        break;
    default:
        value = false;
        return make_stdlib_error(std::errc::invalid_argument);
    }
    return error_type();
}
error_type json::get(uint8_t& value) const noexcept
{
    return json_to_integer(*this, value);
}
error_type json::get(uint16_t& value) const noexcept
{
    return json_to_integer(*this, value);
}
error_type json::get(uint32_t& value) const noexcept
{
    return json_to_integer(*this, value);
}
error_type json::get(uint64_t& value) const noexcept
{
    json_type_integer integer = 0;
    ICY_ERROR(get(integer));
    if (integer < 0)
        value = 0;
    else
        value = uint64_t(integer);
    return error_type();
}
error_type json::get(int8_t& value) const noexcept
{
    return json_to_integer(*this, value);
}
error_type json::get(int16_t& value) const noexcept
{
    return json_to_integer(*this, value);
}
error_type json::get(int32_t& value) const noexcept
{
    return json_to_integer(*this, value);
}
error_type json::get(int64_t& value) const noexcept
{
    switch (m_type)
    {
    case json_type::boolean:
        value = m_boolean;
        break;
    case json_type::integer:
        value = m_integer;
        break;
    case json_type::floating:
        value = llround(m_floating);
        break;
    default:
        value = 0;
        return make_stdlib_error(std::errc::invalid_argument);
    }
    return error_type();
}
error_type json::get(float& value) const noexcept
{
    const auto max = std::numeric_limits<float>::max();
    const auto min = std::numeric_limits<float>::min();
    json_type_double ext_value = 0;
    ICY_ERROR(get(ext_value));
    const auto abs_value = fabs(ext_value);
    const auto sign = std::signbit(ext_value);
    if (abs_value > max)
    {
        value = sign ? -max : +max;        
    }
    else if (abs_value < min)
    {
        value = sign ? -min : +min;
    }
    else
    {
        value = float(ext_value);
    }
    return error_type();
}
error_type json::get(double& value) const noexcept
{
    switch (m_type)
    {
    case json_type::boolean:
        value = m_boolean;
        break;
    case json_type::integer:
        value = json_type_double(m_integer);
        break;
    case json_type::floating:
        value = m_floating;
        break;
    default:
        value = 0;
        return make_stdlib_error(std::errc::invalid_argument);
    }
    return error_type();
}
string_view json::get() const noexcept
{
    if (m_type == json_type::string)
        return m_string;
    else
        return string_view{};
}
string_view json::get(const string_view key) const noexcept
{
    if (const auto ptr = find(key))
        return ptr->get();
    return string_view{};
}
error_type json::get(const string_view key, json_type_string& value) const noexcept
{
    if (const auto ptr = find(key))
        return to_string(ptr->get(), value);
    return make_stdlib_error(std::errc::invalid_argument);
}
size_t json::size() const noexcept
{
    if (m_type == json_type::array)
        return m_array.size();
    else if (m_type == json_type::object)
        return m_object.size();
    else
        return 0;
}
const json* json::find(const string_view key) const noexcept
{
    if (m_type == json_type::object)
    {
        const auto keys = m_object.keys();
        const auto it = binary_search(keys.begin(), keys.end(), key);
        if (it != keys.end())
            return &m_object.vals()[size_t(it - keys.begin())];
    }
    return nullptr;
}
const json* json::at(const size_t index) const noexcept
{
    if (m_type == json_type::array)
    {
        if (index < size())
            return &m_array[index];
    }
    return nullptr;
}
error_type json::insert(const string_view key, json&& json) noexcept
{
    if (m_type == json_type::object)
    {
        auto ptr = find(key);
        if (!ptr)
        {
            string key_str;
            ICY_ERROR(to_string(key, key_str));
            ICY_ERROR(m_object.insert(std::move(key_str), std::move(json)));
        }
        else
        {
            *ptr = std::move(json);
        }
        return error_type();
    }
    return make_stdlib_error(std::errc::invalid_argument);
}
error_type json::insert(const string_view key, const string_view value) noexcept
{
    string str;
    ICY_ERROR(to_string(value, str));
    ICY_ERROR(insert(key, json(std::move(str))));
    return error_type();
}
error_type json::push_back(json&& json) noexcept
{
    if (m_type == json_type::array)
        return m_array.push_back(std::move(json));
    else
        return make_stdlib_error(std::errc::invalid_argument);
}
error_type json::reserve(const size_t size) noexcept
{
    if (m_type == json_type::object)
        return m_object.reserve(size);
    else if (m_type == json_type::array)
        return m_array.reserve(size);
    else
        return make_stdlib_error(std::errc::invalid_argument);
}
const_array_view<string> json::keys() const noexcept
{
    if (m_type == json_type::object)
        return m_object.keys();
    return const_array_view<string>();
}
const_array_view<json> json::vals() const noexcept
{
    if (m_type == json_type::object)
        return m_object.vals();
    else if (m_type == json_type::array)
        return m_array;
    return const_array_view<json>();
}

static error_type to_string(const json& json, string& str, const string_view tab, const string_view prefix) noexcept
{
    switch (json.type())
    {
    case json_type::none:
    {
        ICY_ERROR(str.append("null"_s));
        break;
    }
    case json_type::boolean:
    {
        string append;
        auto value = false;
        ICY_ERROR(json.get(value));
        ICY_ERROR(to_string(value, append));
        ICY_ERROR(str.append(append));
        break;
    }
    case json_type::integer:
    {
        string append;
        json_type_integer value = 0;
        ICY_ERROR(json.get(value));
        ICY_ERROR(to_string(value, append));
        ICY_ERROR(str.append(append));
        break;
    }
    case json_type::floating:
    {
        string append;
        json_type_integer value = 0;
        ICY_ERROR(json.get(value));
        ICY_ERROR(to_string(value, append));
        ICY_ERROR(str.append(append));
        break;
    }
    case json_type::string:
    {
        ICY_ERROR(str.append("\""_s));
        ICY_ERROR(str.append(json.get()));
        ICY_ERROR(str.append("\""_s));
        break;
    }
    case json_type::array:
    {
        ICY_ERROR(str.append("["_s));        
        for (auto k = 0u; k < json.size(); ++k)
        {
            if (k)
            {
                ICY_ERROR(str.append(", "_s));
            }
            if (const auto sub = json.at(k))
            {
                string substr;
                ICY_ERROR(::to_string(*sub, substr, {}, {}));
                ICY_ERROR(str.append(substr));
            }
        }
        ICY_ERROR(str.append("]"_s));
        break;
    }

    case json_type::object:
    {
        ICY_ERROR(str.appendf("%1%2"_s, prefix, string_view("{"_s)));
        string new_prefix;
        ICY_ERROR(new_prefix.appendf("%1%2"_s, prefix, tab));

        for (auto k = 0u; k < json.size(); ++k)
        {
            const auto& key = json.keys()[k];
            const auto& val = json.vals()[k];
            string substr;
            if (val.type() == json_type::object)
            {
                ICY_ERROR(::to_string(val, substr, tab, new_prefix));
            }
            else
            {
                ICY_ERROR(::to_string(val, substr, {}, {}));
            }
            ICY_ERROR(str.appendf("%1\"%2\": %3%4"_s, string_view(new_prefix), string_view(key), string_view(substr), k + 1 == json.size()? ""_s : ","_s));
        }
        ICY_ERROR(str.appendf("%1%2", prefix, "}"_s));
        break;
    }

    default:
        break;
    }
    return error_type();
}
error_type icy::to_string(const json& json, string& str, const string_view tab) noexcept
{
    return ::to_string(json, str, tab, tab.empty() ? ""_s : "\r\n"_s);
}
error_type icy::to_value(const string_view str, json& root) noexcept
{
    root = json_type::none;
    array<jsmntok_t> tokens;
    while (true)
    {
        jsmn_parser parser = {};
        jsmn_init(&parser);

        const auto count = jsmn_parse(&parser, str.bytes().data(), str.bytes().size(), tokens.data(), uint32_t(tokens.size()));
        if (count == JSMN_ERROR_NOMEM)
            return make_stdlib_error(std::errc::not_enough_memory);
        else if (count < 0)
            return make_stdlib_error(std::errc::illegal_byte_sequence);
        else if (count == 0)
            return error_type();
        else if (tokens.size() == size_t(count))
            break;
        ICY_ERROR(tokens.resize(size_t(count)));
    }
    return json::create(str, root, tokens, 0);
}
error_type icy::copy(const json& src, json& dst) noexcept
{
    switch (src.m_type)
    {
    case icy::json_type::none:
        dst = {};
        break;

    case icy::json_type::boolean:
        dst = json{ src.m_boolean };
        break;

    case icy::json_type::integer:
        dst = json{ src.m_integer };
        break;

    case icy::json_type::floating:
        dst = json{ src.m_floating };
        break;

    case icy::json_type::string:
    {
        json_type_string str;
        ICY_ERROR(to_string(src.m_string, str));
        dst = json{ std::move(str) };
        break;
    }
    case icy::json_type::array:
    {
        dst = json_type::array;
        ICY_ERROR(dst.m_array.reserve(src.size()));
        for (auto k = 0u; k < src.size(); ++k)
        {
            json tmp;
            ICY_ERROR(copy(src.m_array[k], tmp));
            ICY_ERROR(dst.m_array.push_back(std::move(tmp)));
        }
        break;
    }
    case icy::json_type::object:
    {
        dst = json_type::object;
        ICY_ERROR(dst.m_object.reserve(src.size()));

        for (auto k = 0u; k < src.size(); ++k)
        {
            json_type_string key;
            ICY_ERROR(copy(src.m_object.keys()[k], key));
            json val;
            ICY_ERROR(copy(src.m_object.vals()[k], val));
            ICY_ERROR(dst.m_object.insert(std::move(key), std::move(val)));
        }
        break;
    }
    }
    return error_type();
}
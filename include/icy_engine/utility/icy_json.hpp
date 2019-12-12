#pragma once

#include <icy_engine/core/icy_string.hpp>
#include <icy_engine/core/icy_array.hpp>
#include <icy_engine/core/icy_map.hpp>

struct jsmntok_t;

#pragma warning(disable:4582)
#pragma warning(disable:4062)

namespace icy
{
    template<typename K, typename V> class map;
    class string;

	class json;
	enum class json_type : uint32_t
	{
		none,
		boolean,
		integer,
		floating,
		string,
		array,
		object,
	};
	using json_type_boolean = bool;
	using json_type_integer = int64_t;
	using json_type_double = double;
	using json_type_string = string;
	using json_type_array = array<json>;
	using json_type_object = map<json_type_string, json>;
		
	class json
	{
		static error_type create(const string_view str, json& parent, const const_array_view<jsmntok_t> tokens, const int token) noexcept;
	public:
		static error_type create(const string_view str, json& root) noexcept;
		json(const json_type type = json_type::none) noexcept : m_type(type)
		{
			memset(&m_string, 0, sizeof(m_string));
		}
        json_type type() const noexcept
        {
            return m_type;
        }
        json(const json_type_boolean value) noexcept;
        json(const json_type_integer value) noexcept;
        json(const json_type_double value) noexcept;
		explicit json(const char*) noexcept = delete;
        json(json_type_string&& value) noexcept;
        json(json&& rhs) noexcept;
		ICY_DEFAULT_MOVE_ASSIGN(json);
        ~json() noexcept;
        error_type copy(json& dst) const noexcept;
		error_type get(json_type_boolean& value) const noexcept;
		error_type get(json_type_integer& value) const noexcept;
        error_type get(json_type_double& value) const noexcept;
        string_view get() const noexcept;
		error_type get(const string_view key, json_type_boolean& value) const noexcept;
		error_type get(const string_view key, json_type_integer& value) const noexcept;
        error_type get(const string_view key, json_type_double& value) const noexcept;
        string_view get(const string_view key) const noexcept;
        error_type get(const string_view key, json_type_string& value) const noexcept;
        size_t size() const noexcept;
        const json* find(const string_view key) const noexcept;
        const json* at(const size_t index) const noexcept;
		json* find(const string_view key) noexcept
		{
			return const_cast<json*>(static_cast<const json*>(this)->find(key));
		}
		json* at(const size_t index) noexcept
		{
			return const_cast<json*>(static_cast<const json*>(this)->at(index));
		}
		error_type insert(const string_view key, json&& json) noexcept;
        error_type insert(const string_view key, const string_view value) noexcept;
		error_type push_back(json&& json) noexcept;
		error_type reserve(const size_t size) noexcept;
		const_array_view<string> keys() const noexcept;
        const_array_view<json> vals() const noexcept;
	private:
        union
        {
            json_type_boolean m_boolean;
            json_type_integer m_integer;
            json_type_double m_floating;
            json_type_string m_string;
            json_type_array m_array;
            json_type_object m_object;
        };
        json_type m_type = json_type::none;
	};
    error_type to_string(const json& json, string& str, const string_view tab = {}) noexcept;
}
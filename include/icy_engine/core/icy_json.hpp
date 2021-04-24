#pragma once

#include "icy_string.hpp"
#include "icy_array.hpp"
#include "icy_map.hpp"

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
		
    error_type to_value(const string_view str, json& root) noexcept;

	class json
	{
        friend error_type to_value(const string_view str, json& root) noexcept;
        friend error_type copy(const json& src, json& dst) noexcept;
		static error_type create(const string_view str, json& parent, const const_array_view<jsmntok_t> tokens, const int token) noexcept;
	public:
		json(const json_type type = json_type::none) noexcept : m_type(type)
		{
            switch (type)
            {
            case json_type::boolean:
                new (&m_boolean) json_type_boolean{};
                break;
            case json_type::integer:
                new (&m_integer) json_type_integer{};
                break;
            case json_type::floating:
                new (&m_floating) json_type_double{};
                break;
            case json_type::string:
                new (&m_string) json_type_string{};
                break;
            case json_type::array:
                new (&m_array) json_type_array{};
                break;
            case json_type::object:
                new (&m_object) json_type_object{};
                break;
            default:
                memset(this, 0, sizeof(*this));
                break;
            }
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
        error_type get(bool& value) const noexcept;
        error_type get(uint8_t& value) const noexcept;
        error_type get(uint16_t& value) const noexcept;
        error_type get(uint32_t& value) const noexcept;
        error_type get(uint64_t& value) const noexcept;
        error_type get(int8_t& value) const noexcept;
        error_type get(int16_t& value) const noexcept;
        error_type get(int32_t& value) const noexcept;
        error_type get(int64_t& value) const noexcept;
        error_type get(float& value) const noexcept;
        error_type get(double& value) const noexcept;
        string_view get() const noexcept;
        template<typename T>
        error_type get(const string_view key, T& value) const noexcept
        {
            if (const auto ptr = find(key))
                return ptr->get(value);
            return make_stdlib_error(std::errc::invalid_argument);
        }
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
        error_type insert(const string_view key, const bool value) noexcept
        {
            return insert(key, json(value));
        }
        template<typename T, typename = std::enable_if_t<std::is_integral<T>::value>>
        error_type insert(const string_view key, const T value) noexcept
        {
            return insert(key, json(json_type_integer(value)));
        }
        error_type insert(const string_view key, const float value) noexcept
        {
            return insert(key, json(json_type_double(value)));
        }
        error_type insert(const string_view key, const double value) noexcept
        {
            return insert(key, json(value));
        }
        error_type insert(const string_view key, const string_view value) noexcept;
		error_type push_back(json&& json) noexcept;
        template<typename T, typename = std::enable_if_t<std::is_integral<T>::value>>
        error_type push_back(const T value) noexcept
        {
            return push_back(json(json_type_integer(value)));
        }
        error_type push_back(const float value) noexcept
        {
            return push_back(json(json_type_double(value)));
        }
        error_type push_back(const double value) noexcept
        {
            return push_back(json(value));
        }
        error_type push_back(const string_view str) noexcept
        {
            string new_str;
            ICY_ERROR(copy(str, new_str));
            return push_back(json(std::move(new_str)));
        }
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
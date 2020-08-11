#pragma once

#include <icy_engine/core/icy_string.hpp>
#include <icy_engine/core/icy_smart_pointer.hpp>

#define LUA_NOEXCEPT noexcept(false)

namespace icy
{
    extern const error_source error_source_lua;

    enum class lua_type : uint32_t
    {
        none,
        boolean,
        number,
        pointer,
        string,
        function,
        table,
        binary,
    };
    extern const error_type lua_error_execute;

    class lua_system;
    class lua_object;
    class lua_variable;
    using lua_cfunction = error_type(*)(void* user, const const_array_view<lua_variable> input, array<lua_variable>* output);
    extern const char* lua_keywords;

    enum class lua_default_library
    {
        none,
        math,
        string,
        utf8,
        table,
        threads,
    };
    class lua_system
    {
    public:
        virtual ~lua_system() noexcept = 0
        {

        }
        virtual error_type parse(lua_variable& function, const string_view str, const char* const name = nullptr) const LUA_NOEXCEPT = 0;
        virtual error_type make_table(lua_variable& var) const LUA_NOEXCEPT = 0;
        virtual error_type make_string(lua_variable& var, const string_view str) const LUA_NOEXCEPT = 0;
        template<typename T>
        error_type make_binary(lua_variable& var, const T& value) const LUA_NOEXCEPT
        {
            static_assert(std::is_trivially_destructible<T>::value, "TYPE MUST BE TRIVIAL");
            return make_binary(var, const_array_view<uint8_t>(reinterpret_cast<const uint8_t*>(&value), sizeof(T)));
        }
        virtual error_type make_binary(lua_variable& var, const const_array_view<uint8_t> bytes) const LUA_NOEXCEPT = 0;
        virtual error_type make_function(lua_variable& var, const lua_cfunction func, void* const data) const LUA_NOEXCEPT = 0;
        virtual error_type make_library(lua_variable& var, const lua_default_library type) const LUA_NOEXCEPT = 0;
        virtual error_type copy(const lua_variable& src, lua_variable& dst) const LUA_NOEXCEPT = 0;
        virtual error_type post_error(const string_view str) const LUA_NOEXCEPT = 0;
        virtual error_type print(const lua_variable& var, error_type(*pfunc)(void* pdata, const string_view str), void* const pdata) const LUA_NOEXCEPT = 0;
        virtual realloc_func realloc() const noexcept = 0;
        virtual void* user() const noexcept = 0;
    };

    error_type create_lua_system(shared_ptr<lua_system>& system, const realloc_func realloc, void* const user) noexcept;
    inline error_type create_lua_system(shared_ptr<lua_system>& system, heap* const heap = nullptr)
    {
        return create_lua_system(system, heap ? heap_realloc : global_realloc, heap);
    }

    class lua_object
    {
    public:
        virtual void add_ref() noexcept = 0;
        virtual void release() noexcept = 0;
        virtual error_type find(const lua_variable& key, lua_variable& val) LUA_NOEXCEPT = 0;
        virtual error_type insert(const lua_variable& key, const lua_variable& val) LUA_NOEXCEPT = 0;
        virtual error_type map(array<lua_variable>& keys, array<lua_variable>& vals) LUA_NOEXCEPT = 0;
        virtual error_type operator()(const const_array_view<lua_variable> input, array<lua_variable>* output) LUA_NOEXCEPT = 0;
    };
    class lua_variable
    {
    public:
        using boolean_type = bool;
        using number_type = double;
        using pointer_type = const void*;
        using binary_type = const_array_view<uint8_t>;
    public:
        lua_variable() noexcept : m_type(lua_type::none)
        {

        }
        lua_variable(const string_view str) noexcept : m_type(lua_type::string)
        {
            const realloc_func null_realloc = [](const void*, size_t, void*) -> void* { return nullptr; };
            new (&m_string) string(null_realloc, nullptr);
            
            auto end = str.begin();
            string_view short_str = { str.begin(), str.begin() };
            while (end != str.end())
            {
                ++end;
                auto new_short_str = string_view(str.begin(), end);
                if (new_short_str.bytes().size() >= m_string.capacity())
                    break;
                short_str = new_short_str;
            }
            m_string.append(short_str);
        }
        explicit lua_variable(const boolean_type value) noexcept : m_type(lua_type::boolean)
        {
            new (&m_boolean) boolean_type(value);
        }
        lua_variable(const number_type value) noexcept : m_type(lua_type::number)
        {
            new (&m_number) number_type(value);
        }
        lua_variable(const pointer_type value) noexcept : m_type(lua_type::pointer)
        {
            new (&m_pointer) pointer_type(value);
        }
        lua_variable(lua_object& value, const lua_type type) noexcept : m_type(type)
        {
            new (&m_object) (lua_object*)(&value);
            value.add_ref();
        }
        lua_variable(string&& value) noexcept : m_type(lua_type::string)
        {
            new (&m_string) string(std::move(value));
        }
        lua_variable(array<uint8_t>&& value) noexcept : m_type(lua_type::binary)
        {
            new (&m_binary) array<uint8_t>(std::move(value));
        }
        lua_variable(lua_variable&& rhs) noexcept : m_type(rhs.m_type)
        {
            switch (m_type)
            {
            case lua_type::boolean:
                new (&m_boolean) boolean_type(rhs.m_boolean);
                break;
            case lua_type::number:
                new (&m_number) number_type(rhs.m_number);
                break;
            case lua_type::pointer:
                new (&m_pointer) pointer_type(rhs.m_pointer);
                break;
            case lua_type::string:
                new (&m_string) string(std::move(rhs.m_string));
                break;
            case lua_type::table:
            case lua_type::function:
                new (&m_object) (lua_object*)(rhs.m_object);
                rhs.m_object = nullptr;
                break;
            case lua_type::binary:
                new (&m_binary) array<uint8_t>(std::move(rhs.m_binary));
                break;
            }
            rhs.m_type = lua_type::none;
        }
        ICY_DEFAULT_MOVE_ASSIGN(lua_variable);
        ~lua_variable() noexcept
        {
            switch (m_type)
            {
            case lua_type::string:
                m_string.~string();
                break;
            case lua_type::function:
            case lua_type::table:
                m_object->release();
                break;
            case lua_type::binary:
                m_binary.~array();
                break;
            }
        }
    public:
        lua_type type() const noexcept
        {
            return m_type;
        }
        boolean_type as_boolean() const noexcept
        {
            return m_type == lua_type::boolean ? m_boolean : false;
        }
        number_type as_number() const noexcept
        {
            return m_type == lua_type::number ? m_number : 0;
        }
        pointer_type as_pointer() const noexcept
        {
            return m_type == lua_type::pointer ? m_pointer : nullptr;
        }
        string_view as_string() const noexcept
        {
            return m_type == lua_type::string ? string_view(m_string) : string_view();
        }
        lua_object* as_object() const noexcept
        {
            return m_type == lua_type::function || m_type == lua_type::table ? m_object : nullptr;
        }
        binary_type as_binary() const noexcept
        {
            return m_type == lua_type::binary ? binary_type(m_binary) : binary_type();
        }
    public:
        error_type operator()(const const_array_view<lua_variable> input = {}) LUA_NOEXCEPT
        {
            if (auto ptr = as_object())
                return (*ptr)(input, nullptr);
            else
                return make_stdlib_error(std::errc::invalid_argument);
        }
        error_type operator()(const const_array_view<lua_variable> input, array<lua_variable>& output) LUA_NOEXCEPT
        {
            if (auto ptr = as_object())
                return (*ptr)(input, &output);
            else
                return make_stdlib_error(std::errc::invalid_argument);
        }
        error_type find(const lua_variable& key, lua_variable& val) LUA_NOEXCEPT
        {
            if (auto ptr = as_object())
                return ptr->find(key, val);
            else
                return make_stdlib_error(std::errc::invalid_argument);
        }
        error_type insert(const lua_variable& key, const lua_variable& val) LUA_NOEXCEPT
        {
            if (auto ptr = as_object())
                return ptr->insert(key, val);
            else
                return make_stdlib_error(std::errc::invalid_argument);
        }
        error_type map(array<lua_variable>& keys, array<lua_variable>& vals) LUA_NOEXCEPT
        {
            if (auto ptr = as_object())
                return ptr->map(keys, vals);
            else
                return make_stdlib_error(std::errc::invalid_argument);
        }
    private:
        lua_type m_type = lua_type::none;
        union
        {
            boolean_type m_boolean;
            number_type m_number;
            string m_string;
            pointer_type m_pointer;
            lua_object* m_object;
            array<uint8_t> m_binary;
        };
    };
    
    string_view to_string(const lua_default_library type) noexcept;
    error_type lua_error_to_string(const error_type& error, string& msg) noexcept;
}
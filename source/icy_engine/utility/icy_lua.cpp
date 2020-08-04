#include <icy_engine/core/icy_array.hpp>
#include <icy_engine/core/icy_set.hpp>
#include <icy_engine/utility/icy_lua.hpp>
//extern "C"
//{
#include "../../libs/lua/lua.h"
#include "../../libs/lua/lualib.h"
#include "../../libs/lua/lauxlib.h"
//}
//#include "../../libs/lua/lua.hpp"
#if _DEBUG
#pragma comment(lib, "libluad.lib")
#else
#pragma comment(lib, "liblua.lib")
#endif

using namespace icy;

static error_type make_lua_error(int code) noexcept
{
    if (code == LUA_ERRMEM)
        return make_stdlib_error(std::errc::not_enough_memory);
    else if (code != LUA_OK)
        return error_type(code, error_source_lua);
    else
        return error_type();
}
class lua_object_data;
struct lua_object_init
{
    enum class type : uint32_t
    {
        unknown,
        script,
        table,
        cfunc,
        luafunc,
        library,
        global,
    };
    lua_object_init(const type type) noexcept : type(type)
    {

    }
    const type type;
    string_view parse;
    const char* name = nullptr;
    lua_cfunction cfunc = nullptr;
    void* cdata = nullptr;
    lua_default_library lib = lua_default_library::none;
    int index = 0;
};
class lua_system_data : public lua_system
{
public:
    lua_system_data(const realloc_func realloc, void* const user) noexcept : m_realloc(realloc), m_user(user)
    {

    }
    ~lua_system_data() noexcept override
    {
        if (m_state)
            lua_close(m_state);
    }
    error_type initialize() noexcept;
    error_type destroy(lua_object_data* self) const LUA_NOEXCEPT;
    error_type find(const lua_object_data& object, const lua_variable& key, lua_variable& val) const LUA_NOEXCEPT;
    error_type insert(const lua_object_data& object, const lua_variable& key, const lua_variable& val) const LUA_NOEXCEPT;
    error_type map(const lua_object_data& object, array<lua_variable>& keys, array<lua_variable>& vals) const LUA_NOEXCEPT;
    error_type exec(const lua_object_data& object, const const_array_view<lua_variable> input, array<lua_variable>* output) const LUA_NOEXCEPT;
    void push(const lua_variable& var) const LUA_NOEXCEPT;
    void push(const lua_object_data& object) const LUA_NOEXCEPT;
    error_type to_value(const int index, lua_variable& var) const LUA_NOEXCEPT;
private:   
    error_type parse(lua_variable& func, const string_view parse, const char* const name) const LUA_NOEXCEPT override;
    error_type make_table(lua_variable& var) const LUA_NOEXCEPT override;
    error_type make_string(lua_variable& var, const string_view str) const LUA_NOEXCEPT override;
    error_type make_binary(lua_variable& var, const const_array_view<uint8_t> bytes) const LUA_NOEXCEPT override;
    error_type make_function(lua_variable& var, const lua_cfunction func, void* fdata) const LUA_NOEXCEPT override;
    error_type make_library(lua_variable& var, const lua_default_library lib) const LUA_NOEXCEPT override;
    error_type copy(const lua_variable& src, lua_variable& dst) const LUA_NOEXCEPT override;
    error_type post_error(const string_view msg) const LUA_NOEXCEPT override;
    error_type print(const lua_variable& var, error_type(*pfunc)(void* pdata, const string_view str), void* const pdata) const LUA_NOEXCEPT override;
    realloc_func realloc() const noexcept override
    {
        return m_realloc;
    }
    void* user() const noexcept override
    {
        return m_user;
    }
private:    
    template<typename func_type> error_type safe_call(const func_type& func, const bool only_safe = false) const LUA_NOEXCEPT
    {
        const auto proc = [](const void* const ptr) -> error_type
        {
            return (*static_cast<const func_type*>(ptr))();
        };
        return safe_call(proc, &func, only_safe);
    }
    error_type create(lua_variable& var, const lua_object_init& init) const LUA_NOEXCEPT;
    error_type print(int index, error_type(*func)(void* pdata, const string_view str), void* const pdata, const string_view tabs = ""_s) const LUA_NOEXCEPT;
    error_type safe_call(error_type(*func)(const void* const ptr), const void* const data, const bool only_safe) const LUA_NOEXCEPT;
private:
    const realloc_func m_realloc;
    void* const m_user;
    lua_State* m_state = nullptr;
    mutable bool m_safe = false;
};
class lua_object_data : public lua_object
{
    friend lua_system_data;
public:
    lua_object_data(const weak_ptr<lua_system_data> system, const decltype(lua_object_init::type::unknown) type) noexcept : 
        m_system(system), m_type(type)
    {

    }
    void add_ref() noexcept override
    {
        ++m_ref;
    }
    void release() noexcept override
    {
        if (--m_ref == 0)
        {
            if (auto ptr = shared_ptr<lua_system_data>(m_system))
                ptr->destroy(this);
        }
    }
    uint32_t ref_count() const noexcept
    {
        return m_ref;
    }
private:
    error_type find(const lua_variable& key, lua_variable& val) LUA_NOEXCEPT override
    {
        if (auto ptr = shared_ptr<lua_system_data>(m_system))
            return ptr->find(*this, key, val);
        return error_type();
    }
    error_type insert(const lua_variable& key, const lua_variable& val) LUA_NOEXCEPT override
    {
        if (auto ptr = shared_ptr<lua_system_data>(m_system))
            return ptr->insert(*this, key, val);
        return error_type();
    }
    error_type map(array<lua_variable>& keys, array<lua_variable>& vals) LUA_NOEXCEPT override
    {
        if (auto ptr = shared_ptr<lua_system_data>(m_system))
            return ptr->map(*this, keys, vals);
        return error_type();
    }
    error_type operator()(const const_array_view<lua_variable> input, array<lua_variable>* output) LUA_NOEXCEPT
    {
        if (m_func)
            return m_func(m_data, input, output);
        if (auto ptr = shared_ptr<lua_system_data>(m_system))
            return ptr->exec(*this, input, output);
        return error_type();
    }
private:
    uint32_t m_ref = 1;
    const weak_ptr<lua_system_data> m_system;
    decltype(lua_object_init::type::unknown) m_type;
    lua_cfunction m_func = nullptr;
    void* m_data = nullptr;
};

error_type lua_system_data::initialize() noexcept
{
    const lua_Alloc lua_realloc = [](void* user, void* ptr, size_t osize, size_t nsize)
    {
        if (nsize == 0 || nsize > osize)
        {
            const auto self = static_cast<lua_system_data*>(user);
            return self->m_realloc(ptr, nsize, self->m_user);
        }
        else
        {
            return ptr;
        }
    };
    m_state = lua_newstate(lua_realloc, this);
    if (!m_state)
        return make_stdlib_error(std::errc::not_enough_memory);
    return error_type();
}
error_type lua_system_data::destroy(lua_object_data* self) const LUA_NOEXCEPT
{
    const auto proc = [&]
    {
        const auto lua = m_state;
        if (!lua_checkstack(lua, 4))
            return make_stdlib_error(std::errc::not_enough_memory);

        lua_pushlightuserdata(lua, self);           //  self
        lua_gettable(lua, LUA_REGISTRYINDEX);       //  lua
        auto has_gc = false;
        if (lua_getmetatable(lua, -1))
        {                                           //  lua, metatable
            lua_pushstring(lua, "__gc");            //  lua, metatable; __gc
            lua_gettable(lua, -2);                  //  lua; metatable; gc_func
            if (lua_iscfunction(lua, -1))
                has_gc = true;
            lua_pop(lua, 2);                        //  lua;
        }
        else
        {

        }
        lua_pop(lua, 1);                            //  -
        if (!has_gc)
        {
            allocator_type::destroy(self);
            m_realloc(self, 0, m_user);
        }
        lua_pushlightuserdata(lua, self);   //  self
        lua_pushnil(lua);                   //  self; nil
        lua_settable(lua, -3);              //  -
        return error_type();
    };
    return safe_call(proc, true);
}
error_type lua_system_data::find(const lua_object_data& object, const lua_variable& key, lua_variable& val) const LUA_NOEXCEPT
{
    const auto proc = [&]
    {
        const auto lua = m_state;
        if (!lua_checkstack(lua, 3))
            return make_stdlib_error(std::errc::not_enough_memory);

        push(object);
        push(key);
        lua_gettable(lua, -2);
        ICY_SCOPE_EXIT{ lua_pop(lua, 2); };
        ICY_ERROR(to_value(-1, val));
        return error_type();
    };
    return safe_call(proc);
}
error_type lua_system_data::insert(const lua_object_data& object, const lua_variable& key, const lua_variable& val) const LUA_NOEXCEPT
{
    const auto proc = [&]
    {
        const auto lua = m_state;
        if (!lua_checkstack(lua, 3))
            return make_stdlib_error(std::errc::not_enough_memory);

        push(object);
        push(key);
        push(val);
        lua_settable(lua, -3);
        lua_pop(lua, 1);
        return error_type();
    };
    return safe_call(proc);
}
error_type lua_system_data::map(const lua_object_data& object, array<lua_variable>& keys, array<lua_variable>& vals) const LUA_NOEXCEPT
{
    const auto proc = [&]
    {
        const auto lua = m_state;
        if (!lua_checkstack(lua, 3))
            return make_stdlib_error(std::errc::not_enough_memory);

        keys = array<lua_variable>(m_realloc, m_user);
        vals = array<lua_variable>(m_realloc, m_user);

        push(object);
        ICY_SCOPE_EXIT{ lua_pop(lua, 1); };

        lua_pushnil(lua);
        while (lua_next(lua, -2))
        {
            auto success = false;
            ICY_SCOPE_EXIT{ if (!success) lua_pop(lua, 2); };
            lua_variable key;
            lua_variable val;
            ICY_ERROR(to_value(-2, key));
            ICY_ERROR(to_value(-1, val));
            ICY_ERROR(keys.push_back(std::move(key)));
            ICY_ERROR(vals.push_back(std::move(val)));
            success = true;
            lua_pop(lua, 1);
        }
        return error_type();
    };
    return safe_call(proc);
}
error_type lua_system_data::exec(const lua_object_data& object, const const_array_view<lua_variable> input, array<lua_variable>* output) const LUA_NOEXCEPT
{
    const auto proc = [&]
    {
        const auto lua = m_state;

        if (!lua_checkstack(lua, int(3 + input.size())))
            return make_stdlib_error(std::errc::not_enough_memory);

        const auto old_size = lua_gettop(lua);

        lua_pushlightuserdata(lua, const_cast<lua_object_data*>(&object));  //  &object
        lua_gettable(lua, LUA_REGISTRYINDEX);                               //  object
        for (auto&& var : input)
            push(var);

        //  function(lua); lua args [input.size()]
        lua_callk(lua, int(input.size()), output ? LUA_MULTRET : 0, 0, nullptr);
        if (output)
        {
            const auto new_size = lua_gettop(lua);
            const auto count = new_size - old_size;
            ICY_SCOPE_EXIT{ lua_settop(lua, old_size); };

            *output = array<lua_variable>(m_realloc, m_user);
            ICY_ERROR(output->reserve(count));

            for (auto k = 0; k < count; ++k)
            {
                const auto index = 1 + old_size + k;
                lua_variable new_var;
                ICY_ERROR(to_value(index, new_var));
                ICY_ERROR(output->push_back(std::move(new_var)));
            }
        }
        return error_type();
    };
    return safe_call(proc);
}
void lua_system_data::push(const lua_variable& var) const LUA_NOEXCEPT
{
    const auto lua = m_state;

    switch (var.type())
    {
    case lua_type::boolean:
    {
        lua_pushboolean(lua, var.as_boolean());
        break;
    }
    case lua_type::number:
    {
        lua_pushnumber(lua, var.as_number());
        break;
    }
    case lua_type::pointer:
    {
        lua_pushlightuserdata(lua, const_cast<void*>(var.as_pointer()));
        break;
    }
    case lua_type::string:
    {
        lua_pushlstring(lua, var.as_string().bytes().data(), var.as_string().bytes().size());
        break;
    }
    case lua_type::function:
    case lua_type::table:
    {
        push(*static_cast<const lua_object_data*>(var.as_object()));
        break;
    }
    case lua_type::binary:
    {
        const auto ptr = lua_newuserdata(lua, var.as_binary().size());
        memcpy(ptr, var.as_binary().data(), var.as_binary().size());
        break;
    }
    default:
    {
        lua_pushnil(lua);
        break;
    }
    }
}
void lua_system_data::push(const lua_object_data& object) const LUA_NOEXCEPT
{
    const auto lua = m_state;
    const auto s0 = lua_gettop(lua);
    lua_pushlightuserdata(lua, const_cast<lua_object_data*>(&object));  //  &object
    lua_gettable(lua, LUA_REGISTRYINDEX);                               //  table
    if (lua_getmetatable(lua, -1))      //  table; meta
    {
        lua_pushstring(lua, "__call");  //  table; meta; __call
        lua_gettable(lua, -2);          //  table; meta; call
        lua_remove(lua, -2);            //  table; call
        if (lua_getupvalue(lua, -1, 1)) //  table; call; ?env
        {
            lua_remove(lua, -2);        //  table; ?env
            if (::lua_type(lua, -1) == LUA_TTABLE) 
            {
                lua_remove(lua, -2);    //  env
            }
            else
            {
                lua_pop(lua, 1);        //  table
            }
        }
        else
        {
            lua_pop(lua, 1);            //  table
        }
    } 
    //ICY_ASSERT(lua_gettop(lua) == s0 + 1, "INVALID STACK");
}
error_type lua_system_data::to_value(const int index, lua_variable& var) const LUA_NOEXCEPT
{
    const auto lua = m_state;
    switch (::lua_type(lua, index))
    {
    case LUA_TBOOLEAN:
        var = lua_toboolean(lua, index);
        break;

    case LUA_TLIGHTUSERDATA:
        var = lua_touserdata(lua, index);
        break;

    case LUA_TNUMBER:
        var = lua_tonumberx(lua, index, nullptr);
        break;

    case LUA_TSTRING:
    {
        auto len = 0_z;
        auto ptr = lua_tolstring(lua, index, &len);
        string str(m_realloc, m_user);
        ICY_ERROR(icy::copy(string_view(ptr, len), str));
        var = std::move(str);
        break;
    }
    case LUA_TTABLE:
    {
        lua_object_data* object = nullptr;
        const auto ptr = lua_topointer(lua, index);
        lua_pushnil(lua);
        while (lua_next(lua, LUA_REGISTRYINDEX))
        {
            if (lua_topointer(lua, -1) == ptr)
            {
                object = static_cast<lua_object_data*>(lua_touserdata(lua, -2));
                lua_pop(lua, 2);
                break;
            }
            lua_pop(lua, 1);
        }
        auto is_func = false;
        if (lua_getmetatable(lua, index))
        {
            lua_pushstring(lua, "__call");      //  meta; "__call"
            lua_gettable(lua, -2);              //  meta; ?lua_func
            if (::lua_type(lua, -1) == LUA_TFUNCTION)
                is_func = true;
            lua_pop(lua, 2);
        }
        if (object)
        {
            var = lua_variable(*object, is_func ? lua_type::function : lua_type::table);
        }
        else
        {
            auto init = lua_object_init(lua_object_init::type::unknown);
            init.index = index;
            ICY_ERROR(create(var, init));
        }
        break;
    }
    case LUA_TFUNCTION:
    {
        auto init = lua_object_init(lua_object_init::type::unknown);
        init.index = index;
        ICY_ERROR(create(var, init));
        break;
    }
    case LUA_TUSERDATA:
    {
        const auto ptr = static_cast<const uint8_t*>(lua_touserdata(lua, index));
        const auto len = lua_rawlen(lua, index);
        array<uint8_t> vec(m_realloc, m_user);
        ICY_ERROR(vec.append(ptr, ptr + len));
        var = std::move(vec);
        break;
    }

    default:
        break;
    }
    return error_type();
}
error_type lua_system_data::parse(lua_variable& func, const string_view parse, const char* const name) const LUA_NOEXCEPT
{
    if (parse.empty())
        return make_stdlib_error(std::errc::invalid_argument);

    const auto proc = [&]
    {
        lua_object_init init(lua_object_init::type::script);
        init.name = name;
        init.parse = parse;
        return create(func, init);
    };
    return safe_call(proc);
}
error_type lua_system_data::make_table(lua_variable& var) const LUA_NOEXCEPT
{
    const auto proc = [&]
    {
        lua_object_init init(lua_object_init::type::table);
        return create(var, init);
    };
    return safe_call(proc);
}
error_type lua_system_data::make_string(lua_variable& var, const string_view str) const LUA_NOEXCEPT
{
    string new_str(m_realloc, m_user);
    ICY_ERROR(icy::copy(str, new_str));
    var = std::move(new_str);
    return error_type();
}
error_type lua_system_data::make_binary(lua_variable& var, const const_array_view<uint8_t> bytes) const LUA_NOEXCEPT
{
    array<uint8_t> new_binary(m_realloc, m_user);
    ICY_ERROR(icy::copy(bytes, new_binary));
    var = std::move(new_binary);
    return error_type();
}
error_type lua_system_data::make_function(lua_variable& var, const lua_cfunction func, void* fdata) const LUA_NOEXCEPT
{
    if (func == nullptr)
        return make_stdlib_error(std::errc::invalid_argument);
    const auto proc = [&]
    {
        lua_object_init init(lua_object_init::type::cfunc);
        init.cfunc = func;
        init.cdata = fdata;
        return create(var, init);
    };
    return safe_call(proc);
}
error_type lua_system_data::make_library(lua_variable& var, const lua_default_library lib) const LUA_NOEXCEPT
{
    if (lib == lua_default_library::none)
        return make_stdlib_error(std::errc::invalid_argument);

    const auto proc = [&]
    {
        lua_object_init init(lua_object_init::type::library);
        init.lib = lib;
        return create(var, init);
    };
    return safe_call(proc);
}
error_type lua_system_data::copy(const lua_variable& src, lua_variable& dst) const LUA_NOEXCEPT
{
    switch (src.type())
    {
    case lua_type::none:
        dst = lua_variable();
        break;
    case lua_type::boolean:
        dst = lua_variable(src.as_boolean());
        break;
    case lua_type::number:
        dst = lua_variable(src.as_number());
        break;
    case lua_type::pointer:
        dst = lua_variable(src.as_pointer());
        break;
    case lua_type::string:
    {
        string copy_str(m_realloc, m_user);
        ICY_ERROR(icy::copy(src.as_string(), copy_str));
        dst = std::move(copy_str);
        break;
    }
    case lua_type::table:
    case lua_type::function:
        dst = lua_variable(*src.as_object(), src.type());
        break;

    case lua_type::binary:
    {
        array<uint8_t> copy_vec(m_realloc, m_user);
        ICY_ERROR(icy::copy(src.as_binary(), copy_vec));
        dst = std::move(copy_vec);
        break;
    }
    default:
        return make_stdlib_error(std::errc::invalid_argument);
    }
    return error_type();
}
error_type lua_system_data::post_error(const string_view msg) const LUA_NOEXCEPT
{
    const auto proc = [&]
    {
        const auto lua = m_state;
        lua_pushlstring(lua, msg.bytes().data(), msg.bytes().size());
        lua_error(lua);
        return error_type();
    };
    return safe_call(proc);
}
error_type lua_system_data::print(const lua_variable& var, error_type(*pfunc)(void* pdata, const string_view str), void* const pdata) const LUA_NOEXCEPT
{
    if (!pfunc) pfunc = [](void*, const string_view str) { return win32_debug_print(str); };
    const auto proc = [&]
    {
        push(var);
        const auto error = print(-1, pfunc, pdata);
        lua_pop(m_state, 1);
        return error;
    };
    return safe_call(proc);
}
error_type lua_system_data::create(lua_variable& var, const lua_object_init& init) const LUA_NOEXCEPT
{
    const auto lua = m_state;
    if (!lua_checkstack(lua, 5))
        return make_stdlib_error(std::errc::not_enough_memory);

    auto new_object = static_cast<lua_object_data*>(m_realloc(nullptr, sizeof(lua_object_data), m_user));
    if (!new_object)
        return make_stdlib_error(std::errc::not_enough_memory);
    allocator_type::construct(new_object, make_shared_from_this(this), init.type);
    ICY_SCOPE_EXIT{ new_object->release(); };

    auto type = lua_type::table;
    if (init.index)
    {
        ICY_ASSERT(init.type == lua_object_init::type::unknown, "INVALID INIT TYPE");
        const auto abs = lua_absindex(lua, init.index);

        if (::lua_type(lua, abs) == LUA_TTABLE)
        {
            if (lua_getmetatable(lua, abs))
            {
                lua_pushstring(lua, "__call");  //  meta; "__call"
                lua_gettable(lua, -2);          //  meta; call
                if (::lua_type(lua, -1) != LUA_TNIL)
                {
                    type = lua_type::function;
                }
                lua_pop(lua, 2);
            }
        }
        else if (::lua_type(lua, abs) == LUA_TFUNCTION)
        {
            type = lua_type::function;
        }
        else
        {
            return make_stdlib_error(std::errc::invalid_argument);
        }
        lua_pushlightuserdata(lua, new_object); //  new_object;
        lua_pushvalue(lua, abs);                //  new_object; table
        lua_settable(lua, LUA_REGISTRYINDEX);   //  -
    }
    else
    {
        const auto gcproc = [](lua_State* lua)
        {
            const auto object_data = static_cast<lua_object_data*>(lua_touserdata(lua, lua_upvalueindex(1)));
            ICY_ASSERT(object_data->ref_count() == 0, "OBJECT STILL HAS USER REFERENCES");
            const auto ptr = shared_ptr<lua_system_data>(object_data->m_system);
            allocator_type::destroy(object_data);
            if (ptr)
                ptr->realloc()(object_data, 0, ptr->user());
            return 0;
        };
        if (init.type != lua_object_init::type::library && init.lib != lua_default_library::none)
            return make_stdlib_error(std::errc::invalid_argument);

        lua_pushlightuserdata(lua, new_object); //  new_object;
        switch (init.lib)
        {
        case lua_default_library::threads:
        {
            luaopen_coroutine(lua);
            break;
        }
        case lua_default_library::math:
        {
            luaopen_math(lua);
            break;
        }
        case lua_default_library::table:
        {
            luaopen_table(lua);
            break;
        }
        case lua_default_library::string:
        {
            luaopen_string(lua);
            break;
        }
        case lua_default_library::utf8:
        {
            luaopen_utf8(lua);
            break;
        }
        default:
        {
            lua_createtable(lua, 0, 0);
            break;
        }
        }                                       //  new_object; table
        lua_createtable(lua, 0, 0);             //  new_object; table; meta
        lua_pushstring(lua, "__gc");            //  new_object; table; meta; "__gc"
        lua_pushlightuserdata(lua, new_object); //  new_object; table; meta; "__gc"; new_object
        lua_pushcclosure(lua, gcproc, 1);       //  new_object; table; meta; "__gc"; gc
        lua_settable(lua, -3);                  //  new_object; table; meta

        if (init.type == lua_object_init::type::cfunc)
        {
            type = lua_type::function;
            new_object->m_func = init.cfunc;
            new_object->m_data = init.cdata;
            const auto cfunc_proc = [](lua_State* lua) -> int
            {
                const auto data = static_cast<lua_object_data*>(lua_touserdata(lua, lua_upvalueindex(1)));
                auto count = 0;
                if (auto ptr = shared_ptr<lua_system_data>(data->m_system))
                {
                    const auto error = [&]
                    {
                        array<lua_variable> input(ptr->realloc(), ptr->user());
                        array<lua_variable> output(ptr->realloc(), ptr->user());

                        for (auto k = 2; k <= lua_gettop(lua); ++k)
                        {
                            lua_variable var;
                            ICY_ERROR(ptr->to_value(k, var));
                            ICY_ERROR(input.push_back(std::move(var)));
                        }
                        ICY_ERROR(data->m_func(data->m_data, input, &output));

                        if (!lua_checkstack(lua, int(output.size())))
                            return make_stdlib_error(std::errc::not_enough_memory);

                        for (auto&& var : output)
                        {
                            ptr->push(var);
                            ++count;
                        }
                        return error_type();
                    }();
                    if (error)
                    {
                        string error_str(ptr->realloc(), ptr->user());
                        to_string(error, error_str);
                        if (!error_str.empty() && lua_checkstack(lua, 1))
                            lua_pushlstring(lua, error_str.bytes().data(), error_str.bytes().size());
                        return lua_error(lua);
                    }
                }
                return count;
            };
            lua_pushstring(lua, "__call");          //  new_object; table; meta; "__call"
            lua_pushlightuserdata(lua, new_object); //  new_object; table; meta; "__call"; new_object
            lua_pushcclosure(lua, cfunc_proc, 1);   //  new_object; table; meta; "__call"; cfunc_proc
            lua_settable(lua, -3);                  //  new_object; table; meta; 
        }
        else if (init.type == lua_object_init::type::script)
        {
            type = lua_type::function;
            lua_pushstring(lua, "__call");          //  new_object; table; meta; "__call"
            if (const auto code = luaL_loadbufferx(lua, init.parse.bytes().data(), init.parse.bytes().size(), init.name, "t"))
                lua_error(lua);
            //  new_object; table; meta; "__call"; lua_script; 
            lua_createtable(lua, 0, 0);     //  new_object; table; meta; "__call"; lua_script; env
            lua_pushstring(lua, "_G");      //  new_object; table; meta; "__call"; lua_script; env; "_G"
            lua_pushglobaltable(lua);       //  new_object; table; meta; "__call"; lua_script; env; "_G"; globals
            lua_settable(lua, -3);          //  new_object; table; meta; "__call"; lua_script; env
            lua_setupvalue(lua, -2, 1);     //  new_object; table; meta; "__call"; lua_script
            lua_settable(lua, -3);          //  new_object; table; meta                
        }
        //  new_object; table; meta
        lua_setmetatable(lua, -2);              //  new_object; table
        lua_settable(lua, LUA_REGISTRYINDEX);   //  -          
    }
    var = lua_variable(*new_object, type);
    return error_type();
}
error_type lua_system_data::print(int index, error_type(*func)(void* pdata, const string_view str), void* const pdata, const string_view tabs) const LUA_NOEXCEPT
{
    const auto lua = m_state;

    const auto proc = [&](const string_view tabs_str, const string_view name, int key)
    {
        ICY_ERROR(func(pdata, tabs_str));
        ICY_ERROR(func(pdata, "\""_s));
        if (key != 0)
        {
            ICY_ERROR(print(key, func, pdata, tabs_str));
        }
        else
        {
            ICY_ERROR(func(pdata, name));
        }
        ICY_ERROR(func(pdata, "\": "_s));
        const auto is_value_string = ::lua_type(lua, -1) == LUA_TSTRING;
        const auto is_value_table = ::lua_type(lua, -1) == LUA_TTABLE;
        if (is_value_string) ICY_ERROR(func(pdata, "\""_s));
        if (is_value_table) ICY_ERROR(func(pdata, "\r\n"_s));
        ICY_ERROR(print(-1, func, pdata, tabs_str));
        if (is_value_string) ICY_ERROR(func(pdata, "\""_s));
        ICY_ERROR(func(pdata, "\r\n"_s));
        return error_type();
    };

    switch (::lua_type(lua, index))
    {
    case LUA_TBOOLEAN:
        ICY_ERROR(func(pdata, lua_toboolean(lua, index) ? "true" : "false"));
        break;
    case LUA_TNUMBER:
    {
        string str(m_realloc, m_user);
        ICY_ERROR(to_string(lua_tonumberx(lua, index, nullptr), str));
        ICY_ERROR(func(pdata, str));
        break;
    }
    case LUA_TSTRING:
    {
        auto len = 0_z;
        auto ptr = lua_tolstring(lua, index, &len);
        ICY_ERROR(func(pdata, string_view(ptr, len)));
        break;
    }
    case LUA_TLIGHTUSERDATA:
    case LUA_TUSERDATA:
    {
        string str(m_realloc, m_user);
        ICY_ERROR(to_string(reinterpret_cast<uint64_t>(lua_touserdata(lua, index)), 16, str));
        ICY_ERROR(func(pdata, str));
        break;
    }
    case LUA_TFUNCTION:
    {
        if (const auto ptr = lua_tocfunction(lua, index))
        {
            string str(m_realloc, m_user);
            ICY_ERROR(to_string(reinterpret_cast<uint64_t>(ptr), 16, str));
            ICY_ERROR(str.insert(str.begin(), "__c function "_s));
            ICY_ERROR(func(pdata, str));
        }
        else
        {
            string tabs_str(m_realloc, m_user);
            ICY_ERROR(tabs_str.append(tabs));
            ICY_ERROR(tabs_str.append("  "_s));

            ICY_ERROR(func(pdata, "__lua function\r\n"_s));
            ICY_ERROR(func(pdata, tabs));
            ICY_ERROR(func(pdata, "{\r\n"_s));
            auto upvalue = 1u;
            while (auto ptr = lua_getupvalue(lua, index, upvalue++))
            {
                const auto error = proc(tabs_str, string_view(ptr, strlen(ptr)), 0);
                lua_pop(lua, 1);
                ICY_ERROR(error);
            }
            ICY_ERROR(func(pdata, tabs));
            ICY_ERROR(func(pdata, "}"));
        }
        break;
    }
    case LUA_TTABLE:
    {
        auto recursive = false;
        const auto abs = lua_absindex(lua, index);
        for (auto k = 1; k < abs; ++k)
        {
            if (lua_topointer(lua, k) == lua_topointer(lua, abs))
            {
                recursive = true;
                break;
            }
        }
        if (recursive)
        {
            ICY_ERROR(func(pdata, tabs));
            ICY_ERROR(func(pdata, "{ ... }\r\n"_s));
            return error_type();
        }

        string tabs_str(m_realloc, m_user);
        ICY_ERROR(tabs_str.append(tabs));
        ICY_ERROR(tabs_str.append("  "_s));
        ICY_ERROR(func(pdata, tabs));
        ICY_ERROR(func(pdata, "{\r\n"_s));
        lua_pushnil(lua);
        while (lua_next(lua, (index < 0 ? index - 1 : index)))
        {
            if (const auto error = proc(tabs_str, string_view(), -2))
            {
                lua_pop(lua, 2);
                return error;
            }
            else
            {
                lua_pop(lua, 1);
            }
        }
        if (lua_getmetatable(lua, index))
        {
            const auto proc = [&]
            {
                ICY_ERROR(func(pdata, tabs_str));
                ICY_ERROR(func(pdata, "\"__meta\":\r\n"_s));
                ICY_ERROR(print(-1, func, pdata, tabs_str));
                ICY_ERROR(func(pdata, "\r\n"_s));
                return error_type();
            };
            const auto error = proc();
            lua_pop(lua, 1);
            ICY_ERROR(error);
        }
        ICY_ERROR(func(pdata, tabs));
        ICY_ERROR(func(pdata, "}"));
        break;
    }
    default:
        ICY_ERROR(func(pdata, "null"));
        break;
    }
    return error_type();
}
error_type lua_system_data::safe_call(error_type(*func)(const void* const ptr), const void* const data, const bool only_safe) const LUA_NOEXCEPT
{
    const auto lua = m_state;
    const auto lua_proc = [](lua_State* lua)
    {
        const auto func = static_cast<error_type(*)(const void* const ptr)>(lua_touserdata(lua, lua_upvalueindex(1)));
        const auto data = lua_touserdata(lua, lua_upvalueindex(2));
        const auto system = static_cast<lua_system_data*>(lua_touserdata(lua, lua_upvalueindex(3)));

        const auto old_stack_size = lua_gettop(lua);
        const error_type error = (*func)(data);
        const auto new_stack_size = lua_gettop(lua);
        ICY_ASSERT(new_stack_size == old_stack_size, "STACK CHANGED");

        string error_str(system->m_realloc, system->m_user);
        if (error)
            to_string(error, error_str);

        if (!error_str.empty() && !lua_checkstack(lua, 1))
        {
            lua_pushlstring(lua, error_str.bytes().data(), error_str.bytes().size());
            return lua_error(lua);
        }
        return 0;
    };
    if (!m_safe || only_safe)
    {
        const auto old_safe = m_safe;
        m_safe = true;
        ICY_SCOPE_EXIT{ m_safe = old_safe; };

        const auto err_proc = [](lua_State* lua)
        {
            const char* msg = nullptr;
            if (lua_isstring(lua, -1))
                msg = lua_tolstring(lua, -1, nullptr);
            luaL_traceback(lua, lua, msg, 0);
            return 1;
        };
        if (!lua_checkstack(lua, 4))
            return make_stdlib_error(std::errc::not_enough_memory);

        const auto old_size = lua_gettop(lua);
        lua_pushcclosure(lua, err_proc, 0);
        const auto err_index = lua_gettop(lua);

        lua_pushlightuserdata(lua, func);
        lua_pushlightuserdata(lua, const_cast<void*>(data));
        lua_pushlightuserdata(lua, const_cast<lua_system_data*>(this));
        lua_pushcclosure(lua, lua_proc, 3);
        const auto code = lua_pcallk(lua, 0, LUA_MULTRET, err_index, 0, nullptr);
        auto error = make_lua_error(code);
        if (code == LUA_ERRRUN || code == LUA_ERRERR)
        {
            if (lua_isstring(lua, -1))
            {
                auto len = 0_z;
                auto ptr = lua_tolstring(lua, -1, &len);
                if (error.message = error_message::allocate(m_realloc, m_user, ptr, len))
                    ;
                else
                    error = make_stdlib_error(std::errc::not_enough_memory);
            }
        }
        lua_settop(lua, old_size);
        return error;
    }
    else
    {
        if (!lua_checkstack(lua, 3))
            return make_stdlib_error(std::errc::not_enough_memory);

        lua_pushlightuserdata(lua, func);
        lua_pushlightuserdata(lua, const_cast<void*>(data));
        lua_pushlightuserdata(lua, const_cast<lua_system_data*>(this));
        lua_pushcclosure(lua, lua_proc, 3);
        lua_callk(lua, 0, 0, 0, nullptr);
    }
    return error_type();
}

static error_type lua_error_to_string(unsigned int code, string_view locale, string& str) noexcept
{
    switch (code)
    {
    case LUA_YIELD: return icy::to_string("LUA_YIELD"_s, str);
    case LUA_ERRRUN: return icy::to_string("LUA_ERRRUN"_s, str);
    case LUA_ERRSYNTAX: return icy::to_string("LUA_ERRSYNTAX"_s, str);
    case LUA_ERRMEM: return icy::to_string("LUA_ERRMEM"_s, str);
    case LUA_ERRGCMM: return icy::to_string("LUA_ERRGCMM"_s, str);
    case LUA_ERRERR: return icy::to_string("LUA_ERRERR"_s, str);
    }
    return make_stdlib_error(std::errc::invalid_argument);
}
const error_source icy::error_source_lua = register_error_source("lua", ::lua_error_to_string);
const error_type icy::lua_error_execute = make_lua_error(LUA_ERRRUN);
const error_type icy::lua_error_parse = make_lua_error(LUA_ERRSYNTAX);

string_view icy::to_string(const lua_default_library lib) noexcept
{
    const char* ptr = nullptr;
    switch (lib)
    {
    case lua_default_library::math:
        ptr = LUA_MATHLIBNAME;
        break;
    case lua_default_library::string:
        ptr = LUA_STRLIBNAME;
        break;
    case lua_default_library::table:
        ptr = LUA_TABLIBNAME;
        break;
    case lua_default_library::threads:
        ptr = LUA_COLIBNAME;
        break;
    case lua_default_library::utf8:
        ptr = LUA_UTF8LIBNAME;
        break;
    default:
        break;
    }
    return ptr ? string_view(ptr, strlen(ptr)) : string_view();
}
error_type icy::lua_error_to_string(const error_type& error, string& msg) noexcept
{
    if (error.message)
    {
        auto error_msg = string_view(static_cast<const char*>(error.message->data()), error.message->size());
        array<string_view> lines;
        ICY_ERROR(split(error_msg, lines, "\r\n"));

        string new_error_msg;
        if (!lines.empty())
        {
            array<string_view> stack;
            for (auto k = 2u; k < lines.size(); ++k)
            {
                if (lines[k] != "\t[C]: in ?")
                    stack.push_back(lines[k]);
            }
            ICY_ERROR(new_error_msg.append(lines[0]));
            if (!stack.empty())
            {
                ICY_ERROR(new_error_msg.append("\r\nLUA Stack trace:"));
                for (auto&& line : stack)
                    ICY_ERROR(new_error_msg.appendf("\r\n%1", line));
            }
            error_msg = new_error_msg;
        }
        ICY_ERROR(copy(error_msg, msg));
    }
    return error_type();
};

error_type icy::create_lua_system(shared_ptr<lua_system>& system, realloc_func realloc, void* const user) noexcept
{
    if (!realloc)
        realloc = icy::global_realloc;

    shared_ptr<lua_system_data> new_ptr;
    ICY_ERROR(make_shared(new_ptr, realloc, user));
    ICY_ERROR(new_ptr->initialize());
    system = std::move(new_ptr);
    return error_type();
}
#include "../mbox_script_system.hpp"
#include <icy_engine/core/icy_set.hpp>
#include <icy_engine/core/icy_file.hpp>
#include <icy_engine/core/icy_thread.hpp>
#include <icy_engine/core/icy_map.hpp>

using namespace icy;
using namespace mbox;

ICY_STATIC_NAMESPACE_BEG
struct mbox_character;
struct mbox_group;
class mbox_system_data;

struct internal_event
{
    mbox_index character;
    input_message input;
};
struct mbox_group
{
    mbox_index index;
    lua_variable lua;
    lua_variable lua_characters;
    set<mbox_character*> characters;
};
enum class mbox_event_type : uint32_t
{
    user,
    timer,
    on_key_press,
    on_key_release,
};
struct mbox_event
{
    mbox_character* character = nullptr;
    lua_variable lua;
    mbox_event_type type = mbox_event_type::user;
    uint64_t uid = 0;
    key key = key::none;
    key_mod mods = key_mod::none;
    mbox_index ref;
};
struct mbox_character
{
    mbox_index index;
    lua_variable lua;
    uint32_t slot = 0;
    string name;
    set<mbox_group*> groups;
    lua_variable lua_groups;
    set<uint64_t> events;
    map<mbox_index, lua_variable> scripts;
    lua_variable func_character;
    lua_variable func_account;
    lua_variable func_profile;
    map<mbox_index, mbox_macro> macros;
    map<mbox_index, map<mbox_index, mbox_macro>> var_macros;
};
struct mbox_send
{
    enum { press = 1, release = 2 };
    mbox_index character;
    uint32_t action = 0;
    key key = key::none;
    key_mod mods = key_mod::none;
};
class mbox_thread : public thread
{
public:
    mbox_system_data* system = nullptr;
    error_type run() noexcept override;
    void cancel() noexcept override;
};
class mbox_system_data final : public mbox_system, public mbox_array
{
public:
    mbox_system_data(const mbox_system_init& init) noexcept : m_init(init)
    {

    }
    ~mbox_system_data() noexcept
    {
        if (m_thread)
            m_thread->wait();
        filter(0);
    }
    error_type initialize(const mbox_array& data) noexcept;
    error_type exec() noexcept override;
    error_type cancel() noexcept 
    {
        return event_system::post(nullptr, event_type::global_quit);
    }
private:
private:
    const icy::thread& thread() const noexcept override
    {
        return *m_thread;
    }
    error_type signal(const event_data& event) noexcept override
    {
        m_cvar.wake();
        return error_type();
    }
    const mbox_system_info& info() const noexcept
    {
        return m_info;
    }
    error_type initialize(mbox_character& chr) noexcept;
    error_type post(const mbox_index& character, const input_message& input) noexcept override;
    error_type prefix(const mbox_object& object, const string_view cmd) LUA_NOEXCEPT;
    error_type on_input(mbox_character& chr, const input_message& input) noexcept;
    error_type execute() LUA_NOEXCEPT;
    error_type compile(const mbox_object& object, lua_variable& function) LUA_NOEXCEPT;
    error_type parse_input(const string_view str, input_message& msg) LUA_NOEXCEPT;
    template<typename... arg_types>
    error_type print(const string_view format, arg_types&&... args) noexcept
    {
        if (m_init.pfunc)
        {
            string str(m_lua->realloc(), m_lua->user());
            ICY_ERROR(str.appendf(format, std::forward<arg_types>(args)...));
            ICY_ERROR(m_init.pfunc(m_init.pdata, str));
        }
        return error_type();
    }
private:
    using func_type = error_type(mbox_system_data::*)(const string_view msg, mbox_character& chr, const const_array_view<lua_variable> input, array<lua_variable>* output);
    error_type append(const mbox_reserved_name name, lua_variable& table, const func_type func) LUA_NOEXCEPT;
    error_type add_character(const const_array_view<lua_variable> input, array<lua_variable>* output) LUA_NOEXCEPT;
    error_type join_group(const string_view msg, mbox_character& chr, const const_array_view<lua_variable> input, array<lua_variable>* output) LUA_NOEXCEPT;
    error_type leave_group(const string_view msg, mbox_character& chr, const const_array_view<lua_variable> input, array<lua_variable>* output) LUA_NOEXCEPT;
    error_type send_key(const string_view msg, mbox_character& chr, const uint32_t action, const const_array_view<lua_variable> input, array<lua_variable>* output) LUA_NOEXCEPT;
    error_type send_key_down(const string_view msg, mbox_character& chr, const const_array_view<lua_variable> input, array<lua_variable>* output) LUA_NOEXCEPT
    {
        return send_key(msg, chr, mbox_send::press, input, output);
    }
    error_type send_key_up(const string_view msg, mbox_character& chr, const const_array_view<lua_variable> input, array<lua_variable>* output) LUA_NOEXCEPT
    {
        return send_key(msg, chr, mbox_send::release, input, output);
    }
    error_type send_key_press(const string_view msg, mbox_character& chr, const const_array_view<lua_variable> input, array<lua_variable>* output) LUA_NOEXCEPT
    {
        return send_key(msg, chr, mbox_send::press | mbox_send::release, input, output);
    }
    error_type send_macro(const string_view msg, mbox_character& chr, const const_array_view<lua_variable> input, array<lua_variable>* output) LUA_NOEXCEPT;
    error_type send_var_macro(const string_view msg, mbox_character& chr, const const_array_view<lua_variable> input, array<lua_variable>* output) LUA_NOEXCEPT;
    error_type run_script(const string_view msg, mbox_character& chr, const const_array_view<lua_variable> input, array<lua_variable>* output) LUA_NOEXCEPT;
    error_type post_event(const string_view msg, mbox_character& chr, const const_array_view<lua_variable> input, array<lua_variable>* output) LUA_NOEXCEPT;
    error_type add_event(const string_view msg, mbox_character& chr, const mbox_event_type type, const const_array_view<lua_variable> input, array<lua_variable>* output) LUA_NOEXCEPT;
    error_type add_key_down_event(const string_view msg, mbox_character& chr, const const_array_view<lua_variable> input, array<lua_variable>* output) LUA_NOEXCEPT
    {
        return add_event(msg, chr, mbox_event_type::on_key_press, input, output);
    }
    error_type add_key_up_event(const string_view msg, mbox_character& chr, const const_array_view<lua_variable> input, array<lua_variable>* output) LUA_NOEXCEPT
    {
        return add_event(msg, chr, mbox_event_type::on_key_release, input, output);
    }
    error_type add_user_event(const string_view msg, mbox_character& chr, const const_array_view<lua_variable> input, array<lua_variable>* output) LUA_NOEXCEPT
    {
        return add_event(msg, chr, mbox_event_type::user, input, output);
    }
private:
    const mbox_system_init m_init;
    mutex m_mutex;
    cvar m_cvar;
    shared_ptr<lua_system> m_lua;
    shared_ptr<mbox_thread> m_thread;
    mbox_system_info m_info;
    map<mbox_index, mbox_character> m_characters;
    map<mbox_index, mbox_group> m_groups;
    map<uint32_t, mbox_character*> m_slots;
private:
    uint64_t m_event = 0;
    map<uint64_t, mbox_event> m_events;
    array<mbox_send> m_send;
    uint32_t m_action = 0;
    array<mbox_character*> m_stack;
};
ICY_STATIC_NAMESPACE_END

error_type mbox_system_data::initialize(const mbox_array& data) noexcept
{    
    const auto& party_obj = data.find(m_init.party);
    if (party_obj.type != mbox_type::party || party_obj.value.empty())
        return make_stdlib_error(std::errc::invalid_argument);

    ICY_ERROR(m_mutex.initialize());
    ICY_ERROR(create_lua_system(m_lua));

    const auto init_funcs = [this](lua_variable& var)
    {
        ICY_ERROR(append(mbox_reserved_name::send_key_down, var, &mbox_system_data::send_key_down));
        ICY_ERROR(append(mbox_reserved_name::send_key_up, var, &mbox_system_data::send_key_up));
        ICY_ERROR(append(mbox_reserved_name::send_key_press, var, &mbox_system_data::send_key_press));
        ICY_ERROR(append(mbox_reserved_name::send_macro, var, &mbox_system_data::send_macro));
        ICY_ERROR(append(mbox_reserved_name::send_var_macro, var, &mbox_system_data::send_var_macro));
        ICY_ERROR(append(mbox_reserved_name::join_group, var, &mbox_system_data::join_group));
        ICY_ERROR(append(mbox_reserved_name::leave_group, var, &mbox_system_data::leave_group));
        ICY_ERROR(append(mbox_reserved_name::run_script, var, &mbox_system_data::run_script));
        ICY_ERROR(append(mbox_reserved_name::post_event, var, &mbox_system_data::post_event));
        ICY_ERROR(append(mbox_reserved_name::on_key_up, var, &mbox_system_data::add_key_up_event));
        ICY_ERROR(append(mbox_reserved_name::on_key_down, var, &mbox_system_data::add_key_down_event));
        ICY_ERROR(append(mbox_reserved_name::on_event, var, &mbox_system_data::add_user_event));
        return error_type();
    };

    lua_variable global;
    ICY_ERROR(m_lua->global(global));
    const auto print_func = [](void* ptr, const_array_view<lua_variable> input, array<lua_variable>* output)
    {
        const auto self = static_cast<mbox_system_data*>(ptr);
        if (self->m_init.pfunc)
        {
            const auto proc = [](void* ptr, string_view str)
            {
                const auto self = static_cast<mbox_system_data*>(ptr);
                return self->m_init.pfunc(self->m_init.pdata, str);
            };
            for (auto&& var : input)
                ICY_ERROR(self->m_lua->print(var, proc, self));
        }
        return error_type();
    };

    lua_variable print_var;
    ICY_ERROR(m_lua->make_function(print_var, print_func, this));
    ICY_ERROR(global.insert("Print"_s, print_var));

    for (auto&& pair : data.objects())
    {
        mbox_object copy_object;
        ICY_ERROR(copy(pair.value, copy_object));
        ICY_ERROR(m_objects.insert(pair.key, std::move(copy_object)));

        if (pair.value.type == mbox_type::group)
        {
            auto it = m_groups.end();
            ICY_ERROR(m_groups.insert(pair.key, mbox_group(), &it));
            it->value.index = pair.key;

            auto& group = it->value;
            ICY_ERROR(m_lua->make_table(group.lua));
            ICY_ERROR(m_lua->make_table(group.lua_characters));
            ICY_ERROR(group.lua.insert("Characters"_s, group.lua_characters));
            ICY_ERROR(init_funcs(group.lua));
        }
    }
    for (auto&& pair : m_groups)
        ICY_ERROR(pair.value.lua.insert("Index"_s, &m_objects.find(pair.key)));
    
    set<mbox_index> party_refs;
    for (auto&& ref : party_obj.refs)
    {
        if (!ref.value)
            continue;

        const auto& obj = find(ref.value);
        if (obj.type == mbox_type::character)
        {
            auto it = m_characters.end();
            ICY_ERROR(m_characters.insert(ref.value, mbox_character(), &it));
            ICY_ERROR(copy(ref.key, it->value.name));
            it->value.index = ref.value;
            ICY_ERROR(initialize(it->value));
            ICY_ERROR(init_funcs(it->value.lua));
        }
        else
        {
            ICY_ERROR(party_refs.insert(ref.value));
        }
    }

    lua_variable party_func;
    ICY_ERROR(compile(party_obj, party_func));
    ICY_ERROR(party_func());

    set<const mbox_object*> party_tree;
    for (auto&& ref : party_refs)
    {
        set<const mbox_object*> tree;
        ICY_ERROR(enum_dependencies(find(ref), tree));
        for (auto&& ptr : tree)
            ICY_ERROR(party_tree.try_insert(ptr));
    }

    map<mbox_character*, set<const mbox_object*>> characters_trees;
    for (auto&& pair : m_slots)
    {
        set<const mbox_object*> tree;
        ICY_ERROR(enum_dependencies(find(pair.value->index), tree));
        for (auto&& ptr : party_tree)
            ICY_ERROR(tree.try_insert(ptr));
        ICY_ERROR(characters_trees.insert(pair.value, std::move(tree)));
    }

    auto max_macros = 0_z;
    for (auto&& pair : characters_trees)
    {
        for (auto&& ptr : pair.value)
        {
            if (ptr->value.empty())
                continue;

            if (ptr->type == mbox_type::action_var_macro)
            {
                map<mbox_index, mbox_macro> map;
                for (auto&& chr : characters_trees)
                    ICY_ERROR(map.insert(chr->key->index, mbox_macro()));
                ICY_ERROR(pair.key->var_macros.insert(ptr->index, std::move(map)));
            }
            else if (ptr->type == mbox_type::action_macro)
            {
                ICY_ERROR(pair.key->macros.insert(ptr->index, mbox_macro()));
            }
        }
        max_macros = std::max(max_macros, pair.key->macros.size() + pair.key->var_macros.size() * characters_trees.size());
    }
    if (max_macros >= data.macros().size())
        return make_mbox_error(mbox_error::not_enough_macros);

    for (auto&& pair : characters_trees)
    {
        ICY_ERROR(m_info.characters.push_back(mbox_system_info::character_data()));
        auto& info_chr = m_info.characters.back();
        info_chr.index = pair->key->index;
        for (auto&& slot : m_slots)
        {
            if (slot.value->index == info_chr.index)
            {
                info_chr.slot = slot.key;
                break;
            }
        }
        for (auto&& ref_pair : party_obj.refs)
        {
            if (ref_pair.value == pair->key->index)
            {
                ICY_ERROR(copy(ref_pair.key, info_chr.name));
                break;
            }
        }

        auto index = 0u;
        for (auto&& var_macro_map : pair.key->var_macros)
        {
            for (auto&& var_macro_pair : var_macro_map.value)
            {
                const auto macro_key = data.macros()[index++];
                var_macro_pair.value = macro_key;
                string macro_str;
                ICY_ERROR(copy(string_view(find(var_macro_map.key).value.data(), find(var_macro_map.key).value.size()), macro_str));
                ICY_ERROR(macro_str.replace("mbox"_s, find(var_macro_pair.key).name));
                ICY_ERROR(info_chr.macros.push_back(std::move(macro_str)));
            }
        }
        for (auto&& macro_pair : pair.key->macros)
        {
            const auto macro_key = data.macros()[index++];
            macro_pair.value = macro_key;
            string macro_str;
            ICY_ERROR(copy(string_view(find(macro_pair.key).value.data(), find(macro_pair.key).value.size()), macro_str));
            ICY_ERROR(info_chr.macros.push_back(std::move(macro_str)));
        }
    }


    ICY_ERROR(make_shared(m_thread));
    m_thread->system = this;
    filter(event_type::global_quit);
    return error_type();
}
error_type mbox_system_data::exec() noexcept
{
    while (true)
    {
        while (auto event = pop())
        {
            if (event->type == event_type::global_quit)
                return error_type();

            if (event->type == event_type::system_internal)
            {
                const auto& event_data = event->data<internal_event>();
                const auto chr = m_characters.try_find(event_data.character);
                if (!chr)
                    continue;
                ICY_ERROR(on_input(*chr, event_data.input));
            }
        }
        ICY_ERROR(m_cvar.wait(m_mutex));
    }
    return error_type();
}
error_type mbox_system_data::initialize(mbox_character& chr) noexcept
{
    ICY_ERROR(m_stack.push_back(&chr));
    ICY_SCOPE_EXIT{ m_stack.pop_back(); };

    ICY_ERROR(m_lua->make_table(chr.lua));
    ICY_ERROR(m_lua->make_table(chr.lua_groups));
    ICY_ERROR(chr.lua.insert("Groups"_s, chr.lua_groups));
    
    auto parent_ptr = &find(chr.index);
    while (*parent_ptr)
    {
        if (parent_ptr->type == mbox_type::character)
        {
            ICY_ERROR(compile(*parent_ptr, chr.func_character));
        }
        else if (parent_ptr->type == mbox_type::account)
        {
            ICY_ERROR(compile(*parent_ptr, chr.func_account));
        }
        else if (parent_ptr->type == mbox_type::profile)
        {
            ICY_ERROR(compile(*parent_ptr, chr.func_profile));            
        }
        parent_ptr = &find(parent_ptr->parent);
    }
    return error_type();
}
error_type mbox_system_data::compile(const mbox_object& object, lua_variable& var) LUA_NOEXCEPT
{
    if (var.type() != lua_type::none)
        return error_type();

    if (object.value.empty())
        return error_type();
        
    if (false
        || object.type == mbox_type::party
        || object.type == mbox_type::character 
        || object.type == mbox_type::account 
        || object.type == mbox_type::profile 
        || object.type == mbox_type::group 
        || object.type == mbox_type::action_script)
    {
        ;
    }
    else
    {
        return error_type();
    }

    string name(m_lua->realloc(), m_lua->user());
    ICY_ERROR(rpath(object.index, name));
    if (!m_stack.empty())
    {
        if (m_init.pfunc)
        {
            string msg(m_lua->realloc(), m_lua->user());
            ICY_ERROR(msg.append("\r\n"_s));
            for (auto k = 0u; k < m_stack.size(); ++k)
                ICY_ERROR(msg.append("  "_s));
            ICY_ERROR(msg.appendf("%1.Compile(\"%2\")... "_s, string_view(m_stack.back()->name), string_view(name)));
            ICY_ERROR(print("%1"_s, string_view(msg)));
        }
    }
    else
    {
        ICY_ERROR(print("\r\nCompile \"%1\"... "_s, string_view(name)));
    }
    if (const auto error = m_lua->parse(var, string_view(object.value.data(), object.value.size()), name.bytes().data()))
    {
        if (error != lua_error_execute)
            return error;

        if (m_init.pfunc)
        {
            string msg(m_lua->realloc(), m_lua->user());
            ICY_ERROR(lua_error_to_string(error, msg));
            ICY_ERROR(print("Error: %1"_s, string_view(msg)));
        }
        return make_mbox_error(mbox_error::execute_lua);
    }
    ICY_ERROR(print("Success!"_s));

    for (auto&& ref : object.refs)
    {
        const auto& ref_obj = find(ref.value);
        if (!ref_obj)
            continue;

        const auto append_ref = [&](const lua_variable& ref_var)
        {
            lua_variable var_str;
            ICY_ERROR(m_lua->make_string(var_str, ref.key));
            ICY_ERROR(var.insert(var_str, ref_var));
            return error_type();
        };

        if (ref_obj.type == mbox_type::group)
        {
            const auto group = m_groups.try_find(ref.value);
            if (group)
            {
                ICY_ASSERT(group->lua.type() != lua_type::none, "GROUP IS NOT INITIALIZED");
                ICY_ERROR(append_ref(group->lua));
            }
        }
        else if (ref_obj.type == mbox_type::character)
        {
            const auto chr = m_characters.try_find(ref.value);
            if (chr)
            {
                ICY_ASSERT(chr->lua.type() != lua_type::none, "CHARACTER IS NOT INITIALIZED");
                ICY_ERROR(append_ref(chr->lua));
            }
        }
        else 
        {
            ICY_ERROR(append_ref(&ref_obj));
        }
    }

    if (object.type == mbox_type::party)
    {
        const auto proc = [](void* ptr, const const_array_view<lua_variable> input, array<lua_variable>* output)
        {
            const auto self = static_cast<mbox_system_data*>(ptr);
            return self->add_character(input, output);
        };
        lua_variable var_func;
        lua_variable var_str;
        ICY_ERROR(m_lua->make_function(var_func, proc, this));
        ICY_ERROR(m_lua->make_string(var_str, to_string(mbox_reserved_name::add_character)));
        ICY_ERROR(var.insert(var_str, var_func));
    }
    else
    {
        if (!m_stack.empty())
        {
            lua_variable var_str;
            ICY_ERROR(m_lua->make_string(var_str, to_string(mbox_reserved_name::character)));
            ICY_ERROR(var.insert(var_str, m_stack.back()->lua));
        }
    }
    m_lua->print(var, [](void*, string_view str) { return win32_debug_print(str); }, nullptr);
    return error_type();
}
error_type mbox_system_data::post(const mbox_index& character, const input_message& input) noexcept
{
    mbox_system_info::character_data* ptr = nullptr;
    for (auto&& chr : m_info.characters)
    {
        if (chr.index == character)
        {
            ptr = &chr;
            break;
        }
    }
    if (!ptr)
        return make_stdlib_error(std::errc::invalid_argument);

    internal_event new_event;
    new_event.character = character;
    new_event.input = input;
    return event_system::post(this, event_type::system_internal, std::move(new_event));
}
error_type mbox_system_data::on_input(mbox_character& chr, const input_message& input) noexcept
{
    auto event_type = mbox_event_type::user;
    if (input.type == input_type::key_press)
        event_type = mbox_event_type::on_key_press;
    else if (input.type == input_type::key_release)
        event_type = mbox_event_type::on_key_release;
    else
        return error_type();

    if (m_init.pfunc)
    {
        string input_str;
        ICY_ERROR(to_string(input, input_str));
        ICY_ERROR(print("\r\nEvent: On%1(%2, %3)"_s, to_string(input.type), string_view(input_str), string_view(chr.name)));
    }
    
    auto offset = 0_z;
    while (true)
    {
        auto it = m_events.upper_bound(offset);
        if (it == m_events.end())
            break;

        for (; it != m_events.end(); ++it)
        {
            offset = it->key;
            if (false
                || it->value.type != event_type
                || it->value.key != input.key
                || !key_mod_and(it->value.mods, key_mod(input.key)))
                continue;

            ICY_ERROR(m_stack.push_back(it->value.character));
            ICY_SCOPE_EXIT{ m_stack.pop_back(); };
            ICY_ERROR(print("\r\n  %1.Callback[%2]"_s, string_view(it->value.character->name), it->key));
            lua_variable copy_callback;
            ICY_ERROR(m_lua->copy(it->value.lua, copy_callback));
            ICY_ERROR(copy_callback());
            break;
        }
    }

    using send_pair_type = std::pair<key_mod, array<input_message>>;
    map<mbox_index, send_pair_type> map;
    for (auto&& send : m_send)
    {
        auto it = map.find(send.character);
        if (it == map.end())
            ICY_ERROR(map.insert(send.character, send_pair_type(), &it));

        auto& mods = it->value.first;
        const auto press = [it, &mods](const key key) noexcept
        {
            ICY_ERROR(it->value.second.push_back(input_message(input_type::key_press, key, mods)));
            if (key == key::left_ctrl) mods = mods | key_mod::lctrl;
            if (key == key::left_alt) mods = mods | key_mod::lalt;
            if (key == key::left_shift) mods = mods | key_mod::lshift;
            if (key == key::right_ctrl) mods = mods | key_mod::rctrl;
            if (key == key::right_alt) mods = mods | key_mod::ralt;
            if (key == key::right_shift) mods = mods | key_mod::rshift;
            return error_type();
        };
        const auto release = [it, &mods](const key key) noexcept
        {
            ICY_ERROR(it->value.second.push_back(input_message(input_type::key_release, key, mods)));
            if (key == key::left_ctrl) mods = key_mod(uint32_t(mods) & ~uint32_t(key_mod::lctrl));
            if (key == key::left_alt) mods = key_mod(uint32_t(mods) & ~uint32_t(key_mod::lalt));
            if (key == key::left_shift) mods = key_mod(uint32_t(mods) & ~uint32_t(key_mod::lshift));
            if (key == key::right_ctrl) mods = key_mod(uint32_t(mods) & ~uint32_t(key_mod::rctrl));
            if (key == key::right_alt) mods = key_mod(uint32_t(mods) & ~uint32_t(key_mod::ralt));
            if (key == key::right_shift) mods = key_mod(uint32_t(mods) & ~uint32_t(key_mod::rshift));
            return error_type();
        };

        if (send.action & mbox_send::press)
        {
            if (send.mods & key_mod::lctrl) { ICY_ERROR(press(key::left_ctrl)); }
            if (send.mods & key_mod::lalt) { ICY_ERROR(press(key::left_alt)); }
            if (send.mods & key_mod::lshift) { ICY_ERROR(press(key::left_shift)); }
            if (send.mods & key_mod::rctrl) { ICY_ERROR(press(key::right_ctrl)); }
            if (send.mods & key_mod::ralt) { ICY_ERROR(press(key::right_alt)); }
            if (send.mods & key_mod::rshift) { ICY_ERROR(press(key::right_shift)); }
            ICY_ERROR(press(send.key));
        }
        if (send.action & mbox_send::release)
        {
            ICY_ERROR(release(send.key));
            if (send.mods & key_mod::rshift) { ICY_ERROR(release(key::right_shift)); }
            if (send.mods & key_mod::lalt) { ICY_ERROR(release(key::right_alt)); }
            if (send.mods & key_mod::rctrl) { ICY_ERROR(release(key::right_ctrl)); }
            if (send.mods & key_mod::lshift) { ICY_ERROR(release(key::left_shift)); }
            if (send.mods & key_mod::lalt) { ICY_ERROR(release(key::left_alt)); }
            if (send.mods & key_mod::lctrl) { ICY_ERROR(release(key::left_ctrl)); }
        }
    }

    for (auto&& pair : map)
    {
        auto& vec = pair.value.second;
        for (auto k = 0u; k < vec.size();)
        {
            if (vec[k].type != input_type::key_release)
            {
                ++k;
                continue;
            }

            auto skip = false;
            auto n = k + 1;
            for (; n < vec.size(); ++n)
            {
                if (vec[n].type == input_type::none)
                    continue;

                skip = vec[n].type == input_type::key_press && vec[n].key == vec[k].key && vec[n].mods == vec[k].mods;
                break;
            }
            if (skip)
            {
                vec[n] = input_message();
                vec[k] = input_message();
                if (k) --k;
            }
        }
        mbox_event_send_input event;
        event.character = pair.key;
        event.messages = std::move(vec);
        ICY_ERROR(event::post(this, mbox_event_type_send_input, std::move(event)));
    }

    return error_type();
}
error_type mbox_system_data::append(const mbox_reserved_name name, lua_variable& table, const func_type func) LUA_NOEXCEPT
{
    const auto str = icy::to_string(name);
    if (str.empty())
        return make_stdlib_error(std::errc::invalid_argument);

    const auto proc = [](array_view<lua_variable> user, const_array_view<lua_variable> input, array<lua_variable>* output)
    {
        const auto self = static_cast<mbox_system_data*>(user[0].as_pointer());
        const auto func_ptr = reinterpret_cast<const func_type*>(user[1].as_binary().data());
        const auto name = to_string(mbox_reserved_name(uint32_t(user[2].as_number())));

        const func_type func = *func_ptr;
        if (input.size() < 1)
        {
            string msg(self->m_lua->realloc(), self->m_lua->user());
            ICY_ERROR(msg.appendf("%1: expected a valid target as input[0]"_s, name));
            return self->m_lua->post_error(msg);
        }
        auto target_type = 0;
        array<mbox_character*> characters;
        if (input[0].type() == lua_type::none)
        {
            target_type = 1;
            for (auto&& pair : self->m_slots)
                ICY_ERROR(characters.push_back(pair.value));
        }
        else if (input[0].type() == lua_type::number)
        {
            target_type = 2;
            const auto chr = self->m_slots.find(uint32_t(input[0].as_number()));
            if (chr != self->m_slots.end())
                ICY_ERROR(characters.push_back(chr->value));
        }
        else if (input[0].type() == lua_type::table)
        {
            for (auto&& pair : self->m_slots)
            {
                if (!(pair.value->lua == input[0]))
                    continue;

                target_type = 2;
                ICY_ERROR(characters.push_back(pair.value));
                break;
            }
            if (!target_type)
            {
                for (auto&& pair : self->m_groups)
                {
                    if (!(pair.value.lua == input[0]))
                        continue;

                    target_type = 3;
                    for (auto&& chr : pair.value.characters)
                        ICY_ERROR(characters.push_back(chr));
                    break;
                }
            }
        }
        if (!target_type)
        {
            string msg(self->m_lua->realloc(), self->m_lua->user());
            ICY_ERROR(msg.appendf("%1: unknown target. Expected 'nil', 'mbox.Group' or 'mbox.Character' as input[0]"_s, name));
            return self->m_lua->post_error(msg);
        }
        for (auto&& chr : characters)
        {
            ICY_ERROR(self->m_stack.push_back(chr));
            ICY_SCOPE_EXIT{ self->m_stack.pop_back(); };

            if (self->m_init.pfunc)
            {
                string msg(self->m_lua->realloc(), self->m_lua->user());
                ICY_ERROR(msg.append("\r\n"_s));
                for (auto k = 0u; k < self->m_stack.size(); ++k)
                    ICY_ERROR(msg.append("  "_s));
                ICY_ERROR(msg.appendf("%1.%2"_s, string_view(chr->name), name));
                ICY_ERROR(self->print("%1"_s, string_view(msg)));
                ICY_ERROR((self->*func)(msg, *chr, const_array_view<lua_variable>(input.data() + 1, input.size() - 1), output));
            }
            else
            {
                ICY_ERROR((self->*func)(""_s, *chr, const_array_view<lua_variable>(input.data() + 1, input.size() - 1), output));
            }
            
        }
        return error_type();
    };

    lua_variable vars[3];
    vars[0] = this;
    ICY_ERROR(m_lua->make_binary(vars[1], func));
    vars[2] = uint32_t(name);

    lua_variable var_func;
    lua_variable var_str;
    ICY_ERROR(m_lua->make_varfunction(var_func, proc, vars));
    ICY_ERROR(m_lua->make_string(var_str, str));
    ICY_ERROR(table.insert(var_str, var_func));
    return error_type();
}
error_type mbox_system_data::add_character(const const_array_view<lua_variable> input, array<lua_variable>* output) LUA_NOEXCEPT
{
    if (input.size() < 2)
        return m_lua->post_error("AddCharacter: not enough arguments"_s);

    if (input[1].type() != lua_type::number || input[1].as_number() <= 0)
        return m_lua->post_error("AddCharacter: expected number > 0 as input[2] (slot)"_s);

    const auto slot = uint32_t(input[1].as_number());
    if (m_slots.try_find(slot))
        return m_lua->post_error("AddCharacter: this slot is already used"_s);

    mbox_character* find = nullptr;
    for (auto&& chr : m_characters)
    {
        if (chr.value.lua == input[0])
        {
            find = &chr.value;
            break;
        }
    }
    if (!find)
        return m_lua->post_error("AddCharacter: unknown character"_s);

    ICY_ERROR(m_slots.insert(slot, find));
    find->slot = slot;
    ICY_ERROR(find->lua.insert("Index"_s, slot));

    ICY_ERROR(m_stack.push_back(find));
    ICY_SCOPE_EXIT{ m_stack.pop_back(); };

    if (find->func_profile.type() != lua_type::none)
        ICY_ERROR(find->func_profile());

    if (find->func_account.type() != lua_type::none)
        ICY_ERROR(find->func_account());

    if (find->func_character.type() != lua_type::none)
        ICY_ERROR(find->func_character());

    return error_type();
}
error_type mbox_system_data::join_group(const string_view msg, mbox_character& chr, const const_array_view<lua_variable> input, array<lua_variable>* output) LUA_NOEXCEPT
{    
    mbox_group* group = nullptr;
    if (input.size() > 0)
    {
        for (auto&& pair : m_groups)
        {
            if (pair.value.lua == input[0])
            {
                group = &pair.value;
                if (group->characters.try_find(&chr))
                    return error_type();
                break;
            }
        }
    }
    if (!group)
        return m_lua->post_error("%1%2"_s, msg, ": expected 'Group' as input[1]"_s);

    const auto& group_object = find(group->index);
    ICY_ERROR(print("(%1)"_s, string_view(group_object.name)));

    ICY_ERROR(group->characters.insert(&chr));
    ICY_ERROR(chr.groups.insert(group));
    ICY_ERROR(group->lua_characters.insert(chr.slot, chr.lua));
    ICY_ERROR(chr.lua_groups.insert(&group_object, chr.lua));

    if (group_object.value.empty())
        return error_type();

    auto it = chr.scripts.find(group_object.index);
    if (it == chr.scripts.end())
    {
        lua_variable script;
        ICY_ERROR(compile(group_object, script));
        ICY_ERROR(chr.scripts.insert(group_object.index, std::move(script), &it));
    }
    ICY_ERROR(it->value());
    return error_type();
}
error_type mbox_system_data::leave_group(const string_view msg, mbox_character& chr, const const_array_view<lua_variable> input, array<lua_variable>* output) LUA_NOEXCEPT
{
    mbox_group* group = nullptr;
    if (input.size() > 0)
    {
        for (auto&& pair : m_groups)
        {
            if (pair.value.lua == input[0])
            {
                group = &pair.value;
                if (group->characters.try_find(&chr) == nullptr)
                    return error_type();
                break;
            }
        }
    }
    if (!group)
        return m_lua->post_error("%1%2"_s, msg, ": expected 'Group' as input[1]"_s);

    const auto& group_object = find(group->index);
    ICY_ERROR(print("(%1)"_s, string_view(group_object.name)));
    group->characters.erase(&chr);
    chr.groups.erase(group);
    ICY_ERROR(group->lua_characters.insert(chr.slot, lua_variable()));
    ICY_ERROR(chr.lua_groups.insert(&group_object, lua_variable()));

    return error_type();
}
error_type mbox_system_data::send_key(const string_view msg, mbox_character& chr, const uint32_t action, const const_array_view<lua_variable> input, array<lua_variable>* output) LUA_NOEXCEPT
{
    input_message input_msg;
    if (input.size() > 0 && input[0].type() == lua_type::string)
        ICY_ERROR(parse_input(input[0].as_string(), input_msg));
    if (input_msg.key == key::none)
        return m_lua->post_error("%1%2"_s, msg, ": expected valid [mods+]key string as input[1]"_s);

    ICY_ERROR(print("(%1)"_s, string_view(input[0].as_string())));
    mbox_send send;
    send.action = action;
    send.character = chr.index;
    send.key = input_msg.key;
    send.mods = key_mod(input_msg.mods);
    ICY_ERROR(m_send.push_back(send));
    return error_type();
}
error_type mbox_system_data::send_macro(const string_view msg, mbox_character& chr, const const_array_view<lua_variable> input, array<lua_variable>* output) LUA_NOEXCEPT
{
    const mbox_object* object = nullptr;
    if (input.size() > 0 && input[0].type() == lua_type::pointer)
        object = static_cast<const mbox_object*>(input[0].as_pointer());

    if (!object || object->type != mbox_type::action_macro)
        return m_lua->post_error("%1%2"_s, msg, ": expected valid macro as input[1]"_s);

    const auto macro = chr.macros.try_find(object->index);
    if (!macro)
    {
        if (object->value.empty())
            return error_type();
        return m_lua->post_error("%1: macro \"%2\" does not have keybind"_s, msg, string_view(object->name));
    }
    ICY_ERROR(print("(%1)"_s, string_view(object->name)));

    mbox_send send;
    send.action = mbox_send::press | mbox_send::release;
    send.character = chr.index;
    send.key = macro->key;
    send.mods = macro->mods;
    ICY_ERROR(m_send.push_back(send));
    return error_type();
}
error_type mbox_system_data::send_var_macro(const string_view msg, mbox_character& chr, const const_array_view<lua_variable> input, array<lua_variable>* output) LUA_NOEXCEPT
{
    const mbox_object* object = nullptr;
    if (input.size() > 0 && input[0].type() == lua_type::pointer)
        object = static_cast<const mbox_object*>(input[0].as_pointer());

    if (!object || object->type != mbox_type::action_var_macro)
        return m_lua->post_error("%1%2"_s, msg, ": expected valid varmacro as input[1]"_s);

    mbox_character* target = nullptr;
    if (input.size() > 1)
    {
        if (input[1].type() == lua_type::number)
        {
            auto find = m_slots.try_find(uint32_t(input[1].as_number()));
            if (find)
                target = *find;
        }
        else if (input[1].type() == lua_type::table)
        {
            for (auto&& other : m_characters)
            {
                if (other.value.lua == input[1])
                {
                    target = &other.value;
                    break;
                }
            }
        }
    }
    if (!target)
        return m_lua->post_error("%1%2"_s, msg, ": expected valid target (character or index) as input[2]"_s);

    const auto var_macro = chr.var_macros.try_find(object->index);
    if (!var_macro)
    {
        if (object->value.empty())
            return error_type();
        return m_lua->post_error("%1: varmacro \"%2\" not found"_s, msg, string_view(object->name));
    }
    const auto var_macro_target = var_macro->try_find(target->index);
    if (!var_macro_target)
        return m_lua->post_error("%1: varmacro \"%2\" -> \"%3\" has no keybind"_s, msg, string_view(object->name), string_view(target->name));

    ICY_ERROR(print("(%1 @%2)"_s, string_view(object->name), string_view(target->name)));
    mbox_send send;
    send.action = mbox_send::press | mbox_send::release;
    send.character = chr.index;
    send.key = var_macro_target->key;
    send.mods = var_macro_target->mods;
    ICY_ERROR(m_send.push_back(send));
    return error_type();
}
error_type mbox_system_data::run_script(const string_view msg, mbox_character& chr, const const_array_view<lua_variable> input, array<lua_variable>* output) LUA_NOEXCEPT
{
    const mbox_object* object = nullptr;
    if (input.size() > 0 && input[0].type() == lua_type::pointer)
        object = static_cast<const mbox_object*>(input[0].as_pointer());
    
    if (!object || object->type != mbox_type::action_script)
        return m_lua->post_error("%1%2"_s, msg, ": expected 'Action Script' as input[1]"_s);

    if (object->value.empty())
        return error_type();

    auto it = chr.scripts.find(object->index);
    if (it == chr.scripts.end())
    {
        lua_variable script;
        ICY_ERROR(compile(*object, script));
        ICY_ERROR(chr.scripts.insert(object->index, std::move(script), &it));
    }
    ICY_ERROR(print("(%1)"_s, string_view(object->name)));
    ICY_ERROR(it->value());
    return error_type();
}
error_type mbox_system_data::post_event(const string_view msg, mbox_character& chr, const const_array_view<lua_variable> input, array<lua_variable>* output) LUA_NOEXCEPT
{
    const mbox_object* object = nullptr;
    if (input.size() > 0 && input[0].type() == lua_type::pointer)
        object = static_cast<const mbox_object*>(input[0].as_pointer());
    if (!object || object->type != mbox_type::event)
        return m_lua->post_error("%1%2"_s, msg, ": expected valid event as input[1]"_s);

    ICY_ERROR(print("(%1)"_s, string_view(object->name)));

    auto offset = 0_z;
    while (true)
    {
        auto it = chr.events.upper_bound(offset);
        if (it == chr.events.end())
            break;

        for (; it != chr.events.end(); ++it)
        {
            offset = *it;
            const auto event = m_events.try_find(offset);

            if (!event || event->type != mbox_event_type::user || event->ref != object->index)
                continue;

            ICY_ERROR(m_stack.push_back(event->character));
            ICY_SCOPE_EXIT{ m_stack.pop_back(); };
            if (m_init.pfunc)
            {
                string msg(m_lua->realloc(), m_lua->user());
                ICY_ERROR(msg.append("\r\n"_s));
                for (auto&& x : m_stack)
                    ICY_ERROR(msg.append("  "_s));
                ICY_ERROR(print("%1%2.Callback[%3]"_s, string_view(msg), string_view(event->character->name), event->uid));
            }

            lua_variable copy_callback;
            ICY_ERROR(m_lua->copy(event->lua, copy_callback));
            ICY_ERROR(copy_callback());
            break;
        }
    }
    return error_type();
}
error_type mbox_system_data::add_event(const string_view msg, mbox_character& chr, const mbox_event_type type, const const_array_view<lua_variable> input, array<lua_variable>* output) LUA_NOEXCEPT
{
    mbox_event new_event;
    new_event.character = &chr;
    new_event.uid = ++m_event;
    new_event.type = type;
    if (type == mbox_event_type::user)
    {
        const mbox_object* object = nullptr;
        if (input.size() > 0 && input[0].type() == lua_type::pointer)
            object = static_cast<const mbox_object*>(input[0].as_pointer());
        if (!object || object->type != mbox_type::event)
            return m_lua->post_error("%1%2"_s, msg, ": expected valid event as input[1]"_s);
        new_event.ref = object->index;
        ICY_ERROR(print("(%1)"_s, string_view(object->name)));
    }
    else if (type == mbox_event_type::on_key_press || type == mbox_event_type::on_key_release)
    {
        input_message input_msg;
        if (input.size() > 0 && input[0].type() == lua_type::string)
            ICY_ERROR(parse_input(input[0].as_string(), input_msg));
        if (input_msg.key == key::none)
            return m_lua->post_error("%1%2"_s, msg, ": expected valid [mods+]key string as input[1]"_s);
        
        new_event.key = input_msg.key;
        new_event.mods = key_mod(input_msg.mods);
        ICY_ERROR(print("(%1)"_s, input[0].as_string()));
    }
    else
    {
        return error_type();
    }
    ICY_ERROR(m_events.insert(new_event.uid, std::move(new_event)));
    ICY_ERROR(chr.events.insert(m_event));
    
    return error_type();
}
error_type mbox_system_data::parse_input(const string_view str, input_message& msg) LUA_NOEXCEPT
{
    array<string_view> words(m_lua->realloc(), m_lua->user());
    ICY_ERROR(split(str, words, '+'));
    if (words.empty())
        return m_lua->post_error("ParseInput: expected format is '[Mods+]Key'"_s);
    
    const auto remove_space = [](string_view& word_ref)
    {
        while (!word_ref.empty())
        {
            char32_t chr = 0;
            ICY_ERROR(word_ref.begin().to_char(chr));
            if (chr == ' ' || chr == '\t')
                word_ref = { word_ref.begin() + 1, word_ref.end() };
            else
                break;
        }
        while (!word_ref.empty())
        {
            char32_t chr = 0;
            ICY_ERROR((word_ref.end() - 1).to_char(chr));
            if (chr == ' ' || chr == '\t')
                word_ref = { word_ref.begin(), word_ref.end() - 1 };
            else
                break;
        }
        return error_type();
    };

    key_mod mod = key_mod::none;
    for (auto k = 1u; k < words.size(); ++k)
    {
        ICY_ERROR(remove_space(words[k - 1]));

        string lower_word(m_lua->realloc(), m_lua->user());
        ICY_ERROR(to_lower(words[k - 1], lower_word));

        if (lower_word == "left shift"_s || lower_word == "lshift"_s)
            mod = mod | key_mod::lshift;
        else if (lower_word == "right shift"_s || lower_word == "rshift"_s)
            mod = mod | key_mod::rshift;
        else if (lower_word == "shift"_s)
            mod = mod | key_mod::lshift | key_mod::rshift;
        else if (lower_word == "left control"_s || lower_word == "lctrl"_s)
            mod = mod | key_mod::lctrl;
        else if (lower_word == "right control"_s || lower_word == "rctrl"_s)
            mod = mod | key_mod::rctrl;
        else if (lower_word == "control"_s || lower_word == "ctrl"_s)
            mod = mod | key_mod::lctrl | key_mod::rctrl;
        else if (lower_word == "left alt"_s || lower_word == "lalt"_s)
            mod = mod | key_mod::lalt;
        else if (lower_word == "right alt"_s || lower_word == "ralt"_s)
            mod = mod | key_mod::ralt;
        else if (lower_word == "alt"_s)
            mod = mod | key_mod::lalt | key_mod::ralt;
        else
        {
            string msg_str(m_lua->realloc(), m_lua->user());
            ICY_ERROR(icy::to_string("ParseInput: unknown mod '%1'"_s, msg_str, words[k - 1]));
            return m_lua->post_error(msg_str);
        }
    }
    ICY_ERROR(remove_space(words.back()));
    string lower_word(m_lua->realloc(), m_lua->user());
    ICY_ERROR(to_lower(words.back(), lower_word));

    for (auto k = 1u; k < 256u; ++k)
    {
        string lower_key(m_lua->realloc(), m_lua->user());
        ICY_ERROR(to_lower(icy::to_string(key(k)), lower_key));
        if (lower_key == lower_word)
        {
            msg = input_message(input_type::none, key(k), mod);
            return error_type();
        }
    }
    string msg_str(m_lua->realloc(), m_lua->user());
    ICY_ERROR(icy::to_string("ParseInput: unknown key '%1'"_s, msg_str, words.back()));
    return m_lua->post_error(msg_str);
}

error_type icy::create_event_system(shared_ptr<mbox::mbox_system>& system, const mbox_array& data, const mbox_system_init& init) noexcept
{
    shared_ptr<mbox_system_data> new_ptr;
    ICY_ERROR(make_shared(new_ptr, init));
    if (const auto error = new_ptr->initialize(data))
    {
        if (error == lua_error_execute)
        {
            auto new_error = make_mbox_error(mbox_error::execute_lua);
            new_error.message = error.message;
            if (new_error.message)
                new_error.message->add_ref();
            return new_error;
        }
        return error;
    }
    system = std::move(new_ptr);
    return error_type();
}

error_type mbox_thread::run() noexcept
{
    if (auto error = system->exec())
        return event::post(system, event_type::system_error, std::move(error));
    return error_type();
}
void mbox_thread::cancel() noexcept
{
    system->cancel();
}

error_type mbox_system_info::update_wow_addon(const mbox_array& data, const string_view path) const noexcept
{
    string dir;
    ICY_ERROR(dir.appendf("%1%2"_s, path, "IcyMBox"_s));
    ICY_ERROR(icy::make_directory(dir));

    {

        string toc_str;
        ICY_ERROR(toc_str.append("## Interface: 11305"_s));
        ICY_ERROR(toc_str.append("\r\n## Title: IcyMBox Macros"_s));
        ICY_ERROR(toc_str.append("\r\n## Notes: Autogenerated macros for current party"_s));
        ICY_ERROR(toc_str.append("\r\n## Author: Icybull"_s));
        ICY_ERROR(toc_str.append("\r\n## Version: 1.0.0"_s));
        ICY_ERROR(toc_str.append("\r\n\r\nmain.lua"_s));

        string file_toc_name;
        ICY_ERROR(file_toc_name.appendf("%1/IcyMBox.toc"_s, string_view(dir)));
        file file_toc;
        ICY_ERROR(file_toc.open(file_toc_name, file_access::write, file_open::create_always, file_share::none));
        ICY_ERROR(file_toc.append(toc_str.bytes().data(), toc_str.bytes().size()));
    }


    auto max_macro = 0_z;
    for (auto&& chr : characters)
        max_macro = std::max(max_macro, chr.macros.size());
    
    if (max_macro > data.macros().size())
        return make_mbox_error(mbox_error::not_enough_macros);

    string lua_str;

    ICY_ERROR(lua_str.append("local onLoadTable = {}"_s));
    for (auto k = 0u; k < max_macro; ++k)
    {
        string input_str;
        ICY_ERROR(copy(to_string(data.macros()[k].key), input_str));
        ICY_ERROR(input_str.replace("Num "_s, "NUMPAD"_s));
        if (data.macros()[k].mods & key_mod::lalt)
        {
            ICY_ERROR(input_str.insert(input_str.begin(), "LALT+"_s));
        }
        else if (data.macros()[k].mods & key_mod::ralt)
        {
            ICY_ERROR(input_str.insert(input_str.begin(), "RALT+"_s));
        }

        if (data.macros()[k].mods & key_mod::lctrl)
        {
            ICY_ERROR(input_str.insert(input_str.begin(), "LCTRL+"_s));
        }
        else if (data.macros()[k].mods & key_mod::rctrl)
        {
            ICY_ERROR(input_str.insert(input_str.begin(), "RCTRL+"_s));
        }

        if (data.macros()[k].mods & key_mod::lshift)
        {
            ICY_ERROR(input_str.insert(input_str.begin(), "LSHIFT+"_s));
        }
        else if (data.macros()[k].mods & key_mod::rshift)
        {
            ICY_ERROR(input_str.insert(input_str.begin(), "RSHIFT+"_s));
        }


        ICY_ERROR(lua_str.appendf("\r\n\r\nlocal btn_%1 = CreateFrame(\"Button\", \"MBox_Button_%1\", UIParent, \"SecureActionButtonTemplate\")"_s, k + 1));
        ICY_ERROR(lua_str.appendf("\r\n  btn_%1:SetAttribute(\"type\", \"macro\")"_s, k + 1));
        ICY_ERROR(lua_str.appendf("\r\n  SetOverrideBindingClick(btn_%2, false, \"%1\", btn_%2:GetName())"_s, string_view(input_str), k + 1));
    }
    for (auto&& chr : characters)
    {
        ICY_ERROR(lua_str.appendf("\r\n\r\nlocal function OnLoad_%1()"_s, chr.slot));
        auto row = 0u;
        for (auto&& macro : chr.macros)
            ICY_ERROR(lua_str.appendf("\r\n  btn_%1:SetAttribute(\"macrotext\", \"%2\")"_s, ++row, string_view(macro)));        
        ICY_ERROR(lua_str.append("\r\nend"_s));
        ICY_ERROR(lua_str.appendf("\r\nonLoadTable[\"%1\"]=OnLoad_%2"_s, string_view(data.find(chr.index).name), chr.slot));
    }

    ICY_ERROR(lua_str.append("\r\n\r\nlocal name = UnitName(\"player\")"_s));
    ICY_ERROR(lua_str.append("\r\nlocal server = GetRealmName()"_s));
    ICY_ERROR(lua_str.append("\r\nlocal func = onLoadTable[name .. \"-\" .. server]"_s));
    ICY_ERROR(lua_str.append("\r\nif (func == nil) then func = onLoadTable[name] end"_s));
    ICY_ERROR(lua_str.append("\r\nif (func == nil) then"_s));
    ICY_ERROR(lua_str.append("\r\n  print(\"MBox has not found character by name '\" .. name .. \"' or by server - name '\" .. name .. \"-\" .. server .. \"'\")"_s));
    ICY_ERROR(lua_str.append("\r\nelse func() end"_s));


    string file_lua_name;
    ICY_ERROR(file_lua_name.appendf("%1/main.lua"_s, string_view(dir)));
    file file_lua;
    ICY_ERROR(file_lua.open(file_lua_name, file_access::write, file_open::create_always, file_share::none));
    ICY_ERROR(file_lua.append(lua_str.bytes().data(), lua_str.bytes().size()));

    return error_type();
}

const event_type mbox::mbox_event_type_send_input = icy::next_event_user();
#include "../mbox_script_system.hpp"
#include <icy_engine/core/icy_set.hpp>
#include <icy_engine/core/icy_file.hpp>
#include <icy_engine/core/icy_thread.hpp>

using namespace icy;
using namespace mbox;

ICY_STATIC_NAMESPACE_BEG
struct mbox_actor_type;
class mbox_system_data;
struct mbox_event
{
    mbox_actor_type* actor = nullptr;
    mbox_index source;  //  script
    uint64_t uid = 0;
    key key = key::none;
    key_mod mods = key_mod::none;
    mbox_index ref;
    lua_variable callback;
};
struct mbox_actor_type
{
    mbox_actor_type() noexcept = default;
    mbox_actor_type(const mbox_type type, const mbox_index index) noexcept : type(type), index(index)
    {

    }
    mbox_type type = mbox_type::none;
    mbox_index index;
    string name;
    set<mbox_actor_type*> refs;
    map<mbox_index, mbox_function> scripts;
    map<mbox_index, mbox_macro> macros;
    map<mbox_index, map<mbox_index, mbox_macro>> var_macros;
};
struct mbox_stack_element
{
    mbox_stack_element(mbox_actor_type& source, const const_array_view<mbox_actor_type*> characters, mbox_function& function) noexcept : 
        source(source), characters(characters), function(function)
    {

    }
    mbox_actor_type& source;
    const_array_view<mbox_actor_type*> characters;
    mbox_function& function;
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
};
class mbox_system_data : public mbox_system, public mbox_array
{
public:
    mbox_system_data(const mbox_print_func pfunc, void* const pdata) noexcept :
        m_pfunc(pfunc), m_pdata(pdata)
    {

    }
    ~mbox_system_data() noexcept
    {
        if (m_thread)
            m_thread->wait();
        filter(0);
    }
    error_type initialize(const mbox_array& data, const mbox_index& party) noexcept;
    error_type exec() noexcept override;
private:
    struct internal_event
    {
        mbox_index index;
        input_message input;
    };
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
    error_type post(const mbox_index& character, const input_message& input) noexcept override;
    error_type prefix(const string_view cmd) noexcept;
    error_type compile() noexcept;
    error_type prepare(array<mbox_action>& actions) noexcept;
    error_type execute(const const_array_view<mbox_action> actions) noexcept;
private:
    const mbox_print_func m_pfunc;
    void* const m_pdata;
    shared_ptr<lua_system> m_lua;
    shared_ptr<mbox_thread> m_thread;
    mutex m_mutex;
    cvar m_cvar;
    mbox_system_info m_info;
    map<mbox_index, mbox_actor_type> m_actors;
    array<mbox_stack_element> m_stack;
    map<uint32_t, mbox_index> m_slots;
    uint64_t m_event = 0;
    map<uint64_t, mbox_event> m_events;
    array<mbox_send> m_send;
};
ICY_STATIC_NAMESPACE_END

template<> int icy::compare<mbox_actor_type*>(mbox_actor_type* const& lhs, mbox_actor_type* const& rhs) noexcept
{
    return icy::compare(size_t(lhs), size_t(rhs));
}


error_type mbox_system_data::initialize(const mbox_array& data, const mbox_index& party) noexcept
{    
    const auto& party_obj = data.find(party);
    if (party_obj.type != mbox_type::party || party_obj.value.empty())
        return make_stdlib_error(std::errc::invalid_argument);

    ICY_ERROR(m_mutex.initialize());
    ICY_ERROR(create_lua_system(m_lua));

    for (auto&& pair : data.objects())
    {
        mbox_object copy_object;
        ICY_ERROR(copy(pair.value, copy_object));
        ICY_ERROR(m_objects.insert(pair.key, std::move(copy_object)));
    
        if (pair.value.type == mbox_type::group || pair.value.type == mbox_type::character)
            ICY_ERROR(m_actors.insert(pair.key, mbox_actor_type(pair.value.type, pair.value.index)));
    }

    auto party_actor = m_actors.end();
    ICY_ERROR(m_actors.insert(party, mbox_actor_type(mbox_type::party, party), &party_actor));

    for (auto&& pair : m_objects)
    {
        if (pair.value.value.empty())
            continue;

        if (pair.value.type == mbox_type::character || pair.value.type == mbox_type::account || pair.value.type == mbox_type::profile)
        {
            ICY_ERROR(party_actor->value.scripts.insert(pair.key, mbox_function(m_objects, pair.value, *m_lua)));
        }
        if (pair.value.type == mbox_type::group || pair.value.type == mbox_type::action_script)
        {
            for (auto&& actor : m_actors)
                ICY_ERROR(actor.value.scripts.insert(pair.key, mbox_function(m_objects, pair.value, *m_lua)));
        }
    }

    auto party_func = party_actor->value.scripts.end();
    ICY_ERROR(party_actor->value.scripts.insert(party, mbox_function(m_objects, party_obj, *m_lua), &party_func));

    array<mbox_action> actions;
    ICY_ERROR(m_stack.push_back(mbox_stack_element(party_actor->value, {}, party_func->value)));
    ICY_ERROR(compile());
    ICY_ERROR(prepare(actions));
    ICY_ERROR(execute(actions));

    set<mbox_index> refs;
    for (auto&& ref : party_obj.refs)
    {
        if (find(ref.value).type == mbox_type::character)
            ;
        else if (ref.value)
            ICY_ERROR(refs.insert(ref.value));
    }

    set<const mbox_object*> party_tree;
    for (auto&& ref : refs)
    {
        set<const mbox_object*> tree;
        ICY_ERROR(enum_dependencies(find(ref), tree));
        for (auto&& ptr : tree)
            ICY_ERROR(party_tree.try_insert(ptr));
    }
    map<mbox_actor_type*, set<const mbox_object*>> characters_trees;
    for (auto&& pair : m_slots)
    {
        set<const mbox_object*> tree;
        ICY_ERROR(enum_dependencies(find(pair.value), tree));
        for (auto&& ptr : party_tree)
            ICY_ERROR(tree.try_insert(ptr));
        ICY_ERROR(characters_trees.insert(m_actors.try_find(pair.value), std::move(tree)));
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
            if (slot.value == info_chr.index)
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
error_type mbox_system_data::prefix(const string_view cmd) noexcept
{
    if (m_stack.empty())
        return error_type(make_stdlib_error(std::errc::invalid_argument));

    if (m_pfunc)
    {
        const auto& back = m_stack.back();

        string_view script_name;
        string_view target_name;
        for (auto k = m_stack.size(); k; --k)
        {
            script_name = m_stack[k - 1].function.name(back.function.object().index);
            if (!script_name.empty())
                break;
        }
        for (auto k = m_stack.size(); k; --k)
        {
            target_name = m_stack[k - 1].function.name(m_stack.back().source.index);
            if (!target_name.empty())
                break;
        }
        string script_name_str;
        string target_name_str;
        if (script_name.empty())
        {
            ICY_ERROR(rpath(back.function.object().index, script_name_str));
            script_name = script_name_str;
        }

        string msg;
        ICY_ERROR(msg.append("\r\n"_s));
        for (auto&& x : m_stack)
            ICY_ERROR(msg.append(" "_s));

        ICY_ERROR(msg.appendf("%1 \"%2\""_s, cmd, script_name));
        if (!target_name.empty())
        {
            ICY_ERROR(msg.appendf(" for \"%1\"... "_s, target_name));
        }
        else
        {
            ICY_ERROR(msg.append(" ... "_s));
        }
        ICY_ERROR(m_pfunc(m_pdata, msg));
    }
    return error_type();
}
error_type mbox_system_data::compile() noexcept
{
    if (m_stack.empty())
        return error_type(make_stdlib_error(std::errc::invalid_argument));
    
    const auto& back = m_stack.back();
    if (back.function)
        return error_type();

    ICY_ERROR(prefix("Compile"_s));
    if (const auto error = back.function.initialize())
    {
        if (error != lua_error_execute)
            return error;

        if (m_pfunc)
        {
            string msg;
            ICY_ERROR(lua_error_to_string(error, msg));
            ICY_ERROR(msg.insert(msg.begin(), "Error: "_s));
            ICY_ERROR(m_pfunc(m_pdata, msg));
        }
        return make_mbox_error(mbox_error::execute_lua);
    }
    if (m_pfunc)
        ICY_ERROR(m_pfunc(m_pdata, "Success!"_s));

    return error_type();
}
error_type mbox_system_data::prepare(array<mbox_action>& actions) noexcept
{
    if (m_stack.empty())
        return error_type(make_stdlib_error(std::errc::invalid_argument));

    const auto& back = m_stack.back();
    if (!back.function)
        return error_type(make_stdlib_error(std::errc::invalid_argument));

    ICY_ERROR(prefix("Execute"_s));
    if (const auto error = back.function(actions))
    {
        if (error != lua_error_execute)
            return error;

        if (m_pfunc)
        {
            string msg;
            ICY_ERROR(lua_error_to_string(error, msg));
            ICY_ERROR(msg.insert(msg.begin(), "Error: "_s));
            ICY_ERROR(m_pfunc(m_pdata, msg));
        }
        return make_mbox_error(mbox_error::execute_lua);
    }
    if (m_pfunc)
        ICY_ERROR(m_pfunc(m_pdata, "Success!"_s));

    return error_type();
}
error_type mbox_system_data::execute(const const_array_view<mbox_action> actions) noexcept
{
    if (m_stack.empty())
        return error_type(make_stdlib_error(std::errc::invalid_argument));

    string tabs;
    if (m_pfunc)
    {
        ICY_ERROR(tabs.resize(m_stack.size(), ' '));
        ICY_ERROR(tabs.insert(tabs.begin(), "\r\n "_s));
    }

    for (auto&& action : actions)
    {
        if (action.type == mbox_action_type::add_character)
        {
            for (auto&& chr : m_actors)
            {
                for (auto&& slot : m_slots)
                {
                    if (slot.value == action.ref)
                    {
                        if (m_pfunc)
                        {
                            string msg;
                            ICY_ERROR(msg.append(tabs));
                            ICY_ERROR(msg.appendf("Error executing 'AddCharacter(\"%1\")' action: character is already in party in slot %2"_s,
                                m_stack.back().function.name(action.ref), slot.key));
                            ICY_ERROR(m_pfunc(m_pdata, msg));
                        }
                        return make_mbox_error(mbox_error::execute_lua);
                    }
                }
            }
            if (const auto ptr = m_slots.try_find(action.slot))
            {
                if (m_pfunc)
                {
                    string msg;
                    ICY_ERROR(msg.append(tabs));
                    ICY_ERROR(msg.appendf("Error executing 'AddCharacter (\"%1\")' action: character \"%2\" is already in slot %3"_s,
                        m_stack.back().function.name(*ptr), m_stack.back().function.name(action.ref), action.slot));
                    ICY_ERROR(m_pfunc(m_pdata, msg));
                }
                return make_mbox_error(mbox_error::execute_lua);
            }
            ICY_ERROR(m_slots.insert(action.slot, action.ref));

            mbox_index scripts[3];
            auto parent_ptr = &find(action.ref);
            while (*parent_ptr)
            {
                if (!parent_ptr->value.empty())
                {
                    if (parent_ptr->type == mbox_type::character)
                        scripts[2] = parent_ptr->index;
                    else if (parent_ptr->type == mbox_type::account)
                        scripts[1] = parent_ptr->index;
                    else if (parent_ptr->type == mbox_type::profile)
                        scripts[0] = parent_ptr->index;
                }
                parent_ptr = &find(parent_ptr->parent);
            }

            const auto chr = &m_actors.find(action.ref)->value;
            ICY_ERROR(copy(m_stack.back().function.name(chr->index), chr->name));

            set<const mbox_object*> tree;
            for (auto&& script : scripts)
            {
                if (!script)
                    continue;

                ICY_ERROR(m_stack.push_back(mbox_stack_element(m_stack.back().source, { chr }, m_stack.back().function)));
                ICY_SCOPE_EXIT{ m_stack.pop_back(); };
                mbox_action new_action;
                new_action.type = mbox_action_type::run_script;
                new_action.ref = script;
                //new_action.target.type = mbox_action_target::type_character;
                // new_action.target.index = chr->index;
                ICY_ERROR(execute({ &new_action, &new_action + 1 }));
            }
            continue;
        }

        mbox_actor_type* new_target = nullptr;
        if (action.target.type == mbox_action_target::type_group ||
            action.target.type == mbox_action_target::type_character)
        {
            new_target = m_actors.try_find(action.target.index);
        }
        else if (action.target.type == mbox_action_target::type_party)
        {
            if (const auto ptr = m_slots.try_find(action.target.slot))
                new_target = m_actors.try_find(*ptr);
        }
        else if (action.target.type == mbox_action_target::type_none)
        {
            new_target = &m_stack.back().source;
        }
        if (!new_target)
            return error_type(make_stdlib_error(std::errc::invalid_argument));

        array<mbox_actor_type*> characters;
        if (new_target->type == mbox_type::party)
        {
            ICY_ERROR(copy(m_stack.back().characters, characters));
            if (characters.empty())
            {
                for (auto&& slot : m_slots)
                    ICY_ERROR(characters.push_back(m_actors.try_find(slot.value)));
            }
        }
        else if (new_target->type == mbox_type::character)
        {
            ICY_ERROR(characters.push_back(new_target));
        }
        else if (new_target->type == mbox_type::group)
        {
            for (auto&& chr : new_target->refs)
            {
                if (action.target.mod == mbox_action_target::others)
                {
                    if (m_stack.back().source.refs.try_find(chr))
                        continue;
                }
                ICY_ERROR(characters.push_back(new_target));
            }
        }
        else
            return error_type(make_stdlib_error(std::errc::invalid_argument));

        for (auto&& chr : m_stack.back().characters)
        {
            const auto print = [&]
            {
                if (m_pfunc)
                {
                    string str;
                    ICY_ERROR(m_stack.back().function.to_string(chr->name, action, str));
                    ICY_ERROR(str.insert(str.begin(), tabs));
                    ICY_ERROR(m_pfunc(m_pdata, str));
                }
                return error_type();
            };

            switch (action.type)
            {
            case mbox_action_type::join_group:
            {
                const auto group_ptr = m_actors.try_find(action.ref);
                if (chr->refs.try_find(group_ptr) != nullptr)
                    continue;

                ICY_ERROR(chr->refs.insert(group_ptr));
                ICY_ERROR(group_ptr->refs.insert(chr));
                //   no break
            }
            case mbox_action_type::run_script:
            {
                ICY_ERROR(print());

                auto script = new_target->scripts.find(action.ref);
                if (script == new_target->scripts.end())
                    continue;//  make_stdlib_error(std::errc::invalid_argument);

                ICY_ERROR(m_stack.push_back(mbox_stack_element(*new_target, characters, script->value)));
                ICY_SCOPE_EXIT{ m_stack.pop_back(); };

                array<mbox_action> new_actions;
                ICY_ERROR(compile());
                ICY_ERROR(prepare(new_actions));
                ICY_ERROR(execute(new_actions));
                break;
            }
            case mbox_action_type::leave_group:
            {
                const auto group_ptr = m_actors.try_find(action.ref);
                if (chr->refs.try_find(group_ptr) == nullptr)
                    continue;

                ICY_ERROR(print());
                ICY_ERROR(chr->refs.erase(group_ptr));
                ICY_ERROR(group_ptr->refs.erase(chr));

                for (auto it = m_events.begin(); it != m_events.end(); )
                {
                    if (it->value.actor == chr && it->value.source == group_ptr->index)
                    {
                        /*auto jt = m_on_key_dw.find(it->key);
                        if (jt != m_on_key_dw.end()) m_on_key_dw.erase(jt);

                        auto jt = m_on_key_up.find(it->key);
                        if (jt != m_on_key_up.end()) m_on_key_up.erase(jt);

                        auto jt = m_on_user.find(it->key);
                        if (jt != m_on_user.end()) m_on_user.erase(jt);*/

                        it = m_events.erase(it);
                    }
                    else
                        ++it;
                }
                break;
            }
            case mbox_action_type::on_key_down:
            case mbox_action_type::on_key_up:
            case mbox_action_type::on_event:
            {
                ICY_ERROR(print());
                mbox_event event;
                event.uid = ++m_event;
                ICY_ERROR(m_lua->copy(action.callback, event.callback));
                event.key = action.input.key;
                event.mods = key_mod(action.input.mods);
                event.ref = action.ref;
                event.actor = chr;
                for (auto&& x : m_stack)
                {
                    if (x.function.object().type == mbox_type::group)
                        event.source = x.function.object().index;
                }
                ICY_ERROR(m_events.insert(event.uid, std::move(event)));
                break;
            }
            case mbox_action_type::post_event:
            {
                ICY_ERROR(print());
                array<lua_variable> callbacks;
                for (auto&& event : m_events)
                {
                    if (event.value.ref == action.ref)
                    {
                        lua_variable copy_callback;
                        ICY_ERROR(m_lua->copy(event.value.callback, copy_callback));
                        ICY_ERROR(callbacks.push_back(std::move(copy_callback)));
                    }
                }
                for (auto&& cb : callbacks)
                    ICY_ERROR(cb());
                break;
            }
            case mbox_action_type::send_key_down:
            case mbox_action_type::send_key_up:
            case mbox_action_type::send_key_press:
            {
                ICY_ERROR(print());

                mbox_send new_send;
                new_send.character = chr->index;
                if (action.type == mbox_action_type::send_key_down ||
                    action.type == mbox_action_type::send_key_press)
                    new_send.action |= mbox_send::press;
                if (action.type == mbox_action_type::send_key_up ||
                    action.type == mbox_action_type::send_key_press)
                    new_send.action |= mbox_send::release;

                new_send.key = action.input.key;
                new_send.mods = key_mod(action.input.mods);
                ICY_ERROR(m_send.push_back(new_send));
                break;
            }
            case mbox_action_type::send_macro:
            {
                const auto macro = chr->macros.find(action.ref);
                if (macro != chr->macros.end())
                {
                    mbox_send new_send;
                    new_send.character = chr->index;
                    new_send.action = mbox_send::press | mbox_send::release;
                    new_send.key = macro->value.key;
                    new_send.mods = key_mod(macro->value.mods);
                    ICY_ERROR(m_send.push_back(new_send));
                }
                break;
            }
            case mbox_action_type::send_var_macro:
            {
               /* const auto macro = chr->macros.find(action.ref);
                if (macro != chr->macros.end())
                {
                    mbox_send new_send;
                    new_send.character = chr->index;
                    new_send.action = mbox_send::press | mbox_send::release;
                    new_send.key = macro->value.key;
                    new_send.mods = key_mod(macro->value.mods);
                    ICY_ERROR(m_send.push_back(new_send));
                }*/
                break;
            }
            }
        }
    }
    return error_type();
}
error_type mbox_system_data::exec() noexcept
{
    while (true)
    {
        while (auto event = pop())
        {
            if (event->type == event_type::system_internal && shared_ptr<event_system>(event->source).get() == this)
            {
                const auto& event_data = event->data<internal_event>();
                if (event_data.index == mbox_index())
                    return error_type();

                //const auto source = m_data.find(event_data.index);
                //if (!source)
                 //   return make_unexpected_error();


            }
            else if (event->type == event_type::global_quit)
            {
                return error_type();
            }
        }
        ICY_ERROR(m_cvar.wait(m_mutex));
    }
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
    new_event.index = character;
    new_event.input = input;
    return event_system::post(this, event_type::system_internal, std::move(new_event));
}

error_type icy::create_event_system(shared_ptr<mbox::mbox_system>& system, const mbox_array& data, const mbox_index& party, 
    const mbox_print_func pfunc, void* const pdata) noexcept
{
    shared_ptr<mbox_system_data> new_ptr;
    ICY_ERROR(make_shared(new_ptr, pfunc, pdata));
    ICY_ERROR(new_ptr->initialize(data, party));
    system = std::move(new_ptr);
    return error_type();
}

error_type mbox_thread::run() noexcept
{
    return system->exec();
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
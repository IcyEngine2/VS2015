#include "../mbox_script_system.hpp"
#include <icy_engine/core/icy_set.hpp>

using namespace icy;
using namespace mbox;

struct mbox_event_type
{
    mbox_index index;
    input_message input;
    lua_variable callback;
};
struct mbox_character_type
{
    mbox_character_type(const mbox_index index, const uint32_t slot) noexcept : index(index), slot(slot)
    {

    }
    mbox_index index;
    uint32_t slot = 0;
    map<mbox_index, array<mbox_action>> groups;
    array<mbox_event_type> events;
    //array<std::pair<input_message, mbox_index>> key_events; //  if (input)  -> execute action
};

class mbox_system_data : public mbox_system
{    
    
public:
    mbox_system_data(const mbox_array& data, const mbox_index party, const mbox_print_func pfunc, void* const pdata) noexcept : 
        m_data(data), m_pfunc(pfunc), m_pdata(pdata), m_party(data.find(party))
    {

    }
    ~mbox_system_data() noexcept
    {
        filter(0);
    }
    error_type initialize() noexcept;
private:
    struct internal_event
    {
        mbox_index index;
        input_message input;
    };
private:
    error_type signal(const event_data& event) noexcept override
    {
        m_cvar.wake();
        return error_type();
    }
    error_type exec() noexcept override;
    const mbox_system_info& info() const noexcept
    {
        return m_info;
    }
    error_type cancel() noexcept override
    {
        return event_system::post(this, event_type::global_quit);
    }
    error_type post(const mbox_index& character, const input_message& input) noexcept override;
    error_type initialize(const mbox_object& object) noexcept;
    error_type execute(mbox_character_type& chr, mbox_function& script) noexcept;
    error_type execute(mbox_character_type& chr, const mbox_action& action) noexcept;
    error_type run_script(mbox_function& script, array<mbox_action>& actions) noexcept;
private:
    const mbox_array& m_data;
    const mbox_print_func m_pfunc;
    void* const m_pdata;
    const mbox_object* m_party;
    mbox_system_info m_info;
    mutex m_mutex;
    cvar m_cvar;
    shared_ptr<lua_system> m_lua;
    map<mbox_index, mbox_function> m_scripts;    
    array<mbox_character_type> m_characters;
    map<mbox_index, string> m_macros;
};

error_type mbox_system_data::initialize() noexcept
{
    ICY_ERROR(m_mutex.initialize());

    if (!m_party || m_party->type != mbox_type::party)
        return make_stdlib_error(std::errc::invalid_argument);

    ICY_ERROR(create_lua_system(m_lua));
    ICY_ERROR(initialize(*m_party));

    const auto find_party = m_scripts.find(m_party->index);
    if (find_party == m_scripts.end())
        return make_unexpected_error();

    array<mbox_action> actions;
    ICY_ERROR(run_script(find_party->value, actions));

    for (auto&& action : actions)
    {
        if (action.type == mbox_action_type::add_character)
        {
            for (auto&& chr : m_characters)
            {
                if (chr.index == action.ref)
                {
                    if (m_pfunc)
                    {
                        string name;
                        ICY_ERROR(m_data.rpath(action.ref, name));
                        string msg;
                        ICY_ERROR(to_string("\r\nError executing 'AddCharacter(\"%1\")' action: character is already in party"_s, msg, string_view(name)));
                        ICY_ERROR(m_pfunc(m_pdata, msg));
                    }
                    return make_mbox_error(mbox_error::execute_lua);
                }
            }
            for (auto&& chr : m_characters)
            {
                if (chr.slot == action.slot)
                {
                    if (m_pfunc)
                    {
                        string name1;
                        string name2;
                        ICY_ERROR(m_data.rpath(chr.index, name1));
                        ICY_ERROR(m_data.rpath(action.ref, name2));
                        string msg;
                        ICY_ERROR(to_string("\r\nError executing 'AddCharacter (\"%1\")' action: character \"%2\" is already in slot %3"_s, 
                            msg, string_view(name1), string_view(name2), action.slot));
                        ICY_ERROR(m_pfunc(m_pdata, msg));
                    }
                    return make_mbox_error(mbox_error::execute_lua);
                }
            }
            ICY_ERROR(m_characters.emplace_back(action.ref, action.slot));
            
            auto info_character = mbox_system_info::character_data();
            info_character.index = action.ref;
            info_character.position = action.slot;
            ICY_ERROR(m_info.characters.push_back(info_character));

            if (m_pfunc)
            {
                string msg;
                ICY_ERROR(m_data.to_string(mbox_index(), action, msg));
                ICY_ERROR(msg.insert(msg.begin(), "\r\n"_s));
                ICY_ERROR(m_pfunc(m_pdata, msg));
            }

            auto parent_ptr = m_data.find(action.ref);
            auto script_character = m_scripts.end();
            auto script_account = m_scripts.end();
            auto script_profile = m_scripts.end();
            while (parent_ptr)
            {
                if (parent_ptr->type == mbox_type::character)
                    script_character = m_scripts.find(parent_ptr->index);
                else if (parent_ptr->type == mbox_type::account)
                    script_account = m_scripts.find(parent_ptr->index);
                else if (parent_ptr->type == mbox_type::profile)
                    script_profile = m_scripts.find(parent_ptr->index);
                parent_ptr = m_data.find(parent_ptr->parent);
            }

            if (script_profile != m_scripts.end())
            {
                ICY_ERROR(execute(m_characters.back(), script_profile->value));
            }
            if (script_account != m_scripts.end())
            {
                ICY_ERROR(execute(m_characters.back(), script_account->value));
            }
            if (script_character != m_scripts.end())
            {
                ICY_ERROR(execute(m_characters.back(), script_character->value));
            }
        }
        else
        {
            for (auto&& chr : m_characters)
            {
                ICY_ERROR(execute(chr, action));
            }
        }
    }

    filter(event_type::global_quit | event_type::system_internal);
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

                const auto source = m_data.find(event_data.index);
                if (!source)
                    return make_unexpected_error();


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
error_type mbox_system_data::initialize(const mbox_object& object) noexcept
{
    if (false
        || object.type == mbox_type::action_script 
        || object.type == mbox_type::group
        || object.type == mbox_type::party
        || object.type == mbox_type::character
        || object.type == mbox_type::account
        || object.type == mbox_type::profile)
    {
        if (m_scripts.try_find(object.index))
            return error_type();

        if (!object.value.empty())
            ICY_ERROR(m_scripts.insert(object.index, mbox_function(m_data.objects(), object, *m_lua)));
        
        for (auto&& pair : object.refs)
        {
            const auto object_ptr = m_data.find(pair.value);
            if (!object_ptr)
                return make_mbox_error(mbox_error::database_corrupted);
            ICY_ERROR(initialize(*object_ptr));
        }
        if (object.type == mbox_type::character)
        {
            auto parent_ptr = m_data.find(object.parent);
            while (parent_ptr)
            {
                if (parent_ptr->type == mbox_type::account ||
                    parent_ptr->type == mbox_type::profile)
                    ICY_ERROR(initialize(*parent_ptr));
                parent_ptr = m_data.find(parent_ptr->parent);
            }
        }
    }
    else if (object.type == mbox_type::action_macro)
    {
        if (m_macros.try_find(object.index))
            return error_type();

        string str;
        ICY_ERROR(copy(object.value, str));
        ICY_ERROR(m_macros.insert(object.index, std::move(str)));
    }
    return error_type();
}
error_type mbox_system_data::run_script(mbox_function& script, array<mbox_action>& actions) noexcept
{
    if (!script)
    {
        if (m_pfunc)
        {
            string name;
            ICY_ERROR(m_data.rpath(script.object().index, name));
            string msg;
            ICY_ERROR(to_string("\r\nCompile \"%1\"... ", msg, string_view(name)));
            ICY_ERROR(m_pfunc(m_pdata, msg));
        }
        if (const auto error = script.initialize())
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
            ICY_ERROR(m_pfunc(m_pdata, "Success!"));
    }

    if (m_pfunc)
    {
        string name;
        ICY_ERROR(m_data.rpath(script.object().index, name));
        string msg;
        ICY_ERROR(to_string("\r\nExecute \"%1\"... ", msg, string_view(name)));
        ICY_ERROR(m_pfunc(m_pdata, msg));
    }
    if (const auto error = script(actions))
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
        ICY_ERROR(m_pfunc(m_pdata, "Success!"));

    return error_type();
}
error_type mbox_system_data::execute(mbox_character_type& chr, mbox_function& script) noexcept
{
    array<mbox_action> actions;
    ICY_ERROR(run_script(script, actions));
    for (auto&& action : actions)
        ICY_ERROR(execute(chr, action));
    return error_type();
}
error_type mbox_system_data::execute(mbox_character_type& character, const mbox_action& action) noexcept
{
    const auto print = [this, &action](const mbox_character_type& chr)
    {
        if (m_pfunc)
        {
            string str;
            ICY_ERROR(m_data.to_string(chr.index, action, str));
            ICY_ERROR(str.insert(str.begin(), "\r\n"_s));
            ICY_ERROR(m_pfunc(m_pdata, str));
        }
        return error_type();
    };
    array<mbox_character_type*> characters;
    if (action.target.type == mbox_action_target::type_none)
    {
        ICY_ERROR(characters.push_back(&character));
    }
    else if (action.target.type == mbox_action_target::type_character)
    {
        for (auto&& chr : m_characters)
        {
            if (chr.index == action.target.index)
            {
                ICY_ERROR(characters.push_back(&chr));
                break;
            }
        }
    }
    else if (action.target.type == mbox_action_target::type_group)
    {
        for (auto&& chr : m_characters)
        {
            if (!chr.groups.try_find(action.target.index))
                continue;

            if (action.target.mod == mbox_action_target::others && chr.index == character.index)
                continue;

            ICY_ERROR(characters.push_back(&chr));
        }
    }
    else if (action.target.type == mbox_action_target::type_party)
    {
        for (auto&& pair : m_characters)
        {
            if (pair.slot == action.slot)
            {
                ICY_ERROR(characters.push_back(&pair));
                break;
            }
        }
    }

    if (action.type == mbox_action_type::join_group)
    {
        for (auto&& chr : characters)
        {
            if (chr->groups.try_find(action.ref))
                continue;

            ICY_ERROR(chr->groups.insert(action.ref, array<mbox_action>()));
            ICY_ERROR(print(*chr));
        }
        const auto group_script = m_scripts.find(action.ref);
        if (group_script != m_scripts.end())
        {
            array<mbox_action> new_actions;
            ICY_ERROR(run_script(group_script->value, new_actions));
            for (auto&& chr : characters)
            {
                for (auto&& new_action : new_actions)
                    ICY_ERROR(execute(*chr, new_action));
            }
        }
    }
    else if (
        action.type == mbox_action_type::on_event ||
        action.type == mbox_action_type::on_key_down ||
        action.type == mbox_action_type::on_key_up)
    {
        for (auto&& chr : characters)
        {
            mbox_event_type new_event;
            new_event.index = action.ref;
            new_event.input = action.input;
            ICY_ERROR(m_lua->copy(action.callback, new_event.callback));
            ICY_ERROR(chr->events.push_back(std::move(new_event)));
            ICY_ERROR(print(*chr));
        }
    }
    return error_type();
}

error_type icy::create_event_system(shared_ptr<mbox::mbox_system>& system, const mbox_array& data, const mbox_index& party, 
    const mbox_print_func pfunc, void* const pdata) noexcept
{
    shared_ptr<mbox_system_data> new_ptr;
    ICY_ERROR(make_shared(new_ptr, data, party, pfunc, pdata));
    ICY_ERROR(new_ptr->initialize());
    system = std::move(new_ptr);
    return error_type();
}

const event_type mbox::mbox_event_type_send_input = icy::next_event_user();
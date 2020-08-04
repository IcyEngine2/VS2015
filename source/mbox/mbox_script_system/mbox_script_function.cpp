#include "../mbox_script_system.hpp"

using namespace icy;
using namespace mbox;

error_type mbox_function::initialize() LUA_NOEXCEPT
{
    {
        string name;
        ICY_ERROR(mbox_rpath(*m_objects, m_object->index, name));
        ICY_ERROR(m_lua->parse(m_func, m_object->value, name.bytes().data()));
    }

    lua_variable lua_mbox;
    ICY_ERROR(m_lua->make_table(lua_mbox));
    ICY_ERROR(m_func.insert("mbox"_s, lua_mbox));

    for (auto&& pair : m_object->refs)
    {
        if (pair.value != mbox_index())
        {
            lua_variable str;
            ICY_ERROR(m_lua->make_string(str, pair.key));
            ICY_ERROR(lua_mbox.insert(str, lua_variable::pointer_type(&pair.value)));
        }
    }

    const auto append = [&](lua_cfunction func, const mbox_reserved_name name)
    {
        const auto str = string_view(mbox_reserved_names[uint32_t(name)]);
        lua_variable var_func;
        lua_variable var_str;
        ICY_ERROR(m_lua->make_function(var_func, func, this));
        ICY_ERROR(m_lua->make_string(var_str, str));
        ICY_ERROR(lua_mbox.insert(var_str, var_func));
        return error_type();
    };

    if (m_object->type == mbox_type::party)
    {
        const auto add_character = [](void* func, const_array_view<lua_variable> input, array<lua_variable>* output) LUA_NOEXCEPT
        {
            const auto self = static_cast<mbox_function*>(func);
            if (input.size() < 2)// || input[1].type() != lua_type::number)
                return self->m_lua->post_error("AddCharacter: not enough arguments"_s);

            if (input[1].type() != lua_type::number || input[1].as_number() <= 0)
                return self->m_lua->post_error("AddCharacter: expected number > 0 as input[2] (slot)"_s);

            mbox_action new_action;
            new_action.type = mbox_action_type::add_character;
            ICY_ERROR(self->make_index(input[0], { mbox_type::character }, new_action.ref));
            new_action.slot = uint32_t(input[1].as_number());
            ICY_ERROR(self->m_actions.push_back(std::move(new_action)));
            return error_type();
        };
        ICY_ERROR(append(add_character, mbox_reserved_name::add_character));
    }
    if (m_object->type == mbox_type::action_script)
    {
        static const auto send_key_any = [](const mbox_action_type type, void* func, const const_array_view<lua_variable> input, array<lua_variable>* output) LUA_NOEXCEPT
        {
            const auto self = static_cast<mbox_function*>(func);
            if (input.size() < 1)
                return self->m_lua->post_error("SendInput: expected 1 or more arguments"_s);

            mbox_action new_action;
            new_action.type = type;
            ICY_ERROR(self->make_input(input[0], new_action.input));
            if (input.size() >= 2)
                ICY_ERROR(self->make_target(input[1], new_action.target));
            ICY_ERROR(self->m_actions.push_back(std::move(new_action)));
            return error_type();
        };
        static const auto send_macro_var = [](const mbox_action_type type, void* func, const_array_view<lua_variable> input, array<lua_variable>* output) LUA_NOEXCEPT
        {
            const auto self = static_cast<mbox_function*>(func);
            if (input.size() < 1)
                return self->m_lua->post_error("SendVarMacro: expected 1 or more arguments"_s);

            mbox_action new_action;
            new_action.type = type;
            ICY_ERROR(self->make_target(input[0], new_action.var_macro));
            if (input.size() >= 2)
                ICY_ERROR(self->make_target(input[1], new_action.target));
            ICY_ERROR(self->m_actions.push_back(std::move(new_action)));
            return error_type();
        };
        const auto send_key_down = [](void* func, const const_array_view<lua_variable> input, array<lua_variable>* output) LUA_NOEXCEPT
        {
            return send_key_any(mbox_action_type::send_key_down, func, input, output);
        };
        const auto send_key_up = [](void* func, const const_array_view<lua_variable> input, array<lua_variable>* output) LUA_NOEXCEPT
        {
            return send_key_any(mbox_action_type::send_key_up, func, input, output);
        };
        const auto send_key_press = [](void* func, const const_array_view<lua_variable> input, array<lua_variable>* output) LUA_NOEXCEPT
        {
            return send_key_any(mbox_action_type::send_key_press, func, input, output);
        };
        const auto send_macro = [](void* func, const const_array_view<lua_variable> input, array<lua_variable>* output) LUA_NOEXCEPT
        {
            const auto self = static_cast<mbox_function*>(func);
            if (input.size() < 1)
                return self->m_lua->post_error("SendMacro: expected 1 or more arguments"_s);

            mbox_action new_action;
            new_action.type = mbox_action_type::send_macro;
            ICY_ERROR(self->make_index(input[0], { mbox_type::action_macro }, new_action.ref));
            if (input.size() >= 2)
                ICY_ERROR(self->make_target(input[1], new_action.target));
            ICY_ERROR(self->m_actions.push_back(std::move(new_action)));
            return error_type();
        };
        const auto send_macro_follow = [](void* func, const const_array_view<lua_variable> input, array<lua_variable>* output) LUA_NOEXCEPT
        {
            return send_macro_var(mbox_action_type::send_macro_follow, func, input, output);
        };
        const auto send_macro_assist = [](void* func, const const_array_view<lua_variable> input, array<lua_variable>* output) LUA_NOEXCEPT
        {
            return send_macro_var(mbox_action_type::send_macro_assist, func, input, output);
        };
        const auto send_macro_target = [](void* func, const const_array_view<lua_variable> input, array<lua_variable>* output) LUA_NOEXCEPT
        {
            return send_macro_var(mbox_action_type::send_macro_target, func, input, output);
        };
        const auto send_macro_focus = [](void* func, const const_array_view<lua_variable> input, array<lua_variable>* output) LUA_NOEXCEPT
        {
            return send_macro_var(mbox_action_type::send_macro_focus, func, input, output);
        };

        ICY_ERROR(append(send_key_down, mbox_reserved_name::send_key_down));
        ICY_ERROR(append(send_key_up, mbox_reserved_name::send_key_up));
        ICY_ERROR(append(send_key_press, mbox_reserved_name::send_key_press));
        ICY_ERROR(append(send_macro, mbox_reserved_name::send_macro));
        ICY_ERROR(append(send_macro_follow, mbox_reserved_name::follow));
        ICY_ERROR(append(send_macro_assist, mbox_reserved_name::assist));
        ICY_ERROR(append(send_macro_target, mbox_reserved_name::target));
        ICY_ERROR(append(send_macro_focus, mbox_reserved_name::focus));
    }
    static const auto join_or_leave_group = [](const mbox_action_type type, void* func, const const_array_view<lua_variable> input, array<lua_variable>* output) LUA_NOEXCEPT
    {
        const auto self = static_cast<mbox_function*>(func);
        if (input.size() < 1)
            return self->m_lua->post_error("JoinOrLeaveGroup: expected 1 or more arguments"_s);

        mbox_action new_action;
        new_action.type = type;
        ICY_ERROR(self->make_index(input[0], { mbox_type::group }, new_action.ref));
        if (input.size() >= 2)
            ICY_ERROR(self->make_target(input[1], new_action.target));
        ICY_ERROR(self->m_actions.push_back(std::move(new_action)));
        return error_type();
    };
    const auto join_group = [](void* func, const const_array_view<lua_variable> input, array<lua_variable>* output) LUA_NOEXCEPT
    {
        return join_or_leave_group(mbox_action_type::join_group, func, input, output);
    };
    const auto leave_group = [](void* func, const const_array_view<lua_variable> input, array<lua_variable>* output) LUA_NOEXCEPT
    {
        return join_or_leave_group(mbox_action_type::leave_group, func, input, output);
    };
    ICY_ERROR(append(join_group, mbox_reserved_name::join_group));
    ICY_ERROR(append(leave_group, mbox_reserved_name::leave_group));

    const auto run_script = [](void* func, const const_array_view<lua_variable> input, array<lua_variable>* output) LUA_NOEXCEPT
    {
        const auto self = static_cast<mbox_function*>(func);
        if (input.size() < 1)
            return self->m_lua->post_error("RunScript: expected 1 or more arguments"_s);

        mbox_action new_action;
        new_action.type = mbox_action_type::run_script;
        ICY_ERROR(self->make_index(input[0], { mbox_type::action_script }, new_action.ref));
        if (input.size() >= 2)
            ICY_ERROR(self->make_target(input[1], new_action.target));
        ICY_ERROR(self->m_actions.push_back(std::move(new_action)));
        return error_type();
    };
    ICY_ERROR(append(run_script, mbox_reserved_name::run_script));

    const auto post_event = [](void* func, const const_array_view<lua_variable> input, array<lua_variable>* output) LUA_NOEXCEPT
    {
        const auto self = static_cast<mbox_function*>(func);
        if (input.size() < 1)
            return self->m_lua->post_error("PostEvent: expected 1 or more arguments"_s);

        mbox_action new_action;
        new_action.type = mbox_action_type::post_event;
        ICY_ERROR(self->make_index(input[0], { mbox_type::event }, new_action.ref));
        if (input.size() >= 2)
            ICY_ERROR(self->make_target(input[1], new_action.target));
        ICY_ERROR(self->m_actions.push_back(std::move(new_action)));
        return error_type();
    };
    ICY_ERROR(append(run_script, mbox_reserved_name::run_script));

    static const auto on_key_down_or_up = [](const mbox_action_type type, void* func, const const_array_view<lua_variable> input, array<lua_variable>* output) LUA_NOEXCEPT
    {
        const auto self = static_cast<mbox_function*>(func);
        if (input.size() < 2)
            return self->m_lua->post_error("OnKeyUpOrDown: expected 2 or more arguments"_s);
        if (input[1].type() != lua_type::function)
            return self->m_lua->post_error("OnKeyUpOrDown: expected function as input[2]"_s);

        mbox_action new_action;
        new_action.type = type;
        ICY_ERROR(self->make_input(input[0], new_action.input));
        ICY_ERROR(self->m_lua->copy(input[1], new_action.callback));
        if (input.size() >= 3)
            ICY_ERROR(self->make_target(input[2], new_action.target));
        ICY_ERROR(self->m_actions.push_back(std::move(new_action)));
        return error_type();
    };
    const auto on_key_down = [](void* func, const const_array_view<lua_variable> input, array<lua_variable>* output) LUA_NOEXCEPT
    {
        return on_key_down_or_up(mbox_action_type::on_key_down, func, input, output);
    };
    const auto on_key_up = [](void* func, const const_array_view<lua_variable> input, array<lua_variable>* output) LUA_NOEXCEPT
    {
        return on_key_down_or_up(mbox_action_type::on_key_up, func, input, output);
    };
    ICY_ERROR(append(on_key_down, mbox_reserved_name::on_key_down));
    ICY_ERROR(append(on_key_up, mbox_reserved_name::on_key_up));

    static const auto on_event = [](void* func, const const_array_view<lua_variable> input, array<lua_variable>* output) LUA_NOEXCEPT
    {
        const auto self = static_cast<mbox_function*>(func);
        if (input.size() < 2)
            return self->m_lua->post_error("OnEvent: expected 2 or more arguments"_s);
        if (input[1].type() != lua_type::function)
            return self->m_lua->post_error("OnEvent: expected function as input[1]"_s);

        mbox_action new_action;
        new_action.type = mbox_action_type::on_event;
        ICY_ERROR(self->make_index(input[0], { mbox_type::event }, new_action.ref));
        ICY_ERROR(self->m_lua->copy(input[1], new_action.callback));
        if (input.size() >= 3)
            ICY_ERROR(self->make_target(input[2], new_action.target));
        ICY_ERROR(self->m_actions.push_back(std::move(new_action)));
        return error_type();
    };
    ICY_ERROR(append(on_event, mbox_reserved_name::on_event));

    const auto others = [](void* func, const const_array_view<lua_variable> input, array<lua_variable>* output) LUA_NOEXCEPT
    {
        const auto self = static_cast<mbox_function*>(func);
        if (input.size() < 1)
            return self->m_lua->post_error("Others: expected 1 or more arguments"_s);

        mbox_action_target target;
        ICY_ERROR(self->make_index(input[0], { mbox_type::group }, target.index));
        target.mod = mbox_action_target::others;
        lua_variable var;
        ICY_ERROR(self->m_lua->make_binary(var, target));
        ICY_ERROR(output->push_back(std::move(var)));
        return error_type();
    };
    ICY_ERROR(append(others, mbox_reserved_name::others));

    return error_type();
}
error_type mbox_function::operator()(icy::array<mbox_action>& actions) LUA_NOEXCEPT
{
    if (m_func.type() != lua_type::function)
        return make_stdlib_error(std::errc::invalid_argument);
    if (const auto error = m_func())
    {
        m_actions.clear();
        return error;
    }
    actions = std::move(m_actions);
    return error_type();
}
error_type mbox_function::make_target(const lua_variable& input, mbox_action_target& target) const LUA_NOEXCEPT
{
    if (input.type() == lua_type::number)
    {
        target.type = mbox_action_target::type_party;
        target.slot = uint32_t(input.as_number());
        if (target.slot == 0)
            return m_lua->post_error("Target: expected party slot number > 0"_s);
    }
    else if (input.type() == lua_type::pointer)
    {
        const auto ref = *reinterpret_cast<const mbox_index*>(input.as_pointer());
        const auto ref_object = m_objects->try_find(ref);
        if (!ref_object)
            return m_lua->post_error("Target: invalid reference"_s);

        if (ref_object->type == mbox_type::character)
        {
            target.type = mbox_action_target::type_character;
        }
        else if (ref_object->type == mbox_type::group)
        {
            target.type = mbox_action_target::type_group;
        }
        else
        {
            return m_lua->post_error("Target: expected reference to character or group"_s);
        }

        target.index = ref_object->index;
    }
    else if (input.type() == lua_type::binary)
    {
        if (input.as_binary().size() != sizeof(target))
            return m_lua->post_error("Target: invalid input"_s);

        const auto target_ptr = reinterpret_cast<const mbox_action_target*>(input.as_binary().data());
        target = *target_ptr;
    }
    else
    {
        return m_lua->post_error("Target: invalid input"_s);
    }
    return error_type();
};
error_type mbox_function::make_index(const lua_variable& input, const_array_view<mbox_type> types, mbox_index& index) const LUA_NOEXCEPT
{
    if (input.type() != lua_type::pointer)
        return m_lua->post_error("FindReference: invalid input type"_s);

    const auto ref = *reinterpret_cast<const mbox_index*>(input.as_pointer());
    const auto ref_object = m_objects->try_find(ref);
    if (!ref_object)
        return m_lua->post_error("FindReference: invalid reference index"_s);

    for (auto&& type : types)
    {
        if (ref_object->type == type)
        {
            index = ref_object->index;
            return error_type();
        }
    }
    return m_lua->post_error("FindReference: invalid reference type"_s);
}
error_type mbox_function::make_input(const lua_variable& input, input_message& msg) const LUA_NOEXCEPT
{
    if (input.type() != lua_type::string)
        return m_lua->post_error("ParseInput: expected input as string '[Mods+]Key'"_s);

    array<string_view> words(m_lua->realloc(), m_lua->user());
    ICY_ERROR(split(input.as_string(), words, '+'));
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
            ICY_ERROR(to_string("ParseInput: unknown mod '%1'"_s, msg_str, words[k - 1]));
            return m_lua->post_error(msg_str);
        }
    }
    ICY_ERROR(remove_space(words.back()));
    string lower_word(m_lua->realloc(), m_lua->user());
    ICY_ERROR(to_lower(words.back(), lower_word));

    for (auto k = 1u; k < 256u; ++k)
    {
        string lower_key(m_lua->realloc(), m_lua->user());
        ICY_ERROR(to_lower(to_string(key(k)), lower_key));
        if (lower_key == lower_word)
        {
            msg = input_message(input_type::none, key(k), mod);
            return error_type();
        }
    }
    string msg_str(m_lua->realloc(), m_lua->user());
    ICY_ERROR(to_string("ParseInput: unknown key '%1'"_s, msg_str, words.back()));
    return m_lua->post_error(msg_str);
}

#include "icy_mbox_script2.hpp"
#include <icy_engine/core/icy_set.hpp>
using namespace icy;

string_view mbox::to_string(const mbox::type type) noexcept
{
    switch (type)
    {
    case type::directory: return "Directory"_s;
    case type::command: return "Command"_s;
    case type::group: return "Group"_s;
    case type::character: return "Character"_s;
    }
    return {};
}
string_view mbox::to_string(const mbox::action_type type) noexcept
{
    switch (type)
    {
    case action_type::group_join: return "Join Group"_s;
    case action_type::group_leave: return "Leave Group"_s;
    case action_type::button_press: return "Press Button"_s;
    case action_type::button_release: return "Release Button"_s;
    case action_type::button_click: return "Click Button"_s;
    case action_type::command_execute: return "Execute Command"_s;
    case action_type::command_replace: return "Replace Command"_s;
    case action_type::begin_broadcast: return "Begin Broadcast"_s;
    case action_type::begin_multicast: return "Begin Multicast"_s;
    case action_type::end_broadcast: return "End Broadcast"_s;
    case action_type::end_multicast: return "End Multicast"_s;
    case action_type::on_button_press: return "On Button Press"_s;
    case action_type::on_button_release: return "On Button Release"_s;
    }
    return {};
}
error_type mbox::to_string(const mbox::button button, string& str) noexcept
{
    ICY_ERROR(to_string(button.mod, str));
    switch (button.key)
    {
    case button_lmb(): return str.append("LMB"_s);
    case button_rmb(): return str.append("RMB"_s);
    case button_mid(): return str.append("Mid"_s);
    case button_x1(): return str.append("X1"_s);
    case button_x2(): return str.append("X2"_s);
    default: return str.append(to_string(button.key));
    }
}

static error_type mbox_error_to_string(const uint32_t code, const string_view, string& str) noexcept
{
    switch (mbox::mbox_error_code(code))
    {
    case mbox::mbox_error_code::invalid_version:
        return to_string("Invalid version"_s, str);
    case mbox::mbox_error_code::invalid_index:
        return to_string("Invalid index"_s, str);
    case mbox::mbox_error_code::invalid_parent:
        return to_string("Invalid parent"_s, str);
    case mbox::mbox_error_code::invalid_name:
        return to_string("Invalid name"_s, str);
    case mbox::mbox_error_code::invalid_group:
        return to_string("Invalid group"_s, str);
    case mbox::mbox_error_code::invalid_command:
        return to_string("Invalid command"_s, str);
    default:
        return make_stdlib_error(std::errc::invalid_argument);
    }
}

const error_source mbox::error_source_mbox = register_error_source("mbox"_s, mbox_error_to_string);

error_type mbox::transaction::execute(library& target, map<guid, mbox_error_code>* errors) noexcept
{
    const auto new_version = target.m_version + 1;
    if (target.m_version != m_library.m_version)
        return make_mbox_error(mbox::mbox_error_code::invalid_version);

    m_library.m_version = SIZE_MAX;
    for (auto&& oper : m_oper)
    {
        if (oper.type == operation_type::insert)
        {
            base tmp;
            ICY_ERROR(base::copy(oper.value, tmp));
            const auto error = m_library.m_data.insert(oper.value.index, std::move(tmp));
            if (error == make_stdlib_error(std::errc::invalid_argument))
                return make_mbox_error(mbox::mbox_error_code::invalid_index);
            ICY_ERROR(error);
        }
        else if (oper.type == operation_type::modify)
        {
            const auto it = m_library.m_data.find(oper.value.index);
            if (it == m_library.m_data.end())
                return make_mbox_error(mbox::mbox_error_code::invalid_index);
            base tmp;
            ICY_ERROR(base::copy(oper.value, tmp));
            it->value = std::move(tmp);
        }
        else
        {
            return make_stdlib_error(std::errc::invalid_argument);
        }
    }
    map<guid, mbox_error_code> tmp_errors;
    if (!errors) errors = &tmp_errors;

    ICY_ERROR(m_library.validate(*errors));
    if (!errors->empty())
        return make_mbox_error(errors->front().value);

    target = std::move(m_library);
    target.m_version = new_version;
    return {};
}
error_type mbox::library::validate(mbox::base& base, map<icy::guid, mbox::mbox_error_code>& errors) noexcept
{
    switch (base.type)
    {
    case mbox::type::character:
    case mbox::type::command:
    {
        for (auto&& action : base.actions)
        {

        }
        break;
    }
    case mbox::type::group:
        break;

    case mbox::type::directory:
    {
        icy::map<icy::string_view, array<guid>> names;
        for (auto&& pair : m_data)
        {
            if (pair.value.parent != base.index)
                continue;
            
            auto it = names.end();
            ICY_ERROR(names.find_or_insert(pair.value.name, array<guid>(), &it));
            ICY_ERROR(it->value.push_back(pair.key));
            ICY_ERROR(validate(pair.value, errors));
        }
        for (auto&& name : names)
        {
            if (name.value.size() > 1)
            {
                for (auto&& child : name.value)
                    ICY_ERROR(errors.insert(child, mbox_error_code::invalid_name));
                for (auto k = 1u; k < name.value.size(); ++k)
                {
                    string new_name;
                    ICY_ERROR(to_string("%1 (%2)", new_name, name.key, k));
                    m_data[name.value[k]]->value.name = std::move(new_name);
                }
            }
        }
        break;
    }
    default:
        break;
    }
    return {};
}
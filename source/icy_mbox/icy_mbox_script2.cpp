#include "icy_mbox_script2.hpp"
#include <icy_engine/core/icy_set.hpp>
#include <icy_engine/core/icy_file.hpp>
using namespace icy;

string_view mbox::to_string(const mbox::type type) noexcept
{
    switch (type)
    {
    case type::directory: return "Directory"_s;
    case type::command: return "Command"_s;
    case type::group: return "Group"_s;
    case type::character: return "Character"_s;
    case type::timer: return "Timer"_s;
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
    case action_type::timer_start: return "Timer Start"_s;
    case action_type::timer_stop: return "Timer Stop"_s;
    case action_type::timer_pause: return "Timer Pause"_s;
    case action_type::timer_resume: return "Timer Resume"_s;
    case action_type::command_execute: return "Execute Command"_s;
    case action_type::command_replace: return "Replace Command"_s;
    case action_type::on_button_press: return "On Button Press"_s;
    case action_type::on_button_release: return "On Button Release"_s;
    case action_type::on_timer: return "On Timer"_s;
    }
    return {};
}
string_view mbox::to_string(const key key) noexcept
{
    switch (key)
    {
    case button_lmb: return "Mouse Left"_s;
    case button_rmb: return "Mouse Right"_s;
    case button_mid: return "Mouse Mid"_s;
    case button_x1: return "Mouse X1"_s;
    case button_x2: return "Mouse X2"_s;
    }
    return icy::to_string(key);
}
error_type mbox::to_string(const mbox::button_type button, string& str) noexcept
{
    ICY_ERROR(to_string(button.mod, str));
    ICY_ERROR(str.append(mbox::to_string(button.key)));
    return {};
}
string_view mbox::to_string(const mbox::execute_type type) noexcept
{
    switch (type)
    {
    case mbox::execute_type::broadcast:
        return "Everyone in"_s;
    case mbox::execute_type::multicast:
        return "Other in"_s;
    case mbox::execute_type::self:
        return "Self"_s;
    }
    return {};
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
    case mbox::mbox_error_code::invalid_timer:
        return to_string("Invalid timer"_s, str);
    case mbox::mbox_error_code::invalid_timer_count:
        return to_string("Invalid timer"_s, str);
    case mbox::mbox_error_code::invalid_timer_offset:
        return to_string("Invalid timer offset"_s, str);
    case mbox::mbox_error_code::invalid_timer_period:
        return to_string("Invalid timer period"_s, str);
    case mbox::mbox_error_code::invalid_type:
        return to_string("Invalid type"_s, str);
    case mbox::mbox_error_code::invalid_action_type:
        return to_string("Invalid action type"_s, str);

    case mbox::mbox_error_code::invalid_json_parse:
        return to_string("Parse JSON"_s, str);
    case mbox::mbox_error_code::invalid_json_object:
        return to_string("Invalid JSON object"_s, str);
    case mbox::mbox_error_code::invalid_json_array:
        return to_string("Invalid JSON array"_s, str);

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

static const auto str_key_name = "Name"_s;
static const auto str_key_index = "Index"_s;
static const auto str_key_type = "Type"_s;
static const auto str_key_parent = "Parent"_s;
static const auto str_key_timer_offset = "Offset"_s;
static const auto str_key_timer_period = "Period"_s;
static const auto str_key_actions = "Actions"_s;
static const auto str_key_button_key = "Button Key"_s;
static const auto str_key_button_mod = "Button Mod"_s;
static const auto str_key_count = "Count"_s;
static const auto str_key_action_replace_source = "From"_s;
static const auto str_key_action_replace_target = "To"_s;
static const auto str_key_command = "Command"_s;
static const auto str_key_execute_type = "Execute Type"_s;
static const auto str_key_group = "Group"_s;
static const auto str_key_timer = "Timer"_s;

error_type mbox::library::load_from(const icy::string_view filename) noexcept
{
    file file_input;
    ICY_ERROR(file_input.open(filename, file_access::read, file_open::open_existing, file_share::read));
    array<char> str_input;
    auto size = file_input.info().size;
    ICY_ERROR(str_input.resize(size));
    ICY_ERROR(file_input.read(str_input.data(), size));
    ICY_ERROR(str_input.resize(size));
    json json_input;
    if (const auto error = json::create(string_view(str_input.data(), size), json_input))
    {
        if (error == make_stdlib_error(std::errc::illegal_byte_sequence))
            return make_mbox_error(mbox_error_code::invalid_json_parse);
        return error;
    }

    if (json_input.type() != json_type::array)
        return make_mbox_error(mbox_error_code::invalid_json_array);


    map<guid, mbox::base> data;
    for (auto&& json_object : json_input.vals())
    {
        if (json_object.type() != json_type::object)
            return make_mbox_error(mbox_error_code::invalid_json_object);

        mbox::base base;
        string str_index;
        string str_parent;
        if (const auto error = json_object.get(str_key_name, base.name))
        {
            if (error == make_stdlib_error(std::errc::invalid_argument))
                return make_mbox_error(mbox_error_code::invalid_name);
            return error;
        }

        if (const auto error = json_object.get(str_key_index, str_index))
        {
            if (error == make_stdlib_error(std::errc::invalid_argument))
                return make_mbox_error(mbox_error_code::invalid_index);
            return error;
        }
        if (str_index.to_value(base.index))
            return make_mbox_error(mbox_error_code::invalid_index);

        if (const auto error = json_object.get(str_key_parent, str_parent))
        {
            if (error == make_stdlib_error(std::errc::invalid_argument))
                return make_mbox_error(mbox_error_code::invalid_parent);
            return error;
        }
        if (str_parent.to_value(base.parent))
            return make_mbox_error(mbox_error_code::invalid_parent);
        
        for (; base.type < mbox::type::_total; base.type = type(uint32_t(base.type) + 1))
        {
            if (to_string(base.type) == json_object.get(str_key_type))
                break;
        }
        if (base.type == mbox::type::_total)
            return make_mbox_error(mbox_error_code::invalid_type);

        switch (base.type)
        {
        case mbox::type::timer:
        {
            json_object.get(str_key_timer_offset, base.timer.offset);
            json_object.get(str_key_timer_period, base.timer.period);
            break;
        }
        case mbox::type::command:
        case mbox::type::character:
        {
            if (const auto actions = json_object.find(str_key_actions))
            {
                if (actions->type() != json_type::array)
                    return make_mbox_error(mbox_error_code::invalid_json_array);

                for (const auto& json_action : actions->vals())
                {
                    if (json_action.type() != json_type::object)
                        return make_mbox_error(mbox_error_code::invalid_json_object);

                    auto type = action_type::none;
                    for (; type < action_type::_total; type = action_type(uint32_t(type) + 1))
                    {
                        if (json_action.get(str_key_type) == to_string(type))
                            break;
                    }                    
                    switch (type)
                    {
                    case mbox::action_type::group_join:
                    case mbox::action_type::group_leave:
                    {
                        guid group;
                        if (json_action.get(str_key_group).to_value(group))
                            return make_mbox_error(mbox_error_code::invalid_group);

                        base.actions.push_back(type == mbox::action_type::group_join ?
                            action::create_group_join(group) : action::create_group_leave(group));
                        break;
                    }
                  
                    case mbox::action_type::button_press:
                    case mbox::action_type::button_release:
                    case mbox::action_type::button_click:
                    case mbox::action_type::on_button_press:
                    case mbox::action_type::on_button_release:
                    {
                        button_type button;
                        const auto str_key = json_action.get(str_key_button_key);
                        button.key = key(1);
                        for (; button.key < key(256); button.key = key(uint32_t(button.key) + 1))
                        {
                            if (str_key == mbox::to_string(button.key))
                                break;
                        }
                        if (button.key == key(256))
                            return make_mbox_error(mbox_error_code::invalid_button_key);
                        
                        auto mod = 0u;
                        json_action.get(str_key_button_mod, mod);
                        if (mod && mod >= key_mod::_max)
                            return make_mbox_error(mbox_error_code::invalid_button_mod);
                        button.mod = key_mod(mod);
                        
                        if (type == mbox::action_type::on_button_press ||
                            type == mbox::action_type::on_button_release)
                        {
                            guid command;
                            if (json_action.get(str_key_command).to_value(command))
                                return make_mbox_error(mbox_error_code::invalid_command);
                            ICY_ERROR(base.actions.push_back(action::create_on_button_press(button, command)));
                        }
                        else if (type == mbox::action_type::button_click)
                        {
                            ICY_ERROR(base.actions.push_back(action::create_button_click(button)));
                        }
                        else if (type == mbox::action_type::button_press)
                        {
                            ICY_ERROR(base.actions.push_back(action::create_button_release(button)));
                        }
                        else if (type == mbox::action_type::button_release)
                        {
                            ICY_ERROR(base.actions.push_back(action::create_button_press(button)));
                        }
                        break;

                    }
                    case mbox::action_type::timer_start:
                    case mbox::action_type::timer_stop:
                    case mbox::action_type::timer_pause:
                    case mbox::action_type::timer_resume:
                    case mbox::action_type::on_timer:
                    {
                        guid timer;
                        if(json_action.get(str_key_timer).to_value(timer))
                            return make_mbox_error(mbox_error_code::invalid_timer);
                        if (type == mbox::action_type::timer_start)
                        {
                            auto count = 0u;
                            json_action.get(str_key_count, count);
                            ICY_ERROR(base.actions.push_back(mbox::action::create_timer_start(timer, count)));
                        }
                        else if (type == mbox::action_type::timer_start)
                        {
                            ICY_ERROR(base.actions.push_back(mbox::action::create_timer_stop(timer)));
                        }
                        else if (type == mbox::action_type::timer_pause)
                        {
                            ICY_ERROR(base.actions.push_back(mbox::action::create_timer_pause(timer)));
                        }
                        else if (type == mbox::action_type::timer_resume)
                        {
                            ICY_ERROR(base.actions.push_back(mbox::action::create_timer_resume(timer)));
                        }
                        else if (type == mbox::action_type::on_timer)
                        {
                            guid command;
                            if (json_action.get(str_key_command).to_value(command))
                                return make_mbox_error(mbox_error_code::invalid_command);
                            ICY_ERROR(base.actions.push_back(mbox::action::create_on_timer(timer, command)));
                        }
                        break;
                    }
                    case mbox::action_type::command_execute:
                    {
                        action::execute_type execute;
                        if (json_action.get(str_key_command).to_value(execute.command))
                            return make_mbox_error(mbox_error_code::invalid_command);

                        if (to_string(execute_type::broadcast) == json_action.get(str_key_execute_type))
                            execute.etype = execute_type::broadcast;
                        else if (to_string(execute_type::multicast) == json_action.get(str_key_execute_type))
                            execute.etype = execute_type::multicast;

                        if (execute.etype != execute_type::self)
                            json_action.get(str_key_group).to_value(execute.group);
                        
                        ICY_ERROR(base.actions.push_back(mbox::action::create_command_execute(execute)));
                        break;
                    }
                    case mbox::action_type::command_replace:
                    {
                        action::replace_type replace;
                        if (json_action.get(str_key_action_replace_source).to_value(replace.source))
                            return make_mbox_error(mbox_error_code::invalid_command);
                        json_action.get(str_key_action_replace_target).to_value(replace.target);
                        ICY_ERROR(base.actions.push_back(mbox::action::create_command_replace(replace)));
                        break;
                    }
                    default:
                        return make_mbox_error(mbox_error_code::invalid_action_type);
                    }
                }
            }
            break;
        }
        }
        if (base.index == guid())
            return make_mbox_error(mbox_error_code::invalid_index);

        if (const auto error = data.insert(base.index, std::move(base)))
        {
            if (error == make_stdlib_error(std::errc::invalid_argument))
                return make_mbox_error(mbox_error_code::invalid_index);
            return error;
        }
    }

    mbox::library library;
    ICY_ERROR(library.initialize());

    mbox::transaction txn;
    ICY_ERROR(txn.initialize(library));

    const auto root = data.find(mbox::root);
    if (root == data.end() || root->value.type != mbox::type::directory)
        return make_mbox_error(mbox_error_code::invalid_root);

    ICY_ERROR(txn.modify(root->value));
    root->value.index = {};

    while (true)
    {
        auto done = true;
        for (auto it = data.begin(); it != data.end(); ++it)
        {
            if (it->value.index == guid())
                continue;

            auto parent = data.find(it->value.parent);
            if (parent == data.end() || parent->value.index != guid())
                continue;
            
            ICY_ERROR(txn.insert(std::move(it->value)));
            it->value.index = guid();
            done = false;
        }
        if (done)
            break;
    }
    ICY_ERROR(txn.execute(library, nullptr));
    m_data = std::move(library.m_data);
    m_version += 1;
    return {};
}
error_type mbox::library::save_to(const icy::string_view filename) const noexcept
{
    json array = json_type::array;
    
    for (auto&& pair : m_data)
    {
        json object = json_type::object;
        string str_index;
        string str_parent;
        ICY_ERROR(to_string(pair.value.index, str_index));
        ICY_ERROR(to_string(pair.value.parent, str_parent));

        ICY_ERROR(object.insert(str_key_name, pair.value.name));
        ICY_ERROR(object.insert(str_key_type, to_string(pair.value.type)));
        ICY_ERROR(object.insert(str_key_index, str_index));
        ICY_ERROR(object.insert(str_key_parent, str_parent));

        switch (pair.value.type)
        {
        case type::character:
        case type::command:
        {
            json actions = json_type::array;
            for (auto&& action : pair.value.actions)
            {
                if (action.type() == action_type::none)
                    continue;

                json json_action = json_type::object;
                ICY_ERROR(json_action.insert(str_key_type, to_string(action.type())));
                
                switch (action.type())
                {
                case action_type::button_click:
                case action_type::button_press:
                case action_type::button_release:
                {
                    ICY_ERROR(json_action.insert(str_key_button_key, mbox::to_string(action.button().key)));
                    ICY_ERROR(json_action.insert(str_key_button_mod, uint32_t(action.button().mod)));
                    break;
                }
                case action_type::group_join:
                case action_type::group_leave:
                {
                    string str_group;
                    ICY_ERROR(to_string(action.group(), str_group));
                    ICY_ERROR(json_action.insert(str_key_group, str_group));
                    break;
                }
                case action_type::timer_start:
                case action_type::timer_stop:
                case action_type::timer_pause:
                case action_type::timer_resume:
                {
                    string str_timer;
                    ICY_ERROR(to_string(action.timer().timer, str_timer));
                    ICY_ERROR(json_action.insert(str_key_timer, str_timer));
                    if (action.type() == action_type::timer_start)
                        ICY_ERROR(json_action.insert(str_key_count, action.timer().count));
                    break;
                }

                case action_type::command_replace:
                {
                    string str_source;
                    string str_target;
                    ICY_ERROR(to_string(action.replace().source, str_source));
                    ICY_ERROR(to_string(action.replace().target, str_target));
                    ICY_ERROR(json_action.insert(str_key_action_replace_source, str_source));
                    ICY_ERROR(json_action.insert(str_key_action_replace_target, str_target));
                    break;
                }

                case action_type::command_execute:
                {
                    string str_command;
                    string str_group;
                    ICY_ERROR(to_string(action.execute().command, str_command));
                    ICY_ERROR(to_string(action.execute().group, str_group));
                    ICY_ERROR(json_action.insert(str_key_command, str_command));
                    if (action.execute().etype != mbox::execute_type::self)
                    {
                        ICY_ERROR(json_action.insert(str_key_group, str_group));
                        ICY_ERROR(json_action.insert(str_key_execute_type, to_string(action.execute().etype)));
                    }
                    break;
                }

                case action_type::on_button_press:
                case action_type::on_button_release:
                {
                    string str_command;
                    string str_button;
                    ICY_ERROR(to_string(action.event_command(), str_command));
                    ICY_ERROR(json_action.insert(str_key_command, str_command));
                    ICY_ERROR(json_action.insert(str_key_button_key, mbox::to_string(action.event_button().key)));
                    ICY_ERROR(json_action.insert(str_key_button_mod, uint32_t(action.event_button().mod)));
                    break;
                }
                case action_type::on_timer:
                {
                    string str_command;
                    string str_timer;
                    ICY_ERROR(to_string(action.event_command(), str_command));
                    ICY_ERROR(to_string(action.event_timer(), str_timer));
                    ICY_ERROR(json_action.insert(str_key_command, str_command));
                    ICY_ERROR(json_action.insert(str_key_timer, str_timer));
                    break;
                }
                }
                ICY_ERROR(actions.push_back(std::move(json_action)));
            }
            ICY_ERROR(object.insert(str_key_actions, std::move(actions)));
            break;
        }
        case type::timer:
        {
            ICY_ERROR(object.insert(str_key_timer_offset, pair.value.timer.offset));
            ICY_ERROR(object.insert(str_key_timer_period, pair.value.timer.period));
            break;
        }
        }
        ICY_ERROR(array.push_back(std::move(object)));
    }
    string str;
    ICY_ERROR(to_string(array, str));
    
    file file_output;
    string tname;
    ICY_ERROR(file::tmpname(tname));
    ICY_ERROR(file_output.open(tname, file_access::app, file_open::create_always, file_share::read | file_share::write));
    ICY_ERROR(file_output.append(str.bytes().data(), str.bytes().size()));
    file_output.close();
    ICY_ERROR(file::replace(tname, filename));
    return {};
}
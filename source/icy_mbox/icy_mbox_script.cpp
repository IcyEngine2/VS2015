#include "icy_mbox_script.hpp"
#include <icy_engine/core/icy_file.hpp>

using namespace icy;
using namespace mbox;

const guid mbox::root = guid(0x8C8C31B81A56400F, 0xB79ADBFE202E4FFC);
const guid mbox::group_broadcast = guid(0x12A85EC642DF451A, 0xABFDE3C2BE328ED3);
const guid mbox::group_multicast = guid(0xDA0B89EB493F4966, 0x8C538CA27AEA8AB3);
const guid mbox::group_default = guid();

static constexpr auto str_key_index = "Index"_s;
//static constexpr auto str_key_name = "Name"_s;
static constexpr auto str_key_type = "Type"_s;
static constexpr auto str_key_data = "Data"_s;
static constexpr auto str_key_value = "Value"_s;
static constexpr auto str_key_keyboard = "Keyboard"_s;
static constexpr auto str_key_kevent = "Keyboard Event"_s;
static constexpr auto str_key_mevent = "Mouse Event"_s;
static constexpr auto str_key_mouse = "Mouse"_s;
static constexpr auto str_key_macro = "Macro"_s;
static constexpr auto str_key_ctrl = "Ctrl"_s;
static constexpr auto str_key_shift = "Shift"_s;
static constexpr auto str_key_alt = "Alt"_s;
static constexpr auto str_key_group = "Group"_s;
static constexpr auto str_key_event = "Event"_s;
static constexpr auto str_key_command = "Command"_s;
static constexpr auto str_key_button = "Button"_s;
static constexpr auto str_key_offset = "Offset"_s;
static constexpr auto str_key_period = "Period"_s;
static constexpr auto str_key_reference = "Reference"_s;
static constexpr auto str_key_variable = "Variable"_s;
static constexpr auto str_key_profiles = "Profiles"_s;
static constexpr auto str_key_actions = "Actions"_s;

string_view mbox::to_string(const mbox::type type) noexcept
{
    switch (type)
    {
    case type::directory: return "Directory"_s;
    case type::input: return "Input"_s;
    case type::variable: return "Variable"_s;
    case type::timer: return "Timer"_s;
    case type::event: return "Event"_s;
    case type::command: return "Command"_s;
    case type::binding: return "Binding"_s;
    case type::group: return "Group"_s;
    case type::profile: return "Profile"_s;
    case type::list_inputs: return "List Inputs"_s;
    case type::list_variables: return "List Variables"_s;
    case type::list_timers: return "List Timers"_s;
    case type::list_events: return "List Events"_s;
    case type::list_commands: return "List Commands"_s;
    case type::list_bindings: return "List Bindings"_s;
    }
    return {};
}
string_view mbox::to_string(const mbox::input_type type) noexcept
{
    switch (type)
    {
    case mbox::input_type::keyboard: return str_key_keyboard;
    case mbox::input_type::mouse: return str_key_mouse;
    case mbox::input_type::macro: return str_key_macro;
    }
    return {};
}
string_view mbox::to_string(const mbox::variable_type type) noexcept
{
    switch (type)
    {
    case mbox::variable_type::boolean: return "Boolean"_s;
    case mbox::variable_type::integer: return "Integer"_s;
    case mbox::variable_type::counter: return "Counter"_s;
    case mbox::variable_type::string: return "String"_s;
    }
    return {};
}
string_view mbox::to_string(const mbox::event_type type) noexcept
{
    switch (type)
    {
    case event_type::variable_changed: "Var Changed"_s;
    case event_type::variable_equal: return "Var =="_s;
    case event_type::variable_not_equal: return "Var !="_s;
    case event_type::variable_greater: return "Var >"_s;
    case event_type::variable_lesser: return "Var <"_s;
    case event_type::variable_greater_or_equal: return "Var >="_s;
    case event_type::variable_lesser_or_equal: return "Var <="_s;
    case event_type::variable_str_contains: return "Var Substring"_s;
    case event_type::recv_input: return "Recv Input"_s;
    case event_type::button_press: return "Button Press"_s;
    case event_type::button_release: return "Button Release"_s;
    case event_type::timer_first: return "Timer First"_s;
    case event_type::timer_tick: return "Timer Tick"_s;
    case event_type::timer_last: return "Timer Last"_s;
    case event_type::focus_acquire: return "Focus Acquire"_s;
    case event_type::focus_release: return "Focus Release"_s;
    }
    return {};
}
string_view mbox::to_string(const mbox::action_type type) noexcept
{
    switch (type)
    {
    case action_type::variable_assign: return "Variable ="_s;
    case action_type::variable_inc: return "Variable ++"_s;
    case action_type::variable_dec: return "Variable --"_s;
    case action_type::execute_command: return "Execute Command"_s;
    case action_type::timer_start: return "Timer Start"_s;
    case action_type::timer_stop: return "Timer Stop"_s;
    case action_type::timer_pause: return "Timer Pause"_s;
    case action_type::timer_resume: return "Timer Resume"_s;
    case action_type::send_input: return "Send Input"_s;
    case action_type::join_group: return "Join Group"_s;
    case action_type::leave_group: return "Leave Group"_s;
    }
    return {};
}
static string_view to_string(const key_event event) noexcept
{
    switch (event)
    {
    case key_event::press: return "Press"_s;
    case key_event::hold: return "Hold"_s;
    case key_event::release: return "Release"_s;
    }
    return {};
}
static string_view to_string(const mouse_event event) noexcept
{
    switch (event)
    {
    case mouse_event::btn_press: return "Press"_s;
    case mouse_event::btn_hold: return "Hold"_s;
    case mouse_event::btn_double: return "Double"_s;
    case mouse_event::btn_release: return "Release"_s;
    case mouse_event::move: return "Move"_s;
    case mouse_event::wheel: return "Wheel"_s;
    }
    return {};
}
static string_view to_string(const mouse_button button) noexcept
{
    switch (button)
    {
    case mouse_button::left: return "Left"_s;
    case mouse_button::right: return "Right"_s;
    case mouse_button::mid: return "Middle"_s;
    case mouse_button::x1: return "Extra 1"_s;
    case mouse_button::x2: return "Extra 2"_s;
    }
    return {};
}
namespace mbox { using icy::to_string; }

template<typename T>
static error_type to_value(const string_view str, T& value) noexcept
{
    //using T = icy::remove_cvr<decltype(value)>;
    if (!str.empty())
    {
        for (; value < T::_total; value = T(uint32_t(value) + 1))
        {
            if (to_string(value) == str)
                break;
        }
        if (value == T::_total)
            return make_stdlib_error(std::errc::invalid_argument);
    }
    return {};
}

error_type mbox::library::initialize() noexcept
{
    m_data.clear();
    base base_root;
    base_root.type = type::directory;
    base_root.index = root;
    ICY_ERROR(to_string("Root"_s, base_root.name));
    ICY_ERROR(m_data.insert(guid(root), std::move(base_root)));
    return {};
}
error_type mbox::library::load_from(const string_view filename) noexcept
{
    file file_input;
    ICY_ERROR(file_input.open(filename, file_access::read, file_open::open_existing, file_share::read));
    array<char> str_input;
    auto size = file_input.info().size;
    ICY_ERROR(str_input.resize(size));
    ICY_ERROR(file_input.read(str_input.data(), size));
    ICY_ERROR(str_input.resize(size));
    json json_input;
    ICY_ERROR(json::create(string_view(str_input.data(), size), json_input));
    
    auto saved = std::move(m_data);
    auto success = false;
    ICY_SCOPE_EXIT{ if (!success) m_data = std::move(saved); };

    mbox::base root;
    if (json_input.type() != json_type::object || json_input.size() != 1)
        return make_stdlib_error(std::errc::invalid_argument);

    ICY_ERROR(to_string(json_input.keys().front(), root.name));
    if (root.name.empty())
        return make_stdlib_error(std::errc::invalid_argument);
    
    ICY_ERROR(from_json(json_input.vals().front(), root));
    ICY_ERROR(m_data.insert(guid(mbox::root), std::move(root)));
    
    success = true;

    return {};
}
error_type mbox::library::save_to(const string_view filename) noexcept
{
    const auto root_dir = find(root);
    if (!root_dir)
        return make_stdlib_error(std::errc::invalid_argument);

    json json_root = json_type::object;
    json json_output;
    ICY_ERROR(to_json(*root_dir, json_output));
    ICY_ERROR(json_root.insert(root_dir->name, std::move(json_output)));

    string str_output;
    ICY_ERROR(to_string(json_root, str_output));

    file file_output;
    string tname;
    ICY_ERROR(file::tmpname(tname));
    ICY_ERROR(file_output.open(tname, file_access::app, file_open::create_always, file_share::read | file_share::write));
    ICY_ERROR(file_output.append(str_output.bytes().data(), str_output.bytes().size()));
    file_output.close();
    ICY_ERROR(file::replace(tname, filename));
    return {};
}

icy::error_type mbox::library::children(const icy::guid& index, const bool recursive, icy::array<icy::guid>& vec) noexcept
{
    const auto it = m_data.find(index);
    if (it == m_data.end())
        return make_stdlib_error(std::errc::invalid_argument);

    switch (it->value.type)
    {
    case mbox::type::directory:
    case mbox::type::list_inputs:
    case mbox::type::list_variables:
    case mbox::type::list_timers:
    case mbox::type::list_events:
    case mbox::type::list_commands:
    {
        for (auto&& child : it->value.value.directory.indices)
            ICY_ERROR(vec.push_back(child));

        if (recursive)
        {
            for (auto&& child : it->value.value.directory.indices)
                ICY_ERROR(children(child, true, vec));
        }
        break;
    }
    }
    return {};
}
error_type mbox::library::references(const guid& index, array<guid>& vec) noexcept
{
    for (auto&& pair : m_data)
    {
        const auto type = pair.value.type;
        const auto& value = pair.value.value;
        auto use = false;
        switch (type)
        {
        case type::binding:
        {
            use |= index == value.binding.command;
            use |= index == value.binding.event;
            use |= index == value.binding.group;
            break;
        }
        case type::command:
        {
            use |= index == value.command.group;
            for (auto&& action : value.command.actions)
            {
                use |= index == action.group;
                use |= index == action.reference;
            }
            break;
        }
        case type::event:
        {
            use |= index == value.event.group;
            use |= index == value.event.reference;
            use |= index == value.event.variable;
            break;
        }
        case type::group:
        {
            for(auto&& profile : value.group.profiles)
                use |= index == profile;
            break;
        }
        case type::profile:
        {
            use |= index == value.profile.command;
            break;
        }
        case type::timer:
        {
            use |= index == value.timer.group;
            break;
        }
        case type::variable:
        {
            use |= index == value.variable.group;
            break;
        }
        }
        if (use)
            ICY_ERROR(vec.push_back(guid(pair.key)));
    }
    return {};
}
error_type mbox::library::remove(const icy::guid& index, remove_query& query) noexcept
{
    return make_stdlib_error(std::errc::function_not_supported);    
}
error_type mbox::library::insert(const base& base) noexcept
{
    const auto parent = m_data.find(base.parent);
    if (parent == m_data.end() || find(base.index))
        return make_stdlib_error(std::errc::invalid_argument);

    for (auto&& index : parent->value.value.directory.indices)
    {
        if (const auto child = find(index))
        {
            if (child->name == base.name)
                return make_stdlib_error(std::errc::invalid_argument);
        }
    }

    array<guid>* parent_indices;

    switch (parent->value.type)
    {
    case mbox::type::directory:
    {
        switch (base.type)
        {
        case mbox::type::directory:
        case mbox::type::group:
        case mbox::type::profile:
        case mbox::type::list_inputs:
        case mbox::type::list_variables:
        case mbox::type::list_timers:
        case mbox::type::list_events:
        case mbox::type::list_bindings:
        case mbox::type::list_commands:
            break;
        default:
            return make_stdlib_error(std::errc::invalid_argument);
        }
        parent_indices = &parent->value.value.directory.indices;
        break;
    }
    default:
        return make_stdlib_error(std::errc::invalid_argument);
    }
    
    mbox::base new_base;
    new_base.index = base.index;
    new_base.parent = base.parent;
    new_base.type = base.type;
    ICY_ERROR(to_string(base.name, new_base.name));
    if (parent_indices)
    {
        if (std::find(parent_indices->begin(), parent_indices->end(), base.index) != parent_indices->end())
            return make_stdlib_error(std::errc::invalid_argument);
        ICY_ERROR(parent_indices->push_back(base.index));
    }
    if (const auto error = m_data.insert(guid(base.index), std::move(new_base)))
    {
        if (parent_indices)
            parent_indices->pop_back();
        return error;
    }
    return {};
}
error_type mbox::library::to_json(const base& base, icy::json& obj) noexcept
{
    obj = json_type::object;
    string index_str;
    ICY_ERROR(to_string(base.index, index_str));
    ICY_ERROR(obj.insert(str_key_index, std::move(index_str)));
    ICY_ERROR(obj.insert(str_key_type, to_string(base.type)));
    json json_data = json_type::object;
    switch (base.type)
    {
    case type::directory:
    case type::list_bindings:
    case type::list_commands:
    case type::list_events:
    case type::list_inputs:
    case type::list_timers:
    case type::list_variables:
    {
        const auto& indices = base.value.directory.indices;
        json_data = json_type::object;
        ICY_ERROR(json_data.reserve(indices.size()));
        for (auto&& child : indices)
        {
            const auto ptr = find(child);
            if (ptr)
            {
                json json_child = json_type::object;
                ICY_ERROR(to_json(*ptr, json_child));
                ICY_ERROR(json_data.insert(ptr->name, std::move(json_child)));
            }
        }
        break;
    }
    case type::input:
    {
        const auto& value = base.value.input;
        ICY_ERROR(json_data.insert(str_key_type, to_string(value.itype)));
        if (value.itype == input_type::keyboard || value.itype == input_type::macro)
        {
            ICY_ERROR(json_data.insert(str_key_kevent, ::to_string(value.kevent)));
            ICY_ERROR(json_data.insert(str_key_button, to_string(value.kbutton)));
        }
        else if (value.itype == input_type::mouse)
        {
            ICY_ERROR(json_data.insert(str_key_mevent, ::to_string(value.mevent)));
            ICY_ERROR(json_data.insert(str_key_button, ::to_string(value.mbutton)));
        }
        if (const auto integer = uint32_t(value.ctrl))
             ICY_ERROR(json_data.insert(str_key_ctrl, integer));
        if (const auto integer = uint32_t(value.alt))
            ICY_ERROR(json_data.insert(str_key_alt, integer));
        if (const auto integer = uint32_t(value.shift))
            ICY_ERROR(json_data.insert(str_key_shift, integer));

        if (value.itype == input_type::macro && !value.macro.empty())
            ICY_ERROR(json_data.insert(str_key_macro, value.macro));
        
        break;
    }
    case type::variable:
    {
        const auto& value = base.value.variable;
        ICY_ERROR(json_data.insert(str_key_type, to_string(value.vtype)));
        string str_group;
        ICY_ERROR(to_string(value.group, str_group));
        ICY_ERROR(json_data.insert(str_key_group, std::move(str_group)));
        switch (value.vtype)
        {
        case variable_type::boolean:
            ICY_ERROR(json_data.insert(str_key_value, value.boolean));
            break;
        case variable_type::integer:
        case variable_type::counter:
            ICY_ERROR(json_data.insert(str_key_value, value.integer));
            break;
        case variable_type::string:
            ICY_ERROR(json_data.insert(str_key_value, value.string));
            break;
        }
        break;
    }
    case type::timer:
    {
        const auto& value = base.value.timer;
        string str_group;
        ICY_ERROR(to_string(value.group, str_group));
        ICY_ERROR(json_data.insert(str_key_group, std::move(str_group)));
        const auto offset = std::chrono::duration_cast<std::chrono::milliseconds>(value.offset).count();
        const auto period = std::chrono::duration_cast<std::chrono::milliseconds>(value.period).count();
        ICY_ERROR(json_data.insert(str_key_offset, offset));
        ICY_ERROR(json_data.insert(str_key_period, period));
        break;
    }
    case type::event:
    {
        const auto& value = base.value.event;
        ICY_ERROR(json_data.insert(str_key_type, to_string(value.etype)));
        string str_group;
        string str_reference;
        string str_variable;
        ICY_ERROR(to_string(value.group, str_group));
        ICY_ERROR(to_string(value.reference, str_reference));
        ICY_ERROR(to_string(value.variable, str_variable));
        ICY_ERROR(json_data.insert(str_key_group, std::move(str_group)));
        ICY_ERROR(json_data.insert(str_key_reference, std::move(str_reference)));
        ICY_ERROR(json_data.insert(str_key_variable, std::move(str_variable)));
        break;
    }
    case type::command:
    {
        const auto& value = base.value.command;
        string str_group;
        ICY_ERROR(to_string(value.group, str_group));
        ICY_ERROR(json_data.insert(str_key_group, std::move(str_group)));
        json actions = json_type::array;
        for (auto&& action : value.actions)
        {
            json json_action = json_type::object;
            ICY_ERROR(json_action.insert(str_key_type, to_string(action.atype)));

            string str_reference;
            ICY_ERROR(to_string(action.group, str_group));
            ICY_ERROR(to_string(action.reference, str_reference));
            ICY_ERROR(json_action.insert(str_key_group, std::move(str_group)));
            ICY_ERROR(json_action.insert(str_key_reference, std::move(str_reference)));
            ICY_ERROR(actions.push_back(std::move(json_action)));
        }
        ICY_ERROR(json_data.insert(str_key_actions, std::move(actions)));
        break;
    }
    case type::binding:
    {
        const auto& value = base.value.binding;
        string str_group;
        string str_event;
        string str_command;
        ICY_ERROR(to_string(value.group, str_group));
        ICY_ERROR(to_string(value.event, str_event));
        ICY_ERROR(to_string(value.command, str_command));
        ICY_ERROR(json_data.insert(str_key_group, std::move(str_group)));
        ICY_ERROR(json_data.insert(str_key_event, std::move(str_event)));
        ICY_ERROR(json_data.insert(str_key_command, std::move(str_command)));
        break;
    }
    case type::group:
    {
        const auto& value = base.value.group;
        json profiles = json_type::array;
        for (auto&& profile : value.profiles)
        {
            string str_guid;
            ICY_ERROR(to_string(profile, str_guid));
            ICY_ERROR(profiles.push_back(std::move(str_guid)));
        }
        ICY_ERROR(json_data.insert(str_key_profiles, std::move(profiles)));
        break;        
    }
    case type::profile:
    {
        const auto& value = base.value.profile;
        string str_command;
        ICY_ERROR(to_string(value.command, str_command));
        ICY_ERROR(json_data.insert(str_key_command, std::move(str_command)));
        break;
    }
    default:
        return make_stdlib_error(std::errc::invalid_argument);
    }
    if ((json_data.type() == json_type::array || json_data.type() == json_type::object) && json_data.size() == 0)
        ;
    else
        ICY_ERROR(obj.insert(str_key_data, std::move(json_data)));
    return {};
}
error_type mbox::library::from_json(const json& json_input, mbox::base& output) noexcept
{
    if (json_input.type() != json_type::object)
        return make_stdlib_error(std::errc::invalid_argument);

    ICY_ERROR(json_input.get(str_key_index).to_value(output.index));
    
    const auto str_base_type = json_input.get(str_key_type);
    for (; output.type < mbox::type::_total; output.type = mbox::type(uint32_t(output.type) + 1))
    {
        if (to_string(output.type) == str_base_type)
            break;
    }
    if (output.type == mbox::type::_total)
        return make_stdlib_error(std::errc::invalid_argument);

    const auto json_data = json_input.find(str_key_data);
    switch (output.type)
    {
    case mbox::type::directory:
    case mbox::type::list_inputs:
    case mbox::type::list_variables:
    case mbox::type::list_timers:
    case mbox::type::list_events:
    case mbox::type::list_bindings:
    case mbox::type::list_commands:
    {
        auto& value = output.value.directory;
        if (json_data && json_data->type() == json_type::object)
        {
            for (auto k = 0u; k < json_data->size(); ++k)
            {
                mbox::base child;
                ICY_ERROR(from_json(json_data->vals()[k], child));
                child.parent = output.index;
                ICY_ERROR(to_string(json_data->keys()[k], child.name));

                switch (output.type)
                {
                case mbox::type::list_inputs:
                    if (child.type != mbox::type::input)
                        return make_stdlib_error(std::errc::invalid_argument);
                    break;

                case mbox::type::list_variables:
                    if (child.type != mbox::type::variable)
                        return make_stdlib_error(std::errc::invalid_argument);
                    break;

                case mbox::type::list_timers:
                    if (child.type != mbox::type::timer)
                        return make_stdlib_error(std::errc::invalid_argument);
                    break;

                case mbox::type::list_events:
                    if (child.type != mbox::type::event)
                        return make_stdlib_error(std::errc::invalid_argument);
                    break;

                case mbox::type::list_bindings:
                    if (child.type != mbox::type::binding)
                        return make_stdlib_error(std::errc::invalid_argument);
                    break;

                case mbox::type::list_commands:
                    if (child.type != mbox::type::command)
                        return make_stdlib_error(std::errc::invalid_argument);
                    break;

                default:
                    if (false
                        || child.type == mbox::type::directory
                        || child.type == mbox::type::group
                        || child.type == mbox::type::profile
                        || child.type == mbox::type::list_inputs
                        || child.type == mbox::type::list_variables
                        || child.type == mbox::type::list_timers
                        || child.type == mbox::type::list_events
                        || child.type == mbox::type::list_bindings
                        || child.type == mbox::type::list_commands)
                    {
                        ;
                    }
                    else
                        return make_stdlib_error(std::errc::invalid_argument);
                    break;
                }
                ICY_ERROR(value.indices.push_back(child.index));
                ICY_ERROR(m_data.insert(guid(child.index), std::move(child)));
            }
        }
        break;
    }

    case mbox::type::input:
    {
        auto& value = output.value.input;
        if (!json_data || json_data->type() != json_type::object)
            return make_stdlib_error(std::errc::invalid_argument);

        ICY_ERROR(to_value(json_data->get(str_key_type), value.itype));
       /* const auto str_value_type = json_data->get(str_key_type);
        for (; value.itype != mbox::input_type::_total; value.itype = mbox::input_type(uint32_t(value.itype) + 1))
        {
            if (to_string(value.itype) == str_value_type)
                break;
        }*/
        if (value.itype == mbox::input_type::_total)
            return make_stdlib_error(std::errc::invalid_argument);

        auto value_ctrl = 0u;
        auto value_alt = 0u;
        auto value_shift = 0u;
        json_data->get(str_key_ctrl, value_ctrl);
        json_data->get(str_key_alt, value_alt);
        json_data->get(str_key_shift, value_shift);
        if (value_ctrl >= 4 || value_alt >= 4 || value_shift >= 4)
            return make_stdlib_error(std::errc::invalid_argument);
        value.ctrl = value_ctrl;
        value.alt = value_alt;
        value.shift = value_shift;

        if (value.itype == mbox::input_type::keyboard ||
            value.itype == mbox::input_type::macro)
        {
            const auto str_button = json_data->get(str_key_button);
            if (!str_button.empty())
            {
                for (auto n = 0u; n < 256u; ++n)
                {
                    if (to_string(key(n)) == str_button)
                    {
                        value.kbutton = key(n);
                        break;
                    }
                }
                if (value.kbutton == key::none)
                    return make_stdlib_error(std::errc::invalid_argument);
            }
            ICY_ERROR(to_value(json_data->get(str_key_kevent), value.kevent));
        }
        else if (value.itype == input_type::mouse)
        {
            ICY_ERROR(to_value(json_data->get(str_key_mevent), value.mevent));
            ICY_ERROR(to_value(json_data->get(str_key_button), value.mbutton));
        }

        if (value.itype == mbox::input_type::macro)
            ICY_ERROR(to_string(json_data->get(str_key_macro), value.macro));
        
        break;
    }
    case mbox::type::variable:
    case mbox::type::timer:
    case mbox::type::event:
    case mbox::type::command:
    case mbox::type::binding:
    case mbox::type::group:
    case mbox::type::profile:
        break;
    default:
        return make_stdlib_error(std::errc::invalid_argument);
    }
    return {};
}
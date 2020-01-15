#include "icy_mbox_script.hpp"
#include <icy_engine/core/icy_file.hpp>

using namespace icy;
using namespace mbox;

const guid mbox::root = guid(0x8C8C31B81A56400F, 0xB79ADBFE202E4FFC);
const guid mbox::group_broadcast = guid(0x12A85EC642DF451A, 0xABFDE3C2BE328ED3);
const guid mbox::group_multicast = guid(0xDA0B89EB493F4966, 0x8C538CA27AEA8AB3);
const guid mbox::group_default = guid();

const string_view mbox::str_group_broadcast = "Everyone"_s;
const string_view mbox::str_group_multicast = "Other"_s;
const string_view mbox::str_group_default = "Self"_s;

static constexpr auto str_key_index = "Index"_s;
//static constexpr auto str_key_name = "Name"_s;
static constexpr auto str_key_type = "Type"_s;
static constexpr auto str_key_data = "Data"_s;
static constexpr auto str_key_value = "Value"_s;
static constexpr auto str_key_macro = "Macro"_s;
static constexpr auto str_key_button = "Button"_s;
static constexpr auto str_key_ctrl = "Ctrl"_s;
static constexpr auto str_key_shift = "Shift"_s;
static constexpr auto str_key_alt = "Alt"_s;
static constexpr auto str_key_group = "Group"_s;
static constexpr auto str_key_event = "Event"_s;
static constexpr auto str_key_command = "Command"_s;
static constexpr auto str_key_offset = "Offset"_s;
static constexpr auto str_key_period = "Period"_s;
static constexpr auto str_key_reference = "Reference"_s;
static constexpr auto str_key_variable = "Variable"_s;
static constexpr auto str_key_profiles = "Profiles"_s;
static constexpr auto str_key_actions = "Actions"_s;
static constexpr auto str_key_assign = "Assign"_s;

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
string_view mbox::to_string(const mbox::input_type type) noexcept
{
    switch (type)
    {
    case input_type::button_press: return "Button Press"_s;
    case input_type::button_down: return "Button Down"_s;
    case input_type::button_up: return "Button Up"_s;
    }
    return {};
}
string_view mbox::to_string(const mbox::event_type type) noexcept
{
    switch (type)
    {
    case event_type::variable_changed: return "Var Changed"_s;
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
    case action_type::set_focus: return "Set Focus"_s;
    case action_type::enable_binding: return "Enable Binding"_s;
    case action_type::enable_bindings_list: return "Enable Bindings List"_s;
    case action_type::disable_binding: return "Disable Binding"_s;
    case action_type::disable_bindings_list: return "Disable Bindings List"_s;

    case action_type::replace_input: return "Replace Input"_s;
    case action_type::replace_command: return "Replace Command"_s;
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

error_type mbox::base::copy(const base& src, base& dst) noexcept
{
    dst.index = src.index;
    dst.parent = src.parent;
    dst.type = src.type;
    ICY_ERROR(to_string(src.name, dst.name));
    dst.value = {};
    switch (src.type)
    {
    case mbox::type::directory:   
    case mbox::type::list_inputs:
    case mbox::type::list_variables:
    case mbox::type::list_timers:
    case mbox::type::list_events:
    case mbox::type::list_bindings:
    case mbox::type::list_commands:
        ICY_ERROR(dst.value.directory.indices.assign(src.value.directory.indices));
        break;

    case mbox::type::input:
    {
        const auto& src_value = src.value.input;
        auto& dst_value = dst.value.input;
        dst_value.alt = src_value.alt;
        dst_value.button = src_value.button;
        dst_value.ctrl = src_value.ctrl;
        dst_value.itype = src_value.itype;
        dst_value.shift = src_value.shift;
        ICY_ERROR(to_string(src_value.macro, dst_value.macro));
        break;
    }
    case mbox::type::variable:
    {
        const auto& src_value = src.value.variable;
        auto& dst_value = dst.value.variable;
        dst_value.vtype = src_value.vtype;
        dst_value.group = src_value.group;
        switch (src_value.vtype)
        {
        case variable_type::boolean:
            dst_value.boolean = src_value.boolean;
            break;
        case variable_type::counter:
        case variable_type::integer:
            dst_value.integer = src_value.integer;
            break;
        case variable_type::string:
            ICY_ERROR(to_string(src_value.string, dst_value.string));
            break;
        }
        break;
    }
    case mbox::type::timer:
    {
        const auto& src_value = src.value.timer;
        auto& dst_value = dst.value.timer;
        dst_value = src_value;
        break;
    }
    case mbox::type::event:
    {
        const auto& src_value = src.value.event;
        auto& dst_value = dst.value.event;
        dst_value = src_value;
        break;
    }
    case mbox::type::command:
    {
        const auto& src_value = src.value.command;
        auto& dst_value = dst.value.command;
        dst_value.group = src_value.group;
        ICY_ERROR(dst_value.actions.assign(src_value.actions));
        break;
    }

    case mbox::type::binding:
    {
        const auto& src_value = src.value.binding;
        auto& dst_value = dst.value.binding;
        dst_value = src_value;
        break;
    }

    case mbox::type::group:
    {
        const auto& src_value = src.value.group;
        auto& dst_value = dst.value.group;
        ICY_ERROR(dst_value.profiles.assign(src_value.profiles));
        break;
    }

    case mbox::type::profile:
    {
        const auto& src_value = src.value.profile;
        auto& dst_value = dst.value.profile;
        dst_value = src_value;
        break;
    }
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
    ++m_version;
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
    ++m_version;
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
error_type mbox::library::is_valid(const base& base) noexcept
{
    const auto parent = m_data.find(base.parent);
    if (parent == m_data.end() && base.index != mbox::root)
        return make_stdlib_error(std::errc::invalid_argument);

    if (base.name.empty())
        return make_stdlib_error(std::errc::invalid_argument);

    const auto is_group = [this](const guid& index)
    {
        if (index == mbox::group_default ||
            index == mbox::group_multicast ||
            index == mbox::group_broadcast)
        {
            ;
        }
        else
        {
            const auto ptr = find(index);
            if (!ptr || ptr->type != mbox::type::group)
                return make_stdlib_error(std::errc::invalid_argument);
        }
        return error_type();
    };

    switch (base.type)
    {
    case mbox::type::directory:
    case mbox::type::list_inputs:
    case mbox::type::list_variables:
    case mbox::type::list_timers:
    case mbox::type::list_events:
    case mbox::type::list_bindings:
    case mbox::type::list_commands:
        break;

    case mbox::type::input:
        break;

    case mbox::type::variable:
        ICY_ERROR(is_group(base.value.variable.group));
        break;
    
    case mbox::type::timer:
        ICY_ERROR(is_group(base.value.timer.group));
        if (base.value.timer.offset.count() < 0 ||
            base.value.timer.period.count() < 0)
            return make_stdlib_error(std::errc::invalid_argument);
        break;

    case mbox::type::event:
    {
        const auto& value = base.value.event;
        ICY_ERROR(is_group(value.group));
        const auto ref = find(value.reference);

        if (value.etype == mbox::event_type::focus_acquire ||
            value.etype == mbox::event_type::focus_release)
        {
            break;
        }
        else if (!ref)
        {
            return make_stdlib_error(std::errc::invalid_argument);
        }

        switch (value.etype)
        {
        case mbox::event_type::variable_changed:
        {
            if (ref->type != mbox::type::variable)
                return make_stdlib_error(std::errc::invalid_argument);
            break;
        }
        case mbox::event_type::variable_not_equal:
        case mbox::event_type::variable_greater:
        case mbox::event_type::variable_lesser:
        case mbox::event_type::variable_greater_or_equal:
        case mbox::event_type::variable_lesser_or_equal:
        case mbox::event_type::variable_str_contains:
        {
            if (ref->type != mbox::type::variable)
                return make_stdlib_error(std::errc::invalid_argument);
            const auto compare = find(value.compare);
            if (!compare || compare->type != mbox::type::variable)
                return make_stdlib_error(std::errc::invalid_argument);

            if (value.etype == mbox::event_type::variable_str_contains)
            {
                if (ref->value.variable.vtype != mbox::variable_type::string)
                    return make_stdlib_error(std::errc::invalid_argument);
            }
            else
            {
                if (ref->value.variable.vtype == mbox::variable_type::string)
                    ;
                else if (ref->value.variable.vtype != compare->value.variable.vtype)
                    return make_stdlib_error(std::errc::invalid_argument);
            }
            break;
        }
        case mbox::event_type::recv_input:
        {
            if (ref->type != mbox::type::input)
                return make_stdlib_error(std::errc::invalid_argument);
            break;
        }
                    
        case mbox::event_type::timer_first:
        case mbox::event_type::timer_tick:
        case mbox::event_type::timer_last:
            if (ref->type != mbox::type::timer)
                return make_stdlib_error(std::errc::invalid_argument);
            break;

        default:
            return make_stdlib_error(std::errc::invalid_argument);
        }


    }
    case mbox::type::command:
    {
        const auto& value = base.value.command;
        ICY_ERROR(is_group(value.group));
        for (auto&& action : value.actions)
        {
            ICY_ERROR(is_group(action.group));
            const auto lhs = find(action.reference);
            if (!lhs)
                return make_stdlib_error(std::errc::invalid_argument);

            switch (action.atype)
            {
            case mbox::action_type::variable_assign:
            {
                const auto rhs = find(action.assign);
                if (!lhs || !rhs || lhs->type != mbox::type::variable || rhs->type != mbox::type::variable)
                    return make_stdlib_error(std::errc::invalid_argument);
                break;
            }
            case mbox::action_type::variable_inc:
            case mbox::action_type::variable_dec:
            {
                if (!lhs || lhs->type != mbox::type::variable || lhs->value.variable.vtype != mbox::variable_type::counter)
                    return make_stdlib_error(std::errc::invalid_argument);
                break;
            }

            case mbox::action_type::execute_command:
            {
                if (lhs->type != mbox::type::command)
                    return make_stdlib_error(std::errc::invalid_argument);
                break;
            }
            case mbox::action_type::timer_start:
            case mbox::action_type::timer_stop:
            case mbox::action_type::timer_pause:
            case mbox::action_type::timer_resume:
            {
                if (lhs->type != mbox::type::timer)
                    return make_stdlib_error(std::errc::invalid_argument);
                break;
            }

            case mbox::action_type::send_input:
            {
                if (lhs->type != mbox::type::input)
                    return make_stdlib_error(std::errc::invalid_argument);
                break;
            }
            case mbox::action_type::join_group:
            case mbox::action_type::leave_group:
            {
                if (lhs->type != mbox::type::group)
                    return make_stdlib_error(std::errc::invalid_argument);
                break;
            }

            case mbox::action_type::set_focus:
            {
                if (lhs->type != mbox::type::profile)
                    return make_stdlib_error(std::errc::invalid_argument);
                break;
            }

            case mbox::action_type::enable_binding:
            case mbox::action_type::disable_binding:
            {
                if (lhs->type != mbox::type::binding)
                    return make_stdlib_error(std::errc::invalid_argument);
                break;
            }

            case mbox::action_type::enable_bindings_list:
            case mbox::action_type::disable_bindings_list:
            {
                if (lhs->type != mbox::type::list_bindings)
                    return make_stdlib_error(std::errc::invalid_argument);
                break;
            }

            case mbox::action_type::replace_input:
            {
                const auto rhs = find(action.assign);
                if (!lhs || !rhs || lhs->type != mbox::type::input || rhs->type != mbox::type::input)
                    return make_stdlib_error(std::errc::invalid_argument);
                break;
            }
                
            case mbox::action_type::replace_command:
            {
                const auto rhs = find(action.assign);
                if (!lhs || !rhs || lhs->type != mbox::type::command || rhs->type != mbox::type::command)
                    return make_stdlib_error(std::errc::invalid_argument);
                break;
            }
            }
        }
        break;
    }
    case mbox::type::binding:
    {
        const auto& value = base.value.binding;
        ICY_ERROR(is_group(value.group));
        const auto command = find(value.command);
        const auto event = find(value.event);
        if (!command || !event || command->type != mbox::type::command || event->type != mbox::type::event)
            return make_stdlib_error(std::errc::invalid_argument);
        break;
    }
    case mbox::type::profile:
    {
        const auto& value = base.value.profile;
        const auto command = find(value.command);
        if(!command || command->type != mbox::type::command)
            return make_stdlib_error(std::errc::invalid_argument);
        break;
    }

    case mbox::type::group:
    {
        for (auto&& profile : base.value.group.profiles)
        {
            const auto ptr = find(profile);
            if (!ptr || ptr->type != mbox::type::profile)
                return make_stdlib_error(std::errc::invalid_argument);                
        }
        break;
    }

    default:
        return make_stdlib_error(std::errc::invalid_argument);
    }
    return {};
}
error_type mbox::library::children(const icy::guid& index, const bool recursive, icy::array<icy::guid>& vec) noexcept
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
                use |= index == action.assign;
            }
            break;
        }
        case type::event:
        {
            use |= index == value.event.group;
            use |= index == value.event.reference;
            use |= index == value.event.compare;
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
error_type mbox::library::path(const icy::guid& index, icy::string& str) noexcept
{
    if (index == mbox::group_default)
        return icy::to_string(str_group_default, str);
    else if (index == mbox::group_broadcast)
        return icy::to_string(str_group_broadcast, str);
    else if (index == mbox::group_multicast)
        return icy::to_string(str_group_multicast, str);

    auto ptr = find(index);
    if (!ptr)
        return make_stdlib_error(std::errc::invalid_argument);

    ICY_ERROR(to_string(ptr->name, str));
    while (true)
    {
        ptr = find(ptr->parent);
        if (!ptr || ptr->index == mbox::root)
            break;
        ICY_ERROR(str.append(" / "_s));
        ICY_ERROR(str.append(ptr->name));
    }
    return {};
}
error_type mbox::library::remove(const icy::guid& index, remove_query& query) noexcept
{
    const auto base = find(index);
    if (!base || index == mbox::root)
        return make_stdlib_error(std::errc::invalid_argument);

    mbox::library tmp;
    ICY_ERROR(mbox::library::copy(*this, tmp));

    query.m_library = this;
    query.m_indices.clear();

    ICY_ERROR(query.m_indices.push_back(index));
    tmp.m_data.erase(tmp.m_data.find(index));

    while (true)
    {
        auto all_valid = true;
        for (auto it = tmp.m_data.begin(); it != tmp.m_data.end();)
        {
            if (tmp.is_valid(it->value) != error_type())
            {
                ICY_ERROR(query.m_indices.push_back(it->key));
                it = tmp.m_data.erase(it);
                all_valid = false;
            }
            else
            {
                ++it;
            }
        }
        if (all_valid)
            break;
    }
    query.m_version = m_version;
    return {}; 
}
error_type mbox::library::insert(const base& base) noexcept
{
    if (m_data.try_find(base.index))
        return make_stdlib_error(std::errc::invalid_argument);

    ICY_ERROR(is_valid(base));
    auto& parent = m_data.find(base.parent)->value;
    
    for (auto&& index : parent.value.directory.indices)
    {
        if (const auto child = find(index))
        {
            if (child->name == base.name)
                return make_stdlib_error(std::errc::invalid_argument);
        }
    }

    array<guid>* parent_indices;
    parent_indices = &parent.value.directory.indices;

    switch (parent.type)
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
        parent_indices = &parent.value.directory.indices;
        break;
    }
    case mbox::type::list_inputs:
    {
        if (base.type != type::input)
            return make_stdlib_error(std::errc::invalid_argument);
        break;
    }
    case mbox::type::list_events:
    {
        if (base.type != type::event)
            return make_stdlib_error(std::errc::invalid_argument);
        break;
    }
    case mbox::type::list_commands:
    {
        if (base.type != type::command)
            return make_stdlib_error(std::errc::invalid_argument);
        break;
    }
    case mbox::type::list_bindings:
    {
        if (base.type != type::binding)
            return make_stdlib_error(std::errc::invalid_argument);
        break;
    }
    default:
        parent_indices = nullptr;
    }
    
    mbox::base new_base;
    ICY_ERROR(mbox::base::copy(base, new_base));      

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
    ++m_version;
    return {};
}
error_type mbox::library::modify(const base& base) noexcept
{
    const auto it = m_data.find(base.index);
    if (it == m_data.end() || base.type != it->value.type || base.parent != it->value.parent)
        return make_stdlib_error(std::errc::invalid_argument);

    if (base.name.empty())
        return make_stdlib_error(std::errc::invalid_argument);

    if (base.index != mbox::root)
    {
        auto& parent = m_data.find(base.parent)->value;
        for (auto&& index : parent.value.directory.indices)
        {
            if (index == base.index)
                continue;

            if (const auto child = find(index))
            {
                if (child->name == base.name)
                    return make_stdlib_error(std::errc::invalid_argument);
            }
        }
    }

    switch (base.type)
    {
    case mbox::type::directory:
    case mbox::type::list_inputs:
    case mbox::type::list_variables:
    case mbox::type::list_timers:
    case mbox::type::list_events:
    case mbox::type::list_bindings:
    case mbox::type::list_commands:
    {
        ICY_ERROR(to_string(base.name, it->value.name));
        break;
    }

    case mbox::type::input:
    case mbox::type::variable:
    case mbox::type::timer:
    case mbox::type::event:
    case mbox::type::command:
    case mbox::type::binding:
    case mbox::type::group:
    case mbox::type::profile:
    {
        mbox::base old_base;
        ICY_ERROR(mbox::base::copy(it->value, old_base));
        ICY_ERROR(mbox::base::copy(base, it->value));
        if (const auto error = is_valid(it->value))
        {
            it->value = std::move(old_base);
            return error;
        }
        break;
    }
    }
    ++m_version;
    return {};
}
error_type mbox::library::remove_query::commit() noexcept
{
    if (!m_library || m_library->m_version != m_version)
        return make_stdlib_error(std::errc::invalid_argument);

    mbox::library lib;
    ICY_ERROR(mbox::library::copy(*m_library, lib));

    for (auto&& index : m_indices)
        lib.m_data.erase(lib.m_data.find(index));
    
    for (auto&& base : lib.m_data)
    {
        switch (base.value.type)
        {
        case mbox::type::list_bindings:
        case mbox::type::list_commands:
        case mbox::type::list_events:
        case mbox::type::list_inputs:
        case mbox::type::list_timers:
        case mbox::type::list_variables:
        case mbox::type::directory:
        {
            array<guid> new_indices;
            for (auto&& child : base.value.value.directory.indices)
            {
                if (lib.m_data.try_find(child))
                    ICY_ERROR(new_indices.push_back(child));
            }
            base.value.value.directory.indices = std::move(new_indices);
            break;
        }
        default:
            break;
        }
    }
    m_library->m_data = std::move(lib.m_data);
    ++m_version;
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
        ICY_ERROR(json_data.insert(str_key_button, to_string(value.button)));
        if (const auto integer = uint32_t(value.ctrl))
             ICY_ERROR(json_data.insert(str_key_ctrl, integer));
        if (const auto integer = uint32_t(value.alt))
            ICY_ERROR(json_data.insert(str_key_alt, integer));
        if (const auto integer = uint32_t(value.shift))
            ICY_ERROR(json_data.insert(str_key_shift, integer));
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
        ICY_ERROR(to_string(value.compare, str_variable));
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
            string str_assign;
            ICY_ERROR(to_string(action.group, str_group));
            ICY_ERROR(to_string(action.reference, str_reference));
            ICY_ERROR(to_string(action.assign, str_assign));
            ICY_ERROR(json_action.insert(str_key_group, std::move(str_group)));
            ICY_ERROR(json_action.insert(str_key_reference, std::move(str_reference)));
            ICY_ERROR(json_action.insert(str_key_assign, std::move(str_assign)));
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

        const auto str_button = json_data->get(str_key_button);
        if (!str_button.empty())
        {
            for (auto n = 0u; n < 256u; ++n)
            {
                if (to_string(key(n)) == str_button)
                {
                    value.button = key(n);
                    break;
                }
            }
        }
        ICY_ERROR(to_value(json_data->get(str_key_type), value.itype));
        ICY_ERROR(to_string(json_data->get(str_key_macro), value.macro));
        
        break;
    }
    case mbox::type::variable:
    {
        auto& value = output.value.variable;
        if (!json_data || json_data->type() != json_type::object)
            return make_stdlib_error(std::errc::invalid_argument);

        ICY_ERROR(to_value(json_data->get(str_key_type), value.vtype));
        ICY_ERROR(json_data->get(str_key_group).to_value(value.group));
        
        switch (value.vtype)
        {
        case variable_type::boolean:
            ICY_ERROR(json_data->get(str_key_value).to_value(value.boolean));
            break;
        case variable_type::integer:
        case variable_type::counter:
            ICY_ERROR(json_data->get(str_key_value).to_value(value.integer));
            break;
        case variable_type::string:
            ICY_ERROR(to_string(json_data->get(str_key_value), value.string));
            break;
        default:
            return make_stdlib_error(std::errc::invalid_argument);
        }
        break;
    }
    case mbox::type::timer:
    {
        auto& value = output.value.timer;
        if (!json_data || json_data->type() != json_type::object)
            return make_stdlib_error(std::errc::invalid_argument);

        ICY_ERROR(json_data->get(str_key_group).to_value(value.group));
        auto offset = 0u;
        auto period = 0u;
        ICY_ERROR(json_data->get(str_key_offset).to_value(offset));
        ICY_ERROR(json_data->get(str_key_period).to_value(period));
        value.offset = std::chrono::milliseconds(offset);
        value.period = std::chrono::milliseconds(period);
        break;
    }
    case mbox::type::event:
    {
        auto& value = output.value.event;
        if (!json_data || json_data->type() != json_type::object)
            return make_stdlib_error(std::errc::invalid_argument);
        ICY_ERROR(to_value(json_data->get(str_key_type), value.etype));
        ICY_ERROR(json_data->get(str_key_group).to_value(value.group));
        ICY_ERROR(json_data->get(str_key_reference).to_value(value.reference));
        ICY_ERROR(json_data->get(str_key_variable).to_value(value.compare));
        break;
    }
    case mbox::type::command:
    {
        auto& value = output.value.command;
        if (!json_data || json_data->type() != json_type::object)
            return make_stdlib_error(std::errc::invalid_argument);
        ICY_ERROR(json_data->get(str_key_group).to_value(value.group));
      
        auto actions = json_data->find(str_key_actions);
        if (!actions || actions->type() != json_type::array)
            return make_stdlib_error(std::errc::invalid_argument);

        for (auto&& action : actions->vals())
        {
            if (action.type() != json_type::object)
                return make_stdlib_error(std::errc::invalid_argument);

            mbox::value_command::action new_action;
            ICY_ERROR(to_value(action.get(str_key_type), new_action.atype));
            ICY_ERROR(action.get(str_key_group).to_value(new_action.group));
            ICY_ERROR(action.get(str_key_reference).to_value(new_action.reference));
            ICY_ERROR(action.get(str_key_assign).to_value(new_action.assign));
            ICY_ERROR(value.actions.push_back(std::move(new_action)));
        }
        break;
    }
    case mbox::type::binding:
    {
        auto& value = output.value.binding;
        if (!json_data || json_data->type() != json_type::object)
            return make_stdlib_error(std::errc::invalid_argument);
        ICY_ERROR(json_data->get(str_key_group).to_value(value.group));
        ICY_ERROR(json_data->get(str_key_command).to_value(value.command));
        ICY_ERROR(json_data->get(str_key_event).to_value(value.event));
        break;
    }
    case mbox::type::group:
    {
        auto& value = output.value.group;
        if (!json_data || json_data->type() != json_type::object)
            return make_stdlib_error(std::errc::invalid_argument);

        const auto profiles = json_data->find(str_key_profiles);
        if (!profiles || profiles->type() != json_type::array)
            return make_stdlib_error(std::errc::invalid_argument);

        for (auto&& profile : profiles->vals())
        {
            guid index;
            ICY_ERROR(profile.get().to_value(index));
            ICY_ERROR(value.profiles.push_back(index));
        }
        break;
    }
    case mbox::type::profile:
    {
        auto& value = output.value.profile;
        if (!json_data || json_data->type() != json_type::object)
            return make_stdlib_error(std::errc::invalid_argument);
        ICY_ERROR(json_data->get(str_key_command).to_value(value.command));
        break;
    }
    default:
        return make_stdlib_error(std::errc::invalid_argument);
    }
    return {};
}
#include "mbox_server.hpp"
#include <icy_engine/core/icy_json.hpp>
#include <icy_engine/core/icy_file.hpp>
#include <icy_engine/core/icy_set.hpp>


using namespace icy;

ICY_STATIC_NAMESPACE_BEG
enum class mbox_error_code : uint32_t
{
    none,
    corrupted_data,
    invalid_index,
    invalid_parent,
    invalid_type,
    invalid_value,
};
error_type make_mbox_error(const mbox_error_code code) noexcept
{
    return error_type(uint32_t(code), error_source_mbox);
}
error_type mbox_error_to_string(const uint32_t code, const string_view locale, string& str) noexcept
{
    return make_stdlib_error(std::errc::function_not_supported);
}
string_view to_string(const mbox_type type) noexcept
{
    switch (type)
    {
    case mbox_type::device_folder:
        return "Devices"_s;
    
    case mbox_type::device:
        return "Device"_s;

    case mbox_type::application_folder:
        return "Applications"_s;

    case mbox_type::application:
        return "Application"_s;

    case mbox_type::character_group:
        return "Character Group"_s;

    case mbox_type::account:
        return "Account"_s; 
    
    case mbox_type::server:
        return "Server"_s;

    case mbox_type::character:
        return "Character"_s;

    case mbox_type::tag_folder:
        return "Tags"_s;

    case mbox_type::tag:
        return "Tag"_s;

    case mbox_type::script_folder:
        return "Scripts"_s;

    case mbox_type::script:
        return "Script"_s;

    case mbox_type::action_folder:
        return "Actions"_s;

    case mbox_type::action:
        return "Action"_s;

    case mbox_type::macro_folder:
        return "Macros"_s;

    case mbox_type::macro:
        return "Macro"_s;

    case mbox_type::timer_folder:
        return "Timers"_s;

    case mbox_type::timer:
        return "Timer"_s;

    case mbox_type::event_folder:
        return "Events"_s;

    case mbox_type::event_key_press:
        return "Event Key Press"_s;

    case mbox_type::event_key_release:
        return "Event Key Release"_s;
    }
    return string_view();
}
string_view to_string(const mbox_operation_type type) noexcept
{
    switch (type)
    {
    case mbox_operation_type::add_tag:
        return "Add Tag"_s;

    case mbox_operation_type::del_tag:
        return "Remove Tag"_s;

    case mbox_operation_type::launch_timer:
        return "Launch Timer"_s;

    case mbox_operation_type::cancel_timer:
        return "Cancel Timer"_s;

    case mbox_operation_type::send_input:
        return "Send Input"_s;

    case mbox_operation_type::run_macro:
        return "Run Macro"_s;

    case mbox_operation_type::run_script:
        return "Run Script"_s;

    case mbox_operation_type::run_action:
        return "Run Action"_s;

    case mbox_operation_type::add_event:
        return "Add Event"_s;

    case mbox_operation_type::del_event:
        return "Remove Event"_s;

    }
    return string_view();
}
const auto mbox_key_index = "index"_s;
const auto mbox_key_type = "type"_s;
const auto mbox_key_parent = "parent"_s;
const auto mbox_key_name = "name"_s;
const auto mbox_key_device_uid = "deviceUid"_s;
const auto mbox_key_app_launch_path = "appLaunchPath"_s;
const auto mbox_key_app_process_path = "appProcessPath"_s;
const auto mbox_key_character_group = "characterGroup"_s;
const auto mbox_key_character_config_device = "device"_s;
const auto mbox_key_character_config_index = "index"_s;
const auto mbox_key_character_config_character = "character"_s;
const auto mbox_key_text = "text"_s;
const auto mbox_key_operations = "operations"_s;
const auto mbox_key_operation_type = "type"_s;
const auto mbox_key_operation_link = "link"_s;
const auto mbox_key_operation_text = "text"_s;
error_type to_value(const mbox_base& base, json& array) noexcept
{
    json output = json_type::object;
    ICY_ERROR(output.insert(mbox_key_index, base.index));
    ICY_ERROR(output.insert(mbox_key_parent, base.parent));
    ICY_ERROR(output.insert(mbox_key_type, to_string(base.type)));
    if (!base.name.empty())
    {
        ICY_ERROR(output.insert(mbox_key_name, base.name));
    }

    if (base.type == mbox_type::device)
    {
        if (!base.device_uid.empty())
            ICY_ERROR(output.insert(mbox_key_device_uid, base.device_uid));
    }
    if (base.type == mbox_type::application)
    {
        if (!base.app_launch_path.empty())
        {
            ICY_ERROR(output.insert(mbox_key_app_launch_path, base.app_launch_path));
        }
        if (!base.app_process_path.empty())
        {
            ICY_ERROR(output.insert(mbox_key_app_process_path, base.app_process_path));
        }
    }
    if (base.type == mbox_type::character_group)
    {
        json jgroup = json_type::array;
        for (auto&& config : base.character_group)
        {
            json jconfig = json_type::object;
            if (config.device)
            {
                ICY_ERROR(jconfig.insert(mbox_key_character_config_device, config.device));
            }
            if (config.index)
            {
                ICY_ERROR(jconfig.insert(mbox_key_character_config_index, config.index));
            }
            if (config.character)
            {
                ICY_ERROR(jconfig.insert(mbox_key_character_config_character, config.character));
            }
            if (jconfig.size())
                ICY_ERROR(jgroup.push_back(std::move(jconfig)));
        }
        if (jgroup.size())
        {
            ICY_ERROR(output.insert(mbox_key_character_group, std::move(jgroup)));
        }
    }

    if (!base.text.empty())
    {
        ICY_ERROR(output.insert(mbox_key_text, base.text));
    }

    json jopers = json_type::array;
    for (auto&& oper : base.operations)
    {
        json joper = json_type::object;
        if (oper.link)
        {
            ICY_ERROR(joper.insert(mbox_key_operation_link, oper.link));
        }
        ICY_ERROR(joper.insert(mbox_key_operation_type, to_string(oper.type)));
        if (!oper.text.empty())
        {
            ICY_ERROR(joper.insert(mbox_key_operation_text, string_view(oper.text)));           
        }
        ICY_ERROR(jopers.push_back(std::move(joper)));
    }
    if (jopers.size())
    {
        ICY_ERROR(output.insert(mbox_key_operations, std::move(jopers)));
    }

    for (auto&& pair : base.children)
    {
        ICY_ERROR(to_value(*pair.value, array));
    }
    return error_type();
}
ICY_STATIC_NAMESPACE_END

error_type mbox_server_database::load(const string_view path) noexcept
{
    unique_ptr<mbox_base> root;
    ICY_ERROR(make_unique(mbox_base(), root));

    if (path.empty())
    {
        m_data.clear();
        ICY_ERROR(m_data.insert(0u, std::move(root)));
    }
    else
    {
        auto success = false;
        auto data = std::move(m_data);
        ICY_SCOPE_EXIT{ if (!success) m_data = std::move(data); };

        file input;
        ICY_ERROR(input.open(path, file_access::read, file_open::open_existing, file_share::none, file_flag::none));
        array<char> bytes;
        size_t size = 0;
        ICY_ERROR(bytes.resize(size = input.info().size));
        ICY_ERROR(input.read(bytes.data(), size));
        json json;
        ICY_ERROR(to_value(string_view(bytes.data(), size), json));
        if (json.type() != json_type::array)
            return mbox_error_corrupted_data;

        ICY_ERROR(m_data.insert(0u, std::move(root)));

        for (auto&& val : json.vals())
        {
            if (val.type() != json_type::object)
                return mbox_error_corrupted_data;

            const icy::json* jvec[] =
            {
                 val.find(mbox_key_index),
                 val.find(mbox_key_parent),
                 val.find(mbox_key_type),
            };
            for (auto&& ptr : jvec)
            {
                if (!ptr)
                    return mbox_error_corrupted_data;
            }

            mbox_base base;
            if (jvec[0]->get(base.index))
                return mbox_error_corrupted_data;
            if (data.try_find(base.index))
                return mbox_error_corrupted_data;

            if (jvec[1]->get(base.parent))
                return mbox_error_corrupted_data;

            for (auto k = 1u; k < uint32_t(mbox_type::_total); ++k)
            {
                if (to_string(mbox_type(k)) == jvec[2]->get())
                {
                    base.type = mbox_type(k);
                    break;
                }
            }
            if (base.type == mbox_type::none)
                return mbox_error_corrupted_data;

            if (create_base(base.parent, base.type, val.get(mbox_key_name), base.index))
                return mbox_error_corrupted_data;

            auto it = m_data.find(base.index);
            if (it == m_data.end())
                return mbox_error_corrupted_data;

            if (base.type == mbox_type::device)
            {
                if (auto uid = val.find(mbox_key_device_uid))
                {
                    ICY_ERROR(copy(uid->get(), it->value->device_uid));
                }
            }

            if (base.type == mbox_type::character_group)
            {
                if (auto cgroup = val.find(mbox_key_character_group))
                {
                    if (cgroup->type() != json_type::array)
                        return mbox_error_corrupted_data;

                    for (auto k = 0u; k < cgroup->size(); ++k)
                    {
                        const auto jconfig = cgroup->at(k);
                        if (!jconfig || jconfig->type() != json_type::object)
                            return mbox_error_corrupted_data;

                        mbox_character_config config;
                        jconfig->get(mbox_key_character_config_device, config.device);
                        jconfig->get(mbox_key_character_config_index, config.index);
                        jconfig->get(mbox_key_character_config_character, config.character);
                        ICY_ERROR(it->value->character_group.push_back(std::move(config)));
                    }
                }
            }
            if (base.type == mbox_type::application)
            {
                if (auto launch = val.find(mbox_key_app_launch_path))
                {
                    ICY_ERROR(copy(launch->get(), it->value->app_launch_path));
                }
                if (auto process = val.find(mbox_key_app_process_path))
                {
                    ICY_ERROR(copy(process->get(), it->value->app_process_path));
                }
            }

            if (auto opers = val.find(mbox_key_operations))
            {
                if (opers->type() != json_type::array)
                    return mbox_error_corrupted_data;

                for (auto k = 0u; k < opers->size(); ++k)
                {
                    const auto joper = opers->at(k);
                    if (!joper || joper->type() != json_type::object)
                        return mbox_error_corrupted_data;

                    mbox_operation oper;
                    joper->get(mbox_key_operation_link, oper.link);
                    joper->get(mbox_key_operation_text, oper.text);
                    const auto optype = joper->get(mbox_key_operation_type);
                    for (auto n = 1u; n < uint32_t(mbox_operation_type::_total); ++n)
                    {
                        if (to_string(mbox_operation_type(n)) == optype)
                        {
                            oper.type = mbox_operation_type(n);
                            break;
                        }
                    }
                    if (oper.type == mbox_operation_type::none)
                        return mbox_error_corrupted_data;
                    ICY_ERROR(it->value->operations.push_back(std::move(oper)));
                }
            }
            if (auto text = val.find(mbox_key_text))
            {
                ICY_ERROR(copy(text->get(), it->value->text));
            }
        }
        for (auto&& pair : m_data)
        {
            const auto& base = *pair.value.get();
            set<uint32_t> indices;
            for (auto&& config : base.character_group)
            {
                if (indices.try_find(config.index))
                    return mbox_error_invalid_index;

                if (config.device)
                {
                    auto it = m_data.find(config.device);
                    if (it == m_data.end())
                        return mbox_error_invalid_index;
                    if (it->value->type != mbox_type::device)
                        return mbox_error_invalid_type;
                }
                if (config.character)
                {
                    auto it = m_data.find(config.character);
                    if (it == m_data.end())
                        return mbox_error_invalid_index;
                    if (it->value->type != mbox_type::character)
                        return mbox_error_invalid_type;
                    
                    while (it != m_data.end())
                    {
                        it = m_data.find(it->value->parent);
                        if (it->value->type == mbox_type::application)
                        {
                            if (it->value->index != base.parent)
                                return mbox_error_invalid_parent;
                            break;
                        }
                    }
                }
            }
            for (auto&& oper : base.operations)
            {
                switch (oper.type)
                {
                case mbox_operation_type::add_tag:
                case mbox_operation_type::del_tag:
                {
                    if (oper.link)
                    {
                        auto it = m_data.find(oper.link);
                        if (it == m_data.end())
                            return mbox_error_invalid_index;
                        if (it->value->type != mbox_type::tag)
                            return mbox_error_invalid_type;
                    }
                    break;
                }
                case mbox_operation_type::launch_timer:
                case mbox_operation_type::cancel_timer:
                {
                    if (oper.link)
                    {
                        auto it = m_data.find(oper.link);
                        if (it == m_data.end())
                            return mbox_error_invalid_index;
                        if (it->value->type != mbox_type::timer)
                            return mbox_error_invalid_type;
                    }
                    break;
                }

                case mbox_operation_type::send_input:
                    break;
                
                case mbox_operation_type::run_macro:
                {
                    if (oper.link)
                    {
                        auto it = m_data.find(oper.link);
                        if (it == m_data.end())
                            return mbox_error_invalid_index;
                        if (it->value->type != mbox_type::macro)
                            return mbox_error_invalid_type;
                    }
                    break;
                }

                case mbox_operation_type::run_script:
                {
                    if (oper.link)
                    {
                        auto it = m_data.find(oper.link);
                        if (it == m_data.end())
                            return mbox_error_invalid_index;
                        if (it->value->type != mbox_type::script)
                            return mbox_error_invalid_type;
                    }
                    break;
                }

                case mbox_operation_type::run_action:
                {
                    if (oper.link)
                    {
                        auto it = m_data.find(oper.link);
                        if (it == m_data.end())
                            return mbox_error_invalid_index;
                        if (it->value->type != mbox_type::action)
                            return mbox_error_invalid_type;
                    }
                    break;
                }

                case mbox_operation_type::add_event:
                case mbox_operation_type::del_event:
                {
                    if (oper.link)
                    {
                        auto it = m_data.find(oper.link);
                        if (it == m_data.end())
                            return mbox_error_invalid_index;
                        if (it->value->type != mbox_type::event_key_press &&
                            it->value->type != mbox_type::event_key_release)
                            return mbox_error_invalid_type;
                    }
                    break;
                }
                }
            }
        }
        success = true;
    }
    return error_type();
}
error_type mbox_server_database::save(const string_view path) noexcept
{
    json json = json_type::array;
    ICY_ERROR(to_value(*m_data.front().value, json));

    string str;
    ICY_ERROR(to_string(json, str));

    file output;
    ICY_ERROR(output.open(path, file_access::write, file_open::create_always, file_share::none, file_flag::none));
    ICY_ERROR(output.append(str.bytes().data(), str.bytes().size()));
    return error_type();

}
error_type mbox_server_database::tree_view(const uint32_t index, gui_data_write_model& model, const gui_node node) noexcept
{
    auto it = m_data.find(index);
    if (it == m_data.end())
        return mbox_error_invalid_index;

    auto skip = false;
    auto& val = *it->value.get();
    switch (val.type)
    {
    case mbox_type::device_folder:
    case mbox_type::application_folder:
    case mbox_type::application:
    case mbox_type::account:
    case mbox_type::server:
    case mbox_type::tag_folder:
    case mbox_type::action_folder:
    case mbox_type::script_folder:
    case mbox_type::macro_folder:
    case mbox_type::timer_folder:
        break;
    default:
        if (index != 0)
            skip = true;
        break;
    }

    if (val.name.empty())
    {
        if (index == 0)
        {
            ICY_ERROR(model.modify(node, gui_node_prop::data, "Root"_s));
        }
        else
        {
            string str;
            ICY_ERROR(to_string("%1 [%2]"_s, str, to_string(val.type), val.index));
            ICY_ERROR(model.modify(node, gui_node_prop::data, str));
        }
    }
    else
    {
        ICY_ERROR(model.modify(node, gui_node_prop::data, val.name));
    }
    ICY_ERROR(model.modify(node, gui_node_prop::user, val.index));

    auto model_ptr = make_shared_from_this(&model);
    auto jt = val.tree_nodes.find(weak_ptr<gui_data_write_model>(model_ptr));
    if (jt == val.tree_nodes.end())
    {
        ICY_ERROR(val.tree_nodes.insert(model_ptr, array<gui_node>(), &jt));
    }
    ICY_ERROR(jt->value.push_back(node));

    if (skip)
        return error_type();

    auto row = 0u;
    for (auto&& x : val.children)
    {
        gui_node new_node;
        ICY_ERROR(model.insert(node, row++, 0, new_node));
        ICY_ERROR(tree_view(x.key, model, new_node));
    }
    return error_type();
}
error_type mbox_server_database::info_view(const uint32_t index, gui_data_write_model& model, const gui_node node) noexcept
{
    return error_type();

}
error_type mbox_server_database::create_base(const uint32_t parent, const mbox_type type, const string_view name, uint32_t& index) noexcept
{
    auto it = m_data.find(parent);
    if (it == m_data.end())
        return mbox_error_invalid_parent;

    auto& pval = *it->value;
    ICY_ERROR(mbox_try_create(pval, type));

    unique_ptr<mbox_base> new_ptr;
    ICY_ERROR(make_unique(mbox_base(type), new_ptr));
    
    if (index == 0)
    {
        index = new_ptr->index = m_data.back().key + 1;
    }
    new_ptr->parent = parent;
    ICY_ERROR(copy(name, new_ptr->name));
    ICY_ERROR(pval.children.insert(index, new_ptr.get()));
    ICY_ERROR(m_data.insert(index, std::move(new_ptr)));
    
    return error_type();
}

error_type mbox_try_create(const mbox_base& pval, const mbox_type type) noexcept
{
    switch (type)
    {
    case mbox_type::device_folder:
    case mbox_type::application_folder:
    case mbox_type::tag_folder:
    case mbox_type::action_folder:
    case mbox_type::script_folder:
    case mbox_type::macro_folder:
    case mbox_type::timer_folder:
    case mbox_type::event_folder:
    {
        if (pval.type != type && pval.index != 0)
            return mbox_error_invalid_parent;
        break;
    }
    case mbox_type::device:
    {
        if (pval.type != mbox_type::device_folder)
            return mbox_error_invalid_parent;
        break;
    }
    case mbox_type::application:
    {
        if (pval.type != mbox_type::application_folder)
            return mbox_error_invalid_parent;
        break;
    }
    case mbox_type::character_group:
    case mbox_type::account:
    {
        if (pval.type != mbox_type::application)
            return mbox_error_invalid_parent;
        break;
    }
    case mbox_type::server:
    {
        if (pval.type != mbox_type::account)
            return mbox_error_invalid_parent;
        break;
    }
    case mbox_type::character:
    {
        if (pval.type != mbox_type::server)
            return mbox_error_invalid_parent;
        break;
    }
    case mbox_type::tag:
    {
        if (pval.type != mbox_type::tag_folder)
            return mbox_error_invalid_parent;
        break;
    }
    case mbox_type::action:
    {
        if (pval.type != mbox_type::action_folder)
            return mbox_error_invalid_parent;
        break;
    }
    case mbox_type::script:
    {
        if (pval.type != mbox_type::script_folder)
            return mbox_error_invalid_parent;
        break;
    }
    case mbox_type::macro:
    {
        if (pval.type != mbox_type::macro_folder)
            return mbox_error_invalid_parent;
        break;
    }
    case mbox_type::timer:
    {
        if (pval.type != mbox_type::timer_folder)
            return mbox_error_invalid_parent;
        break;
    }
    case mbox_type::event_key_press:
    case mbox_type::event_key_release:
    {
        if (pval.type != mbox_type::event_folder)
            return mbox_error_invalid_parent;
        break;
    }
    default:
        return mbox_error_invalid_type;
    }
    return error_type();
}

const error_source error_source_mbox = register_error_source("mbox"_s, mbox_error_to_string);
const error_type mbox_error_corrupted_data = make_mbox_error(mbox_error_code::corrupted_data);
const error_type mbox_error_invalid_index = make_mbox_error(mbox_error_code::invalid_index);
const error_type mbox_error_invalid_parent = make_mbox_error(mbox_error_code::invalid_parent);
const error_type mbox_error_invalid_type = make_mbox_error(mbox_error_code::invalid_type);
const error_type mbox_error_invalid_value = make_mbox_error(mbox_error_code::invalid_value);
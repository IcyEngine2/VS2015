#include "mbox_server.hpp"
#include <icy_engine/core/icy_json.hpp>
#include <icy_engine/core/icy_file.hpp>

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

    case mbox_type::launch_config:
        return "Launch Config"_s;

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
    }
    return string_view();
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

        map<uint32_t, unique_ptr<mbox_base>> data;
        ICY_ERROR(data.insert(0u, std::move(root)));

        for (auto&& val : json.vals())
        {
            if (val.type() != json_type::object)
                return mbox_error_corrupted_data;
        }
        m_data = std::move(data);
    }
    return error_type();
}
error_type mbox_server_database::save(const string_view path) noexcept
{
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
    switch (type)
    {
    case mbox_type::device_folder:
    case mbox_type::application_folder:
    case mbox_type::tag_folder:
    case mbox_type::action_folder:
    case mbox_type::script_folder:
    case mbox_type::macro_folder:
    case mbox_type::timer_folder:
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
    case mbox_type::launch_config:
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
    default:
        return mbox_error_invalid_type;
    }

    
    unique_ptr<mbox_base> new_ptr;
    ICY_ERROR(make_unique(mbox_base(type), new_ptr));
    
    index = new_ptr->index = m_data.back().key + 1;
    new_ptr->parent = parent;
    ICY_ERROR(copy(name, new_ptr->name));
    ICY_ERROR(pval.children.insert(index, new_ptr.get()));
    ICY_ERROR(m_data.insert(index, std::move(new_ptr)));
    
    return error_type();
}



const error_source error_source_mbox = register_error_source("mbox"_s, mbox_error_to_string);
const error_type mbox_error_corrupted_data = make_mbox_error(mbox_error_code::corrupted_data);
const error_type mbox_error_invalid_index = make_mbox_error(mbox_error_code::invalid_index);
const error_type mbox_error_invalid_parent = make_mbox_error(mbox_error_code::invalid_parent);
const error_type mbox_error_invalid_type = make_mbox_error(mbox_error_code::invalid_type);
const error_type mbox_error_invalid_value = make_mbox_error(mbox_error_code::invalid_value);
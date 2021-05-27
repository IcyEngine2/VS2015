#include <icy_engine/core/icy_thread.hpp>
#include <icy_engine/utility/icy_database.hpp>
#include <icy_engine/core/icy_set.hpp>
#include <icy_dcl/icy_dcl.hpp>
using namespace icy;

ICY_STATIC_NAMESPACE_BEG
class dcl_base_data : public dcl_base
{
public:
    static error_type exec(map<dcl_index, unique_ptr<dcl_base_data>>& data, dcl_action_group& group) noexcept;
    dcl_base_data(const dcl_index index, dcl_base_data* parent, const dcl_base_type type) noexcept :
        m_index(index), m_type(type), m_parent(parent)
    {

    }
    error_type tree_view(gui_data_write_model& model, const gui_node node) noexcept;
    dcl_base_type type() const noexcept override
    {
        return m_type;
    }
private:
    dcl_index index() const noexcept override
    {
        return m_index;
    }
    const dcl_base* parent() const noexcept override
    {
        return m_parent;
    }
    string_view name() const noexcept override
    {
        return m_name;
    }
    const_array_view<uint8_t> binary(const dcl_locale locale) const noexcept override
    {
        const auto ptr = m_binary.try_find(locale);
        return const_array_view<uint8_t>(ptr ? ptr->data() : nullptr, ptr ? ptr->size() : 0);
    }
    const dcl_parameter* param(const dcl_index index) const noexcept override
    {
        return m_param.try_find(index);        
    }
    error_type display_name(string& str) const noexcept
    {
        if (m_name.empty())
        {
            string str_guid;
            ICY_ERROR(to_string(m_index, str_guid));
            ICY_ERROR(str.append("{"_s));
            ICY_ERROR(str.append(string_view(str_guid)));
            ICY_ERROR(str.append("}"_s));
        }
        else
        {
            ICY_ERROR(copy(m_name, str));
        }
        return error_type();
    }
private:
    const dcl_index m_index;
    const dcl_base_type m_type = dcl_base_type::none;
    dcl_base_data* const m_parent = nullptr;
    dcl_state state = dcl_state::_default;
    string m_name;
    array<dcl_base_data*> m_children = nullptr;
    map<dcl_locale, array<uint8_t>> m_binary;
    map<dcl_index, dcl_parameter> m_param;
    map<weak_ptr<gui_data_write_model>, array<gui_node>> m_binds;
};
struct dcl_store_action : detail::dcl_action_base_header
{
    uint32_t name = 0;
    uint32_t binary = 0;
};
struct dcl_store_group : detail::dcl_action_group_header
{
    uint32_t data = 0;
};
static error_type dcl_load(const const_array_view<uint8_t> bytes, dcl_action_group& group)
{
    union
    {
        const uint8_t* as_bytes;
        const char* as_name;
        const dcl_store_group* as_group;
        const dcl_store_action* as_action;
    };
    as_bytes = bytes.data();
    auto end = as_bytes + bytes.size();
    if (as_bytes + sizeof(*as_group) > end)
        return dcl_error_corrupted_data;

    const auto& group_header = *(as_group++);
    static_cast<detail::dcl_action_group_header&>(group) = group_header;

    for (auto n = 0u; n < group_header.data; ++n)
    {
        if (as_bytes + sizeof(*as_action) > end)
            return dcl_error_corrupted_data;

        const auto action_header = *(as_action++);

        dcl_action_base new_action;
        static_cast<detail::dcl_action_base_header&>(new_action) = action_header;
        if (as_bytes + action_header.name > end)
            return dcl_error_corrupted_data;
        
        ICY_ERROR(copy(string_view(as_name, as_name + action_header.name), new_action.name));
        as_bytes += action_header.name;

        if (as_bytes + action_header.binary > end)
            return dcl_error_corrupted_data;

        ICY_ERROR(new_action.binary.assign(as_bytes, as_bytes + action_header.binary));
        ICY_ERROR(group.data.push_back(std::move(new_action)));
    }
    if (as_bytes != end)
        return dcl_error_corrupted_data;

    return error_type();
}
static error_type dcl_store(const dcl_action_group& group, array<uint8_t>& bytes) noexcept
{
    const auto append = [&bytes](const auto& x)
    {
        static_assert(std::is_trivially_destructible<remove_cvr<decltype(x)>>::value, "");
        const auto ptr = reinterpret_cast<const uint8_t*>(&x);
        return bytes.append(ptr, ptr + sizeof(x));
    };

    dcl_store_group store_group;
    static_cast<detail::dcl_action_group_header&>(store_group) = group;
    store_group.data = uint32_t(group.data.size());
    ICY_ERROR(append(store_group));
    for (const auto& action : group.data)
    {
        dcl_store_action store_action;
        static_cast<detail::dcl_action_base_header&>(store_action) = action;
        store_action.name = uint32_t(action.name.ubytes().size());
        store_action.binary = uint32_t(action.binary.size());
        ICY_ERROR(append(store_action));
        ICY_ERROR(bytes.append(action.name.ubytes()));
        ICY_ERROR(bytes.append(action.binary));
    }
    return error_type();
}
class dcl_system_data;
class dcl_project_data : public dcl_project
{
    friend dcl_system_data;
public:
    dcl_project_data(shared_ptr<dcl_system_data> system) noexcept : m_system(system)
    {

    }
    dcl_project_data(dcl_project_data&& rhs) noexcept = default;
    ICY_DEFAULT_MOVE_ASSIGN(dcl_project_data);
    error_type initialize(const string_view name, database_dbi dbi, database_txn_write& txn) noexcept;
    error_type destroy() noexcept;
private:
    string_view name() const noexcept override
    {
        return m_name;
    }
    error_type tree_view(gui_data_write_model& model, const gui_node node, const dcl_index index) noexcept override;
    const_array_view<dcl_action_group> actions() const noexcept override
    {
        return m_actions;
    }    
private:
    error_type tree_view(gui_data_write_model& model, const gui_node node, const dcl_index& index, const dcl_base& base) noexcept;
    error_type create_directory(const dcl_index parent, const string_view name, dcl_index& index) noexcept override;
    error_type push(dcl_action_group& group) noexcept;
private:
    weak_ptr<dcl_system_data> m_system;
    string m_name;
    database_dbi m_dbi;
    map<dcl_index, unique_ptr<dcl_base_data>> m_data;
    array<dcl_action_group> m_actions;
};
class dcl_system_data : public dcl_system
{
public:
    dcl_system_data() noexcept = default;
    error_type initialize(const string_view path, const size_t capacity) noexcept;
    error_type write(database_txn_write& txn) noexcept
    {
        return txn.initialize(m_dbase);
    }
private:
    dcl_locale locale(const string_view lang) const noexcept override
    {
        return dcl_locale();
    }
    error_type get_projects(array<string>& names) const noexcept override;
    error_type add_project(const string_view name, shared_ptr<dcl_project>& project) noexcept override;
    error_type del_project(dcl_project& project) noexcept override;
private:
    database_system_write m_dbase;
    database_dbi m_dbi;
};
enum class dcl_error_code : uint32_t
{
    none,
    corrupted_data,
    invalid_index,
    invalid_parent,
    invalid_type,
    invalid_value,
};
error_type make_dcl_error(const dcl_error_code code) noexcept
{
    return error_type(uint32_t(code), error_source_dcl);
}
error_type dcl_error_to_string(const uint32_t code, const string_view locale, string& str) noexcept
{
    return make_stdlib_error(std::errc::function_not_supported);
}
ICY_STATIC_NAMESPACE_END

error_type dcl_base_data::exec(map<dcl_index, unique_ptr<dcl_base_data>>& data, dcl_action_group& group) noexcept
{
    using iterator = map<dcl_index, unique_ptr<dcl_base_data>>::iterator;
    
    const auto add_base = [&data](dcl_base_data& parent, const dcl_index index, const dcl_base_type type, iterator* it = nullptr) -> error_type
    {
        const auto count = uint32_t(parent.m_children.size());
        unique_ptr<dcl_base_data> new_base;
        ICY_ERROR(make_unique(dcl_base_data(index, &parent, type), new_base));
        ICY_ERROR(parent.m_children.reserve(count + 1));

        auto ptr = new_base.get();
        ICY_ERROR(data.insert(index, std::move(new_base), it));
        ICY_ERROR(parent.m_children.push_back(ptr));

        for (auto it = parent.m_binds.begin(); it != parent.m_binds.end(); )
        {
            auto model = shared_ptr<gui_data_write_model>(it->key);
            if (!model)
            {
                it = parent.m_binds.erase(it);
            }
            else
            {
                for (auto&& bind : it->value)
                {
                    gui_node new_node;
                    ICY_ERROR(model->insert(bind, count, 0, new_node));
                    ICY_ERROR(model->modify(new_node, gui_node_prop::user, index));

                    array<gui_node> new_nodes;
                    ICY_ERROR(new_nodes.push_back(new_node));
                    ICY_ERROR(ptr->m_binds.insert(model, std::move(new_nodes)));                    
                }
                ++it;
            }
        }
        return error_type();
    };
    const auto rename = [&data](dcl_base_data& base, string& name)
    {
        std::swap(base.m_name, name);
        for (auto it = base.m_binds.begin(); it != base.m_binds.end(); )
        {
            auto model = shared_ptr<gui_data_write_model>(it->key);
            if (!model)
            {
                it = base.m_binds.erase(it);
            }
            else
            {
                for (auto&& bind : it->value)
                {
                    string dname;
                    ICY_ERROR(base.display_name(dname));
                    ICY_ERROR(model->modify(bind, gui_node_prop::data, dname));
                }
                ++it;
            }
        }
        return error_type();
    };

    const auto root_ptr = data.try_find(guid());
    if (!root_ptr)
        return dcl_error_corrupted_data;

    auto& root = *root_ptr->get();

    auto actions = std::move(group.data);
    for (const auto& action : actions)
    {
        switch (action.action)
        {
        case dcl_action_type::create_directory:
        case dcl_action_type::create_user:
        case dcl_action_type::create_type:
        case dcl_action_type::create_resource:
        case dcl_action_type::create_list:
        case dcl_action_type::create_bits:
        case dcl_action_type::create_object:
        case dcl_action_type::create_connection:
        {
            auto it = data.find(action.parent);
            if (it == data.end())
            {
                group.state |= dcl_state::is_invalid;
                dcl_action_base new_action;
                new_action.action = dcl_action_type::remove;
                new_action.index = action.parent;
                ICY_ERROR(group.data.push_back(std::move(new_action)));
                ICY_ERROR(add_base(root, action.parent, dcl_base_type::directory, &it));                
            }
            auto& parent_data = *it->value.get();
            if (parent_data.m_type != dcl_base_type::directory)
            {
                group.state |= dcl_state::is_invalid;
                break;
            }

            auto type = dcl_base_type::none;
            switch (action.action)
            {
            case dcl_action_type::create_directory: type = dcl_base_type::directory; break; 
            case dcl_action_type::create_user: type = dcl_base_type::user; break; 
            case dcl_action_type::create_type: type = dcl_base_type::type; break;
            case dcl_action_type::create_resource: type = dcl_base_type::resource; break;
            case dcl_action_type::create_list: type = dcl_base_type::list; break;
            case dcl_action_type::create_bits: type = dcl_base_type::bits; break;
            case dcl_action_type::create_object: type = dcl_base_type::object; break;
            case dcl_action_type::create_connection: type = dcl_base_type::connection; break;
            }

            auto jt = data.find(action.index);
            if (jt == data.end())
            {
                ICY_ERROR(add_base(root, action.index, type, &jt));
            }
            auto& base = *jt->value.get();
            {
                dcl_action_base new_action;
                new_action.action = dcl_action_type::remove;
                new_action.index = action.index;
                ICY_ERROR(copy(action.name, new_action.name));
                if (base.m_type != type)
                {
                    group.state |= dcl_state::is_invalid;
                    break;
                }
                base.m_state ^= dcl_state::is_disabled;
                ICY_ERROR(group.data.push_back(new_action));
            }
            if (action.name != base.m_name)
            {
                dcl_action new_action;
                new_action.action = dcl_action_type::rename;
                new_action.index = action.index;
                ICY_ERROR(rename(base, new_action.name));
                ICY_ERROR(group.data.push_back(new_action));
            }
            break;
        }
        case dcl_action_type::remove:
        {
            break;
        }
        case dcl_action_type::rename:
        {
            auto it = data.find(action.index);
            if (it == data.end())
            {
                group.state |= dcl_state::invalid;
            }
            else
            {

            }
            break;
        }
        default:
            return dcl_error_corrupted_data;
        }
    }
    
    return error_type();
}
error_type dcl_base_data::tree_view(gui_data_write_model& model, const gui_node node) noexcept
{
    auto ptr = make_shared_from_this(&model);
    auto it = m_binds.find(weak_ptr<gui_data_write_model>(ptr));
    if (it == m_binds.end())
    {
        ICY_ERROR(m_binds.insert(ptr, array<gui_node>(), &it));
    }

    ICY_ERROR(it->value.push_back(node));
    string dname;
    ICY_ERROR(display_name(dname));
    ICY_ERROR(model.modify(node, gui_node_prop::user, m_index));
    ICY_ERROR(model.modify(node, gui_node_prop::data, dname));
    ICY_ERROR(model.modify(node, gui_node_prop::visible, !(m_state & dcl_state::disabled)));
    ICY_ERROR(model.modify(node, gui_node_prop::enabled, !(m_state & dcl_state::invalid)));

    auto row = 0u;
    for (auto&& child : m_children)
    {
        gui_node new_node;
        ICY_ERROR(model.insert(node, row++, 0, new_node));
        ICY_ERROR(child->tree_view(model, new_node));
    }
    return error_type();
}

error_type dcl_project_data::initialize(const string_view name, database_dbi dbi, database_txn_write& txn) noexcept
{
    unique_ptr<dcl_base_data> root;
    ICY_ERROR(make_unique(dcl_base_data(dcl_index(), nullptr, dcl_base_type::directory), root));
    ICY_ERROR(m_data.insert(dcl_index(), std::move(root)));

    string dbi_name;
    ICY_ERROR(to_string("project_%1_%2"_s, dbi_name, ICY_DCL_API_BINARY_VERSION, name));
    ICY_ERROR(m_dbi.initialize_create_int_key(txn, dbi_name));

    {
        database_cursor_read cur;
        ICY_ERROR(cur.initialize(txn, dbi));

        const_array_view<uint8_t> val;
        auto key = 0u;
        auto error = cur.get_var_by_type(key, val, database_oper_read::first);
        while (error != database_error_not_found)
        {
            ICY_ERROR(error);
            dcl_group group(key);
            ICY_ERROR(dcl_load(val, group));
            if (!(group.state & dcl_state::disabled))
            {
                ICY_ERROR(dcl_base_data::exec(m_data, group));
            }
            error = cur.get_var_by_type(key, val, database_oper_read::next);
        }
    }
    {
        database_cursor_read cur;
        ICY_ERROR(cur.initialize(txn, m_dbi));

        const_array_view<uint8_t> val;
        auto key = 0u;
        auto error = cur.get_var_by_type(key, val, database_oper_read::first);
        while (error != database_error_not_found)
        {
            ICY_ERROR(error);
            dcl_group group(key);
            ICY_ERROR(dcl_load(val, group));
            if (!(group.state & dcl_state::disabled))
            {
                ICY_ERROR(dcl_base_data::exec(m_data, group));
            }
            error = cur.get_var_by_type(key, val, database_oper_read::next);
            ICY_ERROR(m_actions.push_back(std::move(group)));
        }
    }
    return error_type();
}
error_type dcl_project_data::destroy() noexcept
{
    return make_stdlib_error(std::errc::function_not_supported);
}
error_type dcl_project_data::tree_view(gui_data_write_model& model, const gui_node node, const dcl_index index) noexcept
{
    const auto it = m_data.find(index);
    if (it == m_data.end())
        return dcl_error_invalid_index;

    auto& base = *it->value.get();
    return base.tree_view(model, node);
}
error_type dcl_project_data::create_directory(const dcl_index parent, const string_view name, dcl_index& index) noexcept
{
    const auto it = m_data.find(parent);
    if (it == m_data.end())
        return dcl_error_invalid_parent;

    const auto& pdata = *it->value.get();
    if (pdata.type() != dcl_base_type::directory)
        return dcl_error_invalid_parent;

    index = dcl_index::create();
    if (!index)
        return last_system_error();

    dcl_action new_action;
    ICY_ERROR(copy(name, new_action.name));
    new_action.action = dcl_action_type::create_directory;
    new_action.index = index;
    new_action.parent = parent;

    dcl_group group(m_actions.size());
    ICY_ERROR(group.data.push_back(std::move(new_action)));

    
    ICY_ERROR(push(group));
    return error_type();
}
error_type dcl_project_data::push(dcl_group& group) noexcept
{
    auto system = shared_ptr<dcl_system_data>(m_system);
    if (!system)
        return make_stdlib_error(std::errc::invalid_argument);

    array<uint8_t> bytes;
    ICY_ERROR(dcl_store(group, bytes));

    ICY_ERROR(dcl_base_data::exec(m_data, group));
    
    database_txn_write txn;
    ICY_ERROR(system->write(txn));
    {
        database_cursor_write cur;
        ICY_ERROR(cur.initialize(txn, m_dbi));
        array_view<uint8_t> val;
        ICY_ERROR(cur.put_var_by_type(uint32_t(group.index), bytes.size(), database_oper_write::append, val));
        memcpy(val.data(), bytes.data(), bytes.size());
    }
    ICY_ERROR(txn.commit());
    ICY_ERROR(m_actions.push_back(std::move(group)));

    return error_type();
}
error_type dcl_system_data::initialize(const string_view path, const size_t capacity) noexcept
{
    ICY_ERROR(m_dbase.initialize(path, capacity));
    database_txn_write txn;
    ICY_ERROR(txn.initialize(m_dbase));
    string dbname;
    ICY_ERROR(to_string("data_%1"_s, dbname, ICY_DCL_API_BINARY_VERSION));
    ICY_ERROR(m_dbi.initialize_create_int_key(txn, dbname));
    ICY_ERROR(txn.commit());
    return error_type();
}
error_type dcl_system_data::get_projects(array<string>& names) const noexcept
{
    return make_stdlib_error(std::errc::function_not_supported);
}
error_type dcl_system_data::add_project(const string_view name, shared_ptr<dcl_project>& project) noexcept
{
    shared_ptr<dcl_project_data> new_project;
    ICY_ERROR(make_shared(new_project, make_shared_from_this(this)));
    {
        database_txn_write txn;
        ICY_ERROR(txn.initialize(m_dbase));
        ICY_ERROR(new_project->initialize(name, m_dbi, txn));
        ICY_ERROR(txn.commit());
    }
    project = std::move(new_project);
    return error_type();
}
error_type dcl_system_data::del_project(dcl_project& project) noexcept
{
    return static_cast<dcl_project_data&>(project).destroy();
}

error_type icy::create_dcl_system(shared_ptr<dcl_system>& system, const string_view path, const size_t capacity) noexcept
{
    shared_ptr<dcl_system_data> new_system;
    ICY_ERROR(make_shared(new_system));
    ICY_ERROR(new_system->initialize(path, capacity));
    system = std::move(new_system);
    return error_type();
}

const error_source icy::error_source_dcl = register_error_source("dcl"_s, dcl_error_to_string);
const error_type icy::dcl_error_corrupted_data = make_dcl_error(dcl_error_code::corrupted_data);
const error_type icy::dcl_error_invalid_index = make_dcl_error(dcl_error_code::invalid_index);
const error_type icy::dcl_error_invalid_parent = make_dcl_error(dcl_error_code::invalid_parent);
const error_type icy::dcl_error_invalid_type = make_dcl_error(dcl_error_code::invalid_type);
const error_type icy::dcl_error_invalid_value = make_dcl_error(dcl_error_code::invalid_value);

/*error_type dcl_project_base::exec(const dcl_action& action)
{
    switch (action.atype)
    {
    case dcl_action_type::create:
    {
        if (m_data.try_find(action.index))
            return dcl_error_invalid_index;

        dcl_base new_base;
        const auto parent = m_data.find(action.value.as_index);
        if (parent == m_data.end())
            return dcl_error_invalid_parent;

        new_base.type = action.btype;
        switch (new_base.type = action.btype)
        {
        case dcl_type::dir:
        case dcl_type::txt:
        case dcl_type::usr:
        case dcl_type::enm:
        case dcl_type::obj:
        {
            if (parent->value.type == dcl_type::dir)
                ;
            else
                return dcl_error_invalid_type;
            break;
        }
        case dcl_type::par:
        {
            if (parent->value.type == dcl_type::obj)
                ;
            else
                return dcl_error_invalid_type;
            break;
        }
        default:
            return dcl_error_invalid_type;
        }
        new_base.parent = action.value.as_index;
        ICY_ERROR(m_data.insert(action.value.as_index, std::move(new_base)));
        break;
    }
    default:
        return dcl_error_corrupted_data;
    }
    return error_type();
}
error_type dcl_project_base::initialize(database_txn_read& txn)
{
    database_cursor_read cur;
    ICY_ERROR(cur.initialize(txn, m_dbi));

    auto key = 0u;
    auto val = dcl_action();
    ICY_ERROR(cur.get_type_by_type(key, val, database_oper_read::none));

    int64_t max_action = 0;
    if (m_data.empty())
    {
        if (val.atype != dcl_action_type::create || val.btype != dcl_type::root || val.value.as_integer != DCL_API_VERSION)
            return make_stdlib_error(std::errc::invalid_argument);

        dcl_base base;
        base.type = dcl_type::none;
        ICY_ERROR(m_data.insert(dcl_index(), std::move(base)));
    }
    else
    {
        max_action = val.value.as_integer;
    }
    for (int64_t action = 1; action < max_action; ++action)
    {
        if (cur.get_type_by_type(key, val, database_oper_read::next))
        {

        }
    }

}

dcl_project_data_usr::~dcl_project_data_usr() noexcept
{
    if (auto system = shared_ptr<dcl_system_data>(m_system))
    {
        internal_event msg;
        msg.type = internal_event_type::destroy;
        msg.project = m_index;
        system->post(std::move(msg));
    }
}
error_type dcl_project_data_usr::undo() noexcept
{
    if (auto system = shared_ptr<dcl_system_data>(m_system))
    {
        internal_event msg;
        msg.type = internal_event_type::undo;
        msg.project = m_index;
        system->post(std::move(msg));
    }
    else
    {
        return make_stdlib_error(std::errc::invalid_argument);
    }
    return error_type();
}
error_type dcl_project_data_usr::redo() noexcept
{
    if (auto system = shared_ptr<dcl_system_data>(m_system))
    {
        internal_event msg;
        msg.type = internal_event_type::redo;
        msg.project = m_index;
        system->post(std::move(msg));
    }
    else
    {
        return make_stdlib_error(std::errc::invalid_argument);
    }
    return error_type();
}
error_type dcl_project_data_usr::bind(gui_data_write_model& model, const gui_node node, const dcl_index index) noexcept
{
    if (auto system = shared_ptr<dcl_system_data>(m_system))
    {
        internal_event msg;
        msg.type = internal_event_type::bind;
        msg.project = m_index;
        msg.model = make_shared_from_this(&model);
        msg.node = node;
        msg.action.index = index;
        system->post(std::move(msg));
    }
    else
    {
        return make_stdlib_error(std::errc::invalid_argument);
    }
    return error_type();
}

error_type dcl_project_data_sys::initialize(const string_view name) noexcept
{
    ICY_ERROR(copy(name, m_name));
    database_txn_write txn;
    ICY_ERROR(m_system.write(txn));
    string dbi_name;
    ICY_ERROR(dbi_name.appendf("project_%1"_s, name));
    ICY_ERROR(m_dbi.initialize_create_int_key(txn, dbi_name));
    ICY_ERROR(txn.commit());
    ICY_ERROR(copy(m_system.data(), m_data));
    ICY_ERROR(dcl_project_base::initialize());
    return error_type();
}
error_type dcl_project_data_sys::clear() noexcept
{
    return error_type();
}
error_type dcl_project_data_sys::undo() noexcept
{
    return error_type();
}
error_type dcl_project_data_sys::redo() noexcept
{
    return error_type();
}
error_type dcl_project_data_sys::bind(gui_data_write_model& model, const gui_node node, const dcl_index index) noexcept
{
    return error_type();
}

dcl_system_data::~dcl_system_data()
{
    if (m_thread)
        m_thread->wait();
    filter(0);
}
error_type dcl_system_data::get_projects(array<string>& names) const noexcept
{
    /*database_txn_write txn;
    ICY_ERROR(txn.initialize(m_dbase));

    database_cursor_read cur;
    ICY_ERROR(cur.initialize(txn, database_dbi()));
    string_view name_key;
    string_view name_val;
    auto error = cur.get_str_by_str(name_key, name_val, database_oper_read::first);
    while (error != database_error_not_found)
    {
        error = cur.get_str_by_str(name_key, name_val, database_oper_read::next);
    }

    if (error == database_error_not_found)
        error = error_type();
    ICY_ERROR(error);

    return error_type();
}*/
/*error_type dcl_system_data::add_project(const string_view name) noexcept
{
    string key;
    ICY_ERROR(key.appendf("project_%1"_s, name));
    database_txn_write txn;
    ICY_ERROR(txn.initialize(m_dbase));
    database_dbi dbi;
    ICY_ERROR(dbi.initialize_create_int_key(txn, key));
    ICY_ERROR(txn.commit());
    return error_type();
}
error_type dcl_system_data::del_project(const string_view name) noexcept
{
    internal_event event;
    event.type = internal_event_type::del_project;
    std::move(event.project,;
    event.bind.model = make_shared_from_this(&model);
    event.bind.node = node;
    event.bind.recursive = recursive;
    ICY_ERROR(event_system::post(this, event_type::system_internal, std::move(event)));

    string key;
    ICY_ERROR(key.appendf("project_%1"_s, name));
    database_txn_write txn;
    ICY_ERROR(txn.initialize(m_dbase));
    database_dbi dbi;
    ICY_ERROR(dbi.initialize_open_int_key(txn, key));
    ICY_ERROR(dbi.clear(txn));
    ICY_ERROR(txn.commit());
    if (m_usr_dbi == dbi)
    {
        ICY_ERROR(set_project(string_view()));
    }
    return error_type();
}
error_type dcl_system_data::set_project(const string_view name) noexcept
{


    m_base.clear();
    {
        database_txn_read txn;
        ICY_ERROR(txn.initialize(m_dbase));
        auto key = 0u;
        auto val = dcl_action();
        database_cursor_read cur;
        ICY_ERROR(cur.initialize(txn, m_sys_dbi));
        auto error = cur.get_type_by_type(key, val, database_oper_read::first);
        while (true)
        {
            if (error != database_error_not_found)
            {
                ICY_ERROR(error);
            }
            else
                break;

            ICY_ERROR(exec_action(val));
            error = cur.get_type_by_type(key, val, database_oper_read::next);
        }
    }
    if (name.empty())
    {
        m_usr_dbi = database_dbi();
        return error_type();
    }

    string dbi_key;
    ICY_ERROR(dbi_key.appendf("project_%1"_s, name));
    database_txn_read txn;
    ICY_ERROR(txn.initialize(m_dbase));
    database_dbi dbi;
    ICY_ERROR(dbi.initialize_open_int_key(txn, dbi_key));

    auto key = 0u;
    auto val = dcl_action();
    database_cursor_read cur;
    ICY_ERROR(cur.initialize(txn, m_sys_dbi));
    auto error = cur.get_type_by_type(key, val, database_oper_read::first);
    while (true)
    {
        if (error != database_error_not_found)
        {
            ICY_ERROR(error);
        }
        else
            break;

        ICY_ERROR(exec_action(val));
        error = cur.get_type_by_type(key, val, database_oper_read::next);
    }
    m_usr_dbi = dbi;
    return error_type();
}
error_type dcl_system_data::exec_action(const dcl_action& action) noexcept
{
    switch (action.atype)
    {
    case dcl_action_type::create:
    {
        dcl_base base;
        base.type = action.btype;
        base.parent = action.value.as_index;
        ICY_ERROR(m_base.insert(action.index, base));
        break;
    }
    default:
        break;
    }
    return error_type();
}
error_type dcl_system_data::get_name(const dcl_base& base, string& str) const noexcept
{
    switch (base.type)
    {
    case dcl_type::root:
        ICY_ERROR(to_string("root"_s, str));
        break;
    default:
        break;
    }
    return error_type();
}*/
/*error_type dcl_system_data::bind(gui_data_write_model& model, const gui_node node, const dcl_index index, const bool recursive) noexcept
{
    internal_event event;
    event.type = internal_event_type::add_bind;
    event.bind.index = index;
    event.bind.model = make_shared_from_this(&model);
    event.bind.node = node;
    event.bind.recursive = recursive;
    ICY_ERROR(event_system::post(this, event_type::system_internal, std::move(event)));


    const auto ptr = m_base.try_find(index);
    if (!ptr)
        return make_stdlib_error(std::errc::invalid_argument);

    string str;
    ICY_ERROR(get_name(*ptr, str));
    ICY_ERROR(model.modify(node, gui_node_prop::user, index));
    ICY_ERROR(model.modify(node, gui_node_prop::data, str));
    return error_type();
}*/


/*
switch (action.action)
    {
    case dcl_action_type::rename:
    {
        auto it = m_data.find(action.index);
        if (it == m_data.end())
            return dcl_error_corrupted_data;

        auto& base = *it->value.get();
        ICY_ERROR(base.rename(action));
        break;
    }
    case dcl_action_type::remove:
    {
        auto it = m_data.find(action.index);
        if (it != m_data.end())
            return dcl_error_corrupted_data;

        auto& base = *it->value.get();
        ICY_ERROR(base.remove(action));
        break;
    }

    case dcl_action_type::create_directory:
    case dcl_action_type::create_user:
    case dcl_action_type::create_type:
    case dcl_action_type::create_resource:
    case dcl_action_type::create_list:
    case dcl_action_type::create_bits:
    case dcl_action_type::create_object:
    case dcl_action_type::create_connection:
    {
        auto jt = m_data.find(action.parent);
        if (jt == m_data.end())
            return dcl_error_corrupted_data;

        auto& pbase = *jt->value.get();
        if (pbase.type() != dcl_base_type::directory)
            return dcl_error_corrupted_data;

        auto type = dcl_base_type::none;

       

        auto it = m_data.find(action.index);
        if (it != m_data.end())
        {
            it->value->remove;
        }

        if (jt == m_data.end() || jt->value.type != dcl_base_type::directory)
            return dcl_error_corrupted_data;

        new_base.directory = action.parent;

        if (!m_binds.empty())
        {
            set<dcl_index> parents;
            auto pt = jt;
            while (pt != m_data.end())
            {
                ICY_ERROR(parents.insert(pt->key));
                if (pt->key == dcl_index())
                    break;
                pt = m_data.find(pt->value.directory);
            }
            for (auto&& pair : m_binds)
            {
                pair.value;
            }
            for (auto kt = m_binds.begin(); kt != m_binds.end();)
            {
                if (const auto ptr = parents.try_find(kt->key))
                {

                    auto model = shared_ptr<gui_data_write_model>(kt->value);
                    if (!model)
                    {
                        kt = m_binds.erase(kt);
                    }
                }
                ++kt;
            }
        }


        ICY_ERROR(m_data.insert(action.index, std::move(new_base)));
        action.action = dcl_action_type::remove;
        break;
    }

    case dcl_action_type::modify_resource:
    {
        auto it = m_data.find(action.index);
        if (it == m_data.end())
            return dcl_error_corrupted_data;

        if (it->value.type != dcl_base_type::resource)
            return dcl_error_corrupted_data;

        const auto jt = it->value.binary.find(action.locale);
        if (jt == it->value.binary.end())
        {
            if (action.binary.empty())
                return dcl_error_corrupted_data;

            ICY_ERROR(it->value.binary.insert(action.locale, std::move(action.binary)));
        }
        else
        {
            std::swap(action.binary, jt->value);
            if (jt->value.empty())
                it->value.binary.erase(jt);
        }
        break;
    }

    case dcl_action_type::create_parameter:
    {
        auto it = m_data.find(action.parent);
        if (it == m_data.end())
            return dcl_error_corrupted_data;

        auto& object = it->value;

        const auto jt = object.params.find(action.index);
        if (jt != object.params.end())
            return dcl_error_corrupted_data;

        switch (object.type)
        {
        case dcl_base_type::user:
        {
            if (action.param == dcl_parameter_type::user_access)
                break;
            return dcl_error_corrupted_data;
        }
        case dcl_base_type::resource:
        {
            if (action.param == dcl_parameter_type::resource_type)
                break;
            return dcl_error_corrupted_data;
        }
        case dcl_base_type::list:
        {
            if (action.param == dcl_parameter_type::list_value)
                break;
            return dcl_error_corrupted_data;
        }
        case dcl_base_type::bits:
        {
            if (action.param == dcl_parameter_type::bits_value)
                break;
            return dcl_error_corrupted_data;
        }
        case dcl_base_type::object:
        case dcl_base_type::connection:
        {
            switch (action.param)
            {
            case dcl_parameter_type::object_local_bool:
            case dcl_parameter_type::object_local_int64:
            case dcl_parameter_type::object_local_float:
                break;

            case dcl_parameter_type::object_local_list:
            {
                const auto ptr = m_data.try_find(action.value.as_index);
                if (!ptr)
                    return dcl_error_corrupted_data;
                if (ptr->type != dcl_base_type::list)
                    return dcl_error_corrupted_data;
                break;
            }
            case dcl_parameter_type::object_local_bits:
            {
                const auto ptr = m_data.try_find(action.value.as_index);
                if (!ptr)
                    return dcl_error_corrupted_data;
                if (ptr->type != dcl_base_type::bits)
                    return dcl_error_corrupted_data;
                break;
            }
            case dcl_parameter_type::object_ref_object:
            case dcl_parameter_type::object_local_object:
            {
                const auto ptr = m_data.try_find(action.value.as_index);
                if (!ptr)
                    return dcl_error_corrupted_data;
                if (ptr->type != dcl_base_type::object)
                    return dcl_error_corrupted_data;
                break;
            }
            case dcl_parameter_type::object_local_connection:
            {
                const auto ptr = m_data.try_find(action.value.as_index);
                if (!ptr)
                    return dcl_error_corrupted_data;
                if (ptr->type != dcl_base_type::connection)
                    return dcl_error_corrupted_data;
                break;
            }

            case dcl_parameter_type::object_ref_type:
            {
                const auto ptr = m_data.try_find(action.value.as_index);
                if (!ptr)
                    return dcl_error_corrupted_data;
                if (ptr->type != dcl_base_type::type)
                    return dcl_error_corrupted_data;
                break;
            }
            case dcl_parameter_type::conn_beg:
            case dcl_parameter_type::conn_end:
            {
                if (object.type != dcl_base_type::connection)
                    return dcl_error_corrupted_data;

                const auto ptr = m_data.try_find(action.value.as_index);
                if (!ptr)
                    return dcl_error_corrupted_data;
                if (ptr->type != dcl_base_type::object)
                    return dcl_error_corrupted_data;
                break;
            }
            default:
                return dcl_error_corrupted_data;
            }
        }
        default:
            return dcl_error_corrupted_data;
        }

        dcl_parameter new_param;
        std::swap(new_param.name, action.name);
        std::swap(new_param.type, action.param);
        std::swap(new_param.value, action.value);
        ICY_ERROR(object.params.insert(action.index, std::move(new_param)));
        action.action = dcl_action_type::delete_parameter;
        break;
    }
    case dcl_action_type::modify_parameter:
    {
        auto it = m_data.find(action.parent);
        if (it == m_data.end())
            return dcl_error_corrupted_data;
        auto& object = it->value;

        const auto jt = object.params.find(action.index);
        if (jt == object.params.end())
            return dcl_error_corrupted_data;

        std::swap(action.value, jt->value.value);
        break;
    }
    case dcl_action_type::rename_parameter:
    {
        auto it = m_data.find(action.parent);
        if (it == m_data.end())
            return dcl_error_corrupted_data;
        auto& object = it->value;

        const auto jt = object.params.find(action.index);
        if (jt == object.params.end())
            return dcl_error_corrupted_data;

        std::swap(action.name, jt->value.name);
        break;
    }
    case dcl_action_type::delete_parameter:
    {
        auto it = m_data.find(action.parent);
        if (it == m_data.end())
            return dcl_error_corrupted_data;
        auto& object = it->value;

        const auto jt = object.params.find(action.index);
        if (jt == object.params.end())
            return dcl_error_corrupted_data;

        action.param = jt->value.type;
        action.name = std::move(jt->value.name);
        action.value = jt->value.value;
        action.action = dcl_action_type::create_parameter;
        object.params.erase(jt);
        break;
    }
    default:
        return dcl_error_corrupted_data;
    }
*/

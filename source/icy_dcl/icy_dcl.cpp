#include <icy_engine/core/icy_thread.hpp>
#include <icy_engine/utility/icy_database.hpp>
#include <icy_dcl/icy_dcl.hpp>

using namespace icy;

ICY_STATIC_NAMESPACE_BEG
union dcl_value
{
    dcl_value() noexcept
    {
        memset(this, 0, sizeof(*this));
    }
    dcl_index as_index;
    uint8_t as_bytes[16];
};
struct dcl_action
{
    dcl_action_type atype = dcl_action_type::none;
    dcl_type btype = dcl_type::none;
    dcl_index index;
    dcl_value value;
};
struct dcl_base
{
    dcl_type type = dcl_type::none;
    dcl_index parent;    
    dcl_index name;
};
class dcl_thread_data : public thread
{
public:
    dcl_system* system = nullptr;
    void cancel() noexcept override
    {
        post_quit_event();
    }
    error_type run() noexcept override
    {
        if (auto error = system->exec())
        {
            cancel();
            return error;
        }
        return error_type();
    }
};
class dcl_system_data : dcl_system
{
public:
    dcl_system_data() noexcept = default;
    ~dcl_system_data();
    error_type initialize(const string_view path, const size_t capacity) noexcept;
private:
    error_type exec() noexcept override;
    error_type signal(const icy::event_data* event) noexcept override
    {
        return m_sync.wake();
    }
    const icy::thread& thread() const noexcept override
    {
        return *m_thread;
    }
    error_type bind(gui_data_write_model& model, const gui_node node, const dcl_index index) noexcept override;
    error_type get_projects(array<string>& names) const noexcept;
    error_type add_project(const string_view name) noexcept;
    error_type del_project(const string_view name) noexcept;
    error_type set_project(const string_view name) noexcept;
    error_type exec_action(const dcl_action& action) noexcept;
    error_type get_name(const dcl_base& base, string& str) const noexcept;
private:
    sync_handle m_sync;
    shared_ptr<dcl_thread_data> m_thread;
    database_system_write m_dbase;
    map<dcl_index, dcl_base> m_base;
    database_dbi m_sys_dbi;
    database_dbi m_usr_dbi;
};
ICY_STATIC_NAMESPACE_END

dcl_system_data::~dcl_system_data()
{
    if (m_thread)
        m_thread->wait();
    filter(0);
}
error_type dcl_system_data::initialize(const string_view path, const size_t capacity) noexcept
{
    ICY_ERROR(m_sync.initialize());
    ICY_ERROR(make_shared(m_thread));
    m_thread->system = this;

    ICY_ERROR(m_dbase.initialize(path, capacity));
    database_txn_write txn;
    ICY_ERROR(txn.initialize(m_dbase));
    ICY_ERROR(m_sys_dbi.initialize_create_int_key(txn, "data"_s));

    uint32_t key = 0;
    dcl_action val;
    if (m_sys_dbi.size(txn) == 0)
    {
        database_cursor_write cur;
        ICY_ERROR(cur.initialize(txn, m_sys_dbi));
        val.atype = dcl_action_type::create;
        val.btype = dcl_type::root;
        ICY_ERROR(cur.put_type_by_type(key, database_oper_write::append, val));
    }
    ICY_ERROR(txn.commit());
    ICY_ERROR(set_project(string_view()));

    const uint64_t event_types = event_type::system_internal;
    filter(event_types);

    return error_type();
}
error_type dcl_system_data::bind(gui_data_write_model& model, const gui_node node, const dcl_index index) noexcept
{
    const auto ptr = m_base.try_find(index);
    if (!ptr)
        return make_stdlib_error(std::errc::invalid_argument);

    string str;
    ICY_ERROR(get_name(*ptr, str));
    ICY_ERROR(model.modify(node, gui_node_prop::user, index));
    ICY_ERROR(model.modify(node, gui_node_prop::data, str));
    return error_type();
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
    */
    return error_type();
}
error_type dcl_system_data::add_project(const string_view name) noexcept
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
}
error_type dcl_system_data::exec() noexcept
{
    while (*this)
    {
        while (auto event = pop())
        {

        }
        ICY_ERROR(m_sync.wait());
    }
    return error_type();
}

const event_type icy::event_type_dcl = next_event_user();
error_type icy::create_dcl_system(shared_ptr<dcl_system>& system, const string_view path, const size_t capacity) noexcept
{
    shared_ptr<dcl_system_data> new_system;
    ICY_ERROR(make_shared(new_system));
    ICY_ERROR(new_system->initialize(path, capacity));
    system = std::move(new_system);
    return error_type();
}
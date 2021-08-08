#include <ecsdb/ecsdb_file_system.hpp>
#include <icy_engine/core/icy_json.hpp>

using namespace icy;


error_type ecsdb_file_system::load(const string_view path, const size_t capacity) noexcept
{
    ICY_ERROR(m_file.initialize(path, capacity));
    database_txn_write txn;
    ICY_ERROR(txn.initialize(m_file));
    ICY_ERROR(m_dbi_user.initialize_create_any_key(txn, "user"_s));
    ICY_ERROR(m_dbi_action.initialize_create_any_key(txn, "action"_s));
    ICY_ERROR(m_dbi_folder.initialize_create_any_key(txn, "folder"_s));
    ICY_ERROR(m_dbi_binary.initialize_create_any_key(txn, "binary"_s));
    ICY_ERROR(m_dbi_entity.initialize_create_any_key(txn, "entity"_s));
    return error_type();
}
error_type ecsdb_file_system::save(const string_view path) noexcept
{
    return error_type();
}
error_type ecsdb_file_system::check(const ecsdb_transaction& txn) noexcept
{
    if (!m_file)
        return make_ecsdb_error(ecsdb_error_code::invalid_project);
    database_txn_write write;
    ICY_ERROR(write.initialize(m_file));
    return exec(txn, write);
}
error_type ecsdb_file_system::exec(ecsdb_transaction& txn) noexcept
{
    if (!m_file)
        return make_ecsdb_error(ecsdb_error_code::invalid_project);
    database_txn_write write;
    ICY_ERROR(write.initialize(m_file));
    ICY_ERROR(exec(txn, write));
    database_cursor_write cur;
    ICY_ERROR(cur.initialize(write, m_dbi_action));

    txn.time = time(nullptr);
    txn.index = uint32_t(m_dbi_action.size(write) + 1);

    json json;
    ICY_ERROR(to_json(txn, json));

    string str;
    ICY_ERROR(to_string(json, str));

    array_view<uint8_t> bytes;
    ICY_ERROR(cur.put_var_by_type(txn.index, str.ubytes().size(), database_oper_write::append, bytes));
    memcpy(bytes.data(), str.ubytes().data(), str.ubytes().size());    
    ICY_ERROR(write.commit());
    return error_type();
}
error_type ecsdb_file_system::create_user(const guid& index, const string_view name) noexcept
{
    if (!m_file)
        return make_ecsdb_error(ecsdb_error_code::invalid_project);
    database_txn_write write;
    ICY_ERROR(write.initialize(m_file));

    {
        database_cursor_write cur;
        ICY_ERROR(cur.initialize(write, m_dbi_user));
        ICY_ERROR(cur.put_str_by_type(index, database_oper_write::unique, name));
    }
    ICY_ERROR(write.commit());

    return error_type();
}
error_type ecsdb_file_system::delete_user(const guid& index) noexcept
{
    if (!m_file)
        return make_ecsdb_error(ecsdb_error_code::invalid_project);
    database_txn_write write;
    ICY_ERROR(write.initialize(m_file));
    {
        database_cursor_read cur;
        ICY_ERROR(cur.initialize(write, m_dbi_user));
        auto key = index;
        auto val = string_view();
        if (cur.get_str_by_type(key, val, database_oper_read::none))
            return make_ecsdb_error(ecsdb_error_code::unknown_user);
    }
    {
        database_cursor_write cur;
        ICY_ERROR(cur.initialize(write, m_dbi_user));
        ICY_ERROR(cur.del_by_type(index));
    }
    ICY_ERROR(write.commit());

    return error_type();
}
error_type ecsdb_file_system::rename_user(const guid& index, const string_view name) noexcept
{
    if (!m_file)
        return make_ecsdb_error(ecsdb_error_code::invalid_project);
    database_txn_write write;
    ICY_ERROR(write.initialize(m_file));
    {
        database_cursor_read cur;
        ICY_ERROR(cur.initialize(write, m_dbi_user));
        auto key = index;
        auto val = string_view();
        if (cur.get_str_by_type(key, val, database_oper_read::none))
            return make_ecsdb_error(ecsdb_error_code::unknown_user);
    }
    {
        database_cursor_write cur;
        ICY_ERROR(cur.initialize(write, m_dbi_user));
        ICY_ERROR(cur.put_str_by_type(index, database_oper_write::none, name));
    }
    ICY_ERROR(write.commit());

    return error_type();
}
error_type ecsdb_file_system::exec(const ecsdb_transaction& txn, icy::database_txn_write& write) noexcept
{
    {
        database_cursor_read cur_user;
        ICY_ERROR(cur_user.initialize(write, m_dbi_user));
        auto key = txn.user;
        auto val = string_view();
        if (cur_user.get_str_by_type(key, val, database_oper_read::none))
            return make_ecsdb_error(ecsdb_error_code::unknown_user);
    }
    database_cursor_write cur_folder;
    database_cursor_write cur_binary;
    database_cursor_write cur_entity;
    ICY_ERROR(cur_folder.initialize(write, m_dbi_folder));
    ICY_ERROR(cur_binary.initialize(write, m_dbi_binary));
    ICY_ERROR(cur_entity.initialize(write, m_dbi_entity));

    for (auto&& action : txn.actions)
    {
        auto key = action.index;
        auto val = string_view();
        if (cur_binary.get_str_by_type(key, val, database_oper_read::none) == error_type()) return make_ecsdb_error(ecsdb_error_code::index_already_exists);
        if (cur_folder.get_str_by_type(key, val, database_oper_read::none) == error_type()) return make_ecsdb_error(ecsdb_error_code::index_already_exists);
        if (cur_entity.get_str_by_type(key, val, database_oper_read::none) == error_type()) return make_ecsdb_error(ecsdb_error_code::index_already_exists);

        switch (action.action)
        {
        case ecsdb_action_type::create_folder:
        case ecsdb_action_type::create_entity:
        case ecsdb_action_type::move_folder:
        case ecsdb_action_type::move_entity:
        {
            if (action.folder != ecsdb_root)
            {
                key = action.folder;
                if (cur_folder.get_str_by_type(key, val, database_oper_read::none))
                    return make_ecsdb_error(ecsdb_error_code::folder_not_found);
            }
            break;
        }
        }

        switch (action.action)
        {
        case ecsdb_action_type::create_folder:
        {
            ecsdb_folder new_folder;
            new_folder.folder = action.folder;
            ICY_ERROR(copy(action.name, new_folder.name));
            json json;
            ICY_ERROR(to_json(new_folder, json));
            string str;
            ICY_ERROR(to_string(json, str));
            ICY_ERROR(cur_folder.put_str_by_type(action.index, database_oper_write::unique, str));
            break;
        }

        default:
            return make_ecsdb_error(ecsdb_error_code::unknown_action_type);
        }
    }
    return error_type();
}
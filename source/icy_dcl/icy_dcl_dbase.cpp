#include "icy_dcl_dbase.hpp"

using namespace icy;

static error_type query_locales(database_cursor_read& cursor_main,
    database_cursor_read& cursor_link, const dcl_base& base, map<dcl_index, database_dbi>& output)
{
    dcl_index key = base.index;
    const_array_view<uint8_t> val;
    if (const auto error = cursor_link.get_var_by_type(key, val, database_oper_read::none))
    {
        if (error == database_error_not_found)
            ;
        else
            return error;
    }
    const auto links = const_array_view<dcl_index>(
        reinterpret_cast<const dcl_index*>(val.data()), val.size() / sizeof(dcl_index));
    for (auto&& link : links)
    {
        dcl_index child_key = link;
        dcl_base_val child_val;
        ICY_ERROR(cursor_main.get_type_by_type(child_key, child_val, database_oper_read::none));
        if (child_val.type == dcl_attribute)
        {
            if (child_val.value.index == dcl_attribute_deleted)
                return {};
        }
    }
    
    if (base.type == dcl_directory)
    {
        for (auto&& link : links)
        {
            dcl_index child_key = link;
            dcl_base_val child_val;
            ICY_ERROR(cursor_main.get_type_by_type(child_key, child_val, database_oper_read::none));
            dcl_base child_base;
            child_base.index = link;
            child_base.parent = child_val.parent;
            child_base.type = child_val.type;
            child_base.value = child_val.value;
            ICY_ERROR(query_locales(cursor_main, cursor_link, child_base, output));
        }
    }
    else if (base.type == dcl_locale)
    {
        ICY_ERROR(output.insert(std::move(key), database_dbi()));
    }
    return {};
}

error_type dcl_database::open(const string_view path, const size_t size) noexcept
{
    ICY_ERROR(m_lock.initialize());

    ICY_ERROR(m_base.initialize(path, size));
    database_txn_write txn;
    ICY_ERROR(txn.initialize(m_base));
    ICY_ERROR(m_data_main.initialize_create_any_key(txn, "data_main"_s));
    ICY_ERROR(m_data_link.initialize_create_any_key(txn, "data_link"_s));
    ICY_ERROR(m_log_main.initialize_create_any_key(txn, "log_main"_s));
    ICY_ERROR(m_log_user.initialize_create_any_key(txn, "log_user"_s));
    ICY_ERROR(m_log_time.initialize_create_int_key(txn, "log_time"_s));
    ICY_ERROR(m_log_data.initialize_create_any_key(txn, "log_data"_s));

    database_cursor_write cursor_main;
    database_cursor_write cursor_link;
    ICY_ERROR(cursor_main.initialize(txn, m_data_main));
    ICY_ERROR(cursor_link.initialize(txn, m_data_link));

    dcl_base root_locales;
    root_locales.index = dcl_root_locales;
    root_locales.parent = dcl_root;
    root_locales.type = dcl_directory;
    root_locales.value.index = dcl_locale;
    ICY_ERROR(query_locales(cursor_main, cursor_link, root_locales, m_binary));
    ICY_ERROR(m_binary.find_or_insert(dcl_index(dcl_locale_default), database_dbi()));

    for (auto&& pair : m_binary)
    {
        string locale_str;
        ICY_ERROR(locale_str.appendf("binary_%1"_s, pair.key));
        ICY_ERROR(pair.value.initialize_create_any_key(txn, locale_str));
    }

    dcl_base_val root;
    root.type = dcl_directory;
    root.value.index = dcl_root;
    ICY_ERROR(cursor_main.put_type_by_type(dcl_root, database_oper_write::none, root));
    root.parent = dcl_root;

    root.value.index = dcl_locale;
    ICY_ERROR(cursor_main.put_type_by_type(dcl_root_locales, database_oper_write::none, root));
    root.value.index = dcl_role;
    ICY_ERROR(cursor_main.put_type_by_type(dcl_root_roles, database_oper_write::none, root));
    root.value.index = dcl_user;
    ICY_ERROR(cursor_main.put_type_by_type(dcl_root_users, database_oper_write::none, root));
    root.value.index = dcl_type;
    ICY_ERROR(cursor_main.put_type_by_type(dcl_root_types, database_oper_write::none, root));
    root.value.index = dcl_object;
    ICY_ERROR(cursor_main.put_type_by_type(dcl_root_objects, database_oper_write::none, root));

    struct
    {
        dcl_index indices[5] =
        {
            dcl_root_locales,
            dcl_root_roles,
            dcl_root_users,
            dcl_root_types,
            dcl_root_objects,
        };
    } root_children;
    ICY_ERROR(cursor_link.put_type_by_type(dcl_root, database_oper_write::none, root_children));

    dcl_base_val locale;
    locale.parent = dcl_root_locales;
    locale.type = dcl_locale;
    if (const auto error = cursor_main.put_type_by_type(dcl_locale_default, database_oper_write::unique, locale))
    {
        if (error == database_error_key_exist)
            ;
        else
            return error;
    }
    else
    {
        ICY_ERROR(cursor_link.put_type_by_type(dcl_root_locales, database_oper_write::unique, dcl_locale_default));
    }

    database_cursor_write binary;
    ICY_ERROR(binary.initialize(txn, *m_binary.try_find(dcl_locale_default)));

    const auto func = [&binary](const dcl_index index, const string_view name) -> error_type
    {
        if (const auto error = binary.put_str_by_type(index, database_oper_write::unique, name))
        {
            if (error == database_error_key_exist)
                ;
            else
                return error;
        }
        return {};
    };
    ICY_ERROR(func(dcl_root, "Root"_s));
    ICY_ERROR(func(dcl_root_locales, "Locales"_s));
    ICY_ERROR(func(dcl_root_roles, "Roles"_s));
    ICY_ERROR(func(dcl_root_users, "Users"_s));
    ICY_ERROR(func(dcl_root_types, "Types"_s));
    ICY_ERROR(func(dcl_root_objects, "Objects"_s));
    ICY_ERROR(func(dcl_locale_default, "enUS"_s));


    ICY_ERROR(txn.commit());
    return {};
}
error_type dcl_read_txn::initialize(const dcl_database& dbase) noexcept 
{
    database_txn_read txn;
    ICY_ERROR(txn.initialize(dbase.m_base));
    ICY_ERROR(m_data_main.initialize(txn, dbase.m_data_main));
    ICY_ERROR(m_data_link.initialize(txn, dbase.m_data_link));
    ICY_ERROR(m_log_main.initialize(txn, dbase.m_log_main));
    ICY_ERROR(m_log_user.initialize(txn, dbase.m_log_user));
    ICY_ERROR(m_log_time.initialize(txn, dbase.m_log_time));
    ICY_ERROR(m_log_data.initialize(txn, dbase.m_log_data));
    {
        ICY_LOCK_GUARD(dbase.m_lock);
        for (auto&& pair : dbase.m_binary)
        {
            database_cursor_read binary;
            ICY_ERROR(binary.initialize(txn, pair.value));
            auto key = pair.key;
            ICY_ERROR(m_binary.insert(std::move(key), std::move(binary)));
        }
    }
    m_txn = std::move(txn);
    return {};
}
error_type dcl_read_txn::data_main(const dcl_index& index, dcl_base_val& val) noexcept
{
    dcl_index key = index;
    ICY_ERROR(m_data_main.get_type_by_type(key, val, database_oper_read::none));
    return {};
}
error_type dcl_read_txn::data_link(const dcl_index& index, const_array_view<dcl_index>& indices) noexcept
{
    dcl_index key = index;
    const_array_view<uint8_t> bytes;
    if (const auto error = m_data_link.get_var_by_type(key, bytes, database_oper_read::none))
    {
        if (error == database_error_not_found)
            ;
        else
            return error;
    }
    indices = { reinterpret_cast<const dcl_index*>(bytes.data()), bytes.size() / sizeof(dcl_index) };
    return {};
}
error_type dcl_read_txn::binary(const dcl_index& index, const dcl_index& locale, const_array_view<uint8_t>& bytes) noexcept
{
    const auto it = m_binary.find(locale);
    if (it == m_binary.end())
        return database_error_not_found;

    dcl_index key = index;
    if (const auto error = it->value.get_var_by_type(key, bytes, database_oper_read::none))
    {
        if (error == database_error_not_found)
            ;
        else
            return error;
    }
    return {};    
}
error_type dcl_read_txn::text(const dcl_index& index, const dcl_index& locale, string_view& str) noexcept
{
    const_array_view<uint8_t> bytes;
    ICY_ERROR(binary(index, locale, bytes));
    str = { reinterpret_cast<const char*>(bytes.data()), bytes.size() };
    return {};
}
error_type dcl_read_txn::flags(const dcl_index& index, dcl_flag& flags) noexcept
{
    flags = dcl_flag::none;
    const_array_view<dcl_index> indices;
    ICY_ERROR(data_link(index, indices));
    for (auto&& index : indices)
    {
        dcl_base_val child;
        ICY_ERROR(data_main(index, child));
        if (child.parent != index)
            continue;

        if (child.type == dcl_attribute)
        {
            if (child.value.index == dcl_attribute_const)
                flags = flags | dcl_flag::is_const;
            else if (child.value.index == dcl_attribute_hidden)
                flags = flags | dcl_flag::is_hidden;
            if (child.value.index == dcl_attribute_deleted)
                flags = flags | dcl_flag::is_deleted;
        }
    }
    return {};
}
error_type dcl_read_txn::locales(const dcl_index& locale, map<dcl_index, string_view>& map) noexcept
{
    map.clear();
    for (auto&& pair : m_binary)
    {
        string_view str;
        ICY_ERROR(text(pair.key, locale, str));
        ICY_ERROR(map.insert(dcl_index(pair.key), std::move(str)));
    }
    return {};
}

/*
error_type dcl_database::open(const string_view path, const size_t size) noexcept
{

        
    database_cursor_write cursor;
    ICY_ERROR(cursor.initialize(txn, m_data));
    
    auto key = dcl_root;
    auto val = const_array_view<uint8_t>();
    if (const auto error = cursor.get_var_by_type(key, val, database_oper_read::none))
    {
        if (error != database_error_not_found)
            return error;

        const dcl_index root[] = 
        {
            dcl_index(), dcl_type_directory, 
            dcl_root_locales, 
            dcl_root_accesses, 
            dcl_root_roles, 
            dcl_root_users, 
            dcl_root_types, 
            dcl_root_objects 
        };
        ICY_ERROR(cursor.put_type_by_type(dcl_root, database_oper_write::unique, root));
        const dcl_index root_null[] = { dcl_root, dcl_type_directory };
        const dcl_index root_locales[] = { dcl_root, dcl_type_directory, dcl_locale_default };
        
       
        dcl_index root_accesses[2 + _countof(accesses)] =
        {
            dcl_root, dcl_type_directory,
        };
        for (auto k = 0u; k < _countof(accesses); ++k)
            root_accesses[2 + k] = accesses[k].first;
        
        ICY_ERROR(cursor.put_type_by_type(dcl_root_locales, database_oper_write::unique, root_locales));
        ICY_ERROR(cursor.put_type_by_type(dcl_root_accesses, database_oper_write::unique, root_accesses));
        ICY_ERROR(cursor.put_type_by_type(dcl_root_roles, database_oper_write::unique, root_null));
        ICY_ERROR(cursor.put_type_by_type(dcl_root_users, database_oper_write::unique, root_null));
        ICY_ERROR(cursor.put_type_by_type(dcl_root_types, database_oper_write::unique, root_null));
        ICY_ERROR(cursor.put_type_by_type(dcl_root_objects, database_oper_write::unique, root_null));

        string locale_str;
        database_dbi dbi_binary;
        database_cursor_write binary;
        ICY_ERROR(locale_str.appendf("binary_%1"_s, dcl_locale_default));
        ICY_ERROR(dbi_binary.initialize_create_any_key(txn, locale_str));
        ICY_ERROR(binary.initialize(txn, dbi_binary));
        ICY_ERROR(m_binary.insert(dcl_index(dcl_locale_default), std::move(dbi_binary)));
        ICY_ERROR(binary.put_str_by_type(dcl_root, database_oper_write::unique, "root"_s));
        ICY_ERROR(binary.put_str_by_type(dcl_root_locales, database_oper_write::unique, "locales"_s));
        ICY_ERROR(binary.put_str_by_type(dcl_root_accesses, database_oper_write::unique, "accesses"_s));
        ICY_ERROR(binary.put_str_by_type(dcl_root_roles, database_oper_write::unique, "roles"_s));
        ICY_ERROR(binary.put_str_by_type(dcl_root_users, database_oper_write::unique, "users"_s));
        ICY_ERROR(binary.put_str_by_type(dcl_root_types, database_oper_write::unique, "types"_s));
        ICY_ERROR(binary.put_str_by_type(dcl_root_objects, database_oper_write::unique, "objects"_s));

        for (auto&& pair : accesses)
        {
            const dcl_index access[] = { dcl_root_accesses, dcl_type_access };
            ICY_ERROR(cursor.put_type_by_type(pair.first, database_oper_write::unique, access));
            ICY_ERROR(binary.put_str_by_type(pair.first, database_oper_write::unique, pair.second));
        }        
        ICY_ERROR(txn.commit());
        return {};
    }

    ICY_ERROR(query_locales(cursor, dcl_root_locales, m_binary));
    std::sort(m_binary.begin(), m_binary.end());
    if (!std::binary_search(m_binary.begin(), m_binary.end(), dcl_locale_default))
        return make_dcl_error(dcl_error_code::database_corrupted);
    
    for (auto&& pair : m_binary)
    {
    }
    ICY_ERROR(txn.commit());
    return {};
}
error_type dcl_read_txn::initialize(dcl_database& base) noexcept
{
   

}
error_type dcl_write_txn::initialize() noexcept
{
    {
        database_txn_read txn;
        ICY_ERROR(txn.initialize(m_dbase.m_dbase));
        database_cursor_read cur_usr;
        ICY_ERROR(cur_usr.initialize(txn, m_dbase.m_usr));
        auto key_user = m_user;
        dcl_dbase_user val_user;
        if (const auto error = m_cur_usr.get_type_by_type(key_user, val_user, database_oper_read::none))
        {
            if (error == database_error_not_found)
                return make_dcl_error(dcl_error_code::user_already_exists);
            return error;
        }
        if (val_user.role == dcl_role_admin)
            m_access = access(access_data | access_type | access_user);
        else if (val_user.role == dcl_role_edit_type)
            m_access = access(access_data | access_type);
        else if (val_user.role == dcl_role_edit_data)
            m_access = access(access_data);
        else
            return make_dcl_error(dcl_error_code::access_denied);
    }
    database_txn_write txn;
    ICY_ERROR(txn.initialize(m_dbase.m_dbase));

    if (m_access & access_user)
    {
        ICY_ERROR(m_cur_usr.initialize(txn, m_dbase.m_usr));
    }
    if (m_access & access_type)
    {
        ICY_ERROR(m_cur_typ.initialize(txn, m_dbase.m_typ));
    }
    if (m_access & access_data)
    {
        ICY_ERROR(m_cur_obj.initialize(txn, m_dbase.m_obj));
    }
    for (auto&& pair : m_dbase.m_bin)
    {
        database_cursor_write cur_bin;
        ICY_ERROR(cur_bin.initialize(txn, pair.value));
        ICY_ERROR(m_cur_bin.insert(uint32_t(pair.key), std::move(cur_bin)));
    }
    ICY_ERROR(m_cur_log.initialize(txn, m_dbase.m_log));
    m_version = m_dbase.m_log.size(txn) + 1;
    m_txn = std::move(txn);
    return {};
}
error_type dcl_write_txn::append(dcl_record& record, const string_view name, const const_array_view<uint8_t> bytes) noexcept
{
    if (record.flags & dcl_record_flag::user)
    {
        if (!(m_access & access::access_user))
            return make_dcl_error(dcl_error_code::access_denied);
    }
    else if (record.flags & dcl_record_flag::type)
    {
        if (!(m_access & access::access_type))
            return make_dcl_error(dcl_error_code::access_denied);
    }
    else if (record.flags & dcl_record_flag::data)
    {
        if (!(m_access & access::access_data))
            return make_dcl_error(dcl_error_code::access_denied);
    }
    else
    {
        return make_dcl_error(dcl_error_code::invalid_record_flags);
    }

    if ((record.flags & dcl_record_flag::create) && record.locale)
        return make_dcl_error(dcl_error_code::invalid_locale);

    const auto it = m_cur_bin.find(record.locale);
    if (it == m_cur_bin.end())
        return make_dcl_error(dcl_error_code::invalid_locale);
    const auto binary = &it->value;

    if (record.flags & (dcl_record_flag::create | dcl_record_flag::rename))
    {
        if (name.empty())
            return make_dcl_error(dcl_error_code::name_empty);
        if (name.bytes().size() > dcl_max_string)
            return make_dcl_error(dcl_error_code::name_length);
    }

    if (record.flags & dcl_record_flag::user)
    {
        if (record.flags & dcl_record_flag::create)
        {
            dcl_dbase_user user;
            user.create = m_version;
            user.modify = m_version;
            if (record.value_user == dcl_role_admin)
            {
                if (m_user != dcl_index{})
                    return make_dcl_error(dcl_error_code::access_denied);
            }
            else if (
                record.value_user == dcl_role_edit_data ||
                record.value_user == dcl_role_edit_type ||
                record.value_user == dcl_role_view)
                ;
            else
                return make_dcl_error(dcl_error_code::invalid_user_role);

            if (record.index == dcl_index{})
                return make_dcl_error(dcl_error_code::access_denied);

            if (const auto error = m_cur_usr.put_type_by_type(record.index, database_oper_write::unique, user))
            {
                if (error == database_error_key_exist)
                    return make_dcl_error(dcl_error_code::user_already_exists);
                return error;
            }
        }
        else if (record.flags & (dcl_record_flag::remove | dcl_record_flag::modify | dcl_record_flag::rename))
        {
            if (record.flags & dcl_record_flag::remove)
            {
                if (m_user != dcl_index{})
                    return make_dcl_error(dcl_error_code::access_denied);
            }
            else if (record.flags & dcl_record_flag::modify)
            {
                if (record.value_user == dcl_role_edit_data ||
                    record.value_user == dcl_role_edit_type ||
                    record.value_user == dcl_role_view)
                {
                    ;
                }
                else if (record.value_user == dcl_role_admin)
                {
                    if (m_user != dcl_index{})
                        return make_dcl_error(dcl_error_code::access_denied);
                }
                else
                {
                    return make_dcl_error(dcl_error_code::invalid_user_role);
                }
            }

            auto key = record.index;
            dcl_dbase_user val;
            if (const auto error = m_cur_usr.get_type_by_type(key, val, database_oper_read::none))
            {
                if (error == database_error_not_found)
                    return make_dcl_error(dcl_error_code::user_not_found);
                return error;
            }
            val.modify = m_version;
            if (record.flags & dcl_record_flag::remove)
            {
                val.role = {};
            }
            else if (record.flags & dcl_record_flag::modify)
            {
                val.role = record.value_user;
            }
            else
            {
                guid prev_index = record.index;
                guid next_index;
                ICY_ERROR(create_guid(next_index));

                string_view str;
                if (const auto error = binary->get_str_by_type(prev_index, str, database_oper_read::none))
                {
                    if (error == database_error_not_found)
                        ;
                    else
                        return error;
                }

                //  copy
                ICY_ERROR(binary->put_str_by_type(next_index, database_oper_write::unique, str));
                ICY_ERROR(binary->put_str_by_type(prev_index, database_oper_write::none, name));
                record.value_user = next_index;
            }

            ICY_ERROR(m_cur_usr.put_type_by_type(key, database_oper_write::none, val));
        }
        else if (record.flags & dcl_record_flag::rename)
        {
            auto key = record.index;
            dcl_dbase_user val;
            if (const auto error = m_cur_usr.get_type_by_type(key, val, database_oper_read::none))
            {
                if (error == database_error_not_found)
                    return make_dcl_error(dcl_error_code::user_not_found);
                return error;
            }
        }
        else
        {
            return make_dcl_error(dcl_error_code::invalid_record_flags);
        }
    }
    else if (record.flags & dcl_record_flag::type)
    {

    }


    record.version = m_version;
    ICY_ERROR(m_cur_log.put_type_by_type(m_version, database_oper_write::append, record));
    m_version += 1;
    return {};
}

*/
/*
 const std::pair<dcl_index, string_view> accesses[] =
        {
            { dcl_access_create_locale, "Locale: create"_s },
            { dcl_access_name_locale, "Locale: edit name"_s },
            { dcl_access_note_locale, "Locale: edit note"_s },
            { dcl_access_delete_locale, "Locale: delete"_s },

            { dcl_access_create_role, "Role: create"_s },
            { dcl_access_name_role, "Role: edit name"_s },
            { dcl_access_note_role, "Role: edit note"_s },
            { dcl_access_delete_role, "Role: delete"_s },

            { dcl_access_create_user, "User: create"_s },
            { dcl_access_name_user, "User: edit name"_s },
            { dcl_access_note_user, "User: edit note"_s },
            { dcl_access_edit_user, "User: edit role"_s },
            { dcl_access_delete_user, "User: delete"_s },

            { dcl_access_create_type, "Type: create"_s },
            { dcl_access_name_type, "Type: edit name"_s },
            { dcl_access_note_type, "Type: edit note"_s },
            { dcl_access_edit_type, "Type: edit params"_s },
            { dcl_access_delete_type, "Type: delete"_s },

            { dcl_access_create_object, "Object: create"_s },
            { dcl_access_name_object, "Object: edit name"_s },
            { dcl_access_note_object, "Object: edit note"_s },
            { dcl_access_edit_object, "Object: edit params"_s },
            { dcl_access_delete_object, "Object: delete"_s },
        };

*/
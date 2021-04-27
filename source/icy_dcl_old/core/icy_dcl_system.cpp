#include "icy_dcl_system.hpp"
#include <icy_engine/core/icy_file.hpp>

using namespace icy;

struct icy::dcl_dbase
{
    static error_type create(const const_array_view<uint8_t> bytes, dcl_dbase& val, 
        array<dcl_index>* links) noexcept
    {
        if (bytes.size() < sizeof(dcl_dbase))
            return make_dcl_error(dcl_error::database_corrupted);

        if ((bytes.size() - sizeof(dcl_dbase)) % sizeof(dcl_index) != 0)
            return make_dcl_error(dcl_error::database_corrupted);

        val = *reinterpret_cast<const dcl_dbase*>(bytes.data());
        if (links)
        {
            const auto ptr = reinterpret_cast<const dcl_index*>(bytes.data() + sizeof(dcl_dbase));
            const auto len = (bytes.size() - sizeof(dcl_dbase)) / sizeof(dcl_index);
            ICY_ERROR(links->assign(ptr, ptr + len));
        }
        return error_type();
    }
    dcl_dbase() noexcept = default;
    dcl_dbase(const dcl_type type, const dcl_index parent) noexcept : type(type), parent(parent)
    {

    }
    dcl_dbase(const dcl_base& base) noexcept : type(base.type), flag(base.flag),
        locale(base.locale), parent(base.parent), value(base.value)
    {

    }
    explicit operator bool() const noexcept
    {
        return type != dcl_type::none;
    }
    dcl_base base(const dcl_index index) const noexcept
    {
        dcl_base val;
        val.index = index;
        val.flag = flag;
        val.locale = locale;
        val.parent = parent;
        val.type = type;
        val.value = value;
        return val;
    }
    dcl_flag flag = dcl_flag::none;
    dcl_type type = dcl_type::none;
    uint32_t locale = 0;
    dcl_index parent;
    dcl_value value;
};

static const auto dbi_name_data = "data.2"_s;
static const auto dbi_name_binary = "binary.2"_s;

static error_type dcl_dbase_write(database_cursor_write& binary, const dcl_index index,
    const const_array_view<uint8_t> bytes, const database_oper_write flag) noexcept
{
    if (binary)
    {
        array_view<uint8_t> val;
        ICY_ERROR(binary.put_var_by_type(index, bytes.size(), flag, val));
        memcpy(val.data(), bytes.data(), val.size());
    }
    return error_type();
};


error_type dcl_system::load(const string_view filename, const size_t size) noexcept
{
    ICY_ERROR(m_base.initialize(filename, size));
    database_txn_write txn;
    ICY_ERROR(txn.initialize(m_base));
    ICY_ERROR(m_data.initialize_create_any_key(txn, dbi_name_data));
    ICY_ERROR(m_binary.initialize_create_any_key(txn, dbi_name_binary));
    ICY_ERROR(m_log[log_type::log_by_base].initialize_create_any_key(txn, "log.base"_s));
    ICY_ERROR(m_log[log_type::log_by_index].initialize_create_any_key(txn, "log.index"_s));
    ICY_ERROR(m_log[log_type::log_by_time].initialize_create_int_key(txn, "log.time"_s));
    ICY_ERROR(m_log[log_type::log_by_user].initialize_create_any_key(txn, "log.user"_s));

    database_cursor_write data;
    ICY_ERROR(data.initialize(txn, m_data));
    dcl_dbase root(dcl_type::directory, dcl_index()); 
    if (const auto error = data.put_type_by_type(dcl_root, database_oper_write::unique, root))
    {
        if (error == database_error_key_exist)
            ;
        else
            return error;
    }
    ICY_ERROR(txn.commit());
    return {};
}
error_type dcl_system::cache(string& str) const noexcept
{
    string str_path;
    ICY_ERROR(m_base.path(str_path));
    auto fname = file_name(str_path);
    ICY_ERROR(to_string("%1%2.cache"_s, str, fname.directory, fname.name));
    return {};
}
error_type dcl_system::save(const string_view filename, const dcl_system_save_mode mode) const noexcept
{
    return make_stdlib_error(std::errc::function_not_supported);
}

error_type dcl_writer::initialize(const shared_ptr<dcl_system> system, const string_view path) noexcept
{
    if (!system)
        return make_stdlib_error(std::errc::invalid_argument);

    *this = {};
    m_system = system;
    ICY_ERROR(m_txn.initialize(system->m_base));
    ICY_ERROR(m_data.initialize(m_txn, system->m_data));
    ICY_ERROR(m_binary.initialize(m_txn, system->m_binary));
    for (auto k = 0u; k < dcl_system::log_count; ++k)
        ICY_ERROR(m_log[k].initialize(m_txn, system->m_log[k]));
    
    if (!path.empty())
    {
        ICY_ERROR(m_cache_base.initialize(path, m_system->m_base.size()));
        database_txn_write txn;
        ICY_ERROR(txn.initialize(m_cache_base));
        ICY_ERROR(m_cache_dbi.initialize_create_any_key(txn, "actions"_s));
        database_cursor_read binary;
        ICY_ERROR(binary.initialize(txn, m_cache_dbi));
        dcl_index key_binary;
        auto oper = database_oper_read::first;

        auto action = 0_z;
        array<dcl_action> actions;

        while (true)
        {
            const_array_view<uint8_t> bytes;
            if (const auto error = binary.get_var_by_type(key_binary, bytes, oper))
            {
                if (error == database_error_not_found)
                    break;
                else if (error == database_error_invalid_size)
                    return make_dcl_error(dcl_error::database_corrupted);
                return error;
            }
            oper = database_oper_read::next;
            if (key_binary == dcl_root)
            {
                if (bytes.size() % sizeof(dcl_action) != 0)
                    return make_dcl_error(dcl_error::database_corrupted);

                const auto as_action = reinterpret_cast<const dcl_action*>(bytes.data());
                const auto count = bytes.size() / sizeof(dcl_action);
                if (count)
                {
                    action = as_action[0].values[0].decimal;
                    ICY_ERROR(actions.assign(as_action + 1, as_action + count));
                }
                continue;
            }
            array_view<uint8_t> write_bytes;
            ICY_ERROR(m_binary.put_var_by_type(key_binary, bytes.size(), database_oper_write::none, write_bytes));
            memcpy(write_bytes.data(), bytes.data(), bytes.size());
        }
        ICY_ERROR(txn.commit());

        for (auto&& action : actions)
            ICY_ERROR(append(action));

        while (m_action > action)
            ICY_ERROR(undo());
    }

    return {};
}

error_type dcl_writer::find_type(const dcl_type type, array<dcl_base>& vec) const noexcept
{
    auto oper = database_oper_read::first;
    while (true)
    {
        dcl_index key;
        const_array_view<uint8_t> bytes;
        if (const auto error = m_data.get_var_by_type(key, bytes, oper))
        {
            if (error == database_error_not_found)
                break;
            return error;
        }
        oper = database_oper_read::next;
        if (bytes.size() < sizeof(dcl_dbase))
            return make_dcl_error(dcl_error::database_corrupted);
        const auto ptr = reinterpret_cast<const dcl_dbase*>(bytes.data());
        if (ptr->type == type)
            ICY_ERROR(vec.push_back(ptr->base(key)));
    }
    return {};
}
error_type dcl_writer::find_base(dcl_index key, dcl_dbase& val, array<dcl_index>* const links) const noexcept
{
    const_array_view<uint8_t> bytes;
    if (const auto error = m_data.get_var_by_type(key, bytes, database_oper_read::none))
    {
        if (error != database_error_not_found)
            return error;
        return error_type();
    }
    ICY_ERROR(dcl_dbase::create(bytes, val, links));
    return {};
}
error_type dcl_writer::find_base(const dcl_index index, dcl_base& base) const noexcept
{
    dcl_dbase val(dcl_type::none, dcl_index());
    ICY_ERROR(find_base(index, val));
    base = val ? val.base(index) : dcl_base();
    return {};
}
error_type dcl_writer::find_name(const dcl_index index, const dcl_index locale, string_view& text) const noexcept
{
    dcl_base locale_base;
    if (locale != dcl_index())
    {
        ICY_ERROR(find_base(locale, locale_base));
        if (locale_base.type != dcl_type::locale)
            return make_dcl_error(dcl_error::invalid_locale);
    }

    array<dcl_base> links;
    ICY_ERROR(find_children(index, links));
    
    dcl_index ref_default;
    for (auto&& base : links)
    {
        if (base.type != dcl_type::name)
            continue;
        
        if (base.locale == locale_base.locale)
        {
            ICY_ERROR(find_text(base.value.reference, text));
            if (!text.empty())
                return error_type();
        }
        if (base.locale == 0)
            ref_default = base.value.reference;        
    }
    ICY_ERROR(find_text(ref_default, text));

    if (index == dcl_root && text.empty())
        text = "Root"_s;

    return error_type();
}
error_type dcl_writer::find_children(const dcl_index index, array<dcl_base>& vec) const noexcept
{
    array<dcl_base> links;
    ICY_ERROR(find_links(index, links));
    for (auto&& link : links)
    {
        if (link.parent == index)
            ICY_ERROR(vec.push_back(link));
    }
    return {};
}
error_type dcl_writer::find_links(const dcl_index index, array<dcl_base>& vec) const noexcept
{
    vec.clear();

    dcl_dbase val;
    array<dcl_index> links;
    ICY_ERROR(find_base(index, val, &links));

    for (auto&& link : links)
    {
        ICY_ERROR(find_base(link, val));
        ICY_ERROR(vec.push_back(val.base(link)));
    }
    return {};
}
error_type dcl_writer::find_binary(const dcl_index index, const_array_view<uint8_t>& binary) const noexcept
{
    auto key = index;
    if (const auto error = m_binary.get_var_by_type(key, binary, database_oper_read::none))
    {
        if (error != database_error_not_found)
            return error;
    }
    return {};
}
error_type dcl_writer::find_text(const dcl_index index, string_view& text) const noexcept
{
    text = {};
    auto key = index;
    array_view<uint8_t> bytes;
    if (const auto error = m_binary.get_var_by_type(key, bytes, database_oper_read::none))
    {
        if (error != database_error_not_found)
            return error;
    }
    text = string_view(reinterpret_cast<const char*>(bytes.data()), bytes.size());
    return {};
}
error_type dcl_writer::enumerate_logs(array<dcl_index>& indices) const noexcept
{
    auto key = dcl_root;
    array<uint8_t> bytes;
    ICY_ERROR(m_log[dcl_system::log_by_index].get_var_by_type(key, bytes, database_oper_read::none));
    if (bytes.size() % sizeof(dcl_index))
        return make_dcl_error(dcl_error::database_corrupted);
    
    const auto ptr = reinterpret_cast<const dcl_index*>(bytes.data());
    const auto len = bytes.size() / sizeof(dcl_index);
    ICY_ERROR(indices.assign(ptr, ptr + len));
    return {};
}
error_type dcl_writer::find_logs_by_time(const dcl_time lower, const dcl_time upper, array<dcl_index>& vec) const noexcept
{
    if (upper < lower)
        return make_stdlib_error(std::errc::invalid_argument);

    auto key = lower;
    while (true)
    {
        array<uint8_t> bytes;
        ICY_ERROR(m_log[dcl_system::log_by_time].get_var_by_type(key, bytes, key == lower ?
            database_oper_read::range : database_oper_read::next));
        if (key > upper)
            break;
        if (bytes.size() % sizeof(dcl_index))
            return make_dcl_error(dcl_error::database_corrupted);

        const auto ptr = reinterpret_cast<const dcl_index*>(bytes.data());
        const auto len = bytes.size() / sizeof(dcl_index);
        ICY_ERROR(vec.append(ptr, ptr + len));
    }
    return {};
}
error_type dcl_writer::find_logs_by_user(const dcl_index user, array<dcl_index>& vec) const noexcept
{
    auto key = user;
    array<uint8_t> bytes;
    ICY_ERROR(m_log[dcl_system::log_by_user].get_var_by_type(key, bytes, database_oper_read::none));
    if (bytes.size() % sizeof(dcl_index))
        return make_dcl_error(dcl_error::database_corrupted);

    const auto ptr = reinterpret_cast<const dcl_index*>(bytes.data());
    const auto len = bytes.size() / sizeof(dcl_index);
    ICY_ERROR(vec.append(ptr, ptr + len));
    return {};
}
error_type dcl_writer::find_logs_by_base(const dcl_index base, array<dcl_index>& vec) const noexcept
{
    auto key = base;
    array<uint8_t> bytes;
    ICY_ERROR(m_log[dcl_system::log_by_base].get_var_by_type(key, bytes, database_oper_read::none));
    if (bytes.size() % sizeof(dcl_index))
        return make_dcl_error(dcl_error::database_corrupted);

    const auto ptr = reinterpret_cast<const dcl_index*>(bytes.data());
    const auto len = bytes.size() / sizeof(dcl_index);
    ICY_ERROR(vec.append(ptr, ptr + len));
    return {};
}
error_type dcl_writer::find_log_by_index(const dcl_index index, dcl_record& record, array<dcl_action>& vec) const noexcept
{
    auto key = index;
    array<uint8_t> bytes;
    ICY_ERROR(m_log[dcl_system::log_by_index].get_var_by_type(key, bytes, database_oper_read::none));

    if (bytes.size() < sizeof(dcl_record))
        return make_dcl_error(dcl_error::database_corrupted);
    
    const auto len = (bytes.size() - sizeof(dcl_record)) / sizeof(dcl_action);

    if (bytes.size() != sizeof(dcl_record) + len * sizeof(dcl_action))
        return make_dcl_error(dcl_error::database_corrupted);

    union
    {
        const uint8_t* as_bytes;
        const dcl_record* as_record;
        const dcl_action* as_action;
    };
    as_bytes = bytes.data();
    record = *(as_record++);
    ICY_ERROR(vec.append(as_action, as_action + len));
    return {};
}

error_type dcl_writer::change_flags(const dcl_index index, const dcl_flag flags) noexcept
{
    dcl_action action;
    action.flag = dcl_flag::change_flags;
    action.base = index;
    action.values[1].decimal = uint64_t(flags);
    ICY_ERROR(append(action));
    return {};
}
error_type dcl_writer::change_value(const dcl_index index, const dcl_value value) noexcept
{
    dcl_action action;
    action.flag = dcl_flag::change_value;
    action.base = index;
    action.values[1] = value;
    ICY_ERROR(append(action));
    return {};
}
error_type dcl_writer::change_name(const dcl_index index, const dcl_index locale, const string_view text) noexcept
{
    dcl_base locale_base;
    if (locale != dcl_index())
    {
        ICY_ERROR(find_base(locale, locale_base));
        if (locale_base.type != dcl_type::locale)
            return make_dcl_error(dcl_error::invalid_locale);
    }

    dcl_dbase val;
    array<dcl_index> links;
    ICY_ERROR(find_base(index, val, &links));
    if (!val)
        return make_dcl_error(dcl_error::invalid_index);

    dcl_base name;
    for (auto&& link : links)
    {
        dcl_dbase name_val;
        ICY_ERROR(find_base(link, name_val));
        if (name_val.type == dcl_type::name && name_val.locale == locale_base.locale)
        {
            name = name_val.base(link);
            break;
        }
    }

    dcl_index binary;
    ICY_ERROR(change_binary(name.value.reference, text.ubytes(), binary));
    if (!name.index)
    {
        dcl_action action;
        action.flag = dcl_flag::create_base;
        action.locale = uint32_t(locale_base.locale);
        ICY_ERROR(dcl_index::create(action.base));
        action.values[0].reference = binary;
        action.values[1].reference = index;
        action.type = dcl_type::name;
        ICY_ERROR(append(action));
    }

    return {};
}
error_type dcl_writer::change_binary(const dcl_index old_index, const const_array_view<uint8_t> bytes, dcl_index& new_index) noexcept
{
    ICY_ERROR(dcl_index::create(new_index));

    const_array_view<uint8_t> old_bytes;
    if (old_index)
    {
        dcl_index index = old_index;
        if (const auto error = m_binary.get_var_by_type(index, old_bytes, database_oper_read::none))
        {
            if (error == database_error_not_found)
                return make_dcl_error(dcl_error::database_corrupted);
            return error;
        }
    }
    
    database_txn_write txn;
    database_cursor_write cursor;
    if (m_cache_base)
    {
        ICY_ERROR(txn.initialize(m_cache_base));
        ICY_ERROR(cursor.initialize(txn, m_cache_dbi));
    }

    if (old_index)
    {
        ICY_ERROR(dcl_dbase_write(m_binary, new_index, old_bytes, database_oper_write::unique));
        ICY_ERROR(dcl_dbase_write(cursor, new_index, old_bytes, database_oper_write::unique));
        ICY_ERROR(dcl_dbase_write(m_binary, old_index, bytes, database_oper_write::none));
        ICY_ERROR(dcl_dbase_write(cursor, old_index, bytes, database_oper_write::none));
    }
    else
    {
        ICY_ERROR(dcl_dbase_write(m_binary, new_index, bytes, database_oper_write::unique));
        ICY_ERROR(dcl_dbase_write(cursor, new_index, bytes, database_oper_write::unique));
    }
    if (txn)
    {
        ICY_ERROR(txn.commit());
    }

    dcl_action action;
    action.flag = dcl_flag::change_binary;
    action.values[0].reference = old_index;
    action.values[1].reference = new_index;
    ICY_ERROR(append(action));
    return {};
}
error_type dcl_writer::create_base(dcl_base& base) noexcept
{
    if (!base.index)
        ICY_ERROR(dcl_index::create(base.index));
    ICY_ERROR(append(dcl_action_from_base(base)));
    return {};
}

error_type dcl_writer::append(dcl_action action) noexcept
{
    const auto find_binary = [this](dcl_index index)
    {
        if (index)
        {
            const_array_view<uint8_t> val;
            if (const auto error = m_binary.get_var_by_type(index, val, database_oper_read::none))
            {
                if (error == database_error_not_found)
                    return make_dcl_error(dcl_error::invalid_binary);
                return error;
            }
        }
        return error_type();
    };

    if (action.flag & dcl_flag::change_binary)
    {
        const auto func = [this](dcl_index index)
        {
            const_array_view<uint8_t> bytes;
            if (const auto error = m_binary.get_var_by_type(index, bytes, database_oper_read::none))
            {
                if (error == database_error_not_found)
                    return make_dcl_error(dcl_error::invalid_binary);
                ICY_ERROR(error);
            }
            return error_type();
        };
        const auto old_index = action.values[0].reference;
        const auto new_index = action.values[1].reference;
        if (old_index)
            ICY_ERROR(func(old_index));
        
        ICY_ERROR(func(new_index));
        action = dcl_action();
        action.flag = dcl_flag::change_binary;
        action.values[0].reference = old_index;
        action.values[1].reference = new_index;
    }
    else if (action.flag & dcl_flag::change_flags)
    {
        dcl_base base;
        ICY_ERROR(find_base(action.base, base));
        if (!base.index)
            return make_dcl_error(dcl_error::invalid_index);
        const auto new_flag = action.values[1].decimal;
        if (base.flag == dcl_flag(action.values[1].decimal))
            return {};

        action = dcl_action();
        action.flag = dcl_flag::change_flags;
        action.values[0].decimal = uint64_t(base.flag);
        action.values[1].decimal = new_flag;
        action.base = base.index;
    }
    else if (action.flag & dcl_flag::change_value)
    {
        dcl_base base;
        ICY_ERROR(find_base(action.base, base));
        if (!base.index)
            return make_dcl_error(dcl_error::invalid_index);
        const auto new_value = action.values[1];
        if (base.value == new_value)
            return {};

        action = dcl_action();
        action.flag = dcl_flag::change_value;
        action.values[0] = base.value;
        action.values[1] = new_value;
        action.base = base.index;
    }
    else if (action.flag & dcl_flag::create_base)
    {
        {
            dcl_base base;
            ICY_ERROR(find_base(action.base, base));
            if (base.index)
                return make_dcl_error(dcl_error::invalid_index);
        }

        dcl_base parent;
        ICY_ERROR(find_base(action.values[1].reference, parent));
        if (!parent.index)
            return make_dcl_error(dcl_error::invalid_parent_index);

        switch (action.type)
        {
        case dcl_type::directory:
        case dcl_type::user:
        case dcl_type::enum_class:
        case dcl_type::flags_class:
        case dcl_type::resource_class:
        {
            if (parent.type != dcl_type::directory)
                return make_dcl_error(dcl_error::invalid_parent_type);
            action.values[0] = {};
            break;
        }


        case dcl_type::name:
        {
            switch (parent.type)
            {
            case dcl_type::directory:
            case dcl_type::user:
            case dcl_type::enum_class:
            case dcl_type::enum_value:
            case dcl_type::flags_class:
            case dcl_type::flags_value:
            case dcl_type::resource_class:
            case dcl_type::class_type:
            case dcl_type::class_property_decimal:
            case dcl_type::class_property_boolean:
            case dcl_type::class_property_string:
            case dcl_type::class_property_enum:
            case dcl_type::class_property_flags:
            case dcl_type::class_property_resource:
            case dcl_type::class_property_text:
            case dcl_type::class_property_reference:
            case dcl_type::class_property_inline:
            case dcl_type::object_type:
                break;
            default:
                return make_dcl_error(dcl_error::invalid_parent_type);
            }
            ICY_ERROR(find_binary(action.values[0].reference));
            break;
        }
        case dcl_type::locale:
        {
            if (parent.type != dcl_type::directory)
                return make_dcl_error(dcl_error::invalid_parent_type);

            array<locale> locales;
            ICY_ERROR(locale::enumerate(locales));
            auto is_valid = false;
            for (auto&& loc : locales)
            {
                if (loc.code == action.locale)
                {
                    is_valid = true;
                    break;
                }
            }
            if (!is_valid)
                return make_dcl_error(dcl_error::invalid_locale);

            array<dcl_base> base_locales;
            ICY_ERROR(find_type(dcl_type::locale, base_locales));
            for (auto&& loc : base_locales)
            {
                if (loc.locale == action.locale)
                    return make_dcl_error(dcl_error::invalid_locale);
            }
            action.values[0] = {};
            break;
        }
        default:
            return make_stdlib_error(std::errc::function_not_supported);
        }
        ICY_ERROR(create_base(action));        
    }
    else
    {
        return make_stdlib_error(std::errc::function_not_supported);
    }

    if (m_action < m_actions.size())
    {
        m_actions[m_action] = action;
        m_actions.pop_back(m_actions.size() - m_action);
    }
    else
    {
        ICY_ERROR(m_actions.push_back(action));
    }
    m_action = m_actions.size();
    ICY_ERROR(save());
    return {};
}
error_type dcl_writer::modify(dcl_index parent, const dcl_index link, const bool append) noexcept
{
    const_array_view<uint8_t> read_bytes;
    ICY_ERROR(m_data.get_var_by_type(parent, read_bytes, database_oper_read::none));
    array<dcl_index> links;
    dcl_dbase dbase_parent;
    ICY_ERROR(dcl_dbase::create(read_bytes, dbase_parent, &links));

    const auto it = std::find(links.begin(), links.end(), link);
    if (append && it != links.end() || !append && it == links.end())
        return make_dcl_error(dcl_error::database_corrupted);
        
    auto capacity = sizeof(dcl_dbase) + sizeof(dcl_index) * links.size();
    if (append)
        capacity += sizeof(dcl_index);
    else
        capacity -= sizeof(dcl_index);
    
    array_view<uint8_t> write_bytes;
    ICY_ERROR(m_data.put_var_by_type(parent, capacity, database_oper_write::none, write_bytes));
    union
    {
        uint8_t* as_bytes;
        dcl_dbase* as_dbase;
        dcl_index* as_links;
    };
    as_bytes = write_bytes.data();
    *(as_dbase++) = dbase_parent;
    if (append)
    {
        memcpy(as_links, links.data(), links.size() * sizeof(dcl_index));
        as_links[links.size()] = link;
    }
    else
    {
        const auto count = std::distance(links.begin(), it);
        memcpy(as_links, links.data(), count * sizeof(dcl_index));
        memcpy(as_links + count, links.data() + count + 1, (links.size() - count - 1) * sizeof(dcl_index));
    }
    return {};
}
error_type dcl_writer::create_base(const dcl_action& action) noexcept
{
    const auto parent = action.values[1].reference;
    ICY_ERROR(modify(parent, action.base, true));
    
    dcl_dbase new_dbase(dcl_base_from_action(action));
    array_view<uint8_t> write_bytes;
    if (const auto error = m_data.put_var_by_type(action.base, sizeof(new_dbase), database_oper_write::unique, write_bytes))
    {
        if (error == database_error_key_exist)
            return make_dcl_error(dcl_error::database_corrupted);
        return error;
    }
    memcpy(write_bytes.data(), &new_dbase, sizeof(new_dbase));
    return {};
}
error_type dcl_writer::save() noexcept
{
    if (m_cache_base)
    {
        database_txn_write txn;
        database_cursor_write cursor;
        ICY_ERROR(txn.initialize(m_cache_base));
        ICY_ERROR(cursor.initialize(txn, m_cache_dbi));
        array_view<uint8_t> val;
        ICY_ERROR(cursor.put_var_by_type(dcl_root, (m_actions.size() + 1) * sizeof(dcl_action), database_oper_write::none, val));
        const auto ptr = reinterpret_cast<dcl_action*>(val.data());
        ptr[0] = dcl_action();
        ptr[0].values[0].decimal = m_action;
        memcpy(ptr + 1, m_actions.data(), m_actions.size() * sizeof(dcl_action));
        ICY_ERROR(txn.commit());
    }
    return {};
}
error_type dcl_writer::change_base(const dcl_index& index, const dcl_value* const value, const dcl_flag* const flags) noexcept
{
    array<dcl_index> links;
    dcl_dbase dbase;
    ICY_ERROR(find_base(index, dbase, &links));
    if (value) dbase.value = *value;
    if (flags) dbase.flag = *flags;
    array_view<uint8_t> write_bytes;
    ICY_ERROR(m_data.put_var_by_type(index,
        sizeof(dbase) + links.size() * sizeof(dcl_index), database_oper_write::none, write_bytes));
    memcpy(write_bytes.data() + 0 * sizeof(dbase), &dbase, sizeof(dbase));
    memcpy(write_bytes.data() + 1 * sizeof(dbase), links.data(), links.size() * sizeof(dcl_index));
    return {};
}
error_type dcl_writer::undo() noexcept
{
    if (m_action == 0)
        return {};

    const auto& action = m_actions[m_action - 1];
    if (action.flag & dcl_flag::create_base)
    {
        dcl_base base;
        ICY_ERROR(find_base(action.base, base));
        ICY_ERROR(modify(base.parent, action.base, false));
        ICY_ERROR(m_data.del_by_type(action.base));
    }
    else if (action.flag & dcl_flag::change_binary)
    {
        auto old_index = action.values[0].reference;
        auto new_index = action.values[1].reference;
        
        if (old_index)
        {
            database_txn_write txn;
            database_cursor_write cursor;
            if (m_cache_base)
            {
                ICY_ERROR(txn.initialize(m_cache_base));
                ICY_ERROR(cursor.initialize(txn, m_cache_dbi));
            }
            const_array_view<uint8_t> new_bytes;
            const_array_view<uint8_t> old_bytes;
            ICY_ERROR(m_data.get_var_by_type(old_index, new_bytes, database_oper_read::none));
            ICY_ERROR(m_data.get_var_by_type(new_index, old_bytes, database_oper_read::none));
            
            ICY_ERROR(dcl_dbase_write(m_data, old_index, old_bytes, database_oper_write::none));
            ICY_ERROR(dcl_dbase_write(m_data, new_index, new_bytes, database_oper_write::none));
            if (txn)
            {
                ICY_ERROR(dcl_dbase_write(cursor, old_index, old_bytes, database_oper_write::none));
                ICY_ERROR(dcl_dbase_write(cursor, new_index, new_bytes, database_oper_write::none));
                ICY_ERROR(txn.commit());
            }
        }
    }
    else if (action.flag & dcl_flag::change_value)
    {
        ICY_ERROR(change_base(action.base, &action.values[0], nullptr));
    }
    else if (action.flag & dcl_flag::change_flags)
    {
        auto flags = dcl_flag(action.values[0].decimal);
        ICY_ERROR(change_base(action.base, nullptr, &flags));
    }
    else 
    {
        return make_stdlib_error(std::errc::function_not_supported);
    }
    --m_action;
    ICY_ERROR(save());
    return {};
}
error_type dcl_writer::redo() noexcept
{
    if (m_action == m_actions.size())
        return {};

    const auto action = m_actions[m_action];
    if (action.flag & dcl_flag::create_base)
    {
        ICY_ERROR(create_base(action));
    }
    else if (action.flag & dcl_flag::change_binary)
    {
        auto old_index = action.values[0].reference;
        auto new_index = action.values[1].reference;
        
        if (old_index)
        {
            database_txn_write txn;
            database_cursor_write cursor;
            if (m_cache_base)
            {
                ICY_ERROR(txn.initialize(m_cache_base));
                ICY_ERROR(cursor.initialize(txn, m_cache_dbi));
            }

            const_array_view<uint8_t> new_bytes;
            const_array_view<uint8_t> old_bytes;
            ICY_ERROR(m_data.get_var_by_type(old_index, new_bytes, database_oper_read::none));
            ICY_ERROR(m_data.get_var_by_type(new_index, old_bytes, database_oper_read::none));

            ICY_ERROR(dcl_dbase_write(m_data, old_index, old_bytes, database_oper_write::none));
            ICY_ERROR(dcl_dbase_write(m_data, new_index, new_bytes, database_oper_write::none));
            if (txn)
            {
                ICY_ERROR(dcl_dbase_write(cursor, old_index, old_bytes, database_oper_write::none));
                ICY_ERROR(dcl_dbase_write(cursor, new_index, new_bytes, database_oper_write::none));
                ICY_ERROR(txn.commit());
            }
        }
    }
    else if (action.flag & dcl_flag::change_flags)
    {
        auto flags = dcl_flag(action.values[1].decimal);
        ICY_ERROR(change_base(action.base, nullptr, &flags));
    }
    else if (action.flag & dcl_flag::change_value)
    {
        ICY_ERROR(change_base(action.base, &action.values[1], nullptr));
    }
    else
    {
        return make_stdlib_error(std::errc::function_not_supported);
    }
    ++m_action;
    ICY_ERROR(save());
    return {};
}
error_type dcl_writer::save(const dcl_index user, const string_view text) noexcept
{
    dcl_record new_record;
    new_record.index = guid::create();
    if (!new_record.index)
        return last_system_error();

    if (user)
    {
        dcl_dbase user_base;
        ICY_ERROR(find_base(user, user_base));
        if (user_base.type != dcl_type::user)
            return make_dcl_error(dcl_error::invalid_user);
    }
    if (!m_action)
    {
        *this = {};
        return {};
    }
    

    array<dcl_action> new_actions;
    map<dcl_index, array<uint8_t>> data;

    for (auto k = 0u; k < m_action; ++k)
    {
        dcl_action new_action = m_actions[k];
        if (new_action.flag & dcl_flag::create_base)
        {
            for (auto n = k + 1; n < m_action; ++n)
            {
                if (m_actions[n].base != new_action.base)
                    continue;

                if (m_actions[n].flag & dcl_flag::change_flags)
                {
                    new_action.flag = dcl_flag(m_actions[n].values[1].decimal);
                }
                else if (m_actions[n].flag & dcl_flag::change_value)
                {
                    new_action.values[1] = m_actions[n].values[1];
                }
                m_actions[n] = {};
            }
            if (new_action.flag & dcl_flag::destroyed)
                continue;
        }
        else if (new_action.flag & dcl_flag::change_flags)
        {
            for (auto n = k + 1; n < m_action; ++n)
            {
                if (m_actions[n].base != new_action.base)
                    continue;
                if (m_actions[n].flag & dcl_flag::change_flags)
                {
                    new_action.values[1] = m_actions[n].values[1];
                    m_actions[n] = {};
                }
            }
            if (new_action.values[0] == new_action.values[1])
                continue;
        }
        else if (new_action.flag & dcl_flag::change_value)
        {
            for (auto n = k + 1; n < m_action; ++n)
            {
                if (m_actions[n].base != new_action.base)
                    continue;
                if (m_actions[n].flag & dcl_flag::change_value)
                {
                    new_action.values[1] = m_actions[n].values[1];
                    m_actions[n] = {};
                }
            }
            if (new_action.values[0] == new_action.values[1])
                continue;
        }
        else if (new_action.flag & dcl_flag::change_binary)
        {
            auto old_index = new_action.values[0].reference;
            if (!old_index)
                old_index = new_action.values[1].reference;
            
            for (auto n = k + 1; n < m_action; ++n)
            {
                if (!(m_actions[n].flag & dcl_flag::change_binary))
                    continue;
                if (m_actions[n].values[0].reference == old_index)
                    m_actions[n] = {};
            }
            array<uint8_t> vec;
            const_array_view<uint8_t> new_bytes;
            ICY_ERROR(m_binary.get_var_by_type(old_index, new_bytes, database_oper_read::none));
            ICY_ERROR(vec.assign(new_bytes));
            ICY_ERROR(data.find_or_insert(old_index, std::move(vec)));
        }
        else
        {
            continue;
        }
        new_actions.push_back(new_action);
    }
    
    ICY_ERROR(m_txn.initialize(m_system->m_base));
    ICY_ERROR(m_data.initialize(m_txn, m_system->m_data));
    ICY_ERROR(m_binary.initialize(m_txn, m_system->m_binary));
    for (auto k = 0u; k < dcl_system::log_count; ++k)
        ICY_ERROR(m_log[k].initialize(m_txn, m_system->m_log[k]));

    for (auto&& action : new_actions)
    {
        if (action.flag & dcl_flag::create_base)
        {
            ICY_ERROR(create_base(action));
        }
        else if (action.flag & dcl_flag::change_flags)
        {
            auto flags = dcl_flag(action.values[1].decimal);
            ICY_ERROR(change_base(action.base, nullptr, &flags));
        }
        else if (action.flag & dcl_flag::change_value)
        {
            ICY_ERROR(change_base(action.base, &action.values[1], nullptr));
        }
        else if (action.flag & dcl_flag::change_binary)
        {
            if (auto old_index = action.values[0].reference)
            {
                const_array_view<uint8_t> old_bytes;
                ICY_ERROR(m_binary.get_var_by_type(old_index, old_bytes, database_oper_read::none));
                const auto it = data.find(old_index);
                if (it == data.end())
                    return make_unexpected_error();

                if (it->value == old_bytes)
                {
                    action = {};
                    continue;
                }
                ICY_ERROR(dcl_dbase_write(m_binary, action.values[1].reference, old_bytes, database_oper_write::none));
                ICY_ERROR(dcl_dbase_write(m_binary, it->key, it->value, database_oper_write::none));
            }
            else
            {
                const auto it = data.find(action.values[1].reference);
                if (it == data.end())
                    return make_unexpected_error();

                ICY_ERROR(dcl_dbase_write(m_binary, it->key, it->value, database_oper_write::none));
            }
        }
    }
    
    if (!text.empty())
    {
        auto text_index = guid::create();
        if (!text_index)
            return last_system_error();
        
        ICY_ERROR(dcl_dbase_write(m_binary, text_index, text.ubytes().size(), database_oper_write::unique));
        new_record.text = text_index;
    }
    new_record.time = dcl_time::clock::now();
    new_record.user = user;
    
    auto& log_by_index = m_log[dcl_system::log_by_index];
    log_by_index;

    return {};
}
/*error_type dcl_writer::create(const dcl_index parent, const dcl_type type, dcl_index& index) noexcept
{
    dcl_dbase val;
    array<dcl_index> links;
    ICY_ERROR(find_base(parent, val, &links));
    if (!val)
        return make_dcl_error(dcl_error::invalid_parent_index);

    index = guid::create();
    if (!index)
        return last_system_error();
        
    switch (type)
    {
    case dcl_type::directory:
    {
        if (val.type != dcl_type::directory)
            return make_dcl_error(dcl_error::invalid_parent_type);

        break;
    }
    default:
        return make_stdlib_error(std::errc::function_not_supported);
    }
    ICY_ERROR(create(val, links, dcl_dbase(type, parent).base(index)));

    dcl_base action;
    action.type = dcl_type::action_create;
    action.index = index;
    ICY_ERROR(m_actions.push_back(action));
    return {};
}
error_type dcl_writer::create(const dcl_dbase& parent_val, const const_array_view<dcl_index> links, const dcl_base& new_val) noexcept
{
    const auto capacity = sizeof(dcl_dbase) + (links.size() + 1) * sizeof(dcl_index);
    array_view<uint8_t> bytes;
    ICY_ERROR(m_data.put_type_by_type(new_val.index, database_oper_write::unique, dcl_dbase(new_val)));
    ICY_ERROR(m_data.put_var_by_type(new_val.parent, capacity, database_oper_write::none, bytes));
    union
    {
        dcl_dbase* new_parent_val;
        dcl_index* new_parent_links;
        uint8_t* parent_bytes;
    };
    parent_bytes = bytes.data();
    *(new_parent_val++) = parent_val;
    for (auto&& link : links)
        *(new_parent_links++) = link;
    *(new_parent_links++) = new_val.index;

    return {};
}


error_type dcl_writer::save(const dcl_index user, const string_view text, dcl_transaction& txn) noexcept
{
    dcl_base user_base;
    ICY_ERROR(find_base(user, user_base));
    if (user_base.type != dcl_type::user)
        return make_dcl_error(dcl_error::invalid_user);

    for (auto&& action : m_actions)
    {
        if (action.base == user && action.type == dcl_action_type::create_base)
            return make_dcl_error(dcl_error::invalid_user);
    }

    auto& actions = txn.m_actions;
    auto& binary = txn.m_binary;
    auto& record = txn.m_record;
    txn.m_system = m_system;
    record = {};
    actions.clear();
    binary.clear();

    if (!(record.index = dcl_index::create())) return last_system_error();
    record.time = std::chrono::system_clock::now();
    record.user = user;

    if (!text.empty())
    {
        if (!(record.text = dcl_index::create())) return last_system_error();
        array<uint8_t> bytes;
        ICY_ERROR(copy(text.ubytes(), bytes));
        ICY_ERROR(binary.insert(record.text, std::move(bytes)));
    }

    for (auto k = 0_z; k < m_actions.size(); ++k)
    {
        auto& action = m_actions[k];
        auto new_action = action;

        switch (action.type)
        {
        case dcl_action_type::create_base:
        {
            dcl_base new_base;
            ICY_ERROR(find_base(action.base, new_base));
            for (auto n = k + 1; k < m_actions.size(); ++n)
            {
                auto& next_action = m_actions[n];
                if (next_action.base == action.base)
                    next_action = {};
            }
            if (new_base.flag & dcl_flag::destroyed)
                break;  //  new object create -> destroyed
            new_action.values[0] = new_base.value;
            new_action.values[1].reference = new_base.parent;

            auto is_ref = false;
            if (new_base.type == dcl_type::value)
            {
                dcl_index type;
                ICY_ERROR(value_type(new_base, type));
                if (type == dcl_text)
                {
                    is_ref = true;
                }
                else if (type == dcl_index()
                    || type == dcl_primitive_boolean
                    || type == dcl_primitive_decimal
                    || type == dcl_primitive_string)
                {
                    ;
                }
                else
                {
                    dcl_base base_type;
                    ICY_ERROR(find_base(type, base_type));
                    is_ref = base_type.type == dcl_type::resource;
                }
            }
            if (is_ref)
            {
                const_array_view<uint8_t> view_bytes;
                ICY_ERROR(find_binary(new_base.value.reference, view_bytes));
                array<uint8_t> bytes;
                ICY_ERROR(copy(view_bytes, bytes));
                ICY_ERROR(binary.insert(new_base.value.reference, std::move(bytes)));
            }
            ICY_ERROR(actions.push_back(new_action));
            break;
        }
        case dcl_action_type::change_flags:
        case dcl_action_type::change_value:
        {
            for (auto n = k + 1; k < m_actions.size(); ++n)
            {
                auto& next_action = m_actions[n];
                if (next_action.base == action.base && next_action.type == action.type)
                {
                    new_action.values[1] = next_action.values[1];
                    next_action = {};
                }
            }
            if (new_action.values[0] == new_action.values[1])
                break;  //  no flag change
            ICY_ERROR(actions.push_back(new_action));
            break;
        }
        default:
            break;
        }
    }
}

*/


/*
error_type dcl_patch::initialize(const string_view path) noexcept
{
    file_info info;
    info.initialize(path);
    if (!info.size)
        return {};

    database_system_read dbase;
    ICY_ERROR(dbase.initialize(path, align_up(info.size + 512_mb, 1_gb)));
    database_txn_read txn;
    ICY_ERROR(txn.initialize(dbase));
    database_dbi dbi_data;
    if (const auto error = dbi_data.initialize_open_int_key(txn, dbi_name_data))
    {
        if (error == database_error_not_found)
            return make_dcl_error(dcl_error::database_corrupted);
        return error;
    }
    database_dbi dbi_binary;
    if (const auto error = dbi_binary.initialize_open_any_key(txn, dbi_name_binary))
    {
        if (error == database_error_not_found)
            return make_dcl_error(dcl_error::database_corrupted);
        return error;
    }
    database_cursor_read cursor_data;
    ICY_ERROR(cursor_data.initialize(txn, dbi_data));
    auto key_data = 0u;
    const_array_view<uint8_t> bytes;
    if (const auto error = cursor_data.get_var_by_type(key_data, bytes, database_oper_read::none))
    {
        if (error == database_error_not_found)
            return make_dcl_error(dcl_error::database_corrupted);
        return error;
    }
    if (bytes.size() % sizeof(dcl_action) != 0)
        return make_dcl_error(dcl_error::database_corrupted);

    const auto as_action = reinterpret_cast<const dcl_action*>(bytes.data());
    const auto count = bytes.size() / sizeof(dcl_action);
    if (count)
    {
        m_action = as_action[0].locale;
        ICY_ERROR(m_actions.assign(as_action + 1, as_action + count));
    }

    database_cursor_read cursor_binary;
    ICY_ERROR(cursor_binary.initialize(txn, dbi_binary));

    dcl_index key_binary;
    auto oper = database_oper_read::first;
    while (true)
    {
        if (const auto error = cursor_binary.get_var_by_type(key_binary, bytes, oper))
        {
            if (error == database_error_not_found)
                break;
            else if (error == database_error_invalid_size)
                return make_dcl_error(dcl_error::database_corrupted);
            return error;
        }
        array<uint8_t> vec;
        ICY_ERROR(copy(bytes, vec));
        ICY_ERROR(m_binary.insert(key_binary, std::move(vec)));
        oper = database_oper_read::next;
    }

    return error_type();
}
error_type dcl_patch::initialize(dcl_writer& writer) noexcept
{
    return make_stdlib_error(std::errc::function_not_supported);
}
error_type dcl_patch::apply(const shared_ptr<dcl_system> system, const dcl_index user, const string_view text) noexcept
{
    return make_stdlib_error(std::errc::function_not_supported);
}*/

/*error_type dcl_writer::value_type(const dcl_base& base, dcl_index& type) const noexcept
{
    switch (base.type)
    {
    case dcl_type::directory:
    case dcl_type::user:
    case dcl_type::list:
    case dcl_type::flags:
    case dcl_type::locale:
    {
        type = dcl_index();
        break;
    }
    case dcl_type::name:
    {
        type = dcl_text;
        break;
    }
    case dcl_type::resource:
    {
        type = dcl_primitive_string;
        break;
    }
    case dcl_type::dclass:
    {
        type = dcl_index();
        break;
    }
    case dcl_type::object:
    {
        type = base.value.reference;
        break;
    }
    case dcl_type::value:
    {
        dcl_base parent;
        ICY_ERROR(find_base(base.parent, parent));
        switch (parent.type)
        {
        case dcl_type::property:
        case dcl_type::group:
            ICY_ERROR(value_type(parent, type));
            break;

        case dcl_type::resource:
            type = parent.index;
            break;

        case dcl_type::flags:
        case dcl_type::list:
            type = dcl_primitive_decimal;
            break;

        case dcl_type::reference:
            type = parent.value.reference;
            break;

        default:
            return make_dcl_error(dcl_error::database_corrupted);
        }
        break;
    }
    case dcl_type::group:
    case dcl_type::property:
    {
        dcl_base parent;
        ICY_ERROR(find_base(base.parent, parent));
        if (parent.type == dcl_type::object)
        {
            ICY_ERROR(find_base(parent.value.reference, parent));
        }

        if (parent.type != dcl_type::dclass)
            return make_dcl_error(dcl_error::database_corrupted);

        type = base.value.reference;
        break;
    }
    case dcl_type::reference:
    {
        type = base.value.reference;
        break;
    }
    default:
        return make_stdlib_error(std::errc::invalid_argument);
    }
    return {};
}*/

/* dcl_base base;
 ICY_ERROR(find_base(action.base, base));
 if (!base.index)
     return make_dcl_error(dcl_error::invalid_index);
 if (base.type != dcl_type::value && base.type != dcl_type::name)
     return make_dcl_error(dcl_error::invalid_type);
 if (base.value == action.values[1])
     return {};

 action.values[0] = base.value;
 dcl_index type_index;
 ICY_ERROR(value_type(base, type_index));
 if (type_index == dcl_index())
     return make_dcl_error(dcl_error::invalid_type);

 if (base.type == dcl_type::name)
 {
     ICY_ERROR(find_binary(action.values[1].reference));
     break;
 }

 dcl_base parent_base;
 ICY_ERROR(find_base(base.parent, parent_base));
 if (parent_base.type != dcl_type::property &&
     parent_base.type != dcl_type::group &&
     parent_base.type != dcl_type::reference)
     return make_dcl_error(dcl_error::invalid_parent_type);

 if (type_index == dcl_primitive_string ||
     type_index == dcl_primitive_boolean ||
     type_index == dcl_primitive_decimal ||
     type_index == dcl_primitive_time)
 {
     break;
 }
 else if (type_index == dcl_text)
 {
     const auto reference = action.values[1].reference;
     ICY_ERROR(find_binary(reference));
     break;
 }

 const dcl_index* ref = nullptr;
 dcl_base type_base;
 ICY_ERROR(find_base(type_index, type_base));
 if (type_base.type == dcl_type::resource)
 {
     const auto reference = action.values[1].reference;
     ICY_ERROR(find_binary(reference));
     break;
 }
 else if (type_base.type == dcl_type::object)
 {
     ref = &action.values[1].reference;
     dcl_base ref_base;
     ICY_ERROR(find_base(*ref, ref_base));
     if (ref_base.type != dcl_type::object)
         return make_dcl_error(dcl_error::invalid_value);
 }
 else if (type_base.type == dcl_type::flags || type_base.type == dcl_type::list)
 {
     const auto value = action.values[1].decimal;
     array<dcl_base> vec;
     ICY_ERROR(find_children(type_base.index, vec));
     array<uint64_t> values;
     for (auto&& value_base : vec)
     {
         if (value_base.type == dcl_type::value)
             ICY_ERROR(values.push_back(value_base.value.decimal));
     }
     if (type_base.type == dcl_type::flags)
     {
         auto mask = 0u;
         for (auto&& val : values)
             mask |= val;
         if ((value & mask) != value)
             return make_dcl_error(dcl_error::invalid_value);
     }
     else
     {
         const auto it = std::find(values.begin(), values.end(), value);
         if (it == values.end())
             return make_dcl_error(dcl_error::invalid_value);
     }
 }
 else
 {
     return make_dcl_error(dcl_error::invalid_type);
 }
 if (ref)
 {
     auto old_ref = action.values[0].reference;
     if (old_ref)
         ICY_ERROR(modify(old_ref, action.base, false));
     if (*ref)
         ICY_ERROR(modify(*ref, action.base, true));
 }*/
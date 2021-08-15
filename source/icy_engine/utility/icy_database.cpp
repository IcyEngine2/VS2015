#include <icy_engine/utility/icy_database.hpp>
#include <icy_engine/core/icy_string.hpp>
#include "../../libs/lmdb/lmdb.h"
using namespace icy;

#if _DEBUG
#pragma comment(lib, "lmdbd")
#else
#pragma comment(lib, "lmdb")
#endif

static error_type make_database_error(const unsigned code) noexcept
{
    return error_type(code, error_source_database);
}

static error_type database_error_to_string(const unsigned code, const string_view, string& str) noexcept
{
    switch (code)
    {
    case MDB_KEYEXIST: return icy::to_string("Key/data pair already exists"_s, str);
    case MDB_NOTFOUND: return icy::to_string("No matching key/data pair found"_s, str);
    case MDB_PAGE_NOTFOUND: return icy::to_string("Requested page not found"_s, str);
    case MDB_CORRUPTED: return icy::to_string("Located page was wrong type"_s, str);
    case MDB_PANIC: return icy::to_string("Update of meta page failed or environment had fatal error"_s, str);
    case MDB_VERSION_MISMATCH: return icy::to_string("Database environment version mismatch"_s, str);
    case MDB_INVALID: return icy::to_string("File is not an LMDB file"_s, str);
    case MDB_MAP_FULL: return icy::to_string("Environment mapsize limit reached"_s, str);
    case MDB_DBS_FULL: return icy::to_string("Environment maxdbs limit reached"_s, str);
    case MDB_READERS_FULL: return icy::to_string("Environment maxreaders limit reached"_s, str);
    case MDB_TLS_FULL: return icy::to_string("Thread-local storage keys full - too many environments open"_s, str);
    case MDB_TXN_FULL: return icy::to_string("Transaction has too many dirty pages - transaction too big"_s, str);
    case MDB_CURSOR_FULL: return icy::to_string("Internal error - cursor stack limit reached"_s, str);
    case MDB_PAGE_FULL: return icy::to_string("Internal error - page has no more space"_s, str);
    case MDB_MAP_RESIZED: return icy::to_string("Database contents grew beyond environment mapsize"_s, str);
    case MDB_INCOMPATIBLE: return icy::to_string("Operation and DB incompatible, or DB flags changed"_s, str);
    case MDB_BAD_RSLOT: return icy::to_string("Invalid reuse of reader locktable slot"_s, str);
    case MDB_BAD_TXN: return icy::to_string("Transaction must abort, has a child, or is invalid"_s, str);
    case MDB_BAD_VALSIZE: return icy::to_string("Unsupported size of key/DB name/data, or wrong DUPFIXED size"_s, str);
    case MDB_BAD_DBI: return icy::to_string("The specified DBI handle was closed/changed unexpectedly"_s, str);
    case MDB_PROBLEM: return icy::to_string("Unexpected problem - txn should abort"_s, str);
    default:
        return make_stdlib_error(std::errc::invalid_argument);
    }
}
static error_type database_make_error(const int code) noexcept
{
	if (code >= MDB_KEYEXIST && code <= MDB_LAST_ERRCODE)
		return make_database_error(code);
	else if (code)
		return make_stdlib_error(std::errc(code));
	else
		return {};
}
static error_type database_system_initialize(const string_view path, const size_t size, const int flags, MDB_env*& env) noexcept
{
	auto error = MDB_SUCCESS;
	if (!error) error = mdb_env_create(&env);
	if (!error) error = mdb_env_set_mapsize(env, size);
	if (!error) error = mdb_env_set_maxdbs(env, 256);
	if (!error) error = mdb_env_open(env, path.bytes().data(), path.bytes().size() + 1, MDB_NOSUBDIR | flags, 0664);
	if (error && env)
	{
		mdb_env_close(env);
		env = nullptr;
	}
	return database_make_error(error);
}
static error_type database_dbi_open(MDB_txn* txn, const string_view name, const int flags, MDB_dbi& dbi) noexcept
{
	return database_make_error(mdb_dbi_open(txn, name.bytes().data(), name.bytes().size(), flags, &dbi));
}

database_system_read::~database_system_read() noexcept
{
	if (m_env)
		mdb_env_close(m_env);	
}
error_type database_system_read::initialize(const string_view path, const size_t size) noexcept
{
	if (m_env)
	{
		mdb_env_close(m_env);
		m_env = nullptr;
	}
	return database_system_initialize(path, size, MDB_RDONLY, m_env);
}
error_type database_system_read::path(string& str) const noexcept
{
    str.clear();
    if (m_env)
    {
        const char* ptr = nullptr;
        ICY_ERROR(database_make_error(mdb_env_get_path(m_env, &ptr)));
        ICY_ERROR(to_string(const_array_view<char>(ptr, strlen(ptr)), str));
    }
    return error_type();
}
size_t database_system_read::size() const noexcept
{
    if (m_env)
    {
        MDB_envinfo info;
        mdb_env_info(m_env, &info);
        return info.me_mapsize;
    }
    return 0;
}
error_type database_system_write::initialize(const string_view path, const size_t size) noexcept
{
	if (m_env)
	{
		mdb_env_close(m_env);
		m_env = nullptr;
	}
	return database_system_initialize(path, size, 0, m_env);
}

error_type database_txn_read::initialize(const database_system_read& base) noexcept
{
    if (m_txn)
    {
        mdb_txn_abort(m_txn);
        m_txn = nullptr;
    }
    return database_make_error(mdb_txn_begin(base.m_env, nullptr, MDB_RDONLY, &m_txn));
}
database_txn_read::~database_txn_read() noexcept
{
    if (m_txn)
        mdb_txn_abort(m_txn);
}
error_type database_txn_write::initialize(const database_system_write& base) noexcept
{
    if (m_txn)
    {
        mdb_txn_abort(m_txn);
        m_txn = nullptr;
    }
    return database_make_error(mdb_txn_begin(base.m_env, nullptr, 0, &m_txn));
}
error_type database_txn_write::commit() noexcept
{
    const auto error = mdb_txn_commit(m_txn);
    m_txn = nullptr;
    return database_make_error(error);
}

error_type database_dbi::initialize_open_any_key(const database_txn_read& txn, const string_view name) noexcept
{
    return database_dbi_open(txn.m_txn, name, 0, m_dbi);
}
error_type database_dbi::initialize_open_int_key(const database_txn_read& txn, const string_view name) noexcept
{
    return database_dbi_open(txn.m_txn, name, MDB_INTEGERKEY, m_dbi);
}
error_type database_dbi::initialize_create_any_key(const database_txn_write& txn, const string_view name) noexcept
{
    return database_dbi_open(txn.m_txn, name, MDB_CREATE, m_dbi);
}
error_type database_dbi::initialize_create_int_key(const database_txn_write& txn, const string_view name) noexcept
{
    return database_dbi_open(txn.m_txn, name, MDB_INTEGERKEY | MDB_CREATE, m_dbi);
}
size_t database_dbi::size(const database_txn_read& txn) const noexcept
{
    MDB_stat stat = {};
    mdb_stat(txn.m_txn, m_dbi, &stat);
    return stat.ms_entries;
}
error_type database_dbi::clear(const database_txn_write& txn) noexcept
{
    return database_make_error(mdb_drop(txn.m_txn, m_dbi, 0));
}
database_cursor_read::~database_cursor_read() noexcept
{
    if (m_cur)
        mdb_cursor_close(m_cur);
}

error_type database_cursor_read::initialize(const database_txn_read& txn, const database_dbi dbi) noexcept
{
    if (m_cur)
    {
        mdb_cursor_close(m_cur);
        m_cur = nullptr;
    }
    return database_make_error(mdb_cursor_open(txn.m_txn, dbi.m_dbi, &m_cur));
}
error_type database_cursor_write::initialize(const database_txn_write& txn, const database_dbi dbi) noexcept
{
    if (m_cur)
    {
        mdb_cursor_close(m_cur);
        m_cur = nullptr;
    }
    return database_make_error(mdb_cursor_open(txn.m_txn, dbi.m_dbi, &m_cur));
}
error_type database_cursor_read::get_var_by_var(const_array_view<uint8_t>& key, const_array_view<uint8_t>& val, const database_oper_read oper) noexcept
{
    auto mdb_oper = MDB_SET_KEY;
    switch (oper)
    {
    case database_oper_read::none:
        break;
    case database_oper_read::first:
        mdb_oper = MDB_FIRST;
        break;
    case database_oper_read::last:
        mdb_oper = MDB_LAST;
        break;
    case database_oper_read::next:
        mdb_oper = MDB_NEXT;
        break;
    case database_oper_read::prev:
        mdb_oper = MDB_PREV;
        break;
    case database_oper_read::range:
        mdb_oper = MDB_SET_RANGE;
        break;
    default:
        return make_stdlib_error(std::errc::invalid_argument);
    }

    auto mdb_key = MDB_val{ key.size(), const_cast<uint8_t*>(key.data()) };
    auto mdb_val = MDB_val{ val.size(), const_cast<uint8_t*>(val.data()) };
    if (const auto error = mdb_cursor_get(m_cur, &mdb_key, &mdb_val, mdb_oper))
        return database_make_error(error);
    
    key = { static_cast<const uint8_t*>(mdb_key.mv_data), mdb_key.mv_size };
    val = { static_cast<const uint8_t*>(mdb_val.mv_data), mdb_val.mv_size };
    return{};
}
error_type database_cursor_write::put_var_by_var(const const_array_view<uint8_t> key, const size_t capacity, const database_oper_write oper, array_view<uint8_t>& val) noexcept
{
    auto flags = 0u;
    switch (oper)
    {
    case database_oper_write::append:
        flags = MDB_APPEND;
        break;
    case database_oper_write::unique:
        flags = MDB_NOOVERWRITE;
        break;
    }
    auto mdb_key = MDB_val{ key.size(), const_cast<uint8_t*>(key.data()) };
    auto mdb_val = MDB_val{ capacity, nullptr };
    if (const auto error = mdb_cursor_put(m_cur, &mdb_key, &mdb_val, flags | MDB_RESERVE))
        return database_make_error(error);

    val = { static_cast<uint8_t*>(mdb_val.mv_data), mdb_val.mv_size };
    return {};
}
error_type database_cursor_write::del_by_var(const const_array_view<uint8_t> key) noexcept
{
    auto mdb_key = MDB_val{ key.size(), const_cast<uint8_t*>(key.data()) };
    auto mdb_val = MDB_val{ };
    if (const auto error = mdb_cursor_get(m_cur, &mdb_key, &mdb_val, MDB_cursor_op::MDB_SET))
        return database_make_error(error);
    if (const auto error = mdb_cursor_del(m_cur, 0))
        return database_make_error(error);
    return {};
}

const error_source icy::error_source_database = register_error_source("database"_s, database_error_to_string);
const error_type icy::database_error_key_exist = database_make_error(MDB_KEYEXIST);
const error_type icy::database_error_not_found = database_make_error(MDB_NOTFOUND);
const error_type icy::database_error_invalid_size = database_make_error(MDB_BAD_VALSIZE);
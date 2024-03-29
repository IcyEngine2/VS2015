#pragma once

#include <icy_engine/core/icy_string_view.hpp>

struct MDB_env;
struct MDB_txn;
struct MDB_cursor;

namespace icy
{
	class database_txn_read;
	class database_txn_write;
	class database_dbi;
	class database_cursor_read;
	class database_cursor_write;
	class database_system_read;
	class database_system_write;

	enum class database_oper_read : uint32_t
	{
		none,
		first,
		last,
		next,
		prev,
        range,
	};
	enum class database_oper_write : uint32_t
	{
		none,
		unique,	//	no overwrite
		append,
	};

    extern const error_source error_source_database;
    extern const error_type database_error_key_exist;
    extern const error_type database_error_not_found;
    extern const error_type database_error_invalid_size;
    static constexpr size_t database_record_key_size = 12;

    class database_system_read
    {
        friend database_txn_read;
    public:
        database_system_read() noexcept = default;
        database_system_read(database_system_read&& rhs) noexcept : m_env(rhs.m_env)
        {
            rhs.m_env = nullptr;
        }
        ICY_DEFAULT_MOVE_ASSIGN(database_system_read);
        ~database_system_read() noexcept;
        error_type initialize(const string_view path, const size_t size) noexcept;
        error_type path(string& str) const noexcept;
        size_t size() const noexcept;
        explicit operator bool() const noexcept
        {
            return !!m_env;
        }
    protected:
        MDB_env* m_env = nullptr;
    };
    class database_system_write : public database_system_read
    {
        friend database_txn_write;
    public:
        error_type initialize(const string_view path, const size_t size) noexcept;
    };

	class database_txn_read
	{
		friend database_dbi;
        friend database_cursor_read;
	public:
		database_txn_read() noexcept = default;
		database_txn_read(database_txn_read&& rhs) noexcept : m_txn(rhs.m_txn)
		{
			rhs.m_txn = nullptr;
		}
		ICY_DEFAULT_MOVE_ASSIGN(database_txn_read);
		~database_txn_read() noexcept;
        error_type initialize(const database_system_read& base) noexcept;
		explicit operator bool() const noexcept
		{
			return !!m_txn;
		}
	protected:
		MDB_txn* m_txn = nullptr;
	};
	class database_txn_write : public database_txn_read
	{
		friend database_dbi;
        friend database_cursor_write;
	public:
		database_txn_write() noexcept = default;
		database_txn_write(database_txn_write&& rhs) noexcept : database_txn_read(std::move(rhs))
		{

		}
		ICY_DEFAULT_MOVE_ASSIGN(database_txn_write);
        error_type initialize(const database_system_write& base) noexcept;
		error_type commit() noexcept;
	};

	class database_dbi
	{
        friend database_cursor_read;
        friend database_cursor_write;
    public:
		bool operator==(const database_dbi& rhs) const noexcept
		{
			return m_dbi == rhs.m_dbi;
		}
        error_type initialize_open_any_key(const database_txn_read& txn, const string_view name) noexcept;
        error_type initialize_open_int_key(const database_txn_read& txn, const string_view name) noexcept;
        error_type initialize_create_any_key(const database_txn_write& txn, const string_view name) noexcept;
        error_type initialize_create_int_key(const database_txn_write& txn, const string_view name) noexcept;
        size_t size(const database_txn_read& txn) const noexcept;
        error_type clear(const database_txn_write& txn) noexcept;
	private:
		uint32_t m_dbi = 0;
	};

	class database_cursor_read
	{
	public:
		database_cursor_read() noexcept = default;
		database_cursor_read(database_cursor_read&& rhs) noexcept : m_cur(rhs.m_cur)
		{
			rhs.m_cur = nullptr;
		}
		ICY_DEFAULT_MOVE_ASSIGN(database_cursor_read);
		~database_cursor_read() noexcept;
        error_type initialize(const database_txn_read& txn, const database_dbi dbi) noexcept;
		explicit operator bool() const noexcept
		{
			return !!m_cur;
		}		
		error_type get_var_by_var(const_array_view<uint8_t>& key, const_array_view<uint8_t>& val, const database_oper_read oper) noexcept;
		template<typename K> error_type get_var_by_type(K& key, const_array_view<uint8_t>& val, const database_oper_read oper) noexcept
		{
			auto mdb_key = const_array_view<uint8_t>{ reinterpret_cast<const uint8_t*>(&key), sizeof(key), };
			ICY_ERROR(get_var_by_var(mdb_key, val, oper));
			if (mdb_key.size() == sizeof(K))
			{
				key = *reinterpret_cast<const K*>(mdb_key.data());
				return error_type();
			}
			return database_error_invalid_size;
		}
		template<typename V> error_type get_type_by_var(const_array_view<uint8_t> key, V& val, const database_oper_read oper) noexcept
		{
			auto mdb_val = const_array_view<uint8_t>{};
			ICY_ERROR(get_var_by_var(key, mdb_val, oper));

			if (mdb_val.size() == sizeof(V))
			{
				val = *reinterpret_cast<const V*>(mdb_val.data());
				return error_type();
			}
			return database_error_invalid_size;
		}
		template<typename K, typename V> error_type get_type_by_type(K& key, V& val, const database_oper_read oper) noexcept
		{
			auto mdb_key = const_array_view<uint8_t>{ reinterpret_cast<const uint8_t*>(&key), sizeof(key) };
			auto mdb_val = const_array_view<uint8_t>{ reinterpret_cast<const uint8_t*>(&val), sizeof(val) };
			ICY_ERROR(get_var_by_var(mdb_key, mdb_val, oper));

			if (mdb_key.size() == sizeof(K) && mdb_val.size() == sizeof(V))
			{
				key = *reinterpret_cast<const K*>(mdb_key.data());
				val = *reinterpret_cast<const V*>(mdb_val.data());
				return error_type();
			}
            return database_error_invalid_size;
		}
		inline error_type get_str_by_str(string_view& key, string_view& val, const database_oper_read oper) noexcept
		{
			const_array_view<uint8_t> byte_key = key.ubytes();
			const_array_view<uint8_t> byte_val;
			ICY_ERROR(get_var_by_var(byte_key, byte_val, oper));
			ICY_ERROR(to_string(byte_key, key));
			ICY_ERROR(to_string(byte_val, val));
			return error_type();
		}
		template<typename K> inline error_type get_str_by_type(K& key, string_view& val, const database_oper_read oper) noexcept
		{
			const_array_view<uint8_t> byte_val;
			ICY_ERROR(get_var_by_type(key, byte_val, oper));
			ICY_ERROR(to_string(byte_val, val));
			return error_type();
		}
		template<typename V> inline error_type get_type_by_str(string_view& key, V& val, const database_oper_read oper) noexcept
		{
			const_array_view<uint8_t> byte_key = key.ubytes();
			ICY_ERROR(get_type_by_var(byte_key, val, oper));
			ICY_ERROR(to_string(byte_key, key));
			return error_type();
		}
	protected:
		MDB_cursor* m_cur = nullptr;
	};

	class database_cursor_write : public database_cursor_read
	{
	public:
        error_type initialize(const database_txn_write& txn, const database_dbi dbi) noexcept;
		error_type put_var_by_var(const const_array_view<uint8_t> key, const size_t capacity, const database_oper_write flags, array_view<uint8_t>& val) noexcept;
		template<typename V> error_type put_type_by_var(const const_array_view<uint8_t> key, const database_oper_write flags, const V& val) noexcept
		{
			array_view<uint8_t> bytes;
			ICY_ERROR(put_var_by_var(key, sizeof(val), flags, bytes));
			if (bytes.size() == sizeof(V))
			{
				new (bytes.data()) V(val);
				return error_type();
			}
			return database_error_invalid_size;
		}
		template<typename K> error_type put_var_by_type(const K& key, const size_t capacity, const database_oper_write flags, array_view<uint8_t>& val) noexcept
		{
			return put_var_by_var({ reinterpret_cast<const uint8_t*>(&key), sizeof(key) }, capacity, flags, val);
		}
		template<typename V, typename K> error_type put_type_by_type(const K& key, const database_oper_write flags, const V& val) noexcept
		{
			array_view<uint8_t> bytes;
			ICY_ERROR(put_var_by_var({ reinterpret_cast<const uint8_t*>(&key), sizeof(key) }, sizeof(val), flags, bytes));
			if (bytes.size() == sizeof(V))
			{
				new (bytes.data()) V(val);
				return error_type();
			}
			return database_error_invalid_size;
		}
		error_type put_str_by_str(const string_view key, const string_view val, const database_oper_write flags) noexcept
		{
			array_view<uint8_t> bytes;
			ICY_ERROR(put_var_by_var(key.ubytes(), val.bytes().size(), flags, bytes));
			if (bytes.size() == val.bytes().size())
			{
				memcpy(bytes.data(), val.bytes().data(), val.bytes().size());
				return error_type();
			}
			return database_error_invalid_size;
		}
		template<typename V> error_type put_type_by_str(const string_view key, const database_oper_write flags, const V& val) noexcept
		{
			return put_type_by_var(key.ubytes(), flags, val);
		}
		template<typename K> error_type put_str_by_type(const K& key, const database_oper_write flags, const string_view val) noexcept
		{
			const_array_view<uint8_t> key_var(reinterpret_cast<const uint8_t*>(&key), sizeof(key));
			array_view<uint8_t> bytes;
			ICY_ERROR(put_var_by_var(key_var, val.bytes().size(), flags, bytes));
			if (bytes.size() == val.bytes().size())
			{
				memcpy(bytes.data(), val.bytes().data(), val.bytes().size());
				return error_type();
			}
			return database_error_invalid_size;
		}
        error_type del_by_var(const const_array_view<uint8_t> key) noexcept;
        template<typename K> error_type del_by_type(const K& key) noexcept
        {
            return del_by_var({ reinterpret_cast<const uint8_t*>(&key), sizeof(key) });
        }
        error_type del_by_str(const string_view key) noexcept
        {
            return del_by_var(key.ubytes());
        }
	};	
}
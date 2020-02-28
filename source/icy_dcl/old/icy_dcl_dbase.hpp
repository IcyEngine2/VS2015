#pragma once

#include <icy_engine/core/icy_map.hpp>
#include <icy_engine/utility/icy_database.hpp>
#include <icy_dcl/icy_dcl.hpp>

struct dcl_base_val
{
    icy::dcl_index parent;
    icy::dcl_index type;
    icy::dcl_value value;
};

enum class dcl_flag : uint32_t
{
    none,
    is_deleted  =   0x01,
    is_const    =   0x02,
    is_hidden   =   0x04,
};
inline dcl_flag operator|(const dcl_flag lhs, const dcl_flag rhs) noexcept
{
    return dcl_flag(uint32_t(lhs) | uint32_t(rhs));
}
inline bool operator&(const dcl_flag lhs, const dcl_flag rhs) noexcept
{
    return !!(uint32_t(lhs) & uint32_t(rhs));
}


class dcl_read_txn;
class dcl_write_txn;

class dcl_database
{
    friend dcl_read_txn;
    friend dcl_write_txn;
public:
    icy::error_type open(const icy::string_view path, const size_t size) noexcept;
private:
    icy::database_system_write m_base;
    icy::database_dbi m_data_main;
    icy::database_dbi m_data_link;
    icy::database_dbi m_log_main;   //  [] record values
    icy::database_dbi m_log_user;   //  [user] to record indices
    icy::database_dbi m_log_time;   //  [time] to record indices
    icy::database_dbi m_log_data;   //  [guid] to record indices
    icy::map<icy::dcl_index, icy::database_dbi> m_binary;
    mutable icy::mutex m_lock;
};
class dcl_read_txn
{
public:
    icy::error_type initialize(const dcl_database& dbase) noexcept;
    icy::error_type data_main(const icy::dcl_index& index, dcl_base_val& val) noexcept;
    icy::error_type data_link(const icy::dcl_index& index, icy::const_array_view<icy::dcl_index>& indices) noexcept;
    icy::error_type binary(const icy::dcl_index& index, const icy::dcl_index& locale, icy::const_array_view<uint8_t>& bytes) noexcept;
    icy::error_type text(const icy::dcl_index& index, const icy::dcl_index& locale, icy::string_view& bytes) noexcept;
    icy::error_type flags(const icy::dcl_index& index, dcl_flag& flags) noexcept;
    icy::error_type locales(const icy::dcl_index& locale, icy::map<dcl_index, string_view>& map) noexcept;
private:
    icy::database_txn_read m_txn;
    icy::database_cursor_read m_data_main;
    icy::database_cursor_read m_data_link;
    icy::database_cursor_read m_log_main;   //  [] record values
    icy::database_cursor_read m_log_user;   //  [user] to record indices
    icy::database_cursor_read m_log_time;   //  [time] to record indices
    icy::database_cursor_read m_log_data;   //  [guid] to record indices
    icy::map<icy::dcl_index, icy::database_cursor_read> m_binary;
};
class dcl_write_txn
{
public:
    icy::error_type initialize(const dcl_database& dbase) noexcept;
    icy::error_type rename(const icy::dcl_index index, const icy::dcl_index locale, const icy::string_view name) noexcept;
private:
    icy::database_txn_read m_txn;
    icy::database_cursor_read m_data_main;
    icy::database_cursor_read m_data_link;
    icy::database_cursor_read m_log_main;   //  [] record values
    icy::database_cursor_read m_log_user;   //  [user] to record indices
    icy::database_cursor_read m_log_time;   //  [time] to record indices
    icy::database_cursor_read m_log_data;   //  [guid] to record indices
    icy::map<icy::dcl_index, icy::database_cursor_read> m_binary;
};
#pragma once

#include <icy_engine/core/icy_event.hpp>
#include <icy_engine/core/icy_array.hpp>
#include <icy_engine/core/icy_map.hpp>
#include <icy_engine/utility/icy_database.hpp>
#include "icy_dcl.hpp"

namespace icy
{
    struct dcl_dbase;
    static const auto dcl_system_capacity = 64_gb;
    enum class dcl_system_save_mode : uint32_t
    {
        none    =   0x00,
        release =   0x01,    //  live data
        archive =   0x02,    //  main log
        zip     =   0x04,
    };

    class dcl_writer;
    class dcl_system : public event_system
    {
        friend dcl_writer;
        struct tag { };
    public:
        friend error_type create_dcl_system(shared_ptr<dcl_system>& system) noexcept;
        dcl_system(tag) noexcept
        {

        }
        ~dcl_system() noexcept = default;
        error_type exec() noexcept override
        {
            return{};
        }
        error_type signal(const event_data&) noexcept override
        {
            return {};
        }
        error_type save(const string_view filename, const dcl_system_save_mode mode) const noexcept;
        error_type load(const string_view filename, const size_t size = dcl_system_capacity) noexcept;
        //error_type check(map<dcl_index, dcl_error>& errors) const noexcept;
        error_type cache(string& str) const noexcept;
    private:
        enum log_type
        {
            log_by_index,
            log_by_time,
            log_by_user,
            log_by_base,
            log_count,
        };
    private:
        database_system_write m_base;
        database_dbi m_data;
        database_dbi m_binary;
        database_dbi m_log[log_count];
    };

    class dcl_writer
    {
    public: //  read
        explicit operator bool() const noexcept
        {
            return !!m_txn;
        }
        const_array_view<dcl_action> actions() const noexcept
        {
            return m_actions;
        }
        size_t action() const noexcept
        {
            return m_action;
        }
        error_type find_type(const dcl_type type, array<dcl_base>& base) const noexcept;
        error_type find_base(const dcl_index index, dcl_base& base) const noexcept;
        error_type find_name(const dcl_index index, const dcl_index locale, string_view& text) const noexcept;
        error_type find_children(const dcl_index index, array<dcl_base>& vec) const noexcept;
        error_type find_links(const dcl_index index, array<dcl_base>& vec) const noexcept;
        error_type find_binary(const dcl_index index, const_array_view<uint8_t>& binary) const noexcept;
        error_type find_text(const dcl_index index, string_view& text) const noexcept;
        error_type enumerate_logs(array<dcl_index>& indices) const noexcept;
        error_type find_logs_by_time(const dcl_time lower, const dcl_time upper, array<dcl_index>& vec) const noexcept;
        error_type find_logs_by_user(const dcl_index user, array<dcl_index>& vec) const noexcept;
        error_type find_logs_by_base(const dcl_index base, array<dcl_index>& vec) const noexcept;
        error_type find_log_by_index(const dcl_index index, dcl_record& record, array<dcl_action>& vec) const noexcept;
    public: //  write
        error_type initialize(const shared_ptr<dcl_system> system, const string_view path) noexcept;
        error_type create_base(dcl_base& base) noexcept;
        error_type change_flags(const dcl_index index, const dcl_flag flags) noexcept;
        error_type change_value(const dcl_index index, const dcl_value value) noexcept;
        error_type change_name(const dcl_index index, const dcl_index locale, const string_view text) noexcept;
        error_type change_binary(const dcl_index old_index, const const_array_view<uint8_t> bytes, dcl_index& new_index) noexcept;
        error_type redo() noexcept;
        error_type undo() noexcept;
        error_type copy(const string_view path) const noexcept;
        error_type save(const dcl_index user, const string_view text) noexcept;
    private:
         error_type find_base(dcl_index key, dcl_dbase& val, array<dcl_index>* const links = nullptr) const noexcept;
         error_type append(dcl_action action) noexcept;
         error_type save() noexcept;
         error_type modify(dcl_index parent, const dcl_index link, const bool append) noexcept;
         error_type create_base(const dcl_action& action) noexcept;
         error_type change_base(const dcl_index& base, const dcl_value* const value, const dcl_flag* const flags) noexcept;
    private:
        shared_ptr<dcl_system> m_system;
        database_txn_write m_txn;
        mutable database_cursor_write m_data;
        mutable database_cursor_write m_binary;
        mutable database_cursor_read m_log[dcl_system::log_count];
        database_system_write m_cache_base; 
        database_dbi m_cache_dbi;
        array<dcl_action> m_actions;
        size_t m_action = 0;
    };
    inline error_type create_dcl_system(shared_ptr<dcl_system>& system) noexcept
    {
        return make_shared(system, dcl_system::tag());
    }
}

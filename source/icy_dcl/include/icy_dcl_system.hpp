#pragma once

#include <icy_engine/core/icy_event.hpp>
#include <icy_engine/core/icy_array.hpp>
#include <icy_engine/core/icy_map.hpp>
#include <icy_engine/utility/icy_database.hpp>
#include "icy_dcl.hpp"

namespace icy
{
    extern const event_type dcl_event_type;
    static const auto dcl_system_capacity = 64_gb;

    class dcl_reader;
    class dcl_writer;
    class dcl_system : public event_system
    {
        friend dcl_reader;
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
        error_type save_to_zip(const string_view filename) const noexcept;
        error_type save_to_file(const string_view filename) const noexcept;
        error_type load_from_file(const string_view filename, const size_t size = dcl_system_capacity) noexcept;
        error_type load_from_zip(const string_view zipname, const size_t size = dcl_system_capacity) noexcept;
        error_type validate(map<dcl_index, dcl_error>& errors) const noexcept;
    private:
        database_system_write m_base;
    };
    class dcl_reader
    {
    public:
        error_type initialize(const dcl_system& system) noexcept;
        error_type find_base(const dcl_index index, dcl_base& base) noexcept;
        error_type find_name(const dcl_index index, const dcl_index locale, string_view& text) noexcept;
        error_type find_children(const dcl_index index, array<dcl_base>& vec) noexcept;
        error_type find_links(const dcl_index index, array<dcl_base>& vec) noexcept;
        error_type find_binary(const dcl_index index, const_array_view<uint8_t>& binary) noexcept;
        error_type find_text(const dcl_index index, string_view& text) noexcept;
        error_type find_log_by_record(const dcl_index index, dcl_base& base) noexcept;
        error_type find_log_by_time(const dcl_time time, dcl_base& text) noexcept;
        error_type find_log_by_time(const dcl_time lower, const dcl_time upper, array<dcl_base>& vec) noexcept;
        error_type find_log_by_user(const dcl_index user, array<dcl_base>& vec) noexcept;
        error_type find_log_by_index(const dcl_index index, array<dcl_base>& vec) noexcept;
    protected:
        enum log_type
        {
            log_by_record,
            log_by_time,
            log_by_user,
            log_by_index,
            log_count,
        };
    protected:
        database_txn_write m_txn;
        database_cursor_write m_data;
        database_cursor_write m_binary;
        database_cursor_write m_log[log_count];
    };
    class dcl_writer : public dcl_reader
    {
    public:
        const_array_view<dcl_base> data() const noexcept
        {
            return m_data;
        }
        error_type initialize(dcl_system& system) noexcept;
        error_type save(dcl_base& record) noexcept;
        error_type change_flags(const dcl_index index, const dcl_flag flags) noexcept;
        error_type change_name(const dcl_index index, const dcl_index locale, const string_view text) noexcept;
        error_type change_binary(const dcl_index index, const dcl_index locale, const const_array_view<uint8_t> bytes) noexcept;
        error_type change_value(const dcl_index index, const dcl_value value) noexcept;
        error_type create(const dcl_index parent, const dcl_type type, dcl_index& index) noexcept;
    private:
        array<dcl_base> m_data;
    };
    inline error_type create_dcl_system(shared_ptr<dcl_system>& system) noexcept
    {
        return make_shared(system, dcl_system::tag());
    }
}

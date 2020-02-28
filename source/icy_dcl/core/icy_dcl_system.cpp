#include "../include/icy_dcl_system.hpp"

using namespace icy;

error_type dcl_system::load_from_file(const string_view filename, const size_t size) noexcept
{
    return m_base.initialize(filename, size);
}
error_type dcl_system::load_from_zip(const string_view zipname, const size_t size) noexcept
{
    return make_stdlib_error(std::errc::function_not_supported);
}
error_type dcl_system::save_to_zip(const string_view filename) const noexcept
{
    return make_stdlib_error(std::errc::function_not_supported);
}
error_type dcl_system::save_to_file(const string_view filename) const noexcept
{
    return make_stdlib_error(std::errc::function_not_supported);
}
error_type dcl_system::validate(map<dcl_index, dcl_error>& errors) const noexcept
{
    return make_stdlib_error(std::errc::function_not_supported);
}

error_type dcl_reader::initialize(const dcl_system& system) noexcept
{
    return make_stdlib_error(std::errc::function_not_supported);
}
error_type dcl_reader::find_base(const dcl_index index, dcl_base& base) noexcept
{
    return make_stdlib_error(std::errc::function_not_supported);
}
error_type dcl_reader::find_name(const dcl_index index, const dcl_index locale, string_view& text) noexcept
{
    return make_stdlib_error(std::errc::function_not_supported);
}
error_type dcl_reader::find_children(const dcl_index index, array<dcl_base>& vec) noexcept
{
    return make_stdlib_error(std::errc::function_not_supported);
}
error_type dcl_reader::find_links(const dcl_index index, array<dcl_base>& vec) noexcept
{
    return make_stdlib_error(std::errc::function_not_supported);
}
error_type dcl_reader::find_binary(const dcl_index index, const_array_view<uint8_t>& binary) noexcept
{
    return make_stdlib_error(std::errc::function_not_supported);
}
error_type dcl_reader::find_text(const dcl_index index, string_view& text) noexcept
{
    return make_stdlib_error(std::errc::function_not_supported);
}
error_type dcl_reader::find_log_by_record(const dcl_index index, dcl_base& base) noexcept
{
    return make_stdlib_error(std::errc::function_not_supported);
}
error_type dcl_reader::find_log_by_time(const dcl_time time, dcl_base& text) noexcept
{
    return make_stdlib_error(std::errc::function_not_supported);
}
error_type dcl_reader::find_log_by_time(const dcl_time lower, const dcl_time upper, array<dcl_base>& vec) noexcept
{
    return make_stdlib_error(std::errc::function_not_supported);
}
error_type dcl_reader::find_log_by_user(const dcl_index user, array<dcl_base>& vec) noexcept
{
    return make_stdlib_error(std::errc::function_not_supported);
}
error_type dcl_reader::find_log_by_index(const dcl_index index, array<dcl_base>& vec) noexcept
{
    return make_stdlib_error(std::errc::function_not_supported);
}

error_type dcl_writer::initialize(dcl_system& system) noexcept
{
    return make_stdlib_error(std::errc::function_not_supported);
}
error_type dcl_writer::save(dcl_base& record) noexcept
{
    return make_stdlib_error(std::errc::function_not_supported);
}
error_type dcl_writer::change_flags(const dcl_index index, const dcl_flag flags) noexcept
{
    return make_stdlib_error(std::errc::function_not_supported);
}
error_type dcl_writer::change_name(const dcl_index index, const dcl_index locale, const string_view text) noexcept
{
    return make_stdlib_error(std::errc::function_not_supported);
}
error_type dcl_writer::change_binary(const dcl_index index, const dcl_index locale, const const_array_view<uint8_t> bytes) noexcept
{
    return make_stdlib_error(std::errc::function_not_supported);
}
error_type dcl_writer::change_value(const dcl_index index, const dcl_value value) noexcept
{
    return make_stdlib_error(std::errc::function_not_supported);
}
error_type dcl_writer::create(const dcl_index parent, const dcl_type type, dcl_index& index) noexcept
{
    return make_stdlib_error(std::errc::function_not_supported);
}

const event_type icy::dcl_event_type = icy::next_event_user();
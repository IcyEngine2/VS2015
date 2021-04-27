#include "icy_dcl.hpp"
#include <icy_engine/core/icy_string.hpp>

using namespace icy;

const dcl_index icy::dcl_root = dcl_index(0xD3AFED78EEE9475D, 0x8203AD2B6E18697B);
/*const dcl_index icy::dcl_text = dcl_index(0x4836A95961824BBB, 0xB0AA76D5268CC0D0);
const dcl_index icy::dcl_primitive_boolean = dcl_index(0x87B655A5E1EA48DC, 0x8362A7CFC976D75F);
const dcl_index icy::dcl_primitive_decimal = dcl_index(0xA74A3BC3731E40A2, 0x8EBC6E2AD35E0818);
const dcl_index icy::dcl_primitive_string = dcl_index(0xE8F9CF79FE0F4526, 0x88E49CA24A42FA4C);
const dcl_index icy::dcl_primitive_time = dcl_index(0x79788825B5654A75, 0x910C4211A3BFD02D);
*/
static error_type dcl_error_to_string(const unsigned code, const string_view locale, string& str) noexcept
{
    switch (dcl_error(code))
    {
    case dcl_error::database_corrupted:
        return copy("database corrupted"_s, str);
    case icy::dcl_error::invalid_locale:
        return copy("invalid locale"_s, str);
    case dcl_error::invalid_parent_index:
        return copy("invalid parent index"_s, str);
    case dcl_error::invalid_parent_type:
        return copy("invalid parent type"_s, str);
    case dcl_error::invalid_index:
        return copy("invalid index"_s, str);
    case dcl_error::invalid_user:
        return copy("invalid user"_s, str);
    case dcl_error::invalid_binary:
        return copy("invalid binary"_s, str);
    case dcl_error::invalid_type:
        return copy("invalid type"_s, str);
    case dcl_error::invalid_value:
        return copy("invalid value"_s, str);
    }
    return make_stdlib_error(std::errc::invalid_argument);
}
const error_source icy::error_source_dcl = register_error_source("dcl"_s, dcl_error_to_string);
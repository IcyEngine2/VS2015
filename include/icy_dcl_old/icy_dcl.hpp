#pragma once

#include "../icy_engine/core/icy_core.hpp"
#include "../icy_engine/core/icy_string.hpp"

namespace icy
{
    using dcl_index = guid;
    static const auto dcl_max_integer = (1i64 << 0x3C) - 1;
    
    extern const dcl_index dcl_directory;                   //  value: type
    extern const dcl_index dcl_attribute;                   //  value: resource (text)
    extern const dcl_index dcl_locale;                      //  value: -
    extern const dcl_index dcl_role;                        //  value: -
    extern const dcl_index dcl_user;                        //  value: role (default)
    extern const dcl_index dcl_user_access;                 //  value: directory
    extern const dcl_index dcl_access;                      //  value: [access list]
    extern const dcl_index dcl_type;                        //  value: -
    extern const dcl_index dcl_object;                      //  value: -

    extern const dcl_index dcl_type_primitive_boolean;          //  value: -
    extern const dcl_index dcl_type_primitive_decimal;          //  value: size (bits), min, max, prec(0)
    extern const dcl_index dcl_type_primitive_enumeration;      //  value: size (bits)
    extern const dcl_index dcl_type_primitive_enumeration_value;//  value: integer
    extern const dcl_index dcl_type_primitive_bitfield;         //  value: size
    extern const dcl_index dcl_type_primitive_bitfield_value;   //  value: integer
    extern const dcl_index dcl_type_primitive_string;           //  value: -
    extern const dcl_index dcl_type_resource;                   //  value: size (max) 
    extern const dcl_index dcl_type_resource_property;          //  value: dcl_index (string/decimal)
    extern const dcl_index dcl_type_object;                     //  value: -
    extern const dcl_index dcl_type_object_parameter_unique;    //  value: dcl_index (type)
    extern const dcl_index dcl_type_object_parameter_pointer;   //  value: dcl_index (type)
    extern const dcl_index dcl_type_object_parameter_vector;    //  value: dcl_index (type)

    extern const dcl_index dcl_attribute_const;
    extern const dcl_index dcl_attribute_hidden;
    extern const dcl_index dcl_attribute_deleted;

    extern const dcl_index dcl_access_make_directory;
    extern const dcl_index dcl_access_name_directory;
    extern const dcl_index dcl_access_attr_directory;
    extern const dcl_index dcl_access_move_directory;

    extern const dcl_index dcl_access_make_locale;
    extern const dcl_index dcl_access_name_locale;
    extern const dcl_index dcl_access_attr_locale;
    extern const dcl_index dcl_access_move_locale;
    
    extern const dcl_index dcl_access_make_role;
    extern const dcl_index dcl_access_name_role;
    extern const dcl_index dcl_access_attr_role;
    extern const dcl_index dcl_access_edit_role;  //  add/remove access
    extern const dcl_index dcl_access_move_role;

    extern const dcl_index dcl_access_make_user;
    extern const dcl_index dcl_access_name_user;
    extern const dcl_index dcl_access_attr_user;
    extern const dcl_index dcl_access_edit_user;  //  add/remove directory + access
    extern const dcl_index dcl_access_move_user;

    extern const dcl_index dcl_access_make_type;
    extern const dcl_index dcl_access_name_type;
    extern const dcl_index dcl_access_attr_type;
    extern const dcl_index dcl_access_edit_type;  //  add/remove enum/bitfield values; type(obj) parameters
    extern const dcl_index dcl_access_move_type;
    
    extern const dcl_index dcl_access_make_object;
    extern const dcl_index dcl_access_name_object;
    extern const dcl_index dcl_access_attr_object;
    extern const dcl_index dcl_access_edit_object;  //  modify obj parameters
    extern const dcl_index dcl_access_move_object;
    
    extern const dcl_index dcl_root;
    extern const dcl_index dcl_root_locales;
    extern const dcl_index dcl_root_roles;
    extern const dcl_index dcl_root_users;
    extern const dcl_index dcl_root_types;
    extern const dcl_index dcl_root_objects;

    extern const dcl_index dcl_locale_default;
    
    union dcl_value
    {
        dcl_value() noexcept
        {
            memset(this, 0, sizeof(*this));
        }
        struct
        {
            int64_t min : 0x3C;
            int64_t bit : 0x04; //  8 * 2^bit
            int64_t max : 0x3C;
            int64_t prc : 0x04; //  0..15
        } decimal;
        bool boolean;
        int64_t integer;
        dcl_index index;
    };
    struct dcl_base
    {
        dcl_index index;
        dcl_index parent;
        dcl_index type;
        dcl_value value;
    };
    static_assert(sizeof(dcl_base) == 64, "INVALID DCL BASE SIZE");
    
    extern const error_source error_source_dcl;
    enum class dcl_error_code : uint32_t
    {
        none,
        access_denied,
        database_corrupted,
        user_not_found,
        user_already_exists,
        name_empty,
        name_length,
        invalid_locale,
        invalid_role,
    };
    error_type make_dcl_error(const dcl_error_code code);
}
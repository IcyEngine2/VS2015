#pragma once

#include <icy_engine/core/icy_core.hpp>

namespace icy
{
    using dcl_index = guid;
    using dcl_time = std::chrono::system_clock::time_point;
   
    enum class dcl_flag : uint16_t
    {
        none        =   0x00,
        destroyed   =   0x01,
        hidden      =   0x02,
        readonly    =   0x04,
        group       =   0x08,       //  class_property

        create_base     =   0x10,   //  action
        change_flags    =   0x20,   //  action
        change_value    =   0x40,   //  action
        change_binary   =   0x80,   //  action
    };
    constexpr inline dcl_flag operator|(const dcl_flag lhs, const dcl_flag rhs) noexcept
    {
        return dcl_flag(uint32_t(lhs) | uint32_t(rhs));
    }
    constexpr inline bool operator&(const dcl_flag lhs, const dcl_flag rhs) noexcept
    {
        return !!(uint32_t(lhs) & uint32_t(rhs));
    }
    static constexpr auto dcl_flag_action_mask = dcl_flag::create_base
        | dcl_flag::change_flags
        | dcl_flag::change_binary
        | dcl_flag::change_value;
    static constexpr auto dcl_flag_type_mask = dcl_flag(~uint16_t(dcl_flag_action_mask));


    enum class dcl_type : uint16_t
    {
        none,
        directory,      //  value: -
        name,           //  value: -
        user,           //  value: -
        locale,         //  value: -
        enum_class,     //  value: -
        enum_value,     //  value: decimal (unique)
        flags_class,    //  value: -
        flags_value,    //  value: decimal (unique, pow2)
        resource_class, //  value: short string
        
        class_type,
        class_property_decimal,         //  value: decimal (default)        |   change value
        class_property_boolean,         //  value: boolean (default)        |   change value
        class_property_string,          //  value: short string (default)   |   change value
        class_property_enum,            //  value: dcl_index (enum_class)
        class_property_enum_value,      //  value: decimal                  |   change value
        class_property_flags,           //  value: dcl_index (flags_class)
        class_property_flags_value,     //  value: decimal                  |   change value
        class_property_resource,        //  value: dcl_index (resource_class)
        class_property_resource_value,  //  value: dcl_index (binary)       |   change value
        class_property_text,            //  value: dcl_index (binary)       |   change value
        class_property_reference,       //  value: dcl_index (class_type)
        class_property_inline,          //  value: dcl_index (class_type)

        object_type,                    //  value: dcl_index (class_type)
        object_property_decimal,        //  value: dcl_index (class_property_...)
        object_property_decimal_value,  //  value: decimal              |   change value
        object_property_boolean,        //  value: dcl_index (class_property_...)
        object_property_boolean_value,  //  value: boolean              |   change value
        object_property_string,         //  value: dcl_index (class_property_...)
        object_property_string_value,   //  value: short string         |   change value
        object_property_enum,           //  value: dcl_index (class_property_...)
        object_property_enum_value,     //  value: decimal              |   change value
        object_property_flags,          //  value: dcl_index (class_property_...)
        object_property_flags_value,    //  value: decimal              |   change value
        object_property_resource,       //  value: dcl_index (class_property_...)
        object_property_resource_value, //  value: dcl_index (binary)
        object_property_text,           //  value: dcl_index (class_property_...)
        object_property_text_value,     //  value: dcl_index (binary)
        object_property_reference,      //  value: dcl_index (class_property_...)
        object_property_reference_value,//  value: dcl_index (object)   |   change value
        object_property_inline,         //  value: dcl_index (class_property_...)
        object_property_inline_value,   //  value: dcl_index (object)   |   change value
        _count,
    };
    union dcl_value
    {
        friend int compare(const dcl_value& lhs, const dcl_value& rhs) noexcept;
        rel_ops(dcl_value);
        dcl_value() noexcept
        {
            memset(this, 0, sizeof(*this));
        }
        bool boolean;
        uint64_t decimal;
        char string[16];
        dcl_index reference;
        dcl_time time;
    };
    struct dcl_base
    {
        dcl_flag flag = dcl_flag::none;
        dcl_type type = dcl_type::none;
        uint32_t locale = 0;
        dcl_index index;
        dcl_index parent;
        dcl_value value;
    };
    struct dcl_record
    {
        dcl_index index;
        dcl_index text;
        dcl_index user;
        dcl_time time = {};
    };
    struct dcl_action
    {
        dcl_type type = dcl_type::none;
        dcl_flag flag = dcl_flag::none;
        uint32_t locale = 0;
        dcl_index base;
        // create_base: { value, parent }
        // change_flags: { old_flags, new_flags }
        // change_value: { old_value, new_value }
        // change_binary: { old_index (new bytes), new_index (old bytes)
        dcl_value values[2];
    };

    extern const dcl_index dcl_root;

    inline int compare(const dcl_type lhs, const dcl_type rhs) noexcept
    {
        return icy::compare(uint32_t(lhs), uint32_t(rhs));
    }
    inline int compare(const dcl_value& lhs, const dcl_value& rhs) noexcept
    {
        return memcmp(&lhs, &rhs, sizeof(dcl_value));
    }

    enum class dcl_error : uint32_t
    {
        none,
        database_corrupted,
        invalid_locale,
        invalid_parent_index,
        invalid_parent_type,
        invalid_index,
        invalid_user,
        invalid_binary,
        invalid_type,
        invalid_value,
    };
    extern const error_source error_source_dcl;
    inline error_type make_dcl_error(const dcl_error error) noexcept
    {
        return error_type(uint32_t(error), error_source_dcl);
    }

    inline dcl_base dcl_base_from_action(const dcl_action& action) noexcept
    {
        dcl_base base;
        if (action.flag & dcl_flag::create_base)
        {
            base.index = action.base;
            base.parent = action.values[1].reference;
            base.value = action.values[0];
            base.locale = action.locale;
            base.type = action.type;
            base.flag = dcl_flag(uint16_t(action.flag) & uint16_t(dcl_flag_type_mask));
        }
        return base;
    }
    inline dcl_action dcl_action_from_base(const dcl_base& base) noexcept
    {
        dcl_action action;
        action.base = base.index;
        action.flag = base.flag | dcl_flag::create_base;
        action.locale = base.locale;
        action.type = base.type;
        action.values[1].reference = base.parent;
        action.values[0] = base.value;
        return action;
    }
}
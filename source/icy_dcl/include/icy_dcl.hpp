#pragma once

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
    };
    enum class dcl_type : uint16_t
    {
        none,
        directory,
        user,
        locale,
        extention,
        boolean,
        decimal,
        enumeration,
        bitflags,
        string,
        resource,
        dclass,
        name,
        value,
        group,
        reference,
        object,
        record,
        action,
    };
    union dcl_value
    {
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

    const dcl_index dcl_root;
    const dcl_index dcl_text;

    enum class dcl_error
    {
        none,
    };
    extern const error_source error_source_dcl;
}
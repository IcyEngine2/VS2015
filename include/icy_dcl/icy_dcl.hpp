#pragma once

// #include <icy_engine/core/icy_array_view.hpp>
#include <icy_engine/core/icy_memory.hpp>
#include <icy_engine/core/icy_map.hpp>
#include <icy_gui/icy_gui.hpp>

#define ICY_DCL_API_BINARY_VERSION 0x0000'0005

//#include <icy_engine/core/icy_event.hpp>
//#include <icy_engine/utility/icy_database.hpp>

namespace icy
{
    using dcl_index = guid;
    enum class dcl_base_type : uint32_t
    {
        none,
        directory,
        user,
        type,
        resource,
        list,
        bits,
        object,
        connection,
    };
    enum class dcl_object_type : uint32_t
    {
        none,
        user,
        resource,
        list,
        flags,
        complex,
    };
    enum class dcl_action_type : uint32_t
    {
        none,

        rename,
        remove,

        create_directory,
        create_user,
        create_type,
        create_resource,
        create_list,
        create_bits,
        create_object,
        create_connection,

        modify_resource,
        
        create_parameter,
        modify_parameter,
        rename_parameter,
        delete_parameter,
    };
    enum class dcl_parameter_type : uint32_t
    {
        none,
        
        user_access,

        resource_type,

        list_value,
        bits_value,

        object_local_bool,
        object_local_int64,
        object_local_float,
        object_local_list,
        object_local_bits,
        object_local_object,
        object_local_connection,

        object_ref_type,
        object_ref_object,

        conn_beg,
        conn_end,        
    };

    union dcl_value
    {
        dcl_value() noexcept
        {
            memset(this, 0, sizeof(*this));
        }
        bool as_boolean;
        int64_t as_int64;
        double as_float;
        dcl_index as_index;
    };
    struct dcl_locale;
    template<> inline int compare(const dcl_locale& lhs, const dcl_locale& rhs) noexcept;
    struct dcl_locale
    {
        rel_ops(dcl_locale);
        uint32_t lang = 0;
    };
    template<> inline int compare(const dcl_locale& lhs, const dcl_locale& rhs) noexcept
    {
        return icy::compare(uint32_t(lhs.lang), uint32_t(rhs.lang));
    }
    
    struct dcl_parameter
    {
        dcl_parameter_type type = dcl_parameter_type::none;
        string name;
        dcl_value value;
    };
    struct dcl_base
    {
        virtual ~dcl_base() noexcept = 0
        {

        }
        virtual dcl_index index() const noexcept = 0;
        virtual dcl_base_type type() const noexcept = 0;
        virtual const dcl_base* parent() const noexcept = 0;
        virtual string_view name() const noexcept = 0;
        virtual const_array_view<uint8_t> binary(const dcl_locale locale) const noexcept = 0;
        virtual const dcl_parameter* param(const dcl_index index) const noexcept = 0;
    };

    struct dcl_state_enum
    {
        enum : uint32_t
        {
            _default        =   0x00,
            is_invalid      =   0x01,
            is_disabled     =   0x02,
        };
    };
    using dcl_state = decltype(dcl_state_enum::_default);


    namespace detail
    {
        struct dcl_action_base_header
        {
            dcl_action_type action = dcl_action_type::none;
            dcl_index index;
            dcl_index parent;
            dcl_parameter_type param = dcl_parameter_type::none;
            dcl_value value;
            dcl_locale locale;
        };
        struct dcl_action_group_header
        {
            dcl_index user;
            uint32_t state = dcl_state::_default;
        };
    }
    struct dcl_action_base : detail::dcl_action_base_header
    {
        string name;
        array<uint8_t> binary;
    };
    struct dcl_action_group : detail::dcl_action_group_header
    {
        clock_type::time_point time = clock_type::now();
        array<dcl_action_base> data;
    };

    inline error_type copy(const dcl_parameter& src, dcl_parameter& dst) noexcept
    {
        dst.type = src.type;
        ICY_ERROR(copy(src.name, dst.name));
        dst.value = src.value;
        return error_type();
    }
    /*inline error_type copy(const dcl_action& src, dcl_action& dst) noexcept
    {
        static_cast<detail::dcl_action_header&>(dst) = src;
        ICY_ERROR(copy(src.binary, dst.binary));
        ICY_ERROR(copy(src.name, dst.name));
        return error_type(); 
    }
    inline error_type copy(const dcl_group& src, dcl_group& dst) noexcept
    {
        static_cast<detail::dcl_group_header&>(dst) = src;
        return copy(src.data, dst.data);
    }*/


    class dcl_project
    {
    public:
        virtual ~dcl_project() noexcept = 0
        {

        }
        virtual string_view name() const noexcept = 0;
        virtual error_type tree_view(gui_data_write_model& model, const gui_node node, const dcl_index index) noexcept = 0;        
        virtual const_array_view<dcl_action_group> actions() const noexcept = 0;
        virtual error_type create_directory(const dcl_index parent, const string_view name, dcl_index& index) noexcept = 0;
    };
    class dcl_system
    {
    public:
        virtual dcl_locale locale(const string_view lang) const noexcept = 0;
        virtual error_type get_projects(array<string>& names) const noexcept = 0;
        virtual error_type add_project(const string_view name, shared_ptr<dcl_project>& project) noexcept = 0;
        virtual error_type del_project(dcl_project& project) noexcept = 0;
    };

    error_type create_dcl_system(shared_ptr<dcl_system>& system, const string_view path, const size_t capacity) noexcept;

    extern const error_source error_source_dcl;
    extern const error_type dcl_error_corrupted_data;
    extern const error_type dcl_error_invalid_index;
    extern const error_type dcl_error_invalid_parent;
    extern const error_type dcl_error_invalid_type;
    extern const error_type dcl_error_invalid_value;
}
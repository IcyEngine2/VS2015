#pragma once

// #include <icy_engine/core/icy_array_view.hpp>
#include <icy_engine/core/icy_memory.hpp>
#include <icy_gui/icy_gui.hpp>
//#include <icy_engine/core/icy_event.hpp>
//#include <icy_engine/utility/icy_database.hpp>

namespace icy
{
    using dcl_index = guid;
    enum class dcl_type : uint32_t
    {
        none,
        root,
        dir,        //  directory
        txt,        //  text, localized
        usr,        //  user
        bin,        //  binary, localized
        enm,        //  user enum
        obj,        //  object
        con,        //  connection
        par,        //  parameter (object, connection)
    };
    enum class dcl_action_type : uint32_t
    {
        none,
        create,
        rename,
        modify,
        remove,
    };
    struct dcl_event
    {
        dcl_action_type type = dcl_action_type::none;
        dcl_index index;
    };
    class dcl_system : public event_system
    {
    public:
        virtual const icy::thread& thread() const noexcept = 0;
        icy::thread& thread() noexcept
        {
            return const_cast<icy::thread&>(static_cast<const dcl_system*>(this)->thread());
        }
        virtual error_type bind(gui_data_write_model& model, const gui_node node, const dcl_index index) noexcept = 0;
        virtual error_type get_projects(array<string>& names) const noexcept = 0;
        virtual error_type add_project(const string_view name) noexcept = 0;
        virtual error_type del_project(const string_view name) noexcept = 0;
        virtual error_type set_project(const string_view name) noexcept = 0;
    };

    extern const event_type event_type_dcl;    
    error_type create_dcl_system(shared_ptr<dcl_system>& system, const string_view path, const size_t capacity) noexcept;
}
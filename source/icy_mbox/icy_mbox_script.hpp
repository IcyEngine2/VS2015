#pragma once

#include <icy_engine/core/icy_json.hpp>
#include <icy_engine/core/icy_input.hpp>


namespace mbox
{
    enum class type : uint32_t
    {
        none,
        directory,
        input,
        variable,
        timer,
        event,
        command,    //  group of actions
        binding,
        group,      //  group of profiles
        profile,    //  ref to command
        list_inputs,
        list_variables,        
        list_timers,
        list_events,
        list_bindings,
        list_commands,
        _total,
    };
    enum class input_type : uint32_t
    {
        none,
        keyboard,
        mouse,
        macro,
        _total,
    };
    enum class variable_type : uint32_t
    {
        none,
        boolean,
        integer,
        counter,
        string,
        _total,
    };
    enum class event_type : uint32_t
    {
        none,
        variable_changed,
        variable_equal,
        variable_not_equal,
        variable_greater,
        variable_lesser,
        variable_greater_or_equal,
        variable_lesser_or_equal,
        variable_str_contains,
        recv_input,
        button_press,
        button_release,
        timer_first,
        timer_tick,
        timer_last,
        focus_acquire,
        focus_release,
        _total,
        //load_profile,
    };
    enum class action_type : uint32_t
    {
        none,
        variable_assign,
        variable_inc,
        variable_dec,
        execute_command,
        timer_start,
        timer_stop,
        timer_pause,
        timer_resume,
        send_input,
        send_macro,
        join_group,
        leave_group,
        _total,
    };
    struct value_input
    {
        mbox::input_type itype = mbox::input_type::none;
        icy::key kbutton = icy::key::none;
        icy::key_event kevent = icy::key_event::none;
        icy::mouse_event mevent = icy::mouse_event::none;
        icy::mouse_button mbutton = icy::mouse_button::left;
        icy::key_mod ctrl = 0;
        icy::key_mod alt = 0;
        icy::key_mod shift = 0;
        icy::string macro;
    };
    struct value_directory
    {
        icy::array<icy::guid> indices;
    };
    struct value_variable
    {
        icy::guid group;
        variable_type vtype = variable_type::none;
        bool boolean = false;
        int64_t integer = 0;
        icy::string string;
    };
    struct value_timer
    {
        icy::guid group;
        icy::duration_type period = {};
        icy::duration_type offset = {};
    };
    struct value_event
    {
        icy::guid group;
        event_type etype = event_type::none;
        icy::guid reference;    // lhs input/variable/button/timer/focus(profile)
        icy::guid variable;
    };
    struct value_command
    {
        struct action
        {
            action_type atype = action_type::none;
            icy::guid group;
            icy::guid reference;
        };
        icy::guid group;
        icy::array<action> actions;
    };
    struct value_binding
    {
        icy::guid group;
        icy::guid event;
        icy::guid command;
    };
    struct value_group
    {
        icy::array<icy::guid> profiles;
    };
    struct value_profile
    {
        icy::guid command;
    };
    struct base
    {
        base() noexcept
        {

        }
        icy::guid parent;
        icy::guid index;
        icy::string name;
        type type = type::none;
        struct value_type
        {
            value_directory directory;
            value_input input;
            value_variable variable;
            value_timer timer;
            value_event event;
            value_command command;
            value_binding binding;
            value_group group;
            value_profile profile;
        } value;
    };
    extern const icy::guid root;
    extern const icy::guid group_broadcast;    //  all
    extern const icy::guid group_multicast;    //  all \ self
    extern const icy::guid group_default;      //  local(self)
    
    class library
    {
    public:
        class remove_query
        {
            friend library;
        public:
            icy::array_view<icy::guid> indices() const noexcept
            {
                return m_indices;
            }
            icy::error_type commit() noexcept;
        private:
            library& m_library;
            icy::array<icy::guid> m_indices;
        };
    public:
        icy::error_type initialize() noexcept;
        icy::error_type load_from(const icy::string_view filename) noexcept;
        icy::error_type save_to(const icy::string_view filename) noexcept;
        const base* find(const icy::guid& index) const noexcept
        {
            return m_data.try_find(index);
        }
        icy::error_type remove(const icy::guid& index, remove_query& query) noexcept;
        icy::error_type insert(const base& base) noexcept;   
        icy::error_type modify(const base& base) noexcept;
        icy::error_type children(const icy::guid& index, const bool recursive, icy::array<icy::guid>& vec) noexcept;
        icy::error_type references(const icy::guid& index, icy::array<icy::guid>& vec) noexcept;
    private:
        icy::error_type to_json(const base& base, icy::json& json_output) noexcept;
        icy::error_type from_json(const icy::json& json_input, base& output) noexcept;
    private:
        icy::map<icy::guid, base> m_data;
    };

    icy::string_view to_string(const mbox::type type) noexcept;
    icy::string_view to_string(const mbox::input_type type) noexcept;
    icy::string_view to_string(const mbox::variable_type type) noexcept;
    icy::string_view to_string(const mbox::event_type type) noexcept;
    icy::string_view to_string(const mbox::action_type type) noexcept;
}
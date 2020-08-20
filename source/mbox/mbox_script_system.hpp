#pragma once

#include <icy_engine/core/icy_string.hpp>
#include <icy_engine/core/icy_array.hpp>
#include <icy_engine/core/icy_event.hpp>
#include <icy_engine/core/icy_input.hpp>
#include <icy_engine/core/icy_map.hpp>
#include <icy_engine/core/icy_set.hpp>
#include <icy_engine/utility/icy_lua.hpp>

namespace mbox
{
    class mbox_object;
    enum class mbox_type : uint32_t;
}
namespace icy
{
    template<> inline int compare<mbox::mbox_object>(const mbox::mbox_object& lhs, const mbox::mbox_object& rhs) noexcept;
    template<> inline int compare<mbox::mbox_type>(const mbox::mbox_type& lhs, const mbox::mbox_type& rhs) noexcept;
    class database_txn_read;
    class thread;
}

namespace mbox
{
    using icy::compare;
    using mbox_index = icy::guid;
    enum class mbox_error : uint32_t
    {
        success,
        database_corrupted,

        invalid_object_value,
        invalid_object_type,
        invalid_object_name,
        invalid_object_index,
        invalid_object_references,

        invalid_parent_index,
        invalid_parent_type,

        execute_lua,
        not_enough_macros,
        stack_recursion,
        too_many_actions,
        too_many_operations,        
    };

    extern const icy::event_type mbox_event_type_send_input;

    struct mbox_event_send_input
    {
        mbox_index character;
        double priority = 0;
        icy::array<icy::input_message> messages;
    };

    enum class mbox_type : uint32_t
    {
        none,
        directory,
        profile,
        account,
        character,
        party,
        group,
        layout,
        style,
        action_timer,
        action_macro,
        action_script,
        event,
        action_var_macro,
        _last,
    };
    class mbox_object
    {
    public:
        rel_ops(mbox_object);
        explicit operator bool() const noexcept
        {
            return index != mbox_index() && type != mbox_type::none;
        }
        mbox_index index;
        mbox_type type = mbox_type::none;
        mbox_index parent;
        icy::string name;
        icy::array<char> value;
        icy::map<icy::string, mbox_index> refs;
    }; 
    enum class mbox_reserved_name : uint32_t
    {
        add_character,
        join_group,
        leave_group,
        run_script,
        send_key_press,
        send_key_release,
        send_key_click,
        send_macro,
        send_var_macro,
        post_event,
        on_key_press,
        on_key_release,
        on_event,
        on_timer,
        character,
        group,
        _count,
    };

    using mbox_errors = icy::map<mbox_index, icy::array<icy::error_type>>;
    struct mbox_macro
    {
        icy::key key = icy::key::none;
        icy::key_mod mods = icy::key_mod::none;
    };
    class mbox_array
    {
    public:
        const mbox_object& find(const mbox_index& index) const noexcept
        {
            static const mbox_object null;
            const auto ptr = m_objects.try_find(index);
            return ptr ? *ptr : null;
        }
        icy::error_type path(const mbox_index& index, icy::string& str) const noexcept;
        icy::error_type rpath(const mbox_index& index, icy::string& str) const noexcept;
        icy::error_type enum_by_type(const mbox_index& parent, const mbox_type type, icy::array<const mbox_object*>& objects) const noexcept;
        icy::error_type enum_children(const mbox_index& parent, icy::array<const mbox_object*>& objects) const noexcept;
        icy::error_type enum_dependencies(const mbox_object& object, icy::set<const mbox_object*>& tree) const noexcept;
        icy::error_type validate(const mbox_object& object, bool& valid) const noexcept;
        icy::error_type validate(const mbox_object& object, icy::array<icy::error_type>& errors) const noexcept;
        icy::error_type initialize(const icy::string_view file_path, mbox_errors& errors) noexcept;
        const icy::map<mbox_index, mbox_object>& objects() const noexcept
        {
            return m_objects;
        }
        const icy::const_array_view<mbox_macro> macros() const noexcept
        {
            return m_macros;
        }
    protected:
        icy::error_type initialize(const icy::database_txn_read& txn, mbox_errors& errors) noexcept;
        icy::error_type save(const icy::string_view path, const size_t max_size) const noexcept;
    protected:
        icy::map<mbox_index, mbox_object> m_objects;
        icy::array<mbox_macro> m_macros;
    };
    
    struct mbox_system_info
    {
        struct character_data
        {
            mbox_index index;
            uint32_t slot = 0;
            icy::string name;
            icy::array<icy::string> macros;
            icy::array<icy::input_message> events;
        };
        icy::array<character_data> characters;
        icy::error_type update_wow_addon(const mbox_array& data, const icy::string_view path) const noexcept;
    };

    using mbox_print_func = icy::error_type(*)(void* pdata, const icy::string_view str);

    struct mbox_system_init
    {
        mbox_print_func pfunc = nullptr;
        void* pdata = nullptr;
        mbox_index party;
        uint32_t max_events = 0x1000;
        uint32_t max_timers = 0x1000;
        uint32_t max_actions = 0x1000;
        uint32_t max_lua_op = 0x10000;
    };
    class mbox_system : public icy::event_system
    {
    public:
        virtual const mbox_system_info& info() const noexcept = 0;
        virtual const icy::thread& thread() const noexcept = 0;
        virtual icy::thread& thread() noexcept
        {
            return const_cast<icy::thread&>(static_cast<const mbox_system*>(this)->thread());
        }
        virtual icy::error_type post(const mbox_index& character, const icy::input_message& input) noexcept = 0;
    };

    icy::error_type mbox_rpath(const icy::map<mbox_index, mbox_object>& objects, const mbox_index& index, icy::string& str);
}
namespace icy
{
    extern const error_source error_source_mbox;
    inline error_type make_mbox_error(const mbox::mbox_error code) noexcept
    {
        return error_type(uint32_t(code), error_source_mbox);
    }
    inline error_type copy(const mbox::mbox_object& src, mbox::mbox_object& dst) noexcept
    {
        dst.index = src.index;
        dst.parent = src.parent;
        dst.type = src.type;
        ICY_ERROR(copy(src.name, dst.name));
        ICY_ERROR(copy(src.value, dst.value));
        ICY_ERROR(copy(src.refs, dst.refs));
        return error_type();
    }
    template<> inline int compare<mbox::mbox_object>(const mbox::mbox_object& lhs, const mbox::mbox_object& rhs) noexcept
    {
        if (lhs.type == rhs.type)
            return compare(lhs.name, rhs.name);

        const auto get_level = [](const mbox::mbox_type type)
        {
            switch (type)
            {
            case mbox::mbox_type::directory:
                return 0;

            case mbox::mbox_type::profile:
                return 10;

            case mbox::mbox_type::account:
                return 20;

            case mbox::mbox_type::party:
                return 25;

            case mbox::mbox_type::character:
                return 30;

            default:
                return 40;
            }
        };
        const auto lhs_level = get_level(lhs.type);
        const auto rhs_level = get_level(rhs.type);
        if (lhs_level == rhs_level)
            return compare(lhs.name, rhs.name);
        else if (lhs_level < rhs_level)
            return -1;
        else
            return +1;
    }
    template<> inline int compare<mbox::mbox_type>(const mbox::mbox_type& lhs, const mbox::mbox_type& rhs) noexcept
    {
        return icy::compare<uint32_t>(uint32_t(lhs), uint32_t(rhs));
    }
    template<> inline int compare<const mbox::mbox_object*>(const mbox::mbox_object* const& lhs, const mbox::mbox_object* const& rhs) noexcept
    {
        return icy::compare<size_t>(size_t(lhs), size_t(rhs));
    }

    error_type create_event_system(shared_ptr<mbox::mbox_system>& system, const mbox::mbox_array& data, const mbox::mbox_system_init& init) noexcept;
    string_view to_string(const mbox::mbox_type type) noexcept;
    string_view to_string(const mbox::mbox_reserved_name name) noexcept;
}
#pragma once

#include <icy_engine/core/icy_string.hpp>
#include <icy_engine/core/icy_array.hpp>
#include <icy_engine/core/icy_event.hpp>
#include <icy_engine/core/icy_input.hpp>
#include <icy_engine/core/icy_map.hpp>
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
    };

    extern const icy::event_type mbox_event_type_send_input;

    struct mbox_event_send_input
    {
        mbox_index character;
        icy::array<icy::input_message> messages;
    };
    struct mbox_event_print_debug
    {
        mbox_index character;
        icy::string message;
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
        _last,
    };
    class mbox_object
    {
    public:
        rel_ops(mbox_object);
        mbox_index index;
        mbox_type type = mbox_type::none;
        mbox_index parent;
        icy::string name;
        icy::string value;
        icy::map<icy::string, mbox_index> refs;
    }; 

    struct mbox_action_target
    {
        enum
        {
            type_none,
            type_party,
            type_character,
            type_group,
        } type = type_none;
        enum
        {
            others      =   -1,
            everyone    =   +0,
        } mod = everyone;
        uint32_t slot = 0;
        mbox_index index;
    };
    enum class mbox_action_type : uint32_t
    {
        //  [target] is number: party slot | or guid: character or group | or binary blob: mbox.Everyone(Group) or mbox.Others(Group) 
        //  [target] is not optional if caller source is party (initialize only)
        //  if [target] is nil, than assume target==caller source (character)
        none,
        add_character,      //  exec: party |   args: unique character, unique slot (index)
        join_group,         //  exec: any   |   args: group, [target]
        leave_group,        //  exec: any   |   args: group, [target]
        run_script,         //  exec: any   |   args: script, [target]
        send_key_down,      //  exec: script|   args: "Mods+Key", [target] 
        send_key_up,        //  exec: script|   args: "Mods+Key", [target] 
        send_key_press,     //  exec: script|   args: "Mods+Key", [target] 
        send_macro,         //  exec: script|   args: macro, [target]
        send_macro_follow,  //  exec: script|   args: mbox.Follow, who, [target]
        send_macro_assist,  //  exec: script|   args: mbox.Assist, who, [target]
        send_macro_target,  //  exec: script|   args: mbox.Target, who, [target]
        send_macro_focus,   //  exec: script|   args: mbox.Focus, who, [target]
        post_event,         //  exec: any   |   args: event, [target]
        on_key_down,        //  exec: any   |   args: "Mods+Key", LUA function, [target]
        on_key_up,          //  exec: any   |   args: "Mods+Key", LUA function, [target]
        on_event,           //  exec: any   |   args: event, [target]
    };
    enum class mbox_reserved_name : uint32_t
    {
        add_character,
        join_group,
        leave_group,
        run_script,
        send_key_down,
        send_key_up,
        send_key_press,
        send_macro,
        post_event,
        on_key_down,
        on_key_up,
        on_event,
        everyone,
        others,
        follow,
        assist,
        target,
        focus,
        _count,
    };
    extern const char* mbox_reserved_names[uint32_t(mbox_reserved_name::_count)];

    struct mbox_action
    {
        mbox_action_type type = mbox_action_type::none;
        mbox_action_target target;
        mbox_index ref;
        icy::input_message input;
        icy::lua_variable callback;
        uint32_t slot = 0;
        mbox_action_target var_macro;
    };

    using mbox_errors = icy::map<mbox_index, icy::array<icy::error_type>>;

    class mbox_array
    {
    public:
        const mbox_object* find(const mbox_index& index) const noexcept
        {
            return m_objects.try_find(index);
        }
        icy::error_type path(const mbox_index& index, icy::string& str) const noexcept;
        icy::error_type rpath(const mbox_index& index, icy::string& str) const noexcept;
        icy::error_type enum_by_type(const mbox_index& parent, const mbox_type type, icy::array<const mbox_object*>& objects) const noexcept;
        icy::error_type enum_children(const mbox_index& parent, icy::array<const mbox_object*>& objects) const noexcept;
        icy::error_type validate(const mbox_object& object, bool& valid) const noexcept;
        icy::error_type validate(const mbox_object& object, icy::array<icy::error_type>& errors) const noexcept;
        icy::error_type initialize(const icy::string_view file_path, mbox_errors& errors) noexcept;
        const icy::map<mbox_index, mbox_object>& objects() const noexcept
        {
            return m_objects;
        }
        icy::error_type to_string(const mbox_index context, const mbox_action& action, icy::string& str) const noexcept;
    protected:
        icy::error_type initialize(const icy::database_txn_read& txn, mbox_errors& errors) noexcept;
        icy::error_type save(const icy::string_view path, const size_t max_size) const noexcept;
    protected:
        icy::map<mbox_index, mbox_object> m_objects;
    };
    class mbox_function
    {
    public:
        mbox_function() noexcept = default;
        mbox_function(const icy::map<mbox_index, mbox_object>& objects, const mbox_object& object, const icy::lua_system& lua) noexcept :
            m_objects(&objects), m_object(&object), m_lua(&lua)
        {

        }
        mbox_function(mbox_function&& rhs) noexcept : m_objects(rhs.m_objects), m_object(rhs.m_object), m_lua(rhs.m_lua),
            m_func(std::move(rhs.m_func)), m_actions(std::move(rhs.m_actions))
        {

        }
        ICY_DEFAULT_MOVE_ASSIGN(mbox_function);
        icy::error_type initialize() LUA_NOEXCEPT;
        icy::error_type operator()(icy::array<mbox_action>& actions) LUA_NOEXCEPT;
        explicit operator bool() const noexcept
        {
            return m_func.type() != icy::lua_type::none;
        }
        const mbox_object& object() const noexcept
        {
            return *m_object;
        }
    private:
        icy::error_type make_target(const icy::lua_variable& input, mbox_action_target& target) const LUA_NOEXCEPT;
        icy::error_type make_index(const icy::lua_variable& input, const icy::const_array_view<mbox_type> types, mbox_index& index) const LUA_NOEXCEPT;
        icy::error_type make_input(const icy::lua_variable& input, icy::input_message& msg) const LUA_NOEXCEPT;
    private:
        const icy::map<mbox_index, mbox_object>* const m_objects = nullptr;
        const mbox_object* const m_object = nullptr;
        const icy::lua_system* const m_lua = nullptr;
        icy::lua_variable m_func;
        icy::array<mbox_action> m_actions;
    };

    struct mbox_system_info
    {
        struct character_data
        {
            mbox_index index;
            uint32_t position = 0;
        };
        icy::array<character_data> characters;
    };

    using mbox_print_func = icy::error_type(*)(void* pdata, const icy::string_view str);
    class mbox_system : public icy::event_system
    {
    public:
        virtual const mbox_system_info& info() const noexcept = 0;
        virtual icy::error_type cancel() noexcept = 0;
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

    error_type create_event_system(shared_ptr<mbox::mbox_system>& system, const mbox::mbox_array& data,
        const mbox::mbox_index& party, const mbox::mbox_print_func pfunc = nullptr, void* const pdata = nullptr) noexcept;
    string_view to_string(const mbox::mbox_type type) noexcept;
}
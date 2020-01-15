#pragma once

#include <icy_engine/core/icy_json.hpp>
#include <icy_engine/core/icy_input.hpp>

namespace mbox
{
    extern const icy::error_source error_source_mbox;
    enum class mbox_error_code : uint32_t
    {
        success,
        invalid_version,
        invalid_index,
        invalid_name,
        invalid_parent,
        invalid_group,
        invalid_command,
        invalid_root,
    };
    inline icy::error_type make_mbox_error(const mbox_error_code code) noexcept
    {
        return icy::error_type(uint32_t(code), error_source_mbox);
    }

    enum class type : uint32_t
    {
        directory,
        group,
        command,
        character,
       /* timer,
        string,
        integer,
        boolean,
        counter,*/
        _total,
    };
    enum class action_type : uint32_t
    {
        none,
        group_join,             //  group
        group_leave,            //  group
        button_press,           //  input
        button_release,         //  input
        button_click,           //  input
        command_execute,        //  command
        command_replace,        //  command A, command B,
        begin_broadcast,        //  next actions to group; include self
        begin_multicast,        //  next actions to group; exclude self
        end_broadcast,
        end_multicast,
        on_button_press,        //  event; command
        on_button_release,      //  event; command
        _total,
    };
    inline constexpr auto button_lmb() noexcept { return icy::key(0x01); }
    inline constexpr auto button_rmb() noexcept { return icy::key(0x02); }
    inline constexpr auto button_mid() noexcept { return icy::key(0x03); }
    inline constexpr auto button_x1() noexcept { return icy::key(0x04); }
    inline constexpr auto button_x2() noexcept { return icy::key(0x05); }
    inline constexpr auto button_key(const icy::key key) noexcept { return key; }
    
    struct button
    {
        icy::key key = icy::key::none;
        icy::key_mod mod = icy::key_mod::none;
    };

    struct action
    {
        action() noexcept
        {
            memset(this, 0, sizeof(*this));
        }
        action_type type = action_type::none;
        union
        {
            button button;
            icy::guid reference;    // group/command
            struct
            {
                icy::guid source;
                icy::guid target;
            } replace;
            struct
            {
                union
                {
                    mbox::button button;
                } event;
                icy::guid command;
            } connect;
        };
    };
    struct base
    {
        static icy::error_type copy(const base& src, base& dst) noexcept
        {
            dst.type = src.type;
            dst.index = src.index;
            dst.parent = src.parent;
            ICY_ERROR(icy::to_string(src.name, dst.name));
            ICY_ERROR(dst.actions.assign(src.actions));
            return {};
        }
        type type = type::directory;
        icy::guid index;
        icy::guid parent;
        icy::string name;
        icy::array<action> actions;
    };
    
    class transaction;

    static const icy::guid root = icy::guid(0x8C8C31B81A56400F, 0xB79ADBFE202E4FFC);
    class library 
    {
        friend transaction;
    public:
        icy::error_type initialize() noexcept
        {
            icy::map<icy::guid, base> data;
            mbox::base root;
            root.index = mbox::root;
            root.type = mbox::type::directory;
            ICY_ERROR(to_string("Root", root.name));
            ICY_ERROR(data.insert(root.index, std::move(root)));
            m_version = 0;
            m_data = std::move(data);
            return {};
        }
        icy::error_type load_from(const icy::string_view filename) noexcept
        {
            return icy::make_stdlib_error(std::errc::function_not_supported);
        }
        icy::error_type save_to(const icy::string_view filename) noexcept
        {
            return icy::make_stdlib_error(std::errc::function_not_supported);
        }
        const base* find(const icy::guid& index) const noexcept
        {
            return m_data.try_find(index);
        }
        icy::error_type enumerate(const mbox::type type, icy::array<icy::guid>& indices) const noexcept
        {
            for (auto&& pair : m_data)
            {
                if (pair.value.type == type)
                    ICY_ERROR(indices.push_back(pair.key));
            }
            return {};
        }
        icy::error_type enumerate(const mbox::type type, icy::array<mbox::base>& vals) const noexcept
        {
            for (auto&& pair : m_data)
            {
                if (pair.value.type == type)
                {
                    mbox::base copy;
                    ICY_ERROR(mbox::base::copy(pair.value, copy));
                    ICY_ERROR(vals.push_back(std::move(copy)));
                }
            }
            return {};
        }
        icy::error_type children(const icy::guid& index, const bool recursive, icy::array<icy::guid>& vec) const noexcept
        {
            for (auto&& pair : m_data)
            {
                if (pair.value.parent == index)
                {
                    ICY_ERROR(vec.push_back(pair.key));
                    if (recursive && pair.value.type == mbox::type::directory)
                        ICY_ERROR(children(pair.key, true, vec));
                }
            }
            return {};
        }
        icy::error_type references(const icy::guid& index, icy::array<icy::guid>& vec) noexcept 
        {
           /* for (auto&& pair : m_data)
            {
                if (pair.value.type == mbox::type::group)
                {
                    array<guid> vec_children;
                    ICY_ERROR(children(pair.key, false, vec_children));
                    for (auto&& child : vec_child)
                    {
                        references(index, )
                    }
                }
            }*/
            return icy::make_stdlib_error(std::errc::function_not_supported);
        }
        icy::error_type path(const icy::guid& index, icy::string& str) const noexcept
        {
            return icy::make_stdlib_error(std::errc::function_not_supported);
        }
        icy::error_type reverse_path(const icy::guid& index, icy::string& str) const noexcept 
        {
            return icy::make_stdlib_error(std::errc::function_not_supported);
        }
        static icy::error_type copy(const mbox::library& src, mbox::library& dst) noexcept
        {
            dst.m_data.clear();
            for (auto&& pair : src.m_data)
            {
                mbox::base tmp;
                ICY_ERROR(mbox::base::copy(pair.value, tmp));
                ICY_ERROR(dst.m_data.insert(icy::guid(pair.key), std::move(tmp)));
            }
            dst.m_version = src.m_version;
            return {};
        }
    private:
      /*  icy::error_type is_valid(const base& base) const noexcept
        {
            if (base.type == type::command || base.type == type::character)
            {
                for (auto&& action : base.actions)
                {
                    switch (action.type)
                    {
                    case action_type::group_join:
                    case action_type::group_leave:
                    case action_type::begin_broadcast:
                    case action_type::begin_multicast:
                    case action_type::end_broadcast:
                    case action_type::end_multicast:
                    {
                        if (action.reference != guid())
                        {
                            const auto group = try_find(action.reference);
                            if (!group || group->type != mbox::type::group)
                                return make_mbox_error()
                        }
                        break;
                    }
                    case action_type::command_execute:
                    {

                        break;
                    }
                    case action_type::command_replace:
                    {
                        const auto source = try_find(action.replace.source);

                    }

                    default:
                        break;
                    }
                }
            }
        }*/
        icy::error_type to_json(const base& base, icy::json& json_output) noexcept;
        icy::error_type from_json(const icy::json& json_input, base& output) noexcept;
        icy::error_type validate(base& base, icy::map<icy::guid, mbox::mbox_error_code>& errors) noexcept;
        icy::error_type validate(icy::map<icy::guid, mbox::mbox_error_code>& errors) noexcept
        {
            const auto root = m_data.find(mbox::root);
            if (root == m_data.end() || root->value.type != mbox::type::directory)
                return make_mbox_error(mbox::mbox_error_code::invalid_root);
            ICY_ERROR(validate(root->value, errors));
            return {};
        }
    private:
        size_t m_version = 0;
        icy::map<icy::guid, base> m_data;
    };
    class transaction
    {
    public:
        enum class operation_type : uint32_t
        {
            none,
            insert,
            modify,
            remove,
        };
        struct operation
        {
            operation_type type = operation_type::none;
            mbox::base value;
        };
    public:
        static icy::error_type copy(const transaction& src, transaction& dst) noexcept
        {
            transaction copy_to;
            library::copy(src.m_library, copy_to.m_library);
            for (auto&& src_oper : src.m_oper)
            {
                operation copy_oper;
                copy_oper.type = src_oper.type;
                ICY_ERROR(mbox::base::copy(src_oper.value, copy_oper.value));
                ICY_ERROR(copy_to.m_oper.push_back(std::move(copy_oper)));
            }
            dst = std::move(copy_to);
            return {};
        }
        icy::error_type initialize(const library& source) noexcept
        {
            ICY_ERROR(library::copy(source, m_library));
            m_oper.clear();
            return {};
        }
        icy::error_type remove(const icy::guid& index) noexcept;
        icy::error_type insert(const base& base) noexcept
        {
            operation oper;
            oper.type = operation_type::insert;
            ICY_ERROR(base::copy(base, oper.value));
            ICY_ERROR(m_oper.push_back(std::move(oper)));
            return {};
        }
        icy::error_type modify(const base& base) noexcept
        {
            operation oper;
            oper.type = operation_type::modify;
            ICY_ERROR(base::copy(base, oper.value));
            ICY_ERROR(m_oper.push_back(std::move(oper)));
            return {};
        }
        icy::error_type execute(library& target, icy::map<icy::guid, mbox_error_code>* errors = nullptr) noexcept;
        icy::const_array_view<operation> oper() const noexcept
        {
            return m_oper;
        }
    private:
        library m_library;
        icy::array<operation> m_oper;
    };

    icy::string_view to_string(const type type) noexcept;
    icy::string_view to_string(const action_type type) noexcept;
    icy::error_type to_string(const button button, icy::string& str) noexcept;
}
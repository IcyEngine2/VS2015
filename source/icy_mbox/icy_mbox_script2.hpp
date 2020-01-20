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
        invalid_focus,
        invalid_command,
        invalid_root,
        invalid_timer,
        invalid_timer_count,
        invalid_timer_offset,
        invalid_timer_period,
        invalid_type,
        invalid_action_type,
        invalid_button_key,
        invalid_button_mod,

        invalid_json_parse,
        invalid_json_array,
        invalid_json_object,

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
        timer,
        _total,
    };
    enum class action_type : uint32_t
    {
        none,
        group_join,             //  group
        group_leave,            //  group
        focus,                  //  character
        button_press,           //  input
        button_release,         //  input
        button_click,           //  input
        timer_start,            //  timer
        timer_stop,             //  timer
        timer_pause,            //  timer
        timer_resume,           //  timer
        command_execute,        //  command
        command_replace,        //  command A, command B,
        on_button_press,        //  event; command
        on_button_release,      //  event; command
        on_timer,
        _total,
    };
    enum class execute_type : uint32_t
    {
        self,
        multicast,
        broadcast,
        _total,
    };

    static constexpr auto button_lmb = icy::key(0x01);
    static constexpr auto button_rmb = icy::key(0x02);
    static constexpr auto button_mid = icy::key(0x03);
    static constexpr auto button_x1 = icy::key(0x04);
    static constexpr auto button_x2 = icy::key(0x05);

    struct button_type
    {
        icy::key key = icy::key::none;
        icy::key_mod mod = icy::key_mod::none;
    };

    class action
    {
    public:
        using button_type = mbox::button_type;
        using group_type = icy::guid;
        using focus_type = icy::guid;
        struct execute_type
        {
            icy::guid command;
            icy::guid group;
            mbox::execute_type etype = mbox::execute_type::self;
        };
        struct replace_type
        {
            icy::guid source;
            icy::guid target;
        };
        struct timer_type
        {
            timer_type(const icy::guid& timer, const uint32_t count = 0) noexcept : timer(timer), count(count)
            {

            }
            icy::guid timer;
            uint32_t count;
        };
        union event_type
        {
            event_type() noexcept
            {
                memset(this, 0, sizeof(*this));
            }
            event_type(const button_type button) noexcept : button(button) 
            {

            }
            event_type(const icy::guid& timer) noexcept : timer(timer)
            {

            }
            button_type button;
            icy::guid timer;
        };
        struct connect_type
        {
            connect_type() noexcept = default;
            connect_type(const button_type& button, const icy::guid& command) noexcept : event(button), command(command)
            {

            }
            connect_type(const icy::guid& timer, const icy::guid& command) noexcept : event(timer), command(command)
            {

            }
            event_type event;
            icy::guid command;
        };
    public:
        action() noexcept
        {
            memset(this, 0, sizeof(*this));
        }
        static action create_group_join(const group_type value) noexcept
        {
            action action;
            action.m_type = action_type::group_join;
            new (&action.m_group) group_type(value);
            return action;
        }
        static action create_group_leave(const group_type value) noexcept
        {
            action action;
            action.m_type = action_type::group_leave;
            new (&action.m_group) group_type(value);
            return action;
        }
        static action create_focus(const focus_type value) noexcept
        {
            action action;
            action.m_type = action_type::focus;
            new (&action.m_focus) focus_type(value);
            return action;
        }
        static action create_button_press(const button_type value) noexcept
        {
            action action;
            action.m_type = action_type::button_press;
            new (&action.m_button) button_type(value);
            return action;
        }
        static action create_button_release(const button_type value) noexcept
        {
            action action;
            action.m_type = action_type::button_release;
            new (&action.m_button) button_type(value);
            return action;
        }
        static action create_button_click(const button_type value) noexcept
        {
            action action;
            action.m_type = action_type::button_click;
            new (&action.m_button) button_type(value);
            return action;
        }
        static action create_timer_start(const icy::guid& timer, const uint32_t count) noexcept
        {
            action action;
            action.m_type = action_type::timer_start;
            new (&action.m_timer) timer_type(timer, count);
            return action;
        }
        static action create_timer_stop(const icy::guid& timer) noexcept
        {
            action action;
            action.m_type = action_type::timer_stop;
            new (&action.m_timer) timer_type(timer);
            return action;
        }
        static action create_timer_pause(const icy::guid& timer) noexcept
        {
            action action;
            action.m_type = action_type::timer_pause;
            new (&action.m_timer) timer_type(timer);
            return action;
        }
        static action create_timer_resume(const icy::guid& timer) noexcept
        {
            action action;
            action.m_type = action_type::timer_resume;
            new (&action.m_timer) timer_type(timer);
            return action;
        }
        static action create_command_execute(const execute_type& value) noexcept
        {
            action action;
            action.m_type = action_type::command_execute;
            new (&action.m_execute) execute_type(value);
            return action;
        }
        static action create_command_replace(const replace_type& value) noexcept
        {
            action action;
            action.m_type = action_type::command_replace;
            new (&action.m_replace) replace_type(value);
            return action;
        }
        static action create_on_button_press(const button_type value, const icy::guid& command) noexcept
        {
            action action;
            action.m_type = action_type::on_button_press;
            new (&action.m_connect) connect_type(value, command);
            return action;
        }
        static action create_on_button_release(const button_type value, const icy::guid& command) noexcept
        {
            action action;
            action.m_type = action_type::on_button_release;
            new (&action.m_connect) connect_type(value, command);
            return action;
        }
        static action create_on_timer(const icy::guid& value, const icy::guid& command) noexcept
        {
            action action;
            action.m_type = action_type::on_button_release;
            new (&action.m_connect) connect_type(value, command);
            return action;
        }
        action_type type() const noexcept
        {
            return m_type;
        }
        button_type button() const noexcept
        {
            ICY_ASSERT(
                type() == action_type::button_click ||
                type() == action_type::button_press ||
                type() == action_type::button_release,
                "INVALID TYPE");
            return m_button;
        }
        timer_type timer() const noexcept
        {
            ICY_ASSERT(
                type() == action_type::timer_start ||
                type() == action_type::timer_stop  ||
                type() == action_type::timer_pause ||
                type() == action_type::timer_resume,
                "INVALID TYPE");
            return m_timer;
        }
        group_type group() const noexcept
        {
            ICY_ASSERT(
                type() == action_type::group_join ||
                type() == action_type::group_leave,
                "INVALID TYPE");
            return m_group;
        }
        focus_type focus() const noexcept
        {
            ICY_ASSERT(type() == action_type::focus, "INVALID TYPE");
            return m_focus;
        }
        const execute_type& execute() const noexcept
        {
            ICY_ASSERT(type() == action_type::command_execute, "INVALID TYPE");
            return m_execute;
        }
        const replace_type& replace() const noexcept
        {
            ICY_ASSERT(type() == action_type::command_replace, "INVALID TYPE");
            return m_replace;
        }
        icy::guid event_command() const noexcept
        {
            ICY_ASSERT(
                type() == action_type::on_button_press ||
                type() == action_type::on_button_release, 
                "INVALID TYPE");
            return m_connect.command;
        }
        icy::guid event_timer() const noexcept
        {
            ICY_ASSERT(type() == action_type::on_timer, "INVALID TYPE");
            return m_connect.event.timer;
        }
        mbox::button_type event_button() const noexcept
        {
            ICY_ASSERT(
                type() == action_type::on_button_press ||
                type() == action_type::on_button_release,
                "INVALID TYPE");
            return m_connect.event.button;
        }        
    private:
        action_type m_type = action_type::none;
        union
        {
            button_type m_button;
            group_type m_group;
            focus_type m_focus;
            execute_type m_execute;
            replace_type m_replace;
            connect_type m_connect;
            timer_type m_timer;
        };
    };
    struct timer
    {
        uint32_t offset = 0;
        uint32_t period = 0;
    };
    struct base
    {
        static icy::error_type copy(const base& src, base& dst) noexcept
        {
            dst.type = src.type;
            dst.index = src.index;
            dst.parent = src.parent;
            dst.timer = src.timer;
            ICY_ERROR(icy::to_string(src.name, dst.name));
            ICY_ERROR(dst.actions.assign(src.actions));
            return {};
        }
        type type = type::directory;
        icy::guid index;
        icy::guid parent;
        icy::string name;
        timer timer;
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
        icy::error_type load_from(const icy::string_view filename) noexcept;
        icy::error_type save_to(const icy::string_view filename) const noexcept;
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
        icy::error_type path(const icy::guid& index, icy::string& str) const noexcept
        {
            using namespace icy;

            if (index == guid())
                return {};

            auto ptr = find(index);
            if (!ptr)
                return make_mbox_error(mbox::mbox_error_code::invalid_index);

            ICY_ERROR(to_string(ptr->name, str));
            while (true)
            {
                ptr = find(ptr->parent);
                if (!ptr || ptr->index == mbox::root)
                    break;
                ICY_ERROR(str.append(" / "_s));
                ICY_ERROR(str.append(ptr->name));
            }
            return {};
        }
        icy::error_type reverse_path(const icy::guid& index, icy::string& str) const noexcept 
        {
            using namespace icy;

            if (index == guid())
                return {};

            array<guid> parents;
            auto ptr = find(index);
            if (!ptr)
                return make_mbox_error(mbox::mbox_error_code::invalid_index);

            while (true)
            {
                ICY_ERROR(parents.push_back(ptr->index));
                ptr = find(ptr->parent);
                if (!ptr || ptr->index == mbox::root)
                    break;
            }

            std::reverse(parents.begin(), parents.end());
            auto it = parents.begin();
            ICY_ERROR(to_string(find(*it)->name, str));
            ++it;
            for (; it != parents.end(); ++it)
            {
                ICY_ERROR(str.append(" / "_s));
                ICY_ERROR(str.append(find(*it)->name));
            }
            return {};
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
        icy::error_type insert(base&& base) noexcept
        {
            operation oper;
            oper.type = operation_type::insert;
            oper.value = std::move(base);
            ICY_ERROR(m_oper.push_back(std::move(oper)));
            return {};
        }
        icy::error_type insert(const base& base) noexcept
        {
            mbox::base copy;
            ICY_ERROR(base::copy(base, copy));
            return insert(std::move(copy));
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
    icy::string_view to_string(const execute_type type) noexcept;
    icy::string_view to_string(const icy::key key) noexcept;
    icy::error_type to_string(const button_type button, icy::string& str) noexcept;
}
#pragma once

#include "../icy_mbox_network.hpp"
#include "../icy_mbox_script.hpp"
#include "icy_mbox_server_network.hpp"
#include <icy_engine/core/icy_thread.hpp>

class mbox_script_exec : public icy::thread
{
    enum class mbox_action_type
    {
        none,
        insert_binding,
        remove_binding,
        send_input,
        set_focus,
    };
    struct mbox_action
    {
        mbox_action_type type = mbox_action_type::none;
        icy::guid group;
        icy::guid reference;
    };
    struct mbox_binding
    {
        icy::guid group;
        mbox::value_event event;
        mbox::value_command command;
    };
    struct mbox_group
    {
        icy::map<icy::guid, int> profiles;
        icy::map<icy::guid, mbox_binding> bindings;
    };
    struct mbox_profile
    {
        bool in_group(const icy::guid& index) const noexcept
        {
            return 
                index == mbox::group_broadcast ||
                index == mbox::group_default || 
                groups.try_find(index);
        }
        icy::map<icy::guid, int> groups;
        mbox::key process;
    };
    struct mbox_input
    {
        icy::map<mbox::key, icy::array<icy::input_message>> data;
    };
public:
    mbox_script_exec(mbox_server_network& network) noexcept : m_network(network)
    {

    }
    icy::error_type run() noexcept override
    {
        using namespace icy;
        shared_ptr<event_loop> loop;
        ICY_ERROR(m_library.initialize());
        ICY_ERROR(event_loop::create(loop, event_type::user_any));       
        ICY_SCOPE_EXIT{ event::post(nullptr, event_type::global_quit); };
        while (true)
        {
            event event;
            ICY_ERROR(loop->loop(event));
            if (event->type == event_type::global_quit)
                break;

            mouse_message mouse = {};
            array<mbox_action> actions;

            if (event->type == event_type_user_library)
            {
                ICY_ERROR(process(event->data<event_user_library>()));
            }
            else if (event->type == event_type_user_connect)
            {
                const auto& event_data = event->data<event_user_connect>();
                const auto it = m_profiles.find(event_data.info.profile);
                if (it != m_profiles.end())
                    it->value.process = event_data.info.key;
            }
            else if (event->type == event_type_user_disconnect)
            {
                const auto& event_data = event->data<event_user_disconnect>();
                for (auto&& val : m_profiles.vals())
                {
                    if (val.process == event_data.key)
                        val.process = {};
                }
            }
            else if (event->type == event_type_user_recv_input)
            {
                ICY_ERROR(process(event->data<event_user_recv_input>(), actions));
                for (auto&& msg : event->data<event_user_recv_input>().data)
                {
                    if (msg.type != input_type::mouse)
                        continue;
                    if (msg.mouse.ctrl && msg.mouse.event == mouse_event::btn_release)
                        mouse = msg.mouse;
                }
            }

            if (actions.empty() && mouse.event == mouse_event::none)
                continue;

            mbox_input send;
            for (auto&& profile : m_profiles.vals())
            {
                if (profile.process == mbox::key())
                    continue;
                ICY_ERROR(send.data.insert(mbox::key(profile.process), array<input_message>()));
            }
            ICY_ERROR(exec(actions, send));
            
            if (mouse.event != mouse_event::none)
            {
                mouse.ctrl = 0;
                mouse_message press = mouse;
                press.event = mouse_event::btn_press;
                for (auto&& pair : send.data)
                {                    
                    ICY_ERROR(pair.value.push_back(press));
                    ICY_ERROR(pair.value.push_back(mouse));
                }
            }
            for (auto&& pair : send.data)
                ICY_ERROR(m_network.send(pair.key, pair.value));
        }
        return {};
    }
private:
    icy::error_type process(const event_user_library& event) noexcept
    {
        using namespace icy;

        ICY_ERROR(mbox::library::copy(event.library, m_library));
        m_groups.clear();
        m_profiles.clear();

        array<mbox::base> profiles;
        ICY_ERROR(m_library.enumerate(mbox::type::profile, profiles));
        for (auto&& base : profiles)
            ICY_ERROR(m_profiles.insert(guid(base.index), mbox_profile()));

        array<mbox::base> groups;
        ICY_ERROR(m_library.enumerate(mbox::type::group, groups));

        mbox::base broadcast;
        broadcast.type = mbox::type::group;
        broadcast.index = mbox::group_broadcast;

        for (auto&& base : profiles)
            ICY_ERROR(broadcast.value.group.profiles.push_back(base.index));
        ICY_ERROR(groups.push_back(std::move(broadcast)));

        for (auto&& base : profiles)
        {
            mbox::base new_group;
            new_group.type = mbox::type::group;
            new_group.index = base.index;
            ICY_ERROR(new_group.value.group.profiles.push_back(base.index));
            ICY_ERROR(groups.push_back(std::move(new_group)));
        }

        for (auto&& base : groups)
        {
            mbox_group new_group;
            for (auto&& index : base.value.group.profiles)
            {
                ICY_ERROR(new_group.profiles.insert(guid(index), 0));
                ICY_ERROR(m_profiles.find(index)->value.groups.insert(guid(base.index), 0));
            }
            ICY_ERROR(m_groups.insert(guid(base.index), std::move(new_group)));
        }

        array<mbox_action> actions;
        for (auto&& index : m_profiles.keys())
        {
            const auto& profile = m_library.find(index)->value.profile;
            const auto& command = m_library.find(profile.command)->value.command;
            ICY_ERROR(exec({ &index, 1 }, command, actions));
        }
        mbox_input send;
        ICY_ERROR(exec(actions, send));
        return{};
    }
    icy::error_type process(const event_user_recv_input& event, icy::array<mbox_action>& actions) noexcept
    {
        using namespace icy;

        mbox_profile* profile = nullptr;
        for (auto&& val : m_profiles.vals())
        {
            if (val.process == event.key)
            {
                profile = &val;
                break;
            }
        }
        if (!profile)
            return {};

        for (auto&& index : profile->groups)
        {
            for (auto&& binding : m_groups.find(index.key)->value.bindings.vals())
            {
                auto ok = false;
                if (binding.event.etype == mbox::event_type::recv_input)
                {
                    if (!profile->in_group(binding.event.group))
                        continue;

                    const auto& input = m_library.find(binding.event.reference)->value.input;
                    for (auto&& msg : event.data)
                    {
                        if (msg.type != input_type::key)
                            continue;
                        ok |= true
                            && input.button == msg.key.key
                            && input.event == msg.key.event
                            && key_mod_and(input.ctrl, msg.key.ctrl)
                            && key_mod_and(input.alt, msg.key.alt)
                            && key_mod_and(input.shift, msg.key.shift);                        
                    }
                }
                else if (binding.event.etype == mbox::event_type::focus_acquire)
                {
                    for (auto&& msg : event.data)
                        ok |= msg.type == input_type::active && msg.active;
                }
                else if (binding.event.etype == mbox::event_type::focus_release)
                {
                    for (auto&& msg : event.data)
                        ok |= msg.type == input_type::active && !msg.active;
                }
                if (ok)
                    ICY_ERROR(exec({ &index.key, 1 }, binding.command, actions));
            }
        }
        return {};
    }
    icy::error_type exec(icy::const_array_view<icy::guid> groups, const mbox::value_command& cmd, icy::array<mbox_action>& actions) noexcept
    {
        using namespace icy;
        const auto make_groups = [this](const icy::guid& group, const_array_view<guid> groups, array<guid>& exec_groups)
        {
            if (group == mbox::group_multicast)
            {
                for (auto&& index : m_groups.keys())
                {
                    const auto it = std::find(groups.begin(), groups.end(), index);
                    if (it == groups.end())
                        ICY_ERROR(exec_groups.push_back(index));
                }
            }
            else if (group == mbox::group_default)
            {
                ICY_ERROR(exec_groups.assign(groups));
            }
            else if (group == mbox::group_broadcast)
            {
                ICY_ERROR(exec_groups.assign(m_groups.keys()));
            }
            else
            {
                ICY_ERROR(exec_groups.push_back(group));
            }
            return error_type();
        };
        
        array<guid> cmd_groups;
        ICY_ERROR(make_groups(cmd.group, groups, cmd_groups));
        if (cmd_groups.empty())
            return {};

        for (auto&& action : cmd.actions)
        {
            array<guid> action_groups;
            ICY_ERROR(make_groups(action.group, cmd_groups, action_groups));
            if (action_groups.empty())
                continue;
            
            const auto& ref = *m_library.find(action.reference);
            mbox_action new_action;
            new_action.reference = action.reference;
            
            switch (action.atype)
            {
            case mbox::action_type::enable_bindings_list:
            case mbox::action_type::disable_bindings_list:
            {
                new_action.type = action.atype == mbox::action_type::enable_bindings_list ?
                    mbox_action_type::insert_binding : mbox_action_type::remove_binding;
                for (auto&& index : ref.value.directory.indices)
                {
                    new_action.reference = index;
                    for (auto&& group : action_groups)
                    {
                        new_action.group = group;
                        ICY_ERROR(actions.push_back(new_action));
                    }
                }
                new_action.type = mbox_action_type::none;
                break;
            }
            case mbox::action_type::enable_binding:
            case mbox::action_type::disable_binding:
            {
                new_action.type = action.atype == mbox::action_type::enable_binding ?
                    mbox_action_type::insert_binding : mbox_action_type::remove_binding;
                break;
            }
            case mbox::action_type::execute_command:
            {
                ICY_ERROR(exec(action_groups, ref.value.command, actions));
                break;
            }

            case mbox::action_type::send_input:
            {
                new_action.type = mbox_action_type::send_input;
                break;
            }

            case mbox::action_type::set_focus:
            {
                new_action.type = mbox_action_type::set_focus;
                new_action.group = action.reference;
                ICY_ERROR(actions.push_back(new_action));
                new_action.type = mbox_action_type::none;
                break;
            }

            }
            if (new_action.type != mbox_action_type::none)
            {
                for (auto&& group : action_groups)
                {
                    new_action.group = group;
                    ICY_ERROR(actions.push_back(new_action));
                }
            }
        }
        return {};
    }
    icy::error_type exec(const icy::const_array_view<mbox_action> actions, mbox_input& send) noexcept
    {
        using namespace icy;

        for (auto&& action : actions)
        {
            const auto group = m_groups.find(action.group);
            switch (action.type)
            {
            case mbox_action_type::insert_binding:
            {
                const auto it = group->value.bindings.find(action.reference);
                if (it != group->value.bindings.end())
                    return error_type();

                const auto& source = m_library.find(action.reference)->value.binding;
                const auto& command = m_library.find(source.command)->value.command;
                mbox_binding new_binding;
                new_binding.group = source.group;
                new_binding.event = m_library.find(source.event)->value.event;
                new_binding.command.group = command.group;
                ICY_ERROR(new_binding.command.actions.assign(command.actions));
                ICY_ERROR(group->value.bindings.insert(guid(action.reference), std::move(new_binding)));
                break;
            }
            case mbox_action_type::remove_binding:
            {
                const auto it = group->value.bindings.find(action.reference);
                if (it != group->value.bindings.end())
                    group->value.bindings.erase(it);
                break;
            }
            case mbox_action_type::set_focus:
            case mbox_action_type::send_input:
            {
                for (auto&& index : group->value.profiles.keys())
                {
                    const auto profile = m_profiles.find(index);
                    const auto proc = profile->value.process;
                    if (proc == mbox::key())
                        continue;

                    const auto buffer = send.data.find(proc);
                    if (buffer == send.data.end())
                        continue;

                    if (action.type == mbox_action_type::send_input)
                    {
                        const auto& input = m_library.find(action.reference)->value.input;
                        auto msg = key_message(input.button, input.event,
                            uint32_t(input.ctrl), uint32_t(input.alt), uint32_t(input.shift));
                        ICY_ERROR(buffer->value.push_back(msg));
                    }
                    else
                    {
                        ICY_ERROR(buffer->value.push_back(input_message(true)));
                    }
                }
                break;
            }
            default:
                break;
            }
        }
        return {};
    }
private:
    mbox::library m_library;
    mbox_server_network& m_network;
    icy::map<icy::guid, mbox_profile> m_profiles;
    icy::map<icy::guid, mbox_group> m_groups;
};
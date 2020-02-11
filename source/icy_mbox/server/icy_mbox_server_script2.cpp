#include "icy_mbox_server_script2.hpp"
#include <icy_engine/core/icy_event.hpp>
#include "icy_mbox_server_network.hpp"

using namespace icy;

error_type mbox_script_thread::run() noexcept
{
    shared_ptr<event_loop> loop;
    ICY_ERROR(m_library.initialize());
    ICY_ERROR(event_loop::create(loop, event_type::user_any | event_type::global_timer));
    ICY_SCOPE_EXIT{ event::post(nullptr, event_type::global_quit); };
    while (true)
    {
        event event;
        ICY_ERROR(loop->loop(event));
        if (event->type == event_type::global_quit)
            break;

        //mouse_message mouse = {};
        //array<mbox_action> actions;

        if (event->type == event_type_user_library)
        {
            ICY_ERROR(mbox::library::copy(event->data<event_user_library>().library, m_library));
            array<mbox::base> characters;
            ICY_ERROR(m_library.enumerate(mbox::type::character, characters));
            m_characters.clear();

            map<mbox::key, mbox_send> send;
            for (auto&& base : characters)
            {
                mbox_character new_character;
                new_character.index = base.index;
                ICY_ERROR(process(new_character, base.actions, send));
                ICY_ERROR(m_characters.insert(base.index, std::move(new_character)));                
            }
        }
        else if (event->type == event_type_user_connect)
        {
              const auto& event_data = event->data<event_user_connect>();
              const auto it = m_characters.find(event_data.info.profile);
              if (it != m_characters.end())
                  it->value.process = event_data.info.key;
        }
        else if (event->type == event_type_user_disconnect)
        {
            const auto& event_data = event->data<event_user_disconnect>();
            for (auto&& val : m_characters.vals())
            {
                if (val.process == event_data.key)
                    val.process = {};
            }
        }
        else if (event->type == event_type::global_timer)
        {


        }
        else if (event->type == event_type_user_recv_input)
        {
            const auto& event_data = event->data<event_user_recv_input>();
            mbox_character* character = nullptr;
            for (auto&& val : m_characters.vals())
            {
                if (val.process == event_data.key)
                {
                    character = &val;
                    break;
                }
            }
            if (character)
            {
                map<mbox::key, mbox_send> send;
                for (auto&& val : m_characters.vals())
                {
                    if (val.process != mbox::key())
                        ICY_ERROR(send.find_or_insert(val.process, mbox_send()));
                }
                ICY_ERROR(process(*character, event_data.data, send));
                for (auto&& pair : send)
                {
                    if (pair.value.input.empty())
                        continue;
                    ICY_ERROR(m_network.send(pair.key, pair.value.input));
                }
            }            
        }
    }
    return {};
}
error_type mbox_script_thread::process(mbox_character& character, const const_array_view<mbox::action> actions, map<mbox::key, mbox_send>& send) noexcept
{
    const auto find_send = send.find(character.process);
    for (auto&& action : actions)
    {       
       
        switch (action.type())
        {
        case mbox::action_type::group_join:
        case mbox::action_type::group_leave:
        {
            const auto group = m_library.find(action.group());
            if (group && group->type == mbox::type::group)
            {
                const auto it = character.groups.find(action.group());
                if (action.type() == mbox::action_type::group_join)
                {
                    if (it == character.groups.end())
                        ICY_ERROR(character.groups.insert(action.group()));
                }
                else if (it != character.groups.end())
                {
                    character.groups.erase(it);
                }
            }
            break;
        }
        case mbox::action_type::focus:
        {
            if (const auto focus = m_characters.try_find(action.focus()))
            {
                const auto find_focus = send.find(focus->process);
                if (find_focus != send.end())
                    ICY_ERROR(find_focus->value.input.push_back(input_message(true)));
            }
            break;
        }
        case mbox::action_type::button_click:
        case mbox::action_type::button_press:
        case mbox::action_type::button_release:
        {
            if (find_send == send.end())
                break;

            mbox::button_type button = action.button();
            if (button.key == key::none)
                break;

            auto mouse = mouse_button::none;
            switch (button.key)
            {
            case mbox::button_lmb: mouse = mouse_button::left; break;
            case mbox::button_rmb: mouse = mouse_button::right; break;
            case mbox::button_mid: mouse = mouse_button::mid; break;
            case mbox::button_x1: mouse = mouse_button::x1; break;
            case mbox::button_x2: mouse = mouse_button::x2; break;
            }

            array<key> mods;
            if (button.mod & key_mod::lctrl) ICY_ERROR(mods.push_back(key::left_ctrl));
            if (button.mod & key_mod::rctrl) ICY_ERROR(mods.push_back(key::right_ctrl));
            if (button.mod & key_mod::lalt) ICY_ERROR(mods.push_back(key::left_alt));
            if (button.mod & key_mod::ralt) ICY_ERROR(mods.push_back(key::right_alt));
            if (button.mod & key_mod::lshift) ICY_ERROR(mods.push_back(key::left_shift));
            if (button.mod & key_mod::rshift) ICY_ERROR(mods.push_back(key::right_shift));

            for (auto&& mod : mods)
                ICY_ERROR(find_send->value.input.push_back(key_message(mod, key_event::press)));

            if (mouse)
            {
                if (action.type() == mbox::action_type::button_press ||
                    action.type() == mbox::action_type::on_button_press ||
                    action.type() == mbox::action_type::button_click)
                {
                    mouse_message msg;
                    msg.event = mouse_event::btn_press;
                    msg.button = mouse;
                    msg.mod = button.mod;
                    msg.point.x = -1;
                    msg.point.y = -1;
                    ICY_ERROR(find_send->value.input.push_back(msg));
                }
                if (action.type() == mbox::action_type::button_release ||
                    action.type() == mbox::action_type::on_button_release ||
                    action.type() == mbox::action_type::button_click)
                {
                    mouse_message msg;
                    msg.event = mouse_event::btn_release;
                    msg.button = mouse;
                    msg.mod = button.mod;
                    msg.point.x = -1;
                    msg.point.y = -1;
                    ICY_ERROR(find_send->value.input.push_back(msg));
                }
            }
            else
            {
                if (action.type() == mbox::action_type::button_press ||
                    action.type() == mbox::action_type::on_button_press ||
                    action.type() == mbox::action_type::button_click)
                {
                    auto msg = key_message(button.key, key_event::press, button.mod);
                    ICY_ERROR(find_send->value.input.push_back(msg));
                }
                if (action.type() == mbox::action_type::button_release ||
                    action.type() == mbox::action_type::on_button_release ||
                    action.type() == mbox::action_type::button_click)
                {
                    auto msg = key_message(button.key, key_event::release, button.mod);
                    ICY_ERROR(find_send->value.input.push_back(msg));
                }
            }

            for (auto it = mods.rbegin(); it != mods.rend(); ++it)
                ICY_ERROR(find_send->value.input.push_back(key_message(*it, key_event::release)));
            break;
        }

        case mbox::action_type::on_button_press:
        case mbox::action_type::on_button_release:
        {
            mbox_button_event event;
            event.press = action.type() == mbox::action_type::on_button_press;
            event.button = action.event_button();
            event.command = action.event_command();
            const auto command = m_library.find(event.command);
            if (command && command->type == mbox::type::command && event.button.key != key::none)
                ICY_ERROR(character.bevents.push_back(event));
            break;
        }

        case mbox::action_type::command_execute:
        {
            const auto& execute = action.execute();
            const auto group = m_library.find(execute.group);
            if (execute.group == guid() || group && group->type == mbox::type::group)
            {
                if (execute.etype == mbox::execute_type::self)
                {
                    const auto virt = character.virt.try_find(execute.command);
                    auto command = m_library.find(virt ? *virt : execute.command);
                    if (command && command->type == mbox::type::command)
                    {
                        ICY_ERROR(process(character, command->actions, send));
                    }
                }
                else
                {
                    for (auto&& other : m_characters)
                    {
                        if (other.key == character.index && execute.etype == mbox::execute_type::multicast)
                            continue;

                        if (!group || other.value.groups.try_find(group->index))
                        {
                            auto virt = &execute.command;
                            while (true)
                            {          
                                const auto next_virt = other.value.virt.try_find(*virt);
                                if (!next_virt)
                                    break;
                                virt = next_virt;
                            }
                            auto command = m_library.find(*virt);
                            ICY_ERROR(process(other.value, command->actions, send));
                        }
                    }
                }
            }
           
            break;
        }

        case mbox::action_type::command_replace:
        {
            const auto& replace = action.replace();
            const auto source = m_library.find(replace.source);
            const auto target = m_library.find(replace.target);
            if (source && source->type == mbox::type::command &&
                (replace.target == guid() || target && target->type == mbox::type::command))
            {
                const auto find_virt = character.virt.find(source->index);
                if (find_virt == character.virt.end())
                {
                    ICY_ERROR(character.virt.insert(replace.source, replace.target));
                }
                else if (target && target != source)
                {
                    find_virt->value = replace.target;
                }
                else
                {
                    character.virt.erase(find_virt);
                }
            }
            break;
        }
        }
    }
    return {};
}
error_type mbox_script_thread::process(mbox_character& character, const const_array_view<input_message> data, map<mbox::key, mbox_send>& send) noexcept
{
    for (auto&& msg : data)
    {
        auto mod = key_mod::none;
        auto key = key::none;
        auto press = false;

        if (msg.type == input_type::mouse)
        {
            if (msg.mouse.event == mouse_event::btn_press ||
                msg.mouse.event == mouse_event::btn_release)
            {
                switch (msg.mouse.button)
                {
                case mouse_button::left: key = mbox::button_lmb; break;
                case mouse_button::right: key = mbox::button_rmb; break;
                case mouse_button::mid: key = mbox::button_mid; break;
                case mouse_button::x1: key = mbox::button_x1; break;
                case mouse_button::x2: key = mbox::button_x2; break;
                }
                press = msg.mouse.event == mouse_event::btn_press;
                mod = msg.mouse.mod;
            }
        }
        else if (msg.type == input_type::key)
        {
            if (msg.key.event == key_event::press)
                press = true;
            else if (msg.key.event == key_event::release)
                press = false;
            else
                continue;

            key = msg.key.key;
            mod = msg.key.mod;
        }
        if (key == key::none)
            continue;

        for (auto&& bevent : character.bevents)
        {
            if (bevent.press != press || bevent.button.key != key || !icy::key_mod_and(bevent.button.mod, mod))
                continue;

            const auto virt = character.virt.try_find(bevent.command);
            const auto command = m_library.find(virt ? *virt : bevent.command);
            if (!command || command->type != mbox::type::command)
                continue;

            ICY_ERROR(process(character, command->actions, send));
        }
    }
    return {};
}
#define ICY_QTGUI_STATIC 1
#include "icy_mbox_master_core.hpp"
#include <icy_engine/core/icy_string.hpp>
#include <icy_engine/core/icy_array.hpp>
#include <icy_engine/core/icy_thread.hpp>
#include <icy_engine/core/icy_file.hpp>
#include <icy_engine/core/icy_set.hpp>
#include <icy_engine/graphics/icy_graphics.hpp>
#include <icy_engine/utility/icy_raw_input.hpp>
#include <icy_qtgui/icy_xqtgui.hpp>
#include "../icy_mbox_script2.hpp"
#if _DEBUG
#pragma comment(lib, "icy_engine_cored")
#pragma comment(lib, "icy_engine_graphicsd")
#pragma comment(lib, "icy_qtguid")
#else
#pragma comment(lib, "icy_engine_core")
#pragma comment(lib, "icy_engine_graphics")
#pragma comment(lib, "icy_qtgui")
#endif

using namespace icy;


struct mbox_send
{
    icy::array<icy::input_message> input;
};
struct mbox_button_event
{
    bool press = false;
    mbox::button_type button;
    icy::guid command;
};
struct mbox_timer_event
{
    icy::guid timer;
    icy::guid command;
};
struct pause_type
{
    enum type
    {
        toggle,
        pause,
        resume,
    };
    pause_type(const type type, const input_message msg) noexcept : type(type), msg(msg)
    {

    }
    type type = toggle;
    input_message msg;
};

class application : public thread
{
public:
    static error_type show_error(const error_type error, const string_view text) noexcept;
    error_type initialize() noexcept;
    error_type exec() noexcept;
    error_type run() noexcept;
private:
    struct data_type 
    {
        uint32_t process = 0;
        xgui_action action;
        xgui_widget widget;
        set<icy::guid> groups;
        array<mbox_button_event> bevents;
        icy::array<mbox_timer_event> tevents;
        icy::map<icy::guid, icy::guid> virt;
    };
private:
    error_type open(const guid& key) noexcept;
    error_type process(map<guid, data_type>::pair_type character, const const_array_view<mbox::action> actions, map<guid, mbox_send>& send) noexcept;
    error_type process(map<guid, data_type>::pair_type character, const const_array_view<input_message> data, map<guid, mbox_send>& send) noexcept;
private:
    shared_ptr<event_loop> m_input;
    shared_ptr<gui_queue> m_gui;
    xgui_widget m_window;
    xgui_widget m_menu;
    xgui_action m_load;
    xgui_submenu m_open;
    xgui_widget m_tabs;
    map<guid, data_type> m_data;
    guid m_select;
    mbox::library m_library;
    bool m_paused = false;
    icy::array<pause_type> m_pause;
};
gui_queue* icy::global_gui = nullptr;
static std::atomic<bool> g_running = false;

int main()
{
    const auto gheap_capacity = 256_mb;

    heap gheap;
    if (const auto error = gheap.initialize(heap_init::global(gheap_capacity)))
        return error.code;

    application app;
    if (const auto error = app.initialize())
    {
        show_error(error, "Application initialize"_s);
        return error.code;
    }
    if (const auto error = app.exec())
    {
        show_error(error, "Application execute"_s);
        return error.code;
    }
    return 0;
}

error_type show_error(const error_type error, const string_view text) noexcept
{
    return application::show_error(error, text);
}
error_type application::initialize() noexcept
{
    ICY_ERROR(make_raw_input(m_input));

    ICY_ERROR(create_gui(m_gui));
    global_gui = m_gui.get();

    ICY_ERROR(m_window.initialize(gui_widget_type::window, {}, gui_widget_flag::layout_vbox));
    ICY_ERROR(m_menu.initialize(gui_widget_type::menubar, m_window));
    ICY_ERROR(m_tabs.initialize(gui_widget_type::tabs, m_window));
    ICY_ERROR(m_load.initialize("Load"_s));
    ICY_ERROR(m_open.initialize("Open"_s));
    ICY_ERROR(m_menu.insert(m_load));
    ICY_ERROR(m_menu.insert(m_open.action));
    ICY_ERROR(m_window.show(true));

    return {};
}
error_type application::exec() noexcept
{
    class input_thread : public thread
    {
    public:
        application* app = nullptr;
        error_type run() noexcept override
        {
            ICY_SCOPE_EXIT{ event::post(nullptr, event_type::global_quit); };
            while (true)
            {
                event event;
                ICY_ERROR(app->m_input->loop(event));
                if (event->type == event_type::global_quit)
                    break;
            }
            return {};
        }
    } thread;

    ICY_SCOPE_EXIT
    {
        thread.wait();
        wait();
    };
    ICY_ERROR(launch());
    
    thread.app = this;
    ICY_ERROR(thread.launch());

    g_running = true;
    const auto error = m_gui->loop();
    g_running = false;
    event::post(nullptr, event_type::global_quit);
    wait();
    return {};
}
error_type application::run() noexcept
{
    ICY_SCOPE_EXIT{ m_data = {}; event::post(nullptr, event_type::global_quit); };
 
    shared_ptr<event_loop> loop;
    ICY_ERROR(loop->create(loop, event_type::window_close | 
        event_type::gui_any | event_type::window_input | event_type::window_active));

    ICY_ERROR(m_pause.push_back(pause_type(pause_type::toggle, input_message(input_type::key_press, key::p, key_mod::lctrl))));
    ICY_ERROR(m_pause.push_back(pause_type(pause_type::toggle, input_message(input_type::key_press, key::enter, key_mod::none))));
    ICY_ERROR(m_pause.push_back(pause_type(pause_type::pause, input_message(input_type::key_press, key::slash, key_mod::none))));
    ICY_ERROR(m_pause.push_back(pause_type(pause_type::resume, input_message(input_type::key_press, key::esc, key_mod::none))));

    while (true)
    {
        event event;
        ICY_ERROR(loop->loop(event));
        if (event->type == event_type::global_quit)
            break;

        auto source = shared_ptr<event_queue>(event->source).get();
        const auto is_gui = source == global_gui;

        if (event->type == event_type::window_input && source == m_input.get())
        {
            const auto tab = m_data.find(m_select);
            if (tab != m_data.end() && tab->value.widget)
            {
                const auto input = event->data<input_message>();
                if (input.type == input_type::key_press ||
                    input.type == input_type::key_release)
                {
                    for (auto&& pause : m_pause)
                    {
                        if (pause.msg.key == input.key && key_mod_and(key_mod(pause.msg.mods), key_mod(input.mods)))
                        {
                            if (pause.type == pause_type::resume)
                                m_paused = false;
                            else if (pause.type == pause_type::pause)
                                m_paused = true;
                            else if (pause.type == pause_type::toggle)
                                m_paused = !m_paused;
                        }
                    }
                    if (m_paused)
                    {
                        ICY_ERROR(tab->value.widget.input(input));
                    }
                    else
                    {
                        map<guid, mbox_send> send;
                        for (auto&& pair : m_data)
                        {
                            if (pair.value.widget)
                                ICY_ERROR(send.find_or_insert(pair.key, mbox_send()));
                        }
                        ICY_ERROR(process(*tab, const_array_view<input_message>{ input }, send));
                        for (auto&& pair : send)
                        {
                            if (pair.value.input.empty())
                                continue;
                            const auto target = m_data.find(pair.key);
                            for (auto&& msg : pair.value.input)
                            {
                                ICY_ERROR(target->value.widget.input(msg));
                                if (msg.type == input_type::active && msg.active)
                                    ICY_ERROR(m_gui->scroll(m_tabs, target->value.widget));
                            }
                        }
                    }
                }
                else
                {
                    ICY_ERROR(tab->value.widget.input(input));
                }
            }
        }

        if (!is_gui)
            continue;

        const auto& event_data = event->data<gui_event>();
        if (event->type == event_type::window_close)
        {
            if (event_data.widget == m_window)
                break;
        }
        else if (event->type == event_type::gui_action)
        {
            gui_action action;
            action.index = event_data.data.as_uinteger();
            if (action == m_load)
            {
                string file_name;
                ICY_ERROR(dialog_open_file(file_name));
                if (file_name.empty())
                    continue;

                if (const auto error = m_library.load_from(file_name))
                {
                    ICY_ERROR(show_error(error, "Load mbox script library"_s));
                    continue;
                }
                m_data.clear();
                m_select = guid();

                array<mbox::base> vec;
                ICY_ERROR(m_library.enumerate(mbox::type::character, vec));
                std::sort(vec.begin(), vec.end(), [](const mbox::base& lhs, const mbox::base& rhs) { return lhs.name < rhs.name; });
                for (auto&& base : vec)
                {
                    data_type new_tab;
                    map<guid, mbox_send> send;
                    ICY_ERROR(new_tab.action.initialize(base.name));
                    ICY_ERROR(m_open.widget.insert(new_tab.action));
                    auto it = m_data.end();
                    ICY_ERROR(m_data.insert(base.index, std::move(new_tab), &it));
                    ICY_ERROR(process(*it, base.actions, send));
                    m_select = base.index;
                }
            }
            else
            {
                for (auto&& data : m_data)
                {
                    if (data.value.action != action)
                        continue;
                    if (const auto error = open(data.key))
                        ICY_ERROR(show_error(error, "Open/Select new window in tab"_s));
                    break;
                }
            }
        }
        else if (event->type == event_type::gui_select && event_data.widget == m_tabs)
        {
            for (auto&& tab : m_data)
            {
                if (!tab.value.widget || tab.value.widget.index != event_data.data.as_uinteger())
                    continue;
                m_select = tab.key;
                break;
            }
        }
        else if (event->type == event_type::gui_update)
        {
            if (event_data.widget == m_tabs)
            {
                gui_widget widget;
                widget.index = event_data.data.as_uinteger();
                for (auto&& tab : m_data)
                {
                    if (!tab.value.widget || tab.value.widget != widget)
                        continue;

                    tab.value.process = 0;
                    tab.value.widget = {};
                    if (m_select == tab.key)
                        m_select = guid();
                    break;
                }
            }            
        }
    }
    
    return {};
}
error_type application::show_error(const error_type error, const string_view text) noexcept
{
    string msg;
    ICY_ERROR(to_string("Error: %4 - %1 code [%2]: %3", msg, error.source, long(error.code), error, text));
    if (g_running)
    {
        xgui_widget window;
        ICY_ERROR(window.initialize(gui_widget_type::message, {}));
        ICY_ERROR(window.text(msg));
        ICY_ERROR(window.show(true));
        shared_ptr<event_loop> loop;
        ICY_ERROR(event_loop::create(loop, event_type::window_close));
        while (true)
        {
            event event;
            ICY_ERROR(loop->loop(event));
            if (event->type == event_type::global_quit)
                break;
            if (event->type == event_type::window_close && shared_ptr<event_queue>(event->source).get() == global_gui)
            {
                auto& event_data = event->data<gui_event>();
                if (event_data.widget == window)
                    break;
            }
        }
    }
    else
    {
        ICY_ERROR(win32_message(msg, "Error"_s));
    }
    return {};
}
error_type application::open(const guid& key) noexcept
{
    const auto tab = m_data.find(key);
    if (tab == m_data.end())
        return make_stdlib_error(std::errc::invalid_argument);

    const auto base = m_library.find(tab->key);
    if (!base)
        return make_stdlib_error(std::errc::invalid_argument);

    if (tab->value.widget)
    {
        ICY_ERROR(global_gui->scroll(m_tabs, tab->value.widget));
        return {};
    }
    array<window_info> vec_info;
    ICY_ERROR(enum_windows(vec_info));

    xgui_model model;
    xgui_widget window;
    xgui_widget grid;
    xgui_widget buttons;
    xgui_widget select;
    xgui_widget cancel;

    ICY_ERROR(model.initialize());
    ICY_ERROR(model.insert_rows(0, vec_info.size()));
    ICY_ERROR(model.insert_cols(0, 1));
    
    std::sort(vec_info.begin(), vec_info.end(),
        [](const window_info& lhs, const window_info& rhs){ return lhs.name == rhs.name ? lhs.pid < rhs.pid : lhs.name < rhs.name; });
    for (auto k = 0_z; k < vec_info.size(); ++k)
    {
        auto node = model.node(k, 0);        
        string str;
        ICY_ERROR(to_string("%1 [%2]", str, string_view(vec_info[k].name), vec_info[k].pid));
        ICY_ERROR(node.text(str));
        ICY_ERROR(node.udata(uint64_t(vec_info[k].handle)));
    }

    ICY_ERROR(window.initialize(gui_widget_type::dialog, m_window, gui_widget_flag::layout_vbox));
    ICY_ERROR(grid.initialize(gui_widget_type::list_view, window));
    ICY_ERROR(buttons.initialize(gui_widget_type::none, window, gui_widget_flag::auto_insert | gui_widget_flag::layout_hbox));
    ICY_ERROR(select.initialize(gui_widget_type::text_button, buttons));
    ICY_ERROR(cancel.initialize(gui_widget_type::text_button, buttons));
    
    ICY_ERROR(grid.bind(model));
    ICY_ERROR(select.text("Select"_s));
    ICY_ERROR(cancel.text("Cancel"_s));
    ICY_ERROR(window.show(true));
    
    HWND__* hwnd = nullptr;
           
    shared_ptr<event_loop> loop;
    ICY_ERROR(loop->create(loop, event_type::window_close | event_type::gui_any));
    while (true)
    {
        event event;
        ICY_ERROR(loop->loop(event));
        if (event->type == event_type::global_quit)
        {
            event::post(nullptr, event_type::global_quit);
            break;
        }

        if (event->type == event_type::window_close && shared_ptr<event_queue>(event->source).get() == global_gui)
        {
            const auto& event_data = event->data<gui_event>();
            if (event_data.widget == window)
                break;
        }
        else if (event->type == event_type::gui_select)
        {
            const auto& event_data = event->data<gui_event>();
            if (event_data.widget == grid)
            {
                const auto node = event_data.data.as_node();
                hwnd = reinterpret_cast<HWND__*>(node.udata().as_uinteger());
            }
        }
        else if (event->type == event_type::gui_update)
        {
            const auto& event_data = event->data<gui_event>();
            if (event_data.widget == select)
            {
                ICY_ERROR(tab->value.widget.initialize(hwnd, m_tabs));
                ICY_ERROR(global_gui->text(m_tabs, tab->value.widget, base->name));
                ICY_ERROR(global_gui->scroll(m_tabs, tab->value.widget));
                m_select = tab->key;
                break;
            }
            else if (event_data.widget == cancel)
                break;
        }
    }
    return {};
}
error_type application::process(map<guid, data_type>::pair_type character, const const_array_view<mbox::action> actions, map<guid, mbox_send>& send) noexcept
{
    const auto find_send = send.find(character->key);
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
                const auto it = character->value.groups.find(action.group());
                if (action.type() == mbox::action_type::group_join)
                {
                    if (it == character->value.groups.end())
                        ICY_ERROR(character->value.groups.insert(action.group()));
                }
                else if (it != character->value.groups.end())
                {
                    character->value.groups.erase(it);
                }
            }
            break;
        }
        case mbox::action_type::focus:
        {
            const auto find_focus = send.find(action.focus());
            if (find_focus != send.end())
                ICY_ERROR(find_focus->value.input.push_back(input_message(true)));
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

            array<key> mods;
            if (button.mod & key_mod::lctrl) ICY_ERROR(mods.push_back(key::left_ctrl));
            if (button.mod & key_mod::rctrl) ICY_ERROR(mods.push_back(key::right_ctrl));
            if (button.mod & key_mod::lalt) ICY_ERROR(mods.push_back(key::left_alt));
            if (button.mod & key_mod::ralt) ICY_ERROR(mods.push_back(key::right_alt));
            if (button.mod & key_mod::lshift) ICY_ERROR(mods.push_back(key::left_shift));
            if (button.mod & key_mod::rshift) ICY_ERROR(mods.push_back(key::right_shift));

            for (auto&& mod : mods)
                ICY_ERROR(find_send->value.input.push_back(input_message(input_type::key_press, mod, key_mod::none)));

            if (button.key >= key::mouse_left && button.key <= key::mouse_x2)
            {
               /* if (action.type() == mbox::action_type::button_press ||
                    action.type() == mbox::action_type::on_button_press ||
                    action.type() == mbox::action_type::button_click)
                {
                    input_message msg(input_type::mouse_press, button.key, button.mod);
                    msg.point_x = -1;
                    msg.point_y = -1;
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
                }*/
            }
            else
            {
                if (action.type() == mbox::action_type::button_press ||
                    action.type() == mbox::action_type::on_button_press ||
                    action.type() == mbox::action_type::button_click)
                {
                    auto msg = input_message(input_type::key_press, button.key, button.mod);
                    ICY_ERROR(find_send->value.input.push_back(msg));
                }
                if (action.type() == mbox::action_type::button_release ||
                    action.type() == mbox::action_type::on_button_release ||
                    action.type() == mbox::action_type::button_click)
                {
                    auto msg = input_message(input_type::key_release, button.key, button.mod);
                    ICY_ERROR(find_send->value.input.push_back(msg));
                }
            }

            for (auto it = mods.rbegin(); it != mods.rend(); ++it)
                ICY_ERROR(find_send->value.input.push_back(input_message(input_type::key_release, *it, key_mod::none)));
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
                ICY_ERROR(character->value.bevents.push_back(event));
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
                    const auto virt = character->value.virt.try_find(execute.command);
                    auto command = m_library.find(virt ? *virt : execute.command);
                    if (command && command->type == mbox::type::command)
                    {
                        ICY_ERROR(process(character, command->actions, send));
                    }
                }
                else
                {
                    for (auto&& other : m_data)
                    {
                        if (other.key == character->key && execute.etype == mbox::execute_type::multicast)
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
                            ICY_ERROR(process(other, command->actions, send));
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
                const auto find_virt = character.value.virt.find(source->index);
                if (find_virt == character.value.virt.end())
                {
                    ICY_ERROR(character.value.virt.insert(replace.source, replace.target));
                }
                else if (target && target != source)
                {
                    find_virt->value = replace.target;
                }
                else
                {
                    character.value.virt.erase(find_virt);
                }
            }
            break;
        }
        }
    }
    return {};
}
error_type application::process(map<guid, data_type>::pair_type character, const const_array_view<input_message> data, map<guid, mbox_send>& send) noexcept
{
    for (auto&& msg : data)
    {
        if (msg.key == key::none)
            continue;

        auto press = false;

        if (msg.type == input_type::mouse_press ||
            msg.type == input_type::mouse_release)
        {
            press = msg.type == input_type::mouse_press;
        }
        else if (msg.type == input_type::key_press)
        {
            press = true;
        }
        else if (msg.type == input_type::key_release)
        {
            press = false;
        }
        else
            continue;

        for (auto&& bevent : character.value.bevents)
        {
            if (bevent.press != press || bevent.button.key != msg.key || !icy::key_mod_and(bevent.button.mod, key_mod(msg.mods)))
                continue;

            const auto virt = character.value.virt.try_find(bevent.command);
            const auto command = m_library.find(virt ? *virt : bevent.command);
            if (!command || command->type != mbox::type::command)
                continue;

            ICY_ERROR(process(character, command->actions, send));
        }
    }
    return {};
}
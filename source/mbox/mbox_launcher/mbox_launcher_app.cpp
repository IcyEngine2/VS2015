#include "mbox_launcher_app.hpp"
#include <icy_engine/core/icy_file.hpp>
#include <icy_engine/utility/icy_process.hpp>

using namespace icy;

static const auto hook_lib = 
#if _DEBUG
    "mbox_dlld";
#else
    "mbox_dll";
#endif

error_type mbox_launcher_app::menu_type::initialize(const gui_widget window) noexcept
{
    ICY_ERROR(menu.initialize(gui_widget_type::menubar, window));
    ICY_ERROR(config.initialize("Config"_s));
    ICY_ERROR(debug.initialize("Debug"_s));
    ICY_ERROR(menu.insert(config));
    ICY_ERROR(menu.insert(debug));
    return error_type();
}
error_type mbox_launcher_app::debug_type::initialize(icy::gui_widget parent_window) noexcept
{
    ICY_ERROR(window.initialize(gui_widget_type::window, parent_window, gui_widget_flag::layout_vbox));
    ICY_ERROR(text.initialize(gui_widget_type::text_edit, window));
    ICY_ERROR(text.readonly(true));
    print = [](void* ptr, const icy::string_view str)
    {
        auto widget = static_cast<xgui_text_edit*>(ptr);
        if (str.find("\r\n"_s) == str.begin())
        {
            string time_str;
            ICY_ERROR(to_string(clock_type::now(), time_str));
            string msg;
            ICY_ERROR(msg.appendf("\r\n[%1]: %2"_s, string_view(time_str), string_view(str.begin() + 2, str.end())));
            ICY_ERROR(widget->append(msg));
        }
        else
        {
            ICY_ERROR(widget->append(str));
        }        
        return error_type();
    };
    return error_type();
}
error_type mbox_launcher_app::library_type::initialize(const gui_widget window) noexcept
{
    ICY_ERROR(label.initialize(gui_widget_type::label, window));
    ICY_ERROR(view.initialize(gui_widget_type::line_edit, window));
    ICY_ERROR(button.initialize(gui_widget_type::tool_button, window));
    ICY_ERROR(label.insert(gui_insert(0, 0)));
    ICY_ERROR(view.insert(gui_insert(1, 0)));
    ICY_ERROR(button.insert(gui_insert(2, 0)));
    ICY_ERROR(label.text("Script Path"_s));
    ICY_ERROR(view.enable(false));
    return error_type();
}
error_type mbox_launcher_app::party_type::initialize(const gui_widget window) noexcept
{
    ICY_ERROR(model.initialize());
    ICY_ERROR(label.initialize(gui_widget_type::label, window));
    ICY_ERROR(combo.initialize(gui_widget_type::combo_box, window));
    ICY_ERROR(button.initialize(gui_widget_type::text_button, window));
    ICY_ERROR(button.enable(false));
    ICY_ERROR(label.text("Select Party"_s));    
    ICY_ERROR(label.insert(gui_insert(0, 1)));
    ICY_ERROR(combo.insert(gui_insert(1, 1)));
    ICY_ERROR(button.insert(gui_insert(2, 1)));    
    ICY_ERROR(button.text("Load"_s));
    ICY_ERROR(combo.bind(model));
    return error_type();
}
error_type mbox_launcher_app::games_type::initialize(const gui_widget window) noexcept
{
    ICY_ERROR(model.initialize());
    ICY_ERROR(label.initialize(gui_widget_type::label, window));
    ICY_ERROR(combo.initialize(gui_widget_type::combo_box, window));
    ICY_ERROR(button.initialize(gui_widget_type::text_button, window));
    ICY_ERROR(label.text("Select Game"_s));
    ICY_ERROR(button.text("Pause"_s));
    ICY_ERROR(label.insert(gui_insert(0, 2)));
    ICY_ERROR(combo.insert(gui_insert(1, 2)));
    ICY_ERROR(button.insert(gui_insert(2, 2)));
    ICY_ERROR(combo.bind(model));
    return error_type();
}
error_type mbox_launcher_app::data_type::initialize(const gui_widget window) noexcept
{
    ICY_ERROR(model.initialize());
    ICY_ERROR(view.initialize(gui_widget_type::grid_view, window));
    ICY_ERROR(view.insert(gui_insert(0, 3, 3, 1)));
    ICY_ERROR(model.hheader(0, ""_s));
    ICY_ERROR(view.bind(model));

    string path;
    ICY_ERROR(process_directory(path));
    ICY_ERROR(path.append("mbox_config"_s));
    ICY_ERROR(make_directory(path));
    ICY_ERROR(path.appendf("/%1.txt"_s, process_index()));
    ICY_ERROR(tmp_file.open(path, file_access::none, file_open::create_always, file_share::none, file_flag::delete_on_close))

    return error_type();
}
error_type mbox_launcher_app::initialize() noexcept
{
    ICY_ERROR(create_event_system(m_gui));
    global_gui = m_gui;

    if (const auto error = m_config.load())
        ICY_ERROR(show_error(error, "Load mbox launcher config"_s));

    ICY_ERROR(create_event_system(m_local));
    ICY_ERROR(m_local->thread().launch());
    ICY_ERROR(m_local->thread().rename("Overlay Thread"_s));
    ICY_ERROR(m_local->create(m_overlay, window_flags::layered));

    ICY_ERROR(m_window.initialize(gui_widget_type::window, gui_widget(), gui_widget_flag::layout_grid));

    ICY_ERROR(m_menu.initialize(m_window));
    ICY_ERROR(m_debug.initialize(m_window));
    ICY_ERROR(m_library.initialize(m_window));
    ICY_ERROR(m_party.initialize(m_window));
    ICY_ERROR(m_games.initialize(m_window));
    ICY_ERROR(m_data.initialize(m_window));

    ICY_ERROR(reset_library(m_config.default_script));
    ICY_ERROR(reset_games(m_config.default_game));
    ICY_ERROR(reset_data());

    return error_type();
}
error_type mbox_launcher_app::reset_library(const string_view path) noexcept
{
    if (path.empty())
        return error_type();

    mbox::mbox_errors mbox_errors;
    if (const auto error = m_library.data.initialize(path, mbox_errors))
        return show_error(error, "MBox open script"_s);

    ICY_ERROR(m_library.view.text(path));
    ICY_ERROR(copy(path, m_config.default_script));

    auto party = mbox::mbox_index();
    if (m_library.data.find(m_config.default_party).type == mbox::mbox_type::party)
        party = m_config.default_party;
    else
        m_config.default_party = party;

    array<const mbox::mbox_object*> party_vec;
    ICY_ERROR(m_library.data.enum_by_type(mbox::mbox_index(), mbox::mbox_type::party, party_vec));

    using pair_type = std::pair<string, mbox::mbox_index>;
    array<pair_type> sorted_party;
    for (auto k = 0u; k < party_vec.size(); ++k)
    {
        string str;
        ICY_ERROR(m_library.data.rpath(party_vec[k]->index, str));
        ICY_ERROR(sorted_party.push_back(std::make_pair(std::move(str), party_vec[k]->index)));
    }
    std::sort(sorted_party.begin(), sorted_party.end(), [](const pair_type& lhs, const pair_type& rhs) { return lhs.first < rhs.first; });

    ICY_ERROR(m_party.model.clear());
    ICY_ERROR(m_party.model.insert_rows(0, 1 + sorted_party.size()));

    auto row = 0u;
    ICY_ERROR(m_party.model.node(row, 0).text("-"_s));
    ++row;

    for (auto&& pair : sorted_party)
    {
        ICY_ERROR(m_party.model.node(row, 0).text(pair.first));
        ICY_ERROR(m_party.model.node(row, 0).udata(pair.second));
        if (pair.second == party)
            ICY_ERROR(m_party.combo.scroll(m_party.model.node(row, 0)));
        ++row;
    }
    return on_select_party(party);
}
error_type mbox_launcher_app::reset_games(const string_view select) noexcept
{
    ICY_ERROR(m_games.model.clear());
    ICY_ERROR(m_games.model.insert_rows(0, 1 + m_config.games.size()));

    auto row = 0u;
    ICY_ERROR(m_games.model.node(row, 0).text("Unknown Game"_s));
    ++row;

    for (auto&& pair : m_config.games)
    {
        ICY_ERROR(m_games.model.node(row, 0).text(pair.key));
        if (pair.key == select)
            ICY_ERROR(m_games.combo.scroll(m_games.model.node(row, 0)));
        ++row;
    }
    ICY_ERROR(copy(select, m_config.default_game));
    return error_type();
}
error_type mbox_launcher_app::reset_data() noexcept
{
    ICY_ERROR(m_data.model.clear());
    ICY_ERROR(m_data.model.insert_cols(0, 4));
    ICY_ERROR(m_data.model.insert_rows(0, m_data.characters.size()));

    ICY_ERROR(m_data.model.hheader(0, "Index"_s));
    ICY_ERROR(m_data.model.hheader(1, "Name"_s));
    ICY_ERROR(m_data.model.hheader(2, "Process ID"_s));
    ICY_ERROR(m_data.model.hheader(3, "Window"_s));

    auto row = 0u;
    for (auto&& chr : m_data.characters)
    {
        string str_slot_index;
        ICY_ERROR(to_string(chr.slot, str_slot_index));

        ICY_ERROR(m_data.model.node(row, 0).text(str_slot_index));
        ICY_ERROR(m_data.model.node(row, 1).text(chr.name));
        if (chr.window)
        {
            string str_process_index;
            string str_window_name;
            ICY_ERROR(to_string(chr.window.process(), str_process_index));
            ICY_ERROR(chr.window.name(str_window_name));
            ICY_ERROR(m_data.model.node(row, 2).text(str_process_index));
            ICY_ERROR(m_data.model.node(row, 3).text(str_window_name));
        }
        ++row;
    }
    ICY_ERROR(m_gui->resize_columns(m_data.view));
    return error_type();
}
error_type mbox_launcher_app::run() noexcept
{
    ICY_SCOPE_EXIT{ event::post(nullptr, event_type::global_quit); };
    ICY_SCOPE_EXIT{ m_config.save(); };

    shared_ptr<event_queue> queue;
    ICY_ERROR(create_event_system(queue, event_type::gui_any | event_type::window_data |
        remote_window_event_type | mbox::mbox_event_type_send_input | event_type::system_error));
    ICY_ERROR(m_window.show(true));

    while (true)
    {
        event event;
        ICY_ERROR(queue->pop(event));
        if (event->type == event_type::global_quit)
            return error_type();
        if (event->type == event_type::gui_update)
        {
            const auto& event_data = event->data<gui_event>();
            if (event_data.widget == m_window)
                break;
            if (event_data.widget == m_debug.window)
                ICY_ERROR(m_debug.window.show(false));
        }
        else if (event->type == event_type::system_error)
        {
            const auto system = shared_ptr<event_system>(event->source);
            if (system && system.get() == m_data.system.get())
            {
                ICY_ERROR(show_error(event->data<error_type>(), "MBox system has crashed"_s));
                if (!m_data.characters.empty())
                    ICY_ERROR(launch_mbox());
            }
        }

        const auto proc = [&]
        {
            if (event->type == event_type::window_data)
            {
                const auto& event_data = event->data<window_message>();
                if (event_data.data.bytes.size() == sizeof(input_message))
                {
                    const auto input = reinterpret_cast<const input_message*>(event_data.data.bytes.data());
                    auto set_focus = 0u;

                    for (auto&& focus : m_config.focus)
                    {
                        if (focus.input.type == input->type && focus.input.key == input->key 
                            && key_mod_and(key_mod(focus.input.mods), key_mod(input->mods)))
                        {
                            set_focus = focus.index;
                            break;
                        }
                    }

                    if (set_focus)
                    {
                        for (auto&& chr : m_data.characters)
                        {
                            if (chr.slot == set_focus && chr.window)
                                return chr.window.activate();
                        }
                    }
                    const auto process = event_data.data.user;
                    for (auto&& chr : m_data.characters)
                    {
                        if (chr.window.process() == process)
                        {
                            ICY_ERROR(m_data.system->post(chr.index, *input));
                            break;
                        }
                    }
                }
            }
            else if (event->type == remote_window_event_type)
            {
                const auto hwnd = event->data<HWND__*>();
                for (auto&& chr : m_data.characters)
                {
                    if (chr.window.handle() == hwnd)
                    {
                        chr.window = remote_window();
                        ICY_ERROR(reset_data());
                        break;
                    }
                }
            }
            else if (event->type == mbox::mbox_event_type_send_input)
            {
                const auto& event_data = event->data<mbox::mbox_event_send_input>();
                if (m_data.paused)
                    return error_type();

                for (auto&& chr : m_data.characters)
                {
                    if (chr.index != event_data.character)
                        continue;

                    if (!chr.window)
                        break;

                    for (auto&& msg : event_data.messages)
                    {
                        if (msg.type != input_type::none)
                            ICY_ERROR(chr.window.send(msg, std::chrono::seconds(1)));
                    }
                    break;
                }
            }
            else if (event->type & event_type::gui_any)
            {
                const auto& event_data = event->data<gui_event>();
                if (event->type == event_type::gui_update)
                {
                    if (event_data.widget == m_library.button)
                    {
                        if (!m_data.system)
                        {
                            string file_name;
                            ICY_ERROR(dialog_open_file(file_name));
                            if (file_name.empty())
                                return error_type();

                            ICY_ERROR(reset_library(file_name));
                        }
                    }
                    else if (event_data.widget == m_party.button)
                    {
                        if (m_data.system)
                        {
                            ICY_ERROR(on_stop());
                        }
                        else
                        {
                            ICY_ERROR(on_start());
                        }
                    }
                    else if (event_data.widget == m_games.button)
                    {
                        if (m_data.paused)
                        {
                            m_data.paused = false;
                            for (auto&& chr : m_data.characters)
                                ICY_ERROR(hook(chr));                            
                            ICY_ERROR(m_games.button.text("Pause"_s));
                        }
                        else
                        {
                            m_data.paused = true;
                            for (auto&& chr : m_data.characters)
                            {
                                if (chr.window)
                                    ICY_ERROR(chr.window.hook(hook_lib, nullptr));
                            }
                            ICY_ERROR(m_games.button.text("Resume"_s));
                        }
                    }
                }
                else if (event->type == event_type::gui_select)
                {
                    if (event_data.widget == m_party.combo)
                    {
                        ICY_ERROR(on_select_party(event_data.data.as_node().udata().as_guid()));
                    }
                    else if (event_data.widget == m_games.combo)
                    {
                        const auto row = event_data.data.as_node().row();
                        if (row > 0 && row < m_config.games.size() + 1)
                        {
                            auto it = m_config.games.begin();
                            std::advance(it, row - 1);
                            ICY_ERROR(copy(it->key, m_config.default_game));
                        }
                        else
                        {
                            m_config.default_game.clear();
                        }
                    }
                }
                else if (event->type == event_type::gui_context)
                {
                    if (event_data.widget == m_data.view && event_data.data.as_node())
                    {
                        const auto row = event_data.data.as_node().row();
                        if (row < m_data.characters.size())
                            ICY_ERROR(on_context(m_data.characters[row]));
                    }
                }
                else if (event->type == event_type::gui_action)
                {
                    gui_action action;
                    action.index = event_data.data.as_uinteger();
                    if (action == m_menu.config)
                    {
                        ICY_ERROR(m_config.edit());
                        ICY_ERROR(reset_games(m_config.default_game));
                    }
                    else if (action == m_menu.debug)
                    {
                        m_debug.show = !m_debug.show;
                        ICY_ERROR(m_debug.window.show(m_debug.show));
                    }
                }
            }
            return error_type();
        };
        if (const auto error = proc())
            ICY_ERROR(show_error(error, string_view()));
    }
    m_overlay = window();
    return error_type();
}
error_type mbox_launcher_app::on_start() noexcept
{
    string_view path;
    for (auto&& pair : m_config.games)
    {
        if (pair.key == m_config.default_game)
        {
            path = pair.value.path;
            break;
        }
    }
    if (path.empty())
        return show_error(error_type(), "No executable path has been configured for selected game"_s);

    ICY_ERROR(launch_mbox());
    ICY_ERROR(create_event_system(m_data.remote));    
    ICY_ERROR(m_data.remote->thread().launch());
    ICY_ERROR(m_data.remote->thread().rename("Remote window thread"_s));

    const auto& info = m_data.system->info();
    for (auto&& chr : info.characters)
    {
        character_type new_chr;
        new_chr.index = chr.index;
        new_chr.slot = chr.slot;
        ICY_ERROR(copy(chr.events, new_chr.events));
        ICY_ERROR(copy(chr.name, new_chr.name));
        ICY_ERROR(m_data.characters.push_back(std::move(new_chr)));
    }
    std::sort(m_data.characters.begin(), m_data.characters.end(), [](const character_type& lhs, const character_type& rhs) { return lhs.slot < rhs.slot; });

    ICY_ERROR(m_menu.config.enable(false));
    ICY_ERROR(m_library.button.enable(false));
    ICY_ERROR(m_party.combo.enable(false));
    ICY_ERROR(m_games.combo.enable(false));
    ICY_ERROR(m_party.button.text("Clear"_s));
    ICY_ERROR(m_party.button.enable(true));
    ICY_ERROR(reset_data());

    return error_type();
}
error_type mbox_launcher_app::on_stop() noexcept
{
    m_data.paused = false;
    m_data.system = nullptr;
    m_data.remote = nullptr;
    m_data.characters.clear();
    
    ICY_ERROR(m_menu.config.enable(true));
    ICY_ERROR(m_library.button.enable(true));
    ICY_ERROR(m_party.combo.enable(true));
    ICY_ERROR(m_party.button.text("Load"_s));
    ICY_ERROR(m_games.combo.enable(true));
    ICY_ERROR(m_menu.debug.enable(true));
    ICY_ERROR(reset_data());

    return error_type();
}
error_type mbox_launcher_app::on_context(character_type& chr) noexcept
{
    xgui_widget menu;
    ICY_ERROR(menu.initialize(gui_widget_type::menu, m_window));
    xgui_action action_clear;
    xgui_action action_find;
    xgui_action action_launch;

    string_view path;
    for (auto&& pair : m_config.games)
    {
        if (pair.key == m_config.default_game)
            path = pair.value.path;
    }

    ICY_ERROR(action_clear.initialize("Clear"_s));
    ICY_ERROR(action_find.initialize("Find"_s));
    ICY_ERROR(action_launch.initialize("Launch"_s));

    ICY_ERROR(action_clear.enable(false));
    ICY_ERROR(action_find.enable(false));
    ICY_ERROR(action_launch.enable(false));
    
    ICY_ERROR(menu.insert(action_clear));
    ICY_ERROR(menu.insert(action_find));
    ICY_ERROR(menu.insert(action_launch));

    if (chr.window)
    {
        ICY_ERROR(action_clear.enable(true));
    }
    else
    {
        ICY_ERROR(action_find.enable(true));
        ICY_ERROR(action_launch.enable(!path.empty()));
    }

    gui_action action;
    ICY_ERROR(menu.exec(action));

    if (action == action_clear)
    {
        chr.window = remote_window();
        ICY_ERROR(reset_data());
    }
    else if (action == action_find || action == action_launch)
    {
        remote_window new_window;
        if (action == action_find)
        {
            ICY_ERROR(find_window(path, new_window));
        }
        else if (action == action_launch)
        {
            ICY_ERROR(launch_window(path, new_window));
        }
        if (new_window)
        {
            chr.window = new_window;
            ICY_ERROR(m_data.remote->notify_on_close(new_window));
            ICY_ERROR(hook(chr));
            ICY_ERROR(reset_data());
        }
    }
    return error_type();
}
error_type mbox_launcher_app::on_select_party(const mbox::mbox_index select) noexcept
{
    if (m_data.system)
        return error_type(); // "INVALID LAUNCHER STATE";

    m_config.default_party = select;
    ICY_ERROR(m_party.button.enable(m_config.default_party != mbox::mbox_index()));

    const auto& party_object = m_library.data.find(select);
    array<string> profiles;

    if (party_object.type == mbox::mbox_type::party)
    {
        for (auto&& pair : party_object.refs)
        {
            const auto& chr = m_library.data.find(pair.value);
            if (chr.type != mbox::mbox_type::character)
                continue;

            auto parent_ptr = &chr;
            while (*parent_ptr)
            {
                if (parent_ptr->type == mbox::mbox_type::profile)
                {
                    string str;
                    ICY_ERROR(to_lower(parent_ptr->name, str));
                    ICY_ERROR(profiles.push_back(std::move(str)));
                    break;
                }
                parent_ptr = &m_library.data.find(parent_ptr->parent);
            }
        }
    }

    auto best_game = m_config.games.end();
    auto best_count = 0u;
    
    m_config.default_game.clear();

    for (auto it = m_config.games.begin(); it != m_config.games.end(); ++it)
    {
        array<string_view> tags;
        ICY_ERROR(split(it->value.tags, tags, ";"_s));
        ICY_ERROR(tags.push_back(it->key));

        auto count = 0u;

        array<string> tags_lower;
        for (auto&& tag : tags)
        {
            string str;
            ICY_ERROR(to_lower(tag, str));
            ICY_ERROR(tags_lower.push_back(std::move(str)));
        }
        for (auto&& profile : profiles)
        {
            for (auto&& tag : tags_lower)
            {
                if (profile.find(tag) != profile.end())
                    count++;
            }
        }
        if (!count)
            continue;

        if (count > best_count)
        {
            best_game = it;
            best_count = count;
            ICY_ERROR(copy(it->key, m_config.default_game));
        }
    }
    
    auto row = 0_z;
    if (best_game != m_config.games.end())
        row = 1 + std::distance(m_config.games.begin(), best_game);

    ICY_ERROR(m_games.combo.scroll(m_games.model.node(row, 0)));
    return error_type();
}
error_type mbox_launcher_app::find_window(const string_view path, remote_window& new_window) const noexcept
{
    array<remote_window> vec;
    {
        array<remote_window> find_windows;
        ICY_ERROR(remote_window::enumerate(find_windows));

        for (auto&& window : find_windows)
        {
            if (window.process() == process_index())
                continue;

            auto found = false;
            for (auto&& chr : this->m_data.characters)
            {
                if (chr.window.process() == window.process())
                {
                    found = true;
                    break;
                }
            }
            if (found)
                continue;

            if (!path.empty())
            {
                string str;
                ICY_ERROR(process_path(window.process(), str));
                if (str != path)
                    continue;
            }

            ICY_ERROR(vec.push_back(std::move(window)));
        }
    }
    if (vec.empty())
        return error_type();

    xgui_model find_model;
    ICY_ERROR(find_model.initialize());
    ICY_ERROR(find_model.insert_rows(0, vec.size()));
    for (auto k = 0u; k < vec.size(); ++k)
    {
        string window_name;
        ICY_ERROR(vec[k].name(window_name));

        string str;
        ICY_ERROR(to_string("[%1] %2"_s, str, vec[k].process(), string_view(window_name)));

        auto node = find_model.node(k, 0);
        if (!node)
            return make_stdlib_error(std::errc::not_enough_memory);

        ICY_ERROR(node.text(str));
    }


    xgui_widget window;
    ICY_ERROR(window.initialize(gui_widget_type::dialog, gui_widget(), gui_widget_flag::layout_vbox));

    xgui_widget view;
    ICY_ERROR(view.initialize(gui_widget_type::list_view, window));
    ICY_ERROR(view.bind(find_model));

    xgui_widget buttons;
    ICY_ERROR(buttons.initialize(gui_widget_type::none, window, gui_widget_flag::layout_hbox | gui_widget_flag::auto_insert));

    xgui_widget button_okay;
    ICY_ERROR(button_okay.initialize(gui_widget_type::text_button, buttons));
    ICY_ERROR(button_okay.text("Select"_s));

    xgui_widget button_exit;
    ICY_ERROR(button_exit.initialize(gui_widget_type::text_button, buttons));
    ICY_ERROR(button_exit.text("Cancel"_s));

    ICY_ERROR(window.show(true));

    shared_ptr<event_queue> loop;
    ICY_ERROR(create_event_system(loop, event_type::gui_select | event_type::gui_update));

    remote_window select_window;
    ICY_ERROR(button_okay.enable(false));

    while (true)
    {
        event event;
        ICY_ERROR(loop->pop(event));
        if (event->type == event_type::global_quit)
            return event::post(nullptr, event_type::global_quit);

        const auto& event_data = event->data<gui_event>();
        if (event->type == event_type::gui_update)
        {
            if (event_data.widget == button_exit || event_data.widget == window)
                return error_type();

            if (event_data.widget == button_okay)
            {
                new_window = select_window;
                break;
            }
        }
        else if (event->type == event_type::gui_select)
        {
            auto node = event_data.data.as_node();
            if (node.model() == find_model.model())
            {
                select_window = vec[node.row()];
                ICY_ERROR(button_okay.enable(true));
            }
        }
    }
    return error_type();
}
error_type mbox_launcher_app::launch_window(const string_view path, remote_window& new_window) const noexcept
{
    auto process = 0u;
    auto thread = 0u;
    ICY_ERROR(process_launch(path, process, thread));

    xgui_widget window;
    ICY_ERROR(window.initialize(gui_widget_type::dialog, gui_widget(), gui_widget_flag::layout_vbox));

    string str;
    ICY_ERROR(str.appendf("Please wait: this window will close when '%1' is launched\r\n\r\nGamePath = \"%2\"\r\nProcess ID = %3"_s,
        string_view(m_config.default_game), path, process));

    xgui_widget label;
    ICY_ERROR(label.initialize(gui_widget_type::label, window));
    ICY_ERROR(label.text(str));
    
    xgui_widget buttons;
    ICY_ERROR(buttons.initialize(gui_widget_type::none, window, gui_widget_flag::layout_hbox | gui_widget_flag::auto_insert));

    xgui_widget button_okay;
    ICY_ERROR(button_okay.initialize(gui_widget_type::text_button, buttons));
    ICY_ERROR(button_okay.text("Ok"_s));

    xgui_widget button_exit;
    ICY_ERROR(button_exit.initialize(gui_widget_type::text_button, buttons));
    ICY_ERROR(button_exit.text("Cancel"_s));

    timer timer;
    ICY_ERROR(timer.initialize(SIZE_MAX, std::chrono::milliseconds(500)));

    shared_ptr<event_queue> loop;
    ICY_ERROR(create_event_system(loop, event_type::global_timer | event_type::gui_update));
    ICY_ERROR(window.show(true));
    ICY_ERROR(button_okay.enable(false));

    while (true)
    {
        event event;
        ICY_ERROR(loop->pop(event));
        if (event->type == event_type::gui_update || event->type == event_type::global_quit)
            return error_type();

        if (event->type == event_type::global_timer)
        {
            const auto event_data = event->data<timer::pair>();
            if (event_data.timer == &timer)
            {
                array<remote_window> find_windows;
                ICY_ERROR(remote_window::enumerate(find_windows));
                for (auto&& window : find_windows)
                {
                    if (window.process() == process)
                    {
                        new_window = find_windows.front();
                        if (new_window.thread() == thread)
                            break;
                    }
                }
                if (new_window)
                    break;
            }
        }
    }
    return error_type();
}
error_type mbox_launcher_app::hook(character_type& chr) noexcept
{
    if (!chr.window || m_data.paused)
        return error_type();

    mbox::mbox_dll_config config;
    config.thread = m_local->thread().index();

    config.pause_count = std::min(_countof(config.pause_array), m_config.pause.size());
    for (auto k = 0u; k < config.pause_count; ++k)
        config.pause_array[k] = m_config.pause[k];

    config.event_count = std::min(_countof(config.event_array), m_config.focus.size() + chr.events.size());

    auto offset = 0u;
    for (auto k = 0u; k < m_config.focus.size() && offset < config.event_count; ++k)
        config.event_array[offset++] = m_config.focus[k].input;
    
    for (auto k = 0u; k < chr.events.size() && offset < config.event_count; ++k)
        config.event_array[offset++] = chr.events[k];
    
    ICY_ERROR(config.save(chr.window.process()));
    ICY_ERROR(chr.window.hook(hook_lib, GetMessageHook));
    return error_type();
}

error_type mbox_launcher_app::launch_mbox() noexcept
{
    if (m_debug.show)
    {
        string name;
        ICY_ERROR(m_library.data.rpath(m_config.default_party, name));
        ICY_ERROR(name.insert(name.begin(), "Launch: "_s));
        ICY_ERROR(m_debug.text.text(name));
    }

    if (const auto error = create_event_system(m_data.system, m_library.data, m_config.default_party, m_debug.show ? m_debug.print : nullptr, &m_debug.text))
    {
        ICY_ERROR(show_error(error, "Create mbox system"_s));
    }
    else
    {
        ICY_ERROR(m_data.system->thread().launch());
        ICY_ERROR(m_data.system->thread().rename("MBox system thread"_s));
    }
    if (!m_debug.show)
        ICY_ERROR(m_menu.debug.enable(false));

    return error_type();
}

error_type mbox::mbox_dll_config::save(const uint32_t process) noexcept
{
    string path;
    ICY_ERROR(process_directory(path));
    ICY_ERROR(path.appendf("%1%2.txt"_s, "mbox_config/"_s, process));

    json json_main = json_type::object;
    ICY_ERROR(json_main.insert("thread"_s, thread));
    
    json json_event = json_type::array;
    json json_pause = json_type::array;
    for (auto k = 0u; k < pause_count; ++k)
    {
        json obj;
        ICY_ERROR(pause_array[k].to_json(obj));
        ICY_ERROR(json_pause.push_back(std::move(obj)));        
    }
    for (auto k = 0u; k < event_count; ++k)
    {
        json obj = json_type::object;
        ICY_ERROR(obj.insert("input.type"_s, uint32_t(event_array[k].type)));
        ICY_ERROR(obj.insert("input.key"_s, uint32_t(event_array[k].key)));
        ICY_ERROR(obj.insert("input.mods"_s, uint32_t(event_array[k].mods)));
        ICY_ERROR(json_event.push_back(std::move(obj)));
    }

    ICY_ERROR(json_main.insert("event"_s, std::move(json_event)));
    ICY_ERROR(json_main.insert("pause"_s, std::move(json_pause)));

    string str;
    ICY_ERROR(to_string(json_main, str));

    file output;
    ICY_ERROR(output.open(path, file_access::write, file_open::create_always, file_share::read));
    ICY_ERROR(output.append(str.bytes().data(), str.bytes().size()));

    return error_type();
}
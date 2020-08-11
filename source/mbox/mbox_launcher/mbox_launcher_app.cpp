#include "mbox_launcher_app.hpp"
#include <icy_engine/core/icy_file.hpp>

using namespace icy;

error_type mbox_launcher_app::menu_type::initialize(const gui_widget window) noexcept
{
    ICY_ERROR(menu.initialize(gui_widget_type::menubar, window));
    ICY_ERROR(config.initialize("Config"_s));
    ICY_ERROR(menu.insert(config));
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
    ICY_ERROR(label.text("Select Game"_s));
    ICY_ERROR(label.insert(gui_insert(0, 2)));
    ICY_ERROR(combo.insert(gui_insert(1, 2)));
    ICY_ERROR(combo.bind(model));
    return error_type();
}
error_type mbox_launcher_app::data_type::initialize(const gui_widget window) noexcept
{
    if (lib.initialize() || lib.find(GetMessageHook) == nullptr)
    {
        show_error(error_type(), "Error loading mbox_dll"_s);
        return make_stdlib_error(std::errc::function_not_supported);
    }

    ICY_ERROR(model.initialize());
    ICY_ERROR(view.initialize(gui_widget_type::grid_view, window));
    ICY_ERROR(view.insert(gui_insert(0, 3, 3, 1)));
    ICY_ERROR(model.hheader(0, string_view()));
    ICY_ERROR(view.bind(model));
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
    ICY_ERROR(m_data.model.hheader(2, "Status"_s));
    ICY_ERROR(m_data.model.hheader(3, "Process ID"_s));
    ICY_ERROR(m_data.model.hheader(4, "Window"_s));

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

            ICY_ERROR(m_data.model.node(row, 2).text(chr.paused ? "Paused"_s : "Running"_s));
            ICY_ERROR(m_data.model.node(row, 3).text(str_process_index));
            ICY_ERROR(m_data.model.node(row, 4).text(str_window_name));
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
        remote_window_event_type | mbox::mbox_event_type_send_input));
    ICY_ERROR(m_window.show(true));

    while (true)
    {
        event event;
        ICY_ERROR(queue->pop(event));
        if (event->type == event_type::global_quit)
            break;

        if (event->type == remote_window_event_type)
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
        else if (event->type & event_type::gui_any)
        {
            const auto& event_data = event->data<gui_event>();
            if (event->type == event_type::gui_update)
            {
                if (event_data.widget == m_window)
                    break;

                if (event_data.widget == m_library.button)
                {
                    ICY_ASSERT(!m_data.system, "INVALID LAUNCHER STATE");

                    string file_name;
                    ICY_ERROR(dialog_open_file(file_name));
                    if (file_name.empty())
                        continue;

                    ICY_ERROR(reset_library(file_name));
                }
                else if (event_data.widget == m_party.button)
                {
                    if (m_data.system)
                    {
                        ICY_ERROR(on_stop());
                    }
                    else
                    {
                        ICY_ERROR(on_launch());
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
            }
        }
    }
    return error_type();
}
error_type mbox_launcher_app::on_launch() noexcept
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

    ICY_ASSERT(!m_data.system, "INVALID LAUNCHER STATE");
    if (const auto error = create_event_system(m_data.system, m_library.data, m_config.default_party))
        return show_error(error, "Launch mbox system"_s);
    
    ICY_ERROR(create_event_system(m_data.remote));    
    const auto& info = m_data.system->info();
    for (auto&& chr : info.characters)
    {
        character_type new_chr;
        new_chr.index = chr.index;
        new_chr.slot = chr.slot;
        ICY_ERROR(copy(chr.name, new_chr.name));
        ICY_ERROR(m_data.characters.push_back(std::move(new_chr)));
    }

    ICY_ERROR(m_menu.config.enable(false));
    ICY_ERROR(m_library.button.enable(false));
    ICY_ERROR(m_party.combo.enable(false));
    ICY_ERROR(m_party.button.text("Clear"_s));
    ICY_ERROR(m_games.combo.enable(false));
    ICY_ERROR(reset_data());

    return error_type();
}
error_type mbox_launcher_app::on_stop() noexcept
{
    m_data.system = nullptr;
    m_data.remote = nullptr;
    m_data.characters.clear();
    
    ICY_ERROR(m_menu.config.enable(true));
    ICY_ERROR(m_library.button.enable(true));
    ICY_ERROR(m_party.combo.enable(true));
    ICY_ERROR(m_party.button.text("Load"_s));
    ICY_ERROR(m_games.combo.enable(true));
    ICY_ERROR(reset_data());

    return error_type();
}
error_type mbox_launcher_app::on_context(character_type& chr) noexcept
{
    xgui_widget menu;
    ICY_ERROR(menu.initialize(gui_widget_type::menu, m_window));
    xgui_action action_pause;
    xgui_action action_clear;
    xgui_action action_find;
    xgui_action action_launch;

    string_view path;
    for (auto&& pair : m_config.games)
    {
        if (pair.key == m_config.default_game)
            path = pair.value.path;
    }

    ICY_ERROR(action_pause.initialize(chr.paused ? "Resume"_s : "Pause"_s));
    ICY_ERROR(action_clear.initialize("Clear"_s));
    ICY_ERROR(action_find.initialize("Find"_s));
    ICY_ERROR(action_launch.initialize("Launch"_s));

    ICY_ERROR(action_pause.enable(false));
    ICY_ERROR(action_clear.enable(false));
    ICY_ERROR(action_find.enable(false));
    ICY_ERROR(action_launch.enable(false));
    
    ICY_ERROR(menu.insert(action_pause));
    ICY_ERROR(menu.insert(action_clear));
    ICY_ERROR(menu.insert(action_find));
    ICY_ERROR(menu.insert(action_launch));

    if (chr.window)
    {
        ICY_ERROR(action_pause.enable(true));
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
    else if (action == action_find)
    {
        remote_window new_window;
        ICY_ERROR(find_window(new_window));
        if (new_window)
        {
            chr.window = new_window;
            ICY_ERROR(new_window.hook(m_data.lib, GetMessageHook));
            ICY_ERROR(m_data.remote->notify_on_close(new_window));
            ICY_ERROR(reset_data());
        }
    }
    else if (action == action_pause)
    {
        if (chr.paused)
        {
            ICY_ERROR(chr.window.hook(m_data.lib, GetMessageHook));
        }
        else
        {
            ICY_ERROR(chr.window.hook(m_data.lib, nullptr));
        }
    }
    else if (action == action_launch)
    {
        ICY_ERROR(reset_data());
    }
    return error_type();
}
error_type mbox_launcher_app::on_select_party(const mbox::mbox_index select) noexcept
{
    ICY_ASSERT(!m_data.system, "INVALID LAUNCHER STATE");
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
error_type mbox_launcher_app::find_window(remote_window& new_window) noexcept
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

#include "mbox_launcher_config.hpp"
#include <icy_engine/core/icy_file.hpp>
#include <icy_engine/core/icy_set.hpp>
#include <icy_qtgui/icy_xqtgui.hpp>
using namespace icy;

namespace icy
{
    string_view to_string(mbox::mbox_pause_type type)
    {
        switch (type)
        {
        case mbox::mbox_pause_type::toggle:
            return "Toggle"_s;
        case mbox::mbox_pause_type::enable:
            return "Enable"_s;
        case mbox::mbox_pause_type::disable:
            return "Disable"_s;
        }
        return ""_s;
    }
}

ICY_STATIC_NAMESPACE_BEG
error_type edit_tags(string& tags)
{
    array<string_view> tags_data;
    ICY_ERROR(split(tags, tags_data, ";"_s));

    set<string> data;
    for (auto&& tag : tags_data)
    {
        if (tag.empty())
            continue;

        string str;
        ICY_ERROR(copy(tag, str));
        if (data.find(str) != data.end())
            continue;

        ICY_ERROR(data.insert(std::move(str)));
    }
    
    xgui_model model;
    ICY_ERROR(model.initialize());

    

    xgui_widget window;
    ICY_ERROR(window.initialize(gui_widget_type::dialog, gui_widget(), gui_widget_flag::layout_vbox));
    ICY_ERROR(window.text("Edit game tags"_s));

    xgui_widget view;
    ICY_ERROR(view.initialize(gui_widget_type::list_view, window));
    ICY_ERROR(view.bind(model));

    xgui_widget buttons;
    xgui_widget button_save;
    xgui_widget button_exit;
    ICY_ERROR(buttons.initialize(gui_widget_type::none, window, gui_widget_flag::layout_hbox | gui_widget_flag::auto_insert));
    ICY_ERROR(button_save.initialize(gui_widget_type::text_button, buttons));
    ICY_ERROR(button_exit.initialize(gui_widget_type::text_button, buttons));
    ICY_ERROR(button_save.text("Save"_s));
    ICY_ERROR(button_exit.text("Cancel"_s));

    const auto remodel = [&]
    {
        ICY_ERROR(model.clear());
        ICY_ERROR(model.insert_rows(0, data.size()));
        auto row = 0u;
        for (auto&& str : data)
            ICY_ERROR(model.node(row++, 0).text(str));

        return error_type();
    };
    ICY_ERROR(remodel());

    shared_ptr<event_queue> queue;
    ICY_ERROR(create_event_system(queue, event_type::gui_any));
    ICY_ERROR(window.show(true));

    while (true)
    {
        event event;
        ICY_ERROR(queue->pop(event));
        if (event->type == event_type::global_quit)
            return error_type();

        const auto& event_data = event->data<gui_event>();
        if (event->type == event_type::gui_update)
        {
            if (event_data.widget == window || event_data.widget == button_exit)
                return error_type();
            else if (event_data.widget == button_save)
                break;
        }
        else if (event->type == event_type::gui_context)
        {
            if (event_data.widget != view)
                continue;

            const auto node = event_data.data.as_node();
            xgui_widget menu;
            ICY_ERROR(menu.initialize(gui_widget_type::menu, window));
            xgui_action action_create;
            xgui_action action_edit;
            xgui_action action_remove;
            ICY_ERROR(action_create.initialize("Create"_s));
            ICY_ERROR(action_edit.initialize("Edit"_s));
            ICY_ERROR(action_remove.initialize("Remove"_s));
            if (node)
            {
                ICY_ERROR(menu.insert(action_edit));
                ICY_ERROR(menu.insert(action_remove));
            }
            else
            {
                ICY_ERROR(menu.insert(action_create));
            }
            gui_action action;
            ICY_ERROR(menu.exec(action));

            if (action == action_create)
            {
                string str;
                ICY_ERROR(xgui_rename(str));
                if (str.empty() || data.try_find(str))
                    continue;
                ICY_ERROR(data.insert(std::move(str)));
                ICY_ERROR(remodel());
            }
            else if (action == action_edit)
            {
                auto it = data.begin();
                std::advance(it, node.row());
                string str;
                ICY_ERROR(copy(*it, str));
                ICY_ERROR(xgui_rename(str));
                if (str.empty() || str == *it)
                    continue;
                if (data.try_find(str))
                    continue;
                data.erase(it);
                ICY_ERROR(data.insert(std::move(str)));
                ICY_ERROR(remodel());
            }
            else if (action == action_remove)
            {
                auto it = data.begin();
                std::advance(it, node.row());
                data.erase(it);
                ICY_ERROR(remodel());
            }
        }

    }
    tags.clear();
    for (auto it = data.begin(); it != data.end(); ++it)
    {
        if (it != data.begin())
            ICY_ERROR(tags.append(";"_s));
        ICY_ERROR(tags.append(*it));
    }
    return error_type();
}
ICY_STATIC_NAMESPACE_END

error_type mbox_config_type::game_type::to_json(json& json) const noexcept
{
    using namespace icy;
    json = json_type::object;
    ICY_ERROR(json.insert("tags"_s, tags));
    ICY_ERROR(json.insert("path"_s, path));
    return error_type();
}
error_type mbox_config_type::game_type::from_json(const json& json) noexcept
{
    using namespace icy;
    if (json.type() != json_type::object)
        return error_type();
    ICY_ERROR(copy(json.get("tags"_s), tags));
    ICY_ERROR(copy(json.get("path"_s), path));
    return error_type();
}
error_type mbox_config_type::focus_type::to_json(json& json) const noexcept
{
    using namespace icy;
    json = json_type::object;
    ICY_ERROR(json.insert("focus.index"_s, index));
    ICY_ERROR(json.insert("input.type"_s, uint32_t(input.type)));
    ICY_ERROR(json.insert("input.key"_s, uint32_t(input.key)));
    ICY_ERROR(json.insert("input.mods"_s, uint32_t(input.mods)));
    return error_type();
}
error_type mbox_config_type::focus_type::from_json(const json& json) noexcept
{
    using namespace icy;
    if (json.type() != json_type::object)
        return make_stdlib_error(std::errc::invalid_argument);
    auto int_input_type = 0u;
    auto int_input_key = 0u;
    auto int_input_mods = 0u;
    ICY_ERROR(json.get("focus.index"_s, index));
    ICY_ERROR(json.get("input.type"_s, int_input_type));
    ICY_ERROR(json.get("input.key"_s, int_input_key));
    ICY_ERROR(json.get("input.mods"_s, int_input_mods));
    input = input_message(input_type(int_input_type), key(int_input_key), key_mod(int_input_mods));
    return error_type();
}

static error_type copy(const mbox_config_type::game_type& src, mbox_config_type::game_type& dst) noexcept
{
    ICY_ERROR(copy(src.tags, dst.tags));
    ICY_ERROR(copy(src.path, dst.path));
    return error_type();
}

error_type mbox_config_type::load() noexcept
{
    string path;
    ICY_ERROR(process_directory(path));
    ICY_ERROR(path.append("mbox_launcher_config.txt"_s));

    file_info info;
    if (info.initialize(path) == error_type())
    {
        file input;
        ICY_ERROR(input.open(path, file_access::read, file_open::open_existing, file_share::read));
        array<char> bytes;
        auto count = info.size;
        ICY_ERROR(bytes.resize(info.size));
        ICY_ERROR(input.read(bytes.data(), count));

        json json_config;
        ICY_ERROR(to_value(string_view(bytes.data(), bytes.size()), json_config));
        const auto json_games_vec = json_config.find("Games"_s);
        if (json_games_vec && json_games_vec->type() == json_type::object)
        {
            for (auto k = 0u; k < json_games_vec->size(); ++k)
            {
                string name;
                ICY_ERROR(copy(json_games_vec->keys().at(k), name));
                const auto& value = json_games_vec->vals().at(k);
                game_type new_game;
                ICY_ERROR(new_game.from_json(value));
                ICY_ERROR(games.insert(std::move(name), std::move(new_game)));
            }
        }
        const auto json_pause_vec = json_config.find("Pause"_s);
        if (json_pause_vec && json_pause_vec->type() == json_type::array)
        {
            for (auto k = 0u; k < json_pause_vec->size(); ++k)
            {
                const auto json_pause = json_pause_vec->at(k);
                mbox::mbox_pause new_pause;
                ICY_ERROR(new_pause.from_json(*json_pause));
                ICY_ERROR(pause.push_back(std::move(new_pause)));
            }
        }
        const auto json_focus_vec = json_config.find("Focus"_s);
        if (json_focus_vec && json_focus_vec->type() == json_type::array)
        {
            for (auto k = 0u; k < json_focus_vec->size(); ++k)
            {
                const auto json_focus = json_focus_vec->at(k);
                focus_type new_focus;
                ICY_ERROR(new_focus.from_json(*json_focus));
                ICY_ERROR(focus.push_back(std::move(new_focus)));
            }
        }
        ICY_ERROR(copy(json_config.get("Default Script"_s), default_script));
        to_value(json_config.get("Default Party"_s), default_party);
    }
    else
    {
        {
            game_type new_game;
            string name;
            ICY_ERROR(copy("World of Warcraft: Retail"_s, name));
            ICY_ERROR(new_game.tags.append("WoW;World of Warcraft;Retail"_s));
            ICY_ERROR(games.insert(std::move(name), std::move(new_game)));
        }
        {
            game_type new_game;
            string name;
            ICY_ERROR(copy("World of Warcraft: Classic"_s, name));
            ICY_ERROR(new_game.tags.append("WoW;World of Warcraft;Classic"_s));
            ICY_ERROR(games.insert(std::move(name), std::move(new_game)));
        }
        {
            game_type new_game;
            string name;
            ICY_ERROR(copy("Final Fantasy XIV"_s, name));
            ICY_ERROR(new_game.tags.append("FFXIV"_s));
            ICY_ERROR(games.insert(std::move(name), std::move(new_game)));
        }
        {
            mbox::mbox_pause pause_enter;
            pause_enter.input = input_message(input_type::key_press, key::enter, key_mod::none);
            pause_enter.type = mbox::mbox_pause_type::toggle;
            ICY_ERROR(pause.push_back(pause_enter));
        }

        {
            mbox::mbox_pause pause_ctrl_p;
            pause_ctrl_p.input = input_message(input_type::key_press, key::p, key_mod::lctrl | key_mod::rctrl);
            pause_ctrl_p.type = mbox::mbox_pause_type::toggle;
            ICY_ERROR(pause.push_back(pause_ctrl_p));
        }
        {
            mbox::mbox_pause pause_slash;
            pause_slash.input = input_message(input_type::key_press, key::slash, key_mod::none);
            pause_slash.type = mbox::mbox_pause_type::enable;
            ICY_ERROR(pause.push_back(pause_slash));
        }
        {
            mbox::mbox_pause pause_esc;
            pause_esc.input = input_message(input_type::key_release, key::esc, key_mod::none);
            pause_esc.type = mbox::mbox_pause_type::disable;
            ICY_ERROR(pause.push_back(pause_esc));
        }
        auto slot = 1u;
        for (auto key = key::F1; key <= key::F10; key = icy::key(uint32_t(key) + 1))
        {
            focus_type new_focus;
            new_focus.input = input_message(input_type::key_press, key, key_mod::none);
            new_focus.index = slot++;
            ICY_ERROR(focus.push_back(new_focus));
        }
        ICY_ERROR(save());
    }
    std::sort(focus.begin(), focus.end(), [](const focus_type& lhs, const focus_type& rhs) { return lhs.index < rhs.index; });
    return error_type();
}
error_type mbox_config_type::save() const noexcept
{
    string path;
    ICY_ERROR(process_directory(path));
    ICY_ERROR(path.append("mbox_launcher_config.txt"_s));

    json json_config = json_type::object;
    json json_games_vec = json_type::object;
    json json_pause_vec = json_type::array;
    json json_focus_vec = json_type::array;

    for (auto&& pair : games)
    {
        json json;
        ICY_ERROR(pair.value.to_json(json));
        ICY_ERROR(json_games_vec.insert(pair.key, std::move(json)));
    }
    for (auto&& p : pause)
    {
        json json;
        ICY_ERROR(p.to_json(json));
        ICY_ERROR(json_pause_vec.push_back(std::move(json)));
    }
    for (auto&& f : focus)
    {
        json json;
        ICY_ERROR(f.to_json(json));
        ICY_ERROR(json_focus_vec.push_back(std::move(json)));
    }
    ICY_ERROR(json_config.insert("Games"_s, std::move(json_games_vec)));
    ICY_ERROR(json_config.insert("Pause"_s, std::move(json_pause_vec)));
    ICY_ERROR(json_config.insert("Focus"_s, std::move(json_focus_vec)));
    ICY_ERROR(json_config.insert("Default Script"_s, default_script));
    string str_guid;
    ICY_ERROR(to_string(default_party, str_guid));
    ICY_ERROR(json_config.insert("Default Party"_s, str_guid));

    string str;
    ICY_ERROR(to_string(json_config, str));

    file input;
    ICY_ERROR(input.open(path, file_access::write, file_open::create_always, file_share::none));
    ICY_ERROR(input.append(str.bytes().data(), str.bytes().size()));

    return error_type();
}
error_type mbox_config_type::edit() noexcept
{
    xgui_widget window;
    ICY_ERROR(window.initialize(gui_widget_type::dialog, gui_widget(), gui_widget_flag::layout_vbox));
    ICY_ERROR(window.text("Edit mbox launcher config"_s));

    xgui_widget tabs;
    xgui_widget buttons;
    xgui_widget button_save;
    xgui_widget button_exit;
    ICY_ERROR(tabs.initialize(gui_widget_type::tabs, window));
    ICY_ERROR(buttons.initialize(gui_widget_type::none, window, gui_widget_flag::layout_hbox | gui_widget_flag::auto_insert));
    ICY_ERROR(button_save.initialize(gui_widget_type::text_button, buttons));
    ICY_ERROR(button_exit.initialize(gui_widget_type::text_button, buttons));
    ICY_ERROR(button_save.text("Save"_s));
    ICY_ERROR(button_exit.text("Cancel"_s));

    xgui_widget tab_games;
    xgui_widget tab_pause;
    xgui_widget tab_focus;
    ICY_ERROR(tab_games.initialize(gui_widget_type::grid_view, tabs));
    ICY_ERROR(tab_pause.initialize(gui_widget_type::grid_view, tabs));
    ICY_ERROR(tab_focus.initialize(gui_widget_type::grid_view, tabs));

    auto gui = shared_ptr<gui_queue>(global_gui);
    ICY_ERROR(gui->text(tabs, tab_games, "Games"_s));
    ICY_ERROR(gui->text(tabs, tab_pause, "Pause"_s));
    ICY_ERROR(gui->text(tabs, tab_focus, "Focus"_s));

    decltype(games) new_games;
    decltype(pause) new_pause;
    decltype(focus) new_focus;
    ICY_ERROR(copy(games, new_games));
    ICY_ERROR(copy(pause, new_pause));
    ICY_ERROR(copy(focus, new_focus));

    xgui_model model_games;
    xgui_model model_pause;
    xgui_model model_focus;
    ICY_ERROR(model_games.initialize());
    ICY_ERROR(model_pause.initialize());
    ICY_ERROR(model_focus.initialize());
    ICY_ERROR(model_games.hheader(0, string_view()));
    ICY_ERROR(model_pause.hheader(0, string_view()));
    ICY_ERROR(model_focus.hheader(0, string_view()));
    ICY_ERROR(tab_games.bind(model_games));
    ICY_ERROR(tab_pause.bind(model_pause));
    ICY_ERROR(tab_focus.bind(model_focus));

    const auto remodel_games = [&]
    {
        ICY_ERROR(model_games.clear());
        ICY_ERROR(model_games.insert_rows(0, new_games.size()));
        ICY_ERROR(model_games.insert_cols(0, 2));
        ICY_ERROR(model_games.hheader(0, "Name"_s));
        ICY_ERROR(model_games.hheader(1, "Tags"_s));
        ICY_ERROR(model_games.hheader(2, "Path"_s));
        auto row = 0u;
        for (auto&& pair : new_games)
        {
            ICY_ERROR(model_games.node(row, 0).text(pair.key));
            ICY_ERROR(model_games.node(row, 1).text(pair.value.tags));
            ICY_ERROR(model_games.node(row, 2).text(pair.value.path));
            ++row;
        }
        ICY_ERROR(gui->resize_columns(tab_games));
        return error_type();
    };
    const auto remodel_pause = [&]
    {
        ICY_ERROR(model_pause.clear());
        ICY_ERROR(model_pause.insert_rows(0, new_pause.size()));
        ICY_ERROR(model_pause.insert_cols(0, 1));
        ICY_ERROR(model_pause.hheader(0, "Key"_s));
        ICY_ERROR(model_pause.hheader(1, "Type"_s));
        auto row = 0u;
        for (auto&& pause : new_pause)
        {
            string str_input;
            ICY_ERROR(to_string(pause.input, str_input));
            ICY_ERROR(model_pause.node(row, 0).text(str_input));
            ICY_ERROR(model_pause.node(row, 1).text(to_string(pause.type)));
            ++row;
        }
        ICY_ERROR(gui->resize_columns(tab_pause));
        return error_type();

    };
    const auto remodel_focus = [&]
    {
        ICY_ERROR(model_focus.clear());
        ICY_ERROR(model_focus.insert_rows(0, new_focus.size()));
        ICY_ERROR(model_focus.insert_cols(0, 1));
        ICY_ERROR(model_focus.hheader(0, "Key"_s));
        ICY_ERROR(model_focus.hheader(1, "Index"_s));
        auto row = 0u;
        for (auto&& f : new_focus)
        {
            string str_input;
            string str_index;
            ICY_ERROR(to_string(f.input, str_input));
            ICY_ERROR(to_string(f.index, str_index));
            ICY_ERROR(model_focus.node(row, 0).text(str_input));
            ICY_ERROR(model_focus.node(row, 1).text(str_index));
            ++row;
        }
        ICY_ERROR(gui->resize_columns(tab_focus));
        return error_type();
    };
    ICY_ERROR(remodel_games());
    ICY_ERROR(remodel_pause());
    ICY_ERROR(remodel_focus());


    shared_ptr<event_queue> queue;
    ICY_ERROR(create_event_system(queue, event_type::gui_any));
    ICY_ERROR(window.show(true));
    while (true)
    {
        event event;
        ICY_ERROR(queue->pop(event));
        if (event->type == event_type::global_quit)
            return error_type();

        const auto& event_data = event->data<gui_event>();
        if (event->type == event_type::gui_update)
        {
            if (event_data.widget == window || event_data.widget == button_exit)
                return error_type();
            else if (event_data.widget == button_save)
                break;
        }
        else if (event->type == event_type::gui_context)
        {
            const auto node = event_data.data.as_node();
            if (event_data.widget == tab_games)
            {
                xgui_widget menu;
                ICY_ERROR(menu.initialize(gui_widget_type::menu, window));
                xgui_action action_create;
                xgui_action action_rename;
                xgui_action action_tags;
                xgui_action action_path;
                xgui_action action_delete;
                ICY_ERROR(action_create.initialize("Create"_s));
                ICY_ERROR(action_rename.initialize("Rename"_s));
                ICY_ERROR(action_tags.initialize("Tags"_s));
                ICY_ERROR(action_path.initialize("Path"_s));
                ICY_ERROR(action_delete.initialize("Delete"_s));

                if (!node)
                {
                    ICY_ERROR(menu.insert(action_create));
                }
                else
                {
                    ICY_ERROR(menu.insert(action_rename));
                    ICY_ERROR(menu.insert(action_tags));
                    ICY_ERROR(menu.insert(action_path));
                    ICY_ERROR(menu.insert(action_delete));
                }

                gui_action action;
                ICY_ERROR(menu.exec(action));
                if (action == action_create)
                {
                    string str;
                    ICY_ERROR(xgui_rename(str));
                    if (str.empty())
                        continue;
                    if (new_games.try_find(str))
                    {
                        ICY_ERROR(show_error(error_type(), "This game name is already used"_s));
                        continue;
                    }
                    ICY_ERROR(new_games.insert(std::move(str), game_type()));
                    ICY_ERROR(remodel_games());
                }
                else if (action == action_rename)
                {
                    string str;
                    auto it = new_games.begin();
                    std::advance(it, node.row());
                    ICY_ERROR(copy(it->key, str));
                    ICY_ERROR(xgui_rename(str));
                    if (str.empty())
                        continue;
                    if (str == it->key)
                        continue;

                    if (new_games.try_find(str))
                    {
                        ICY_ERROR(show_error(error_type(), "This game name is already used"_s));
                        continue;
                    }
                    auto tmp = std::move(it->value);
                    new_games.erase(it);
                    ICY_ERROR(new_games.insert(std::move(str), std::move(tmp)));
                    ICY_ERROR(remodel_games());
                }
                else if (action == action_tags)
                {
                    auto it = new_games.begin();
                    std::advance(it, node.row());
                    ICY_ERROR(edit_tags(it->value.tags));
                    ICY_ERROR(remodel_games());
                }
                else if (action == action_path)
                {
                    string new_path;
                    ICY_ERROR(dialog_open_file(new_path));
                    if (new_path.empty())
                        continue;

                    auto it = new_games.begin();
                    std::advance(it, node.row());
                    it->value.path = std::move(new_path);
                    ICY_ERROR(remodel_games());
                }
                else if (action == action_delete)
                {
                    auto it = new_games.begin();
                    std::advance(it, node.row());
                    new_games.erase(it);
                    ICY_ERROR(remodel_games());
                }
            }
            else if (event_data.widget == tab_pause)
            {

            }
            else if (event_data.widget == tab_focus)
            {

            }
        }
    }

    games = std::move(new_games);
    pause = std::move(new_pause);
    focus = std::move(new_focus);
    ICY_ERROR(save());
    return error_type();
}
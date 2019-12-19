#include "icy_dcl_client_menu_bar.hpp"
#include "icy_dcl_client_text.hpp"

using namespace icy;

error_type dcl_client_menu_network::progress_gui::init(gui_queue& gui, const gui_widget main_window) noexcept
{
    gui_widget version_client_text;
    gui_widget version_server_text;

    ICY_ERROR(gui.create(window, gui_widget_type::window, main_window, gui_widget_flag::layout_grid));
    ICY_ERROR(gui.create(version_client_text, gui_widget_type::label, window));
    ICY_ERROR(gui.create(version_client, gui_widget_type::line_edit, window, gui_widget_flag::read_only));
    ICY_ERROR(gui.create(version_server_text, gui_widget_type::label, window));
    ICY_ERROR(gui.create(version_server, gui_widget_type::line_edit, window, gui_widget_flag::read_only));
    ICY_ERROR(gui.create(progress, gui_widget_type::progress, window));
    ICY_ERROR(gui.create(model, gui_widget_type::tree_view, window));

    const auto row_version_client = 0u;
    const auto row_version_server = 1u;
    const auto row_progress = 2u;
    const auto row_model = 3u;
    const auto col_text = 0u;
    const auto col_view = 1u;
    const auto col_span = 2u;

    ICY_ERROR(gui.insert(version_client_text, col_text, row_version_client));
    ICY_ERROR(gui.insert(version_client, col_view, row_version_client));
    ICY_ERROR(gui.insert(version_server_text, col_text, row_version_server));
    ICY_ERROR(gui.insert(version_server, col_view, row_version_server));
    ICY_ERROR(gui.insert(progress, col_text, row_progress, col_span, 1u));
    ICY_ERROR(gui.insert(model, 0, row_model, col_span, 1u));

    ICY_ERROR(gui.text(version_client_text, to_string(text::version_client)));
    ICY_ERROR(gui.text(version_server_text, to_string(text::version_server)));
    
    return {};
}
error_type dcl_client_menu_network::connect_gui::init(gui_queue& gui, const gui_widget parent_menu, const gui_widget main_window) noexcept
{
    ICY_ERROR(gui.create(action, to_string(text::connect)));
    ICY_ERROR(gui.create(window, gui_widget_type::window, main_window, gui_widget_flag::layout_hbox));
    ICY_ERROR(gui.create(model, gui_widget_type::tree_view, window, gui_widget_flag::auto_insert));
    ICY_ERROR(gui.insert(parent_menu, action));
    return {};
}
error_type dcl_client_menu_network::update_gui::init(gui_queue& gui, const gui_widget parent_menu, const gui_widget main_window) noexcept
{
    ICY_ERROR(gui.create(action, to_string(text::update)));
    ICY_ERROR(progress.init(gui, main_window));
    ICY_ERROR(gui.insert(parent_menu, action));
    ICY_ERROR(gui.enable(action, false));
    return {};
}
error_type dcl_client_menu_network::upload_gui::init(gui_queue& gui, const gui_widget parent_menu, const gui_widget main_window) noexcept
{
    gui_widget buttons;

    ICY_ERROR(gui.create(action, to_string(text::upload)));
    ICY_ERROR(gui.create(window, gui_widget_type::window, main_window, gui_widget_flag::layout_hbox));
    ICY_ERROR(gui.create(message, gui_widget_type::text_edit, window, gui_widget_flag::auto_insert));
    ICY_ERROR(gui.create(model, gui_widget_type::tree_view, window, gui_widget_flag::auto_insert));
    ICY_ERROR(gui.create(buttons, gui_widget_type::none, window, gui_widget_flag::auto_insert));
    ICY_ERROR(gui.create(accept, gui_widget_type::text_button, buttons, gui_widget_flag::auto_insert));
    ICY_ERROR(gui.create(aclear, gui_widget_type::text_button, buttons, gui_widget_flag::auto_insert));
    ICY_ERROR(gui.create(cancel, gui_widget_type::text_button, buttons, gui_widget_flag::auto_insert));

    ICY_ERROR(progress.init(gui, window));
    ICY_ERROR(gui.insert(parent_menu, action));
    ICY_ERROR(gui.enable(action, false));
    return {};
}
error_type dcl_client_menu_network::init(gui_queue& gui, const gui_widget parent_menu, const gui_widget main_window) noexcept
{
    ICY_ERROR(gui.create(m_action, to_string(text::network)));
    ICY_ERROR(gui.create(m_menu, gui_widget_type::menu, parent_menu));
    ICY_ERROR(gui.bind(m_action, m_menu));
    ICY_ERROR(m_connect.init(gui, m_menu, main_window));
    ICY_ERROR(m_update.init(gui, m_menu, main_window));
    ICY_ERROR(m_upload.init(gui, m_menu, main_window));
    ICY_ERROR(gui.insert(parent_menu, m_action));
    return {};
}
error_type dcl_client_menu_project::init(gui_queue& gui, const gui_widget parent_menu, const gui_widget main_window) noexcept
{
    ICY_ERROR(gui.create(m_action, to_string(text::project)));
    ICY_ERROR(gui.create(m_menu, gui_widget_type::menu, parent_menu));
    ICY_ERROR(gui.bind(m_action, m_menu));
    ICY_ERROR(gui.create(m_open_action, to_string(text::open)));
    ICY_ERROR(gui.create(m_open_widget, gui_widget_type::dialog_open_file, main_window));
    ICY_ERROR(gui.create(m_close_action, to_string(text::close)));
    ICY_ERROR(gui.create(m_create_action, to_string(text::create)));
    ICY_ERROR(gui.create(m_create_widget, gui_widget_type::dialog_save_file, main_window));
    ICY_ERROR(gui.create(m_save_action, to_string(text::save)));
    ICY_ERROR(gui.insert(m_menu, m_open_action));
    ICY_ERROR(gui.insert(m_menu, m_close_action));
    ICY_ERROR(gui.insert(m_menu, m_create_action));
    ICY_ERROR(gui.insert(m_menu, m_save_action));
    ICY_ERROR(gui.insert(parent_menu, m_action));
    ICY_ERROR(gui.enable(m_close_action, false));
    ICY_ERROR(gui.enable(m_save_action, false));
    return {};
}
error_type dcl_client_menu_bar::init(gui_queue& gui, const gui_widget main_window) noexcept
{
    ICY_ERROR(gui.create(m_menu, gui_widget_type::menubar, main_window));
    ICY_ERROR(m_network.init(gui, m_menu, main_window));
    ICY_ERROR(m_project.init(gui, m_menu, main_window));
    ICY_ERROR(gui.create(m_options, to_string(text::options)));
    ICY_ERROR(gui.insert(m_menu, m_options));
    return {};
}

error_type dcl_client_menu_network::exec(gui_queue& gui, const event event) noexcept
{
    if (event->type == event_type::window_close)
    {
        const auto event_data = event->data<gui_event>();
        if (event_data.widget == m_connect.window)
        {
            ICY_ERROR(gui.show(m_connect.window, false));
        }
    }
    else if (event->type == event_type::gui_action)
    {
        const auto event_data = event->data<gui_event>();
        gui_action action;
        action.index = event_data.data.as_uinteger();
        if (action == m_connect.action)
        {
            ICY_ERROR(gui.show(m_connect.window, true));
        }
        else if (action == m_update.action)
        {
            ICY_ERROR(gui.show(m_update.progress.window, true));
        }
        if (action == m_upload.action)
        {
            ICY_ERROR(gui.show(m_upload.window, true));
        }
    }
    return {};
}
error_type dcl_client_menu_project::exec(gui_queue& gui, const event event) noexcept
{
    if (event->type == event_type::gui_action)
    {
        gui_action action;
        action.index = event->data<gui_event>().data.as_uinteger();
        if (action == m_open_action)
        {
            ICY_ERROR(gui.show(m_open_widget, true));
        }
        else if (action == m_create_action)
        {
            ICY_ERROR(gui.show(m_create_widget, true));
        }
        else if (action == m_save_action)
        {
            ICY_ERROR(event::post(nullptr, dcl_event_type::project_request_save));
        }
        else if (action == m_close_action)
        {
            ICY_ERROR(event::post(nullptr, dcl_event_type::project_request_close));
        }
    }
    else if (event->type == event_type::gui_update)
    {
        auto event_data = event->data<gui_event>();
        if (event_data.widget == m_open_widget ||
            event_data.widget == m_create_widget)
        {
            dcl_event_type::msg_project_request_open msg;
            ICY_ERROR(to_string(event_data.data.as_string(), msg.filename));
            ICY_ERROR(event::post(nullptr, event_data.widget == m_open_widget ?
                dcl_event_type::project_request_open :
                dcl_event_type::project_request_create, 
                std::move(msg)));
        }
    }
    else if (event->type == dcl_event_type::project_opened || event->type == dcl_event_type::project_closed)
    {
        const auto empty = event->type == dcl_event_type::project_closed;
        ICY_ERROR(gui.enable(m_open_action, empty));
        ICY_ERROR(gui.enable(m_close_action, !empty));
        ICY_ERROR(gui.enable(m_create_action, empty));
        ICY_ERROR(gui.enable(m_save_action, !empty));
    }
    return {};
}
error_type dcl_client_menu_bar::exec(gui_queue& gui, const event event) noexcept
{
    ICY_ERROR(m_network.exec(gui, event));
    ICY_ERROR(m_project.exec(gui, event));
    return {};
}

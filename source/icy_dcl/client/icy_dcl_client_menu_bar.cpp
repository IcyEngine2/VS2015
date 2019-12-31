#include "icy_dcl_client_menu_bar.hpp"
#include "icy_dcl_client_text.hpp"
#include "../icy_dcl_dbase.hpp"

using namespace icy;

error_type dcl_client_menu_network::progress_gui::init(gui_queue& gui, const gui_widget main_window) noexcept
{
    gui_widget version_client_text;
    gui_widget version_server_text;

    ICY_ERROR(gui.create(window, gui_widget_type::window, main_window, gui_widget_flag::layout_grid));
    ICY_ERROR(gui.create(version_client_text, gui_widget_type::label, window));
    ICY_ERROR(gui.create(version_client, gui_widget_type::line_edit, window, gui_widget_flag::none));
    ICY_ERROR(gui.create(version_server_text, gui_widget_type::label, window));
    ICY_ERROR(gui.create(version_server, gui_widget_type::line_edit, window, gui_widget_flag::none));
    ICY_ERROR(gui.create(progress, gui_widget_type::progress, window));
    ICY_ERROR(gui.create(model, gui_widget_type::tree_view, window));

    ICY_ERROR(gui.insert(version_client_text, gui_insert(0, 0)));
    ICY_ERROR(gui.insert(version_client, gui_insert(1, 0)));
    ICY_ERROR(gui.insert(version_server_text, gui_insert(0, 1)));
    ICY_ERROR(gui.insert(version_server, gui_insert(1, 1)));
    ICY_ERROR(gui.insert(progress, gui_insert(0, 2, 2, 1)));
    ICY_ERROR(gui.insert(model, gui_insert(0, 3, 2, 1)));

    ICY_ERROR(gui.text(version_client_text, to_string(dcl_text::version_client)));
    ICY_ERROR(gui.text(version_server_text, to_string(dcl_text::version_server)));
    
    return {};
}
error_type dcl_client_menu_network::connect_gui::init(gui_queue& gui, const gui_widget parent_menu, const gui_widget main_window) noexcept
{
    ICY_ERROR(gui.create(action, to_string(dcl_text::connect)));
    ICY_ERROR(gui.create(window, gui_widget_type::window, main_window, gui_widget_flag::layout_hbox));
    ICY_ERROR(gui.create(model, gui_widget_type::tree_view, window, gui_widget_flag::auto_insert));
    ICY_ERROR(gui.insert(parent_menu, action));
    return {};
}
error_type dcl_client_menu_network::update_gui::init(gui_queue& gui, const gui_widget parent_menu, const gui_widget main_window) noexcept
{
    ICY_ERROR(gui.create(action, to_string(dcl_text::update)));
    ICY_ERROR(progress.init(gui, main_window));
    ICY_ERROR(gui.insert(parent_menu, action));
    ICY_ERROR(gui.enable(action, false));
    return {};
}
error_type dcl_client_menu_network::upload_gui::init(gui_queue& gui, const gui_widget parent_menu, const gui_widget main_window) noexcept
{
    gui_widget buttons;

    ICY_ERROR(gui.create(action, to_string(dcl_text::upload)));
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
    ICY_ERROR(gui.create(m_action, to_string(dcl_text::network)));
    ICY_ERROR(gui.create(m_menu, gui_widget_type::menu, parent_menu));
    ICY_ERROR(gui.bind(m_action, m_menu));
    ICY_ERROR(m_connect.init(gui, m_menu, main_window));
    ICY_ERROR(m_update.init(gui, m_menu, main_window));
    ICY_ERROR(m_upload.init(gui, m_menu, main_window));
    ICY_ERROR(gui.insert(parent_menu, m_action));
    return {};
}
error_type dcl_client_menu_bar::init(gui_queue& gui, const gui_widget main_window) noexcept
{
    ICY_ERROR(gui.create(m_menu, gui_widget_type::menubar, main_window));
    ICY_ERROR(m_network.init(gui, m_menu, main_window));
    ICY_ERROR(gui.create(m_options, to_string(dcl_text::options)));
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
error_type dcl_client_menu_bar::exec(gui_queue& gui, const event event) noexcept
{
    ICY_ERROR(m_network.exec(gui, event));
    return {};
}

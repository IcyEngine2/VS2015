#include "icy_dcl_client_menu.hpp"
#include "icy_dcl_client_text.hpp"

using namespace icy;

error_type dcl_client_menu_network::progress_gui::init(gui_queue& gui, const gui_widget parent) noexcept
{
    gui_widget version_client_text;
    gui_widget version_server_text;
    gui_widget progress_total_text;
    gui_widget progress_citem_text;

    ICY_ERROR(gui.create(window, gui_widget_type::window, gui_layout::grid, parent));
    ICY_ERROR(gui.create(version_client_text, gui_widget_type::label, gui_layout::none, window));
    ICY_ERROR(gui.create(version_client, gui_widget_type::line_edit, gui_layout::none, window));
    ICY_ERROR(gui.create(version_server_text, gui_widget_type::label, gui_layout::none, window));
    ICY_ERROR(gui.create(version_server, gui_widget_type::line_edit, gui_layout::none, window));
    ICY_ERROR(gui.create(progress_total_text, gui_widget_type::label, gui_layout::none, window));
    ICY_ERROR(gui.create(progress_total, gui_widget_type::progress, gui_layout::none, window));
    ICY_ERROR(gui.create(progress_citem_text, gui_widget_type::label, gui_layout::none, window));
    ICY_ERROR(gui.create(progress_citem, gui_widget_type::progress, gui_layout::none, window));
    ICY_ERROR(gui.create(output, gui_widget_type::text_edit, gui_layout::none, window));

    const auto row_version_client = 0u;
    const auto row_version_server = 1u;
    const auto row_progress_total = 2u;
    const auto row_progress_citem = 3u;
    const auto row_output = 4u;
    const auto col_text = 0u;
    const auto col_view = 1u;
    const auto col_span = 2u;

    ICY_ERROR(gui.insert(version_client_text, col_text, row_version_client));
    ICY_ERROR(gui.insert(version_client, col_view, row_version_client));
    ICY_ERROR(gui.insert(version_server_text, col_text, row_version_server));
    ICY_ERROR(gui.insert(version_server, col_view, row_version_server));
    ICY_ERROR(gui.insert(progress_total_text, col_text, row_progress_total));
    ICY_ERROR(gui.insert(progress_total, col_view, row_progress_total));
    ICY_ERROR(gui.insert(progress_citem_text, col_text, row_progress_citem));
    ICY_ERROR(gui.insert(progress_citem, col_view, row_progress_citem));
    ICY_ERROR(gui.insert(output, 0, row_output, col_span, 1u));

    ICY_ERROR(gui.text(version_client_text, to_string(text::version_client)));
    ICY_ERROR(gui.text(version_server_text, to_string(text::version_server)));
    ICY_ERROR(gui.text(progress_total_text, to_string(text::progress_total)));
    ICY_ERROR(gui.text(progress_citem_text, to_string(text::progress_citem)));

    return {};
}
error_type dcl_client_menu_network::connect_gui::init(gui_queue& gui, const gui_widget parent) noexcept
{
    ICY_ERROR(gui.create(action, to_string(text::connect)));
    ICY_ERROR(gui.create(window, gui_widget_type::window, gui_layout::hbox));
    ICY_ERROR(gui.create(output, gui_widget_type::text_edit, gui_layout::none, window));
    ICY_ERROR(gui.insert(output));
    ICY_ERROR(gui.insert(parent, action));
    return {};
}
error_type dcl_client_menu_network::update_gui::init(gui_queue& gui, const gui_widget parent) noexcept
{
    ICY_ERROR(gui.create(action, to_string(text::update)));
    ICY_ERROR(progress.init(gui, parent));
    ICY_ERROR(gui.insert(parent, action));
    return {};
}
error_type dcl_client_menu_network::upload_gui::init(gui_queue& gui, const gui_widget parent) noexcept
{
    gui_widget buttons;

    ICY_ERROR(gui.create(action, to_string(text::upload)));
    ICY_ERROR(gui.create(window, gui_widget_type::window, gui_layout::hbox));
    ICY_ERROR(gui.create(desc, gui_widget_type::text_edit, gui_layout::none, window));
    ICY_ERROR(gui.create(view, gui_widget_type::tree_view, gui_layout::none, window));
    ICY_ERROR(gui.create(buttons, gui_widget_type::none, gui_layout::hbox, window));
    ICY_ERROR(gui.create(accept, gui_widget_type::text_button, gui_layout::none, buttons));
    ICY_ERROR(gui.create(aclear, gui_widget_type::text_button, gui_layout::none, buttons));
    ICY_ERROR(gui.create(cancel, gui_widget_type::text_button, gui_layout::none, buttons));

    ICY_ERROR(gui.insert(desc));
    ICY_ERROR(gui.insert(view));
    ICY_ERROR(gui.insert(buttons));
    ICY_ERROR(gui.insert(accept));
    ICY_ERROR(gui.insert(aclear));
    ICY_ERROR(gui.insert(cancel));

    ICY_ERROR(progress.init(gui, parent));
    ICY_ERROR(gui.insert(parent, action));
    return {};
}
error_type dcl_client_menu_network::init(gui_queue& gui, const gui_widget menu) noexcept
{
    ICY_ERROR(gui.create(m_action, to_string(text::network)));
    ICY_ERROR(gui.create(m_menu, gui_widget_type::menu));
    ICY_ERROR(gui.bind(m_action, m_menu));
    ICY_ERROR(m_connect.init(gui, m_menu));
    ICY_ERROR(m_update.init(gui, m_menu));
    ICY_ERROR(m_upload.init(gui, m_menu));
    ICY_ERROR(gui.insert(menu, m_action));
    return {};
}
error_type dcl_client_menu_network::exec(icy::gui_queue& gui, const event_type type, const gui_event event) noexcept
{
    if (type == event_type::window_close)
    {
        if (event.widget == m_connect.window)
        {
            ;// gui.show(event.widget, false);
        }
        else if (event.widget == m_update.progress.window)
        {

        }
    }
    return {};
}
error_type dcl_client_menu_project::init(gui_queue& gui, gui_widget menu) noexcept
{
    ICY_ERROR(gui.create(m_action, to_string(text::project)));
    ICY_ERROR(gui.create(m_menu, gui_widget_type::menu));
    ICY_ERROR(gui.bind(m_action, m_menu));
    ICY_ERROR(gui.create(m_open_action, to_string(text::open)));
    ICY_ERROR(gui.create(m_open_widget, gui_widget_type::dialog_open_file, gui_layout::none, menu));
    ICY_ERROR(gui.create(m_close, to_string(text::close)));
    ICY_ERROR(gui.create(m_create_action, to_string(text::create)));
    ICY_ERROR(gui.create(m_create_widget, gui_widget_type::dialog_save_file, gui_layout::none, menu));
    ICY_ERROR(gui.create(m_save, to_string(text::save)));
    ICY_ERROR(gui.insert(m_menu, m_open_action));
    ICY_ERROR(gui.insert(m_menu, m_close));
    ICY_ERROR(gui.insert(m_menu, m_create_action));
    ICY_ERROR(gui.insert(m_menu, m_save));
    ICY_ERROR(gui.insert(menu, m_action));
    return {};
}
error_type dcl_client_menu_project::exec(gui_queue& gui, const event_type type, const gui_event event) noexcept
{
    if (type == event_type::gui_action)
    {
        gui_action action;
        action.index = event.data.as_uinteger();
        if (action == m_open_action)
        {
            ICY_ERROR(gui.show(m_open_widget, true));
        }
    }
    return {};
}
error_type dcl_client_menu::init(gui_queue& gui, gui_widget window) noexcept
{
    ICY_ERROR(gui.create(m_menu, gui_widget_type::menubar, gui_layout::none, window));
    ICY_ERROR(m_network.init(gui, m_menu));
    ICY_ERROR(m_project.init(gui, m_menu));
    ICY_ERROR(gui.create(m_options, to_string(text::options)));
    ICY_ERROR(gui.insert(m_menu, m_options));
    return {};
}
error_type dcl_client_menu::exec(gui_queue& gui, const event_type type, const gui_event event) noexcept
{
    ICY_ERROR(m_network.exec(gui, type, event));
    ICY_ERROR(m_project.exec(gui, type, event));
    return {};
}

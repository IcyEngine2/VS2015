#include "icy_dcl_client_server.hpp"
#include "icy_dcl_client_text.hpp"

using namespace icy;

error_type gui_server::init(icy::gui_queue& gui, gui_server_config& config) noexcept
{
    gui.create(m_action, to_string(text::server));
    gui.create(m_action_connect, to_string(text::connect));
    gui.create(m_action_disconnect, to_string(text::disconnect));

    icy::gui_action m_menu;
    icy::gui_widget m_action_connect;
    icy::gui_widget m_window_connect;
    icy::gui_widget m_output_connect;
    icy::gui_widget m_action_disconnect;
    icy::gui_widget m_action_update;
    icy::gui_widget m_action_upload;

    m_gui = &gui;
    m_config = &config;
}
error_type gui_server::exec(icy::gui_event event) noexcept
{

}
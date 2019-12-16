#pragma once

#include <icy_qtgui/icy_qtgui.hpp>
#include <libs/icy_crypto.hpp>
#include <icy_engine/utility/icy_json.hpp>

struct gui_server_config
{
    icy::string hostname;
    uint64_t username;
    icy::crypto_key password;
};
class dcl_client_gui_update
{
public:
    icy::error_type init(icy::gui_queue& gui);
    icy::error_type exec(uint64_t action) noexcept;
private:
    gui_action m_action;
    gui_widget m_window;
    gui_widget m_version_client_text;
    gui_widget m_version_client_view;
    gui_widget m_version_server_text;
    gui_widget m_version_server_view;
    gui_widget m_progress_total_text;
    gui_widget m_progress_total_text;
    gui_widget m_progress_citem_text;
    gui_widget m_progress_citem_text;

    gui_widget m_output;
    gui_widget m_output;
    gui_widget m_output;
};

class gui_server
{
public:
    icy::error_type init(icy::gui_queue& gui, gui_server_config& config) noexcept;
    icy::error_type exec(icy::gui_event event) noexcept;
    icy::gui_action action() const noexcept
    {
        return m_action;
    }
private:
    const icy::gui_queue* m_gui = nullptr;
    const gui_server_config* m_config = nullptr;
    icy::gui_action m_action;
    icy::gui_action m_menu;
    icy::gui_action m_action_connect;
    icy::gui_widget m_window_connect;
    icy::gui_widget m_output_connect;
    icy::gui_action m_action_disconnect;

    icy::gui_action m_action_upload;
};

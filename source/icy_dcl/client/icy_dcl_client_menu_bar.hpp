#pragma once

#include "icy_dcl_client_core.hpp"
#include <icy_qtgui/icy_qtgui.hpp>

class dcl_client_menu_network
{
public:
    icy::error_type init(icy::gui_queue& gui, const icy::gui_widget parent_menu, const icy::gui_widget main_window) noexcept;
    icy::error_type exec(icy::gui_queue& gui, const icy::event event) noexcept;
private:
    struct progress_gui
    {
        icy::error_type init(icy::gui_queue& gui, const icy::gui_widget main_window) noexcept;
        icy::gui_widget window;
        icy::gui_widget version_client;
        icy::gui_widget version_server;
        icy::gui_widget progress;
        icy::gui_widget model;
    };
    struct connect_gui
    {
        icy::error_type init(icy::gui_queue& gui, const icy::gui_widget parent_menu, const icy::gui_widget main_window) noexcept;
        icy::gui_action action;
        icy::gui_widget window;
        icy::gui_widget model;
    };
    struct update_gui
    {
        icy::error_type init(icy::gui_queue& gui, const icy::gui_widget parent_menu, const icy::gui_widget main_window) noexcept;
        icy::gui_action action;
        progress_gui progress;
    };
    struct upload_gui
    {
        icy::error_type init(icy::gui_queue& gui, const icy::gui_widget parent_menu, const icy::gui_widget main_window) noexcept;
        icy::gui_action action;
        icy::gui_widget window;
        icy::gui_widget message;
        icy::gui_widget model;
        icy::gui_widget accept; //  confirm commit changes
        icy::gui_widget aclear; //  all clear (checkboxes)
        icy::gui_widget cancel; //  close window
        progress_gui progress;
    };
private:
    icy::gui_action m_action;
    icy::gui_widget m_menu;
    connect_gui m_connect;
    update_gui m_update;
    upload_gui m_upload;
};
class dcl_client_menu_project
{
public:
    icy::error_type init(icy::gui_queue& gui, const icy::gui_widget parent_menu, const icy::gui_widget main_window) noexcept;
    icy::error_type exec(icy::gui_queue& gui, const icy::event event) noexcept;
private:
    icy::gui_action m_action;
    icy::gui_widget m_menu;
    icy::gui_action m_open_action;
    icy::gui_widget m_open_widget;
    icy::gui_action m_close_action;
    icy::gui_action m_create_action;
    icy::gui_widget m_create_widget;
    icy::gui_action m_save_action;
};
class dcl_client_menu_bar
{
public:
    icy::error_type init(icy::gui_queue& gui, const icy::gui_widget main_window) noexcept;
    icy::error_type exec(icy::gui_queue& gui, const icy::event event) noexcept;
private:
    icy::gui_widget m_menu;
    dcl_client_menu_network m_network;
    dcl_client_menu_project m_project;
    icy::gui_action m_options;
};

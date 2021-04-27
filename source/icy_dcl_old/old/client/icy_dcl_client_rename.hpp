#pragma once

#include "icy_dcl_client_core.hpp"
#include <icy_qtgui/icy_qtgui.hpp>

class dcl_database;

class dcl_client_rename
{
public:
    icy::error_type init(const dcl_database& dbase, const icy::guid& locale, icy::gui_queue& gui, const icy::gui_widget parent) noexcept;
    icy::error_type exec(const icy::event event) noexcept;
    icy::error_type open(const icy::guid& key) noexcept;
private:
    struct button_type
    {
        icy::gui_widget widget;
        icy::gui_action action;
    };
private:
    const dcl_database* m_dbase = nullptr;
    icy::gui_queue* m_gui = nullptr;
    icy::guid m_locale;
    icy::gui_widget m_window;
    icy::gui_widget m_lang;
    icy::gui_widget m_path;
    icy::gui_widget m_name;
    button_type m_okay;
    button_type m_exit;
};
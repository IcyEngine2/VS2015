#pragma once

#include "icy_dcl_client_core.hpp"
#include <icy_qtgui/icy_qtgui.hpp>

class dcl_client_tools
{
public:
    icy::error_type init(icy::gui_queue& gui, const icy::gui_widget parent) noexcept;
    icy::error_type exec(icy::gui_queue& gui, const icy::event event) noexcept;
private:
    struct button_type
    {
        icy::gui_widget widget;
        icy::gui_action action;
    };
private:
    icy::gui_widget m_widget;
    button_type m_bck;
    button_type m_fwd;
    button_type m_undo;
    button_type m_redo;
    button_type m_save;
    icy::gui_widget m_filter;
};
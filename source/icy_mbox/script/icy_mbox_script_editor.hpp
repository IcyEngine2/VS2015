#pragma once

#include <icy_qtgui/icy_qtgui.hpp>
#include "../icy_mbox_script.hpp"

class mbox_editor
{
public:
    icy::error_type initialize(icy::gui_queue& gui, mbox::library& library, icy::gui_widget parent)
    {
        using namespace icy;
        m_gui = &gui;
        m_library = &library;
        ICY_ERROR(m_gui->create(m_widget, gui_widget_type::tabs, parent));
        return {};
    }
    icy::gui_widget widget() const noexcept
    {
        return m_widget;
    }
private:
    icy::gui_queue* m_gui = nullptr;
    mbox::library* m_library = nullptr;
    icy::gui_widget m_widget;
};


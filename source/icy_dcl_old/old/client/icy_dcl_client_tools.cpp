#include "icy_dcl_client_tools.hpp"

using namespace icy;

icy::error_type dcl_client_tools::init(icy::gui_queue& gui, const icy::gui_widget parent) noexcept
{
    ICY_ERROR(gui.create(m_widget, gui_widget_type::none, parent, gui_widget_flag::layout_hbox | gui_widget_flag::auto_insert));
    ICY_ERROR(gui.create(m_bck.widget, gui_widget_type::tool_button, m_widget, gui_widget_flag::auto_insert));
    ICY_ERROR(gui.create(m_fwd.widget, gui_widget_type::tool_button, m_widget, gui_widget_flag::auto_insert));
    ICY_ERROR(gui.create(m_undo.widget, gui_widget_type::tool_button, m_widget, gui_widget_flag::auto_insert));
    ICY_ERROR(gui.create(m_redo.widget, gui_widget_type::tool_button, m_widget, gui_widget_flag::auto_insert));
    ICY_ERROR(gui.create(m_save.widget, gui_widget_type::tool_button, m_widget, gui_widget_flag::auto_insert));
    ICY_ERROR(gui.create(m_filter, gui_widget_type::line_edit, m_widget, gui_widget_flag::auto_insert));
    return {};
}
icy::error_type dcl_client_tools::exec(icy::gui_queue& gui, const icy::event event) noexcept
{
    return {};
}
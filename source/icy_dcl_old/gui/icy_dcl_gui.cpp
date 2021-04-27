#include "icy_dcl_gui.hpp"

using namespace icy;

ICY_DECLARE_GLOBAL(dcl_widget::g_create);

error_type dcl_widget::create(dcl_application& app, const dcl_base& base, unique_ptr<dcl_widget>& widget) noexcept
{
    const auto func = g_create[uint32_t(base.type)];
    if (!func)
        return icy::make_stdlib_error(std::errc::function_not_supported);
    func(app, widget);
    if (!widget)
        return icy::make_stdlib_error(std::errc::not_enough_memory);
    ICY_ERROR(widget->initialize(base));
    return {};
}
icy::error_type dcl_widget::initialize(const icy::dcl_base& base) noexcept
{
    ICY_ERROR(m_window.initialize(gui_widget_type::window, {}, gui_widget_flag::layout_grid));
    ICY_ERROR(m_label_index.initialize(gui_widget_type::label, m_window));
    ICY_ERROR(m_label_name.initialize(gui_widget_type::label, m_window));
    ICY_ERROR(m_label_parent.initialize(gui_widget_type::label, m_window));
    ICY_ERROR(m_value_index.initialize(gui_widget_type::line_edit, m_window));
    ICY_ERROR(m_value_name.initialize(gui_widget_type::line_edit, m_window));
    ICY_ERROR(m_value_parent.initialize(gui_widget_type::line_edit, m_window));
    ICY_ERROR(m_buttons.initialize(gui_widget_type::none, m_window, gui_widget_flag::layout_hbox));
    ICY_ERROR(m_button_save.initialize(gui_widget_type::text_button, m_buttons));
    ICY_ERROR(m_button_exit.initialize(gui_widget_type::text_button, m_buttons));
    ICY_ERROR(m_label_index.text(dcl_gui_text::Index));
    ICY_ERROR(m_label_name.text(dcl_gui_text::Name));
    ICY_ERROR(m_label_parent.text(dcl_gui_text::Parent));


    auto row = 0_z;
    enum{ col_label, col_value, _col_count };
    
    ICY_ERROR(m_label_index.insert(gui_insert(col_label, row)));
    ICY_ERROR(m_value_index.insert(gui_insert(col_value, row)));
    ++row;

    ICY_ERROR(m_label_name.insert(gui_insert(col_label, row)));
    ICY_ERROR(m_value_name.insert(gui_insert(col_value, row)));
    ++row;

    ICY_ERROR(m_label_parent.insert(gui_insert(col_label, row)));
    ICY_ERROR(m_value_parent.insert(gui_insert(col_value, row)));
    ++row;

    ICY_ERROR(initialize(row));

    ICY_ERROR(m_buttons.insert(gui_insert(0, row, _col_count, 1)));
    ICY_ERROR(m_button_save.text(dcl_gui_text::Save));
    ICY_ERROR(m_button_exit.text(dcl_gui_text::Cancel));
    ICY_ERROR(m_window.show(true));
    return {};
}
#include "icy_dcl_gui.hpp"

using namespace icy;

class dcl_gui_class : public dcl_widget
{
public:
    dcl_gui_class(dcl_application& app) noexcept : dcl_widget(app, dcl_type::class_type)
    {

    }
private:
    enum
    {
        col_type,
        col_name,
        col_value,
        _col_count,
    };
    error_type initialize(size_t& row) noexcept override;
    error_type exec(const event event) noexcept override;
private:
    xgui_model m_model;
    xgui_widget m_label;
    xgui_widget m_grid;
};

error_type dcl_gui_class::initialize(size_t& row) noexcept
{
    ICY_ERROR(m_model.initialize());
    ICY_ERROR(m_model.insert_cols(0, _col_count));
    
    ICY_ERROR(m_label.initialize(gui_widget_type::label, m_window));
    ICY_ERROR(m_grid.initialize(gui_widget_type::grid_view, m_window));
    ICY_ERROR(m_grid.bind(m_model));

    ICY_ERROR(m_label.insert(gui_insert(0, row++, UINT32_MAX, 1)));
    ICY_ERROR(m_grid.insert(gui_insert(0, row++, UINT32_MAX, 1)));
    ICY_ERROR(m_label.text(dcl_gui_text::Properties));

    return {};
}
error_type dcl_gui_class::exec(const icy::event event) noexcept
{
    return {};
}

static const auto create_widget = dcl_widget::factory(dcl_type::class_type, [](dcl_application& app,
    icy::unique_ptr<dcl_widget>& widget) { widget = make_unique<dcl_gui_class>(app); });
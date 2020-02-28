#include "icy_dcl_client_rename.hpp"
#include "icy_dcl_client_text.hpp"
#include "../icy_dcl_dbase.hpp"

using namespace icy;

error_type dcl_client_rename::init(const dcl_database& dbase, const icy::guid& locale, gui_queue& gui, const gui_widget parent) noexcept
{
    m_dbase = &dbase;
    m_locale = locale;
    m_gui = &gui;

    gui_widget lang;
    gui_widget path;
    gui_widget name;
    gui_widget buttons;
    ICY_ERROR(m_gui->create(m_window, gui_widget_type::window, parent, gui_widget_flag::layout_grid));
    ICY_ERROR(m_gui->create(lang, gui_widget_type::label, m_window));
    ICY_ERROR(m_gui->create(m_lang, gui_widget_type::combo_box, m_window));
    ICY_ERROR(m_gui->create(path, gui_widget_type::label, m_window));
    ICY_ERROR(m_gui->create(m_path, gui_widget_type::line_edit, m_window));
    ICY_ERROR(m_gui->create(name, gui_widget_type::label, m_window));
    ICY_ERROR(m_gui->create(m_name, gui_widget_type::line_edit, m_window));
    ICY_ERROR(m_gui->create(buttons, gui_widget_type::none, m_window, gui_widget_flag::layout_hbox));
    ICY_ERROR(m_gui->create(m_okay.widget, gui_widget_type::text_button, buttons, gui_widget_flag::auto_insert));
    ICY_ERROR(m_gui->create(m_exit.widget, gui_widget_type::text_button, buttons, gui_widget_flag::auto_insert));

    ICY_ERROR(m_gui->text(lang, to_string(dcl_text::locale)));
    ICY_ERROR(m_gui->text(path, to_string(dcl_text::path)));
    ICY_ERROR(m_gui->text(name, to_string(dcl_text::name)));

    return {};
}
error_type dcl_client_rename::exec(const event event) noexcept
{

}
error_type dcl_client_rename::open(const guid& key) noexcept
{
    ICY_ERROR(m_gui->show(m_window, true));
    
    dcl_read_txn read;
    ICY_ERROR(read.initialize(*m_dbase));
    map<dcl_index, string_view> locales;
    ICY_ERROR(read.locales(m_locale, locales));
    this->m_lang;
    

    m_locale;
}
#pragma once

#include <icy_engine/core/icy_string_view.hpp>
#include <icy_qtgui/icy_xqtgui.hpp>
#include "../core/icy_dcl.hpp"

class dcl_application;
namespace icy { class dcl_writer; }

class dcl_widget
{
public:
    using create_func = void(*)(dcl_application& app, icy::unique_ptr<dcl_widget>& widget);
    static int factory(const icy::dcl_type type, const create_func func) noexcept
    {
        g_create[uint32_t(type)] = func;
        return 0;
    }
    static icy::error_type create(dcl_application& app, const icy::dcl_base& base, icy::unique_ptr<dcl_widget>& widget) noexcept;
    icy::dcl_type type() const noexcept
    {
        return m_type;
    }
    icy::gui_widget window() const noexcept
    {
        return m_window;
    }
    icy::dcl_base base() const noexcept
    {
        return m_base;
    }
    virtual icy::error_type exec(const icy::event event) noexcept = 0;
protected:
    dcl_widget(dcl_application& app, const icy::dcl_type type) noexcept : m_app(app), m_type(type)
    {

    }
    //  public
    virtual icy::error_type initialize(const icy::dcl_base& base) noexcept;
    //  internal
    virtual icy::error_type initialize(size_t& row) noexcept { return {}; }
protected:
    dcl_application& m_app;
    const icy::dcl_type m_type;
    icy::xgui_widget m_window;
    icy::dcl_base m_base;
private:
    static create_func g_create[uint32_t(icy::dcl_type::_count)];
private:
    icy::xgui_widget m_label_index;
    icy::xgui_widget m_value_index;
    icy::xgui_widget m_label_name;
    icy::xgui_widget m_value_name;
    icy::xgui_widget m_label_parent;
    icy::xgui_widget m_value_parent;
    icy::xgui_widget m_buttons;
    icy::xgui_widget m_button_save;
    icy::xgui_widget m_button_exit;
};

class dcl_gui_text;
class dcl_application
{
public:
    virtual icy::gui_node root() const noexcept = 0;
    virtual icy::error_type show(const icy::dcl_base& base) noexcept = 0;
    virtual icy::error_type menu(const dcl_gui_text& submenu, const dcl_gui_text& action) noexcept = 0;
    virtual icy::error_type context(const icy::dcl_index index) noexcept = 0;
    virtual icy::dcl_writer* writer() noexcept = 0;
};

struct dcl_gui_locale
{
    enum type
    {
        enUS = 'enUS',
        ruRU = 'ruRU',
        _count = 2,
    };
    static type global;
};
class dcl_gui_text
{
public:
    static const dcl_gui_text File;
    static const dcl_gui_text New;
    static const dcl_gui_text Open;
    static const dcl_gui_text Close;
    static const dcl_gui_text Save;
    static const dcl_gui_text SaveAs;
    static const dcl_gui_text Context;
    static const dcl_gui_text Type;
    static const dcl_gui_text Other;
    static const dcl_gui_text Edit;
    static const dcl_gui_text Rename;
    static const dcl_gui_text Delete;
    static const dcl_gui_text Hide;
    static const dcl_gui_text Show;
    static const dcl_gui_text Lock;
    static const dcl_gui_text Unlock;
    static const dcl_gui_text ShowLog;
    static const dcl_gui_text Index;
    static const dcl_gui_text Name;
    static const dcl_gui_text Parent;
    static const dcl_gui_text Cancel;
    static const dcl_gui_text Properties;
    static const dcl_gui_text Undo;
    static const dcl_gui_text Redo;
    static const dcl_gui_text Actions;
    static const dcl_gui_text Database;
    static const dcl_gui_text Patch;
    static const dcl_gui_text Apply;
    operator icy::string_view() const noexcept
    {
        icy::string_view default_str;
        for (auto&& str : m_str)
        {
            if (str.first == dcl_gui_locale::global)
                return str.second;
            if (str.first == dcl_gui_locale::enUS)
                default_str = str.second;
        }
        return default_str;
    }
private:
    using pair_type = std::pair<dcl_gui_locale::type, icy::string_view>;
    dcl_gui_text(const std::initializer_list<pair_type> str) noexcept
    {
        auto index = 0u;
        for (auto&& pair : str)
        {
            if (index >= _countof(m_str))
            {
                ICY_ASSERT(false, "TOO MANY LOCALES");
                break;
            }
            m_str[index++] = pair;
        }
    }
private:
    pair_type m_str[dcl_gui_locale::_count];
};

icy::error_type show_error(const icy::error_type error, const icy::string_view text = {}) noexcept;

namespace icy
{
    string_view to_string(const dcl_type type) noexcept;
}
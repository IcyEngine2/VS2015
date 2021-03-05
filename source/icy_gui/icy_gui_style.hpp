#pragma once

#include <icy_gui/icy_gui.hpp>
#include <icy_engine/core/icy_map.hpp>

struct gui_widget_data;
struct lwc_context_s;
struct css_stylesheet;
struct css_select_ctx;
struct css_select_results;
struct css_computed_style;
class gui_select_system;

class gui_style_data
{
    friend gui_select_system;
public:
    gui_style_data() noexcept = default;
    gui_style_data(gui_style_data&& rhs) noexcept;
    ICY_DEFAULT_MOVE_ASSIGN(gui_style_data);
    ~gui_style_data() noexcept;
    icy::error_type initialize(lwc_context_s* ctx, const icy::string_view name, const icy::string_view css) noexcept;
    icy::string_view name() const noexcept
    {
        return m_name;
    }
private:
    icy::string m_name;
    css_stylesheet* m_value = nullptr;
};
class gui_window_data_sys;
class gui_select_system;
class gui_select_output
{
    friend gui_select_system;
public:
    gui_select_output() noexcept = default;
    gui_select_output(gui_select_output&& rhs) noexcept
    {
        rhs.m_value = nullptr;
    }
    ICY_DEFAULT_MOVE_ASSIGN(gui_select_output);
    ~gui_select_output() noexcept;
    //icy::error_type preproc(const gui_window_data_sys& window, gui_widget_data& widget) const noexcept;
    //icy::error_type postproc(const gui_window_data_sys& window, gui_widget_data& widget) const noexcept;
    css_computed_style* style() const noexcept;
private:
    css_select_results* m_value = nullptr;
};
class gui_select_system
{
public:
    gui_select_system() noexcept = default;
    gui_select_system(const gui_select_system&) = delete;
    ~gui_select_system() noexcept;
    icy::error_type initialize(lwc_context_s* ctx) noexcept;
    icy::error_type append(gui_style_data& style) noexcept;
    void remove(gui_style_data& style) noexcept;
    icy::error_type calc(gui_widget_data& widget) const noexcept;
    lwc_context_s* ctx() const noexcept
    {
        return m_ctx;
    }
private:
    lwc_context_s* m_ctx = nullptr;
    css_select_ctx* m_value = nullptr;
};
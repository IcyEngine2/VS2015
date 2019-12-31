#pragma once

#include "icy_qtgui_core.hpp"
#include <icy_engine/core/icy_event.hpp>

namespace icy
{
    class gui_model_base;
    struct gui_model_view;
    struct gui_event;
    
    enum class gui_widget_flag : uint32_t
    {
        none,

        layout_hbox =   0x01,
        layout_vbox =   0x02,
        layout_grid =   0x03,
        auto_insert =   0x04,
    };
    enum class gui_widget_type : uint32_t
    {
        none,
        
        window,
        vsplitter,
        hsplitter,
        tabs,
        frame,

        //  -- text --

        label,
        line_edit,
        text_edit,

        //  -- menu --

        menu,
        menubar,

        //  -- model view --

        combo_box,
        list_view,
        tree_view,
        grid_view,

        //  -- utility --

        message,
        progress,

        //  --  buttons --

        text_button,
        tool_button,

        //  --  dialogs --

        dialog_open_file,
        dialog_save_file,
        dialog_select_directory,
        dialog_select_color,
        dialog_select_font,
        dialog_input_line,
        dialog_input_text,
        dialog_input_integer,
        dialog_input_double,
    };

    struct gui_widget_args_keys
    {
        static constexpr string_view layout = "Layout"_s;
        static constexpr string_view stretch = "Stretch"_s;
        static constexpr string_view row = "Row"_s;
        static constexpr string_view col = "Col"_s;
    };

    class gui_system
	{
	public:
		gui_system() noexcept = default;
		gui_system(const gui_system&) noexcept = delete;
        virtual void release() noexcept = 0;
        virtual uint32_t wake() noexcept = 0;
        virtual uint32_t loop(event_type& type, gui_event& args) noexcept = 0;
        virtual uint32_t create(gui_widget& widget, const gui_widget_type type, const gui_widget parent, const gui_widget_flag flags = gui_widget_flag::none) noexcept = 0;
        virtual uint32_t create(gui_action& action, const string_view text) noexcept = 0;
        virtual uint32_t initialize(gui_model_view& view, gui_model_base& base) noexcept = 0;
        virtual uint32_t insert(const gui_widget widget, const gui_insert args) noexcept = 0;
        virtual uint32_t insert(const gui_widget widget, const gui_action action) noexcept = 0;
        virtual uint32_t show(const gui_widget widget, const bool value) noexcept = 0;
        virtual uint32_t text(const gui_widget widget, const string_view text) noexcept = 0;
        virtual uint32_t bind(const gui_action action, const gui_widget menu) noexcept = 0;
        virtual uint32_t enable(const gui_action action, const bool value) noexcept = 0;
        virtual uint32_t modify(const gui_widget widget, const string_view args) noexcept = 0;
        virtual uint32_t destroy(const gui_widget widget) noexcept = 0;
        virtual uint32_t destroy(const gui_action action) noexcept = 0;
    protected:
        ~gui_system() noexcept = default;
    };
    inline gui_widget_flag operator|(const gui_widget_flag lhs, const gui_widget_flag rhs) noexcept
    {
        return gui_widget_flag(uint32_t(lhs) | uint32_t(rhs));
    }
    inline bool operator&(const gui_widget_flag lhs, const gui_widget_flag rhs) noexcept
    {
        return !!(uint32_t(lhs) & uint32_t(rhs));
    }
}

extern "C" uint32_t ICY_QTGUI_API icy_gui_system_create(const int version, icy::gui_system** system) noexcept;
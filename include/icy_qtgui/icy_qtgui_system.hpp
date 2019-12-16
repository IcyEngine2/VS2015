#pragma once

#include "icy_qtgui_core.hpp"
#include <icy_engine/core/icy_event.hpp>

namespace icy
{
    class gui_model_base;
    struct gui_model_view;
    struct gui_event;
    
    enum class gui_layout : uint32_t
    {
        none,
        hbox = 0x01,
        vbox = 0x02,
        grid = 0x03,
    };
    enum class gui_widget_type : uint32_t
    {
        none,
        window,
        message,
        menu,
        menubar,
        line_edit,
        vsplitter,
        hsplitter,
        combo_box,
        list_view,
        tree_view,
        grid_view,
        label,
    };

    class gui_system
	{
	public:
		gui_system() noexcept = default;
		gui_system(const gui_system&) noexcept = delete;
        virtual void release() noexcept = 0;
        virtual uint32_t wake() noexcept = 0;
        virtual uint32_t loop(event_type& type, gui_event& args) noexcept = 0;
        virtual uint32_t create(gui_widget& widget, const gui_widget_type type, const gui_layout layout, const gui_widget parent) noexcept = 0;
        virtual uint32_t create(gui_action& action, const string_view text) noexcept = 0;
        virtual uint32_t initialize(gui_model_view& view, gui_model_base& base) noexcept = 0;
        virtual uint32_t insert(const gui_widget widget, const uint32_t x, const uint32_t y, const uint32_t dx, const uint32_t dy) noexcept = 0;
        virtual uint32_t insert(const gui_widget widget, const gui_action action) noexcept = 0;
        virtual uint32_t show(const gui_widget widget, const bool value) noexcept = 0;
        virtual uint32_t text(const gui_widget widget, const string_view text) noexcept = 0;
        virtual uint32_t bind(const gui_action action, const gui_widget menu) noexcept = 0;
    protected:
        ~gui_system() noexcept = default;
    }; 
}

extern "C" uint32_t ICY_QTGUI_API icy_gui_system_create(const int version, const icy::global_heap_type heap, icy::gui_system** system) noexcept;
#pragma once

#include <icy_engine/core/icy_string_view.hpp>
#include <icy_engine/core/icy_map.hpp>
#include <icy_gui/icy_gui.hpp>

struct gui_widget_attr_enum
{
    enum : uint32_t
    {
        none,
        width,
        width_min,
        width_max,
        height,
        height_min,
        height_max,
        color,
        bkcolor,
        class_,
        weight,
        weight_x,
        weight_y,
        flex_scroll,
    };
};
using gui_widget_attr = decltype(gui_widget_attr_enum::none);

namespace icy
{
    template<> inline int compare(const gui_widget_attr& lhs, const gui_widget_attr& rhs) noexcept
    {
        return icy::compare(uint32_t(lhs), uint32_t(rhs));
    }
}

gui_widget_attr gui_str_to_attr(const icy::string_view str) noexcept;
icy::gui_widget_type gui_str_to_type(const icy::string_view str) noexcept;
icy::gui_widget_layout gui_str_to_layout(const icy::string_view str) noexcept;
icy::gui_variant gui_attr_default(const gui_widget_attr attr) noexcept;
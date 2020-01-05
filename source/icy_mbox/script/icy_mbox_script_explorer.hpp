#pragma once

#include <icy_qtgui/icy_qtgui.hpp>
#include "../icy_mbox_script.hpp"

class mbox_explorer
{
public:
    icy::error_type initialize(icy::gui_queue& gui, mbox::library& library, icy::gui_widget parent) noexcept;
    icy::gui_widget widget() const noexcept
    {
        return m_widget;
    }
    icy::error_type reset() noexcept;
    icy::error_type exec(const icy::event event) noexcept;
private:
    icy::error_type find(const mbox::base& base, icy::gui_node& node) noexcept;
    icy::error_type append(const icy::gui_node parent, const size_t offset, const mbox::base& base) noexcept;
private:
    icy::gui_queue* m_gui = nullptr;
    mbox::library* m_library = nullptr;
    icy::gui_image m_images[uint32_t(mbox::type::_total)];
    icy::gui_widget m_widget;
    icy::gui_node m_root;
};
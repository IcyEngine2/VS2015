#pragma once

#include <icy_qtgui/icy_qtgui.hpp>
#include "../icy_mbox_script.hpp"
#include "icy_mbox_script_form.hpp"

class mbox_context
{
public:
    icy::error_type initialize(icy::gui_queue& gui, mbox::library& library) noexcept
    {
        m_gui = &gui;
        m_library = &library;
        return {};
    }
    icy::error_type exec(const icy::event event) noexcept;
private:
    icy::error_type menu(const icy::guid& index) noexcept;
private:
    icy::gui_queue* m_gui = nullptr;
    mbox::library* m_library = nullptr;
    icy::unique_ptr<mbox_form> m_form;
};
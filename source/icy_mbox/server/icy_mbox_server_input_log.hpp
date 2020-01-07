#pragma once

#include "../icy_mbox_network.hpp"
#include <icy_qtgui/icy_qtgui.hpp>
#include <icy_engine/core/icy_input.hpp>

class mbox_input_log
{
    enum row
    {
        col_profile,
        col_time,
        col_type,
        col_arg,
        col_mods,
        _col_count,
    };
public:
    icy::error_type initialize(icy::gui_queue& gui, mbox::library& library) noexcept
    {
        using namespace icy;
        m_gui = &gui;
        m_library = &library;

        ICY_ERROR(m_gui->create(m_window, gui_widget_type::window, gui_widget(), gui_widget_flag::layout_vbox));
        ICY_ERROR(m_gui->create(m_widget, gui_widget_type::grid_view, m_window, gui_widget_flag::auto_insert));
        ICY_ERROR(m_gui->create(m_root));
        ICY_ERROR(m_gui->insert_cols(m_root, 0, _col_count));

        ICY_ERROR(m_gui->hheader(m_root, col_profile, "Profile"_s));
        ICY_ERROR(m_gui->hheader(m_root, col_time, "Time"_s));
        ICY_ERROR(m_gui->hheader(m_root, col_type, "Type"_s));
        ICY_ERROR(m_gui->hheader(m_root, col_arg, "Arg"_s));
        ICY_ERROR(m_gui->hheader(m_root, col_mods, "Mods"_s));

        ICY_ERROR(m_gui->bind(m_widget, m_root));
        ICY_ERROR(m_gui->show(m_window, true));

        return {};
    }
    icy::error_type append(const icy::guid& profile, const icy::input_message& msg) noexcept
    {
        using namespace icy;
        string_view type;
        string_view arg;

        auto ctrl = key_mod(0u);
        auto alt = key_mod(0u);
        auto shift = key_mod(0u);

        if (msg.type == input_type::key)
        {
            if (msg.key.event == key_event::press)
                type = "Key press"_s;
            else if (msg.key.event == key_event::hold)
                type = "Key hold"_s;
            else if (msg.key.event == key_event::release)
                type = "Key release"_s;

            arg = to_string(msg.key.key);
            ctrl = msg.key.ctrl;
            alt = msg.key.alt;
            shift = msg.key.shift;
        }
        else if (msg.type == input_type::mouse)
        {
            if (msg.mouse.event == mouse_event::btn_press)
                type = "Btn press"_s;
            if (msg.mouse.event == mouse_event::btn_double)
                type = "Btn double"_s;
            if (msg.mouse.event == mouse_event::btn_release)
                type = "Btn release"_s;

            if (msg.mouse.button == mouse_button::left)
                arg = "Left"_s;
            else if (msg.mouse.button == mouse_button::right)
                arg = "Right"_s;
            else if (msg.mouse.button == mouse_button::mid)
                arg = "Middle"_s;
            else if (msg.mouse.button == mouse_button::x1)
                arg = "Extra[1]"_s;
            else if (msg.mouse.button == mouse_button::x2)
                arg = "Extra[2]"_s;

            ctrl = msg.mouse.ctrl;
            alt = msg.mouse.alt;
            shift = msg.mouse.shift;
        }
        else if (msg.type == input_type::active)
        {
            if (msg.active)
                type = "Active"_s;
            else
                type = "Inactive"_s;
        }

        if (type.empty())
            return {};

        string mods;

        const auto append_mod = [&mods](const string_view text)
        {
            if (!mods.empty())
                ICY_ERROR(mods.append(" + "));
            ICY_ERROR(mods.append(text));
            return error_type();
        };
        if (ctrl.right())
        {
            ICY_ERROR(append_mod("RCtrl"_s));
        }
        else if (ctrl.left())
        {
            ICY_ERROR(append_mod("Ctrl"_s));
        }

        if (alt.right())
        {
            ICY_ERROR(append_mod("RAlt"_s));
        }
        else if (alt.left())
        {
            ICY_ERROR(append_mod("Alt"_s));
        }

        if (shift.right())
        {
            ICY_ERROR(append_mod("RShift"_s));
        }
        else if (shift.left())
        {
            ICY_ERROR(append_mod("Shift"_s));
        }

        string time;
        ICY_ERROR(to_string(clock_type::now(), time));
        ICY_ERROR(m_gui->insert_rows(m_root, m_size, 1));

        string str_profile;
        if (profile != guid())
            m_library->path(profile, str_profile);

        ICY_ERROR(m_gui->text(m_gui->node(m_root, m_size, col_profile), str_profile));
        ICY_ERROR(m_gui->text(m_gui->node(m_root, m_size, col_time), time));
        ICY_ERROR(m_gui->text(m_gui->node(m_root, m_size, col_type), type));
        ICY_ERROR(m_gui->text(m_gui->node(m_root, m_size, col_arg), arg));
        ICY_ERROR(m_gui->text(m_gui->node(m_root, m_size, col_mods), mods));
        ICY_ERROR(m_gui->scroll(m_widget, m_gui->node(m_root, m_size, 0)));
        m_size += 1;
        return {};
    }
    icy::gui_widget window() const noexcept
    {
        return m_window;
    }
private:
    icy::gui_queue* m_gui = nullptr;
    mbox::library* m_library = nullptr;
    icy::gui_widget_scoped m_window;
    icy::gui_widget_scoped m_widget;
    icy::gui_model_scoped m_root;
    uint32_t m_size = 0;
};
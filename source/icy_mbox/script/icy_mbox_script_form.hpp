#pragma once

#include <icy_qtgui/icy_qtgui.hpp>
#include "../icy_mbox_script.hpp"
#include "icy_mbox_script_event.hpp"
#include <icy_engine/image/icy_image.hpp>
#include "icons/command.h"
#include "icons/binding.h"
#include "icons/directory.h"
#include "icons/event.h"
#include "icons/group.h"
#include "icons/input.h"
#include "icons/profile.h"
#include "icons/timer.h"
#include "icons/variable.h"

class mbox_form_key_value_pair
{
public:
    mbox_form_key_value_pair(icy::gui_queue& gui) noexcept : key(gui), value(gui)
    {

    }
    icy::error_type initialize(const icy::string_view text, const icy::gui_widget parent, const icy::gui_widget_type type) noexcept
    {
        ICY_ERROR(key.initialize(parent, icy::gui_widget_type::label));
        ICY_ERROR(key.gui->text(key, text));
        ICY_ERROR(value.initialize(parent, type));
        return {};
    }
    icy::error_type insert(uint32_t& row) noexcept
    {
        ICY_ERROR(key.gui->insert(key, icy::gui_insert(0, row)));
        ICY_ERROR(value.gui->insert(value, icy::gui_insert(1, row)));
        ++row;
        return {};
    }
public:
    icy::gui_widget_scoped key;
    icy::gui_widget_scoped value;
};
class mbox_form_key_value_combo : public mbox_form_key_value_pair
{
public:
    mbox_form_key_value_combo(icy::gui_queue& gui) noexcept : mbox_form_key_value_pair(gui), model(gui)
    {

    }
    icy::error_type initialize(const icy::string_view text, const icy::gui_widget parent) noexcept
    {
        ICY_ERROR(mbox_form_key_value_pair::initialize(text, parent, icy::gui_widget_type::combo_box));
        ICY_ERROR(model.initialize());
        ICY_ERROR(model.gui->insert_cols(model, 0, 1));
        ICY_ERROR(model.gui->bind(value, model));
        return {};
    }
    icy::error_type exec(const icy::event event) noexcept
    {
        if (event->type == icy::event_type::gui_select)
        {
            const auto& event_data = event->data<icy::gui_event>();
            if (event_data.widget == value)
                select = uint32_t(event_data.data.as_node().udata().as_uinteger());
        }
        return {};
    }
    icy::error_type append(const uint64_t value, const icy::string_view text) noexcept
    {
        ICY_ERROR(model.gui->insert_rows(model, m_offset, 1));
        const auto node = model.gui->node(model, m_offset, 0);
        ICY_ERROR(model.gui->text(node, text));
        ICY_ERROR(model.gui->udata(node, value));
        ++m_offset;
        return {};
    }
    template<typename T>
    icy::error_type append(const T value) noexcept
    {
        static_assert(std::is_enum<T>::value, "INVALID TYPE");
        const auto str = to_string(value);
        return append(uint64_t(value), str.empty() ? "None"_s : str);
    }
    icy::error_type clear() noexcept
    {
        ICY_ERROR(model.gui->clear(model));
        m_offset = 0;
        return {};
    }
    uint32_t select = 0;
public:
    icy::gui_model_scoped model;
private:
    size_t m_offset = 0;
};
class mbox_form_key_value_text : public mbox_form_key_value_pair
{
public:
    using mbox_form_key_value_pair::mbox_form_key_value_pair;
    icy::error_type initialize(const icy::string_view text, const icy::gui_widget parent) noexcept
    {
        return mbox_form_key_value_pair::initialize(text, parent, icy::gui_widget_type::line_edit);
    }
    icy::error_type exec(const icy::event event) noexcept
    {
        if (event->type == icy::event_type::gui_update)
        {
            const auto& event_data = event->data<icy::gui_event>();
            if (event_data.widget == value)
                ICY_ERROR(to_string(event_data.data.as_string(), string));
        }
        return {};
    }
    icy::string string;
};

class mbox_form_value
{
public:
    mbox_form_value(icy::gui_queue& gui, mbox::library& library) : m_library(library), m_widget(gui)
    {

    }
    virtual icy::error_type initialize(const icy::gui_widget parent) noexcept
    {
        return {};
    }
    virtual icy::error_type create(mbox::base& base) noexcept
    {
        return {};
    }
    virtual icy::error_type exec(const icy::event event) noexcept
    {
        return {};
    }
    icy::gui_widget widget() const noexcept
    {
        return m_widget;
    }
protected:
    mbox::library& m_library;
    icy::gui_widget_scoped m_widget;
};
class mbox_form_value_input : public mbox_form_value
{
public:
    mbox_form_value_input(icy::gui_queue& gui, mbox::library& library) : mbox_form_value(gui, library),
        m_type(gui), m_button(gui), m_event(gui), m_ctrl(gui), m_alt(gui), m_shift(gui), m_macro(gui)
    {

    }
    icy::error_type initialize(const icy::gui_widget parent) noexcept override;
    icy::error_type create(mbox::base& base) noexcept override
    {
        using namespace icy;

        auto& value = base.value.input;
        if (!m_type.select || !m_button.select || !m_event.select)
            return make_stdlib_error(std::errc::invalid_argument);

        value.itype = mbox::input_type(m_type.select);
        value.ctrl = m_ctrl.select;
        value.alt = m_alt.select;
        value.shift = m_shift.select;

        if (value.itype == mbox::input_type::keyboard ||
            value.itype == mbox::input_type::macro)
        {
            value.kevent = key_event(m_event.select);
            value.kbutton = key(m_button.select);
        }
        else if (value.itype == mbox::input_type::mouse)
        {
            value.mevent = mouse_event(m_event.select);
            value.mbutton = mouse_button(m_button.select);
        }
        else
            return make_stdlib_error(std::errc::invalid_argument);
        
        if (value.itype == mbox::input_type::macro)
            ICY_ERROR(to_string(m_macro.string, value.macro));

        return {};
    }
    icy::error_type exec(const icy::event event) noexcept override
    {
        ICY_ERROR(m_type.exec(event));
        ICY_ERROR(m_button.exec(event));
        ICY_ERROR(m_event.exec(event));
        ICY_ERROR(m_ctrl.exec(event));
        ICY_ERROR(m_alt.exec(event));
        ICY_ERROR(m_shift.exec(event));
        ICY_ERROR(m_macro.exec(event));
        return {};
    }
private:
    mbox_form_key_value_combo m_type;
    mbox_form_key_value_combo m_button;
    mbox_form_key_value_combo m_event;
    mbox_form_key_value_combo m_ctrl;
    mbox_form_key_value_combo m_alt;
    mbox_form_key_value_combo m_shift;
    mbox_form_key_value_text m_macro;
};

icy::error_type mbox_form_value_input::initialize(const icy::gui_widget parent) noexcept
{
    using namespace icy;
    ICY_ERROR(mbox_form_value::initialize(parent));
    ICY_ERROR(m_widget.initialize(parent, gui_widget_type::none, gui_widget_flag::layout_grid));
    ICY_ERROR(m_type.initialize("Input Type"_s, m_widget));
    ICY_ERROR(m_button.initialize("Button"_s, m_widget));
    ICY_ERROR(m_event.initialize("Event"_s, m_widget));
    ICY_ERROR(m_ctrl.initialize("Ctrl"_s, m_widget));
    ICY_ERROR(m_alt.initialize("Alt"_s, m_widget));
    ICY_ERROR(m_shift.initialize("Shift"_s, m_widget));
    ICY_ERROR(m_macro.initialize("Macro"_s, m_widget));

    auto row = 0u;
    ICY_ERROR(m_type.insert(row));
    ICY_ERROR(m_button.insert(row));
    ICY_ERROR(m_event.insert(row));
    ICY_ERROR(m_ctrl.insert(row));
    ICY_ERROR(m_alt.insert(row));
    ICY_ERROR(m_shift.insert(row));
    ICY_ERROR(m_macro.insert(row));
    
    ICY_ERROR(m_type.append(mbox::input_type::none));
    ICY_ERROR(m_type.append(mbox::input_type::keyboard));
    ICY_ERROR(m_type.append(mbox::input_type::mouse));
    ICY_ERROR(m_type.append(mbox::input_type::macro));

    const auto func = [](mbox_form_key_value_combo& mod)
    {
        const string_view texts[] = { "None"_s, "Left"_s, "Right"_s, "Any"_s };
        for (auto k = 0u; k < _countof(texts); ++k)
            ICY_ERROR(mod.append(k, texts[k]));
        return error_type();
    };
    ICY_ERROR(func(m_ctrl));
    ICY_ERROR(func(m_alt));
    ICY_ERROR(func(m_shift));


    return {};
}

class mbox_form
{
public:
    mbox_form(icy::gui_queue& gui, mbox::library& library) noexcept : m_library(library),
        m_window(gui), m_name(gui), m_parent(gui), m_type(gui), m_index(gui), m_tabs(gui),
        m_links(gui), m_buttons(gui), m_save(gui), m_exit(gui)
    {

    }
    icy::error_type initialize(const mbox::base& base) noexcept
    {
        using namespace icy;
        auto& gui = *m_window.gui;
        ICY_ERROR(m_window.initialize(gui_widget(), gui_widget_type::dialog, gui_widget_flag::layout_grid | gui_widget_flag::auto_insert));
        ICY_ERROR(m_name.initialize("Name"_s, m_window));
        ICY_ERROR(m_parent.initialize("Parent"_s, m_window, gui_widget_type::line_edit));
        ICY_ERROR(m_type.initialize("Type"_s, m_window, gui_widget_type::line_edit));
        ICY_ERROR(m_index.initialize("Index"_s, m_window, gui_widget_type::line_edit));
        ICY_ERROR(m_buttons.initialize(m_window, gui_widget_type::none, gui_widget_flag::layout_hbox));
        ICY_ERROR(m_save.initialize(m_buttons, gui_widget_type::text_button));
        ICY_ERROR(m_exit.initialize(m_buttons, gui_widget_type::text_button));
        switch (base.type)
        {
        case mbox::type::directory:
        case mbox::type::list_inputs:
        case mbox::type::list_variables:
        case mbox::type::list_timers:
        case mbox::type::list_events:
        case mbox::type::list_bindings:
        case mbox::type::list_commands:
            break;
        case mbox::type::input:
            m_value = make_unique<mbox_form_value_input>(gui, m_library);
            break;
        default:
            return make_stdlib_error(std::errc::invalid_argument);
        }
        if (m_value)
        {
            ICY_ERROR(m_tabs.initialize(m_window, gui_widget_type::tabs));            
            ICY_ERROR(m_value->initialize(m_tabs));
            //ICY_ERROR(m_links.initialize(m_tabs, gui_widget_type::tree_view));
            gui.text();
            //ICY_ERROR(gui.text(m_value->widget(), "Value"_s));
           // ICY_ERROR(gui.text(m_links, "Links"_s));
        }
        
       
        ICY_ERROR(gui.text(m_save, "Save"_s));
        ICY_ERROR(gui.text(m_exit, "Cancel"_s));

        auto row = 0u;
        ICY_ERROR(m_name.insert(row));
        ICY_ERROR(m_parent.insert(row));
        ICY_ERROR(m_type.insert(row));
        ICY_ERROR(m_index.insert(row));
        ICY_ERROR(gui.insert(m_tabs, gui_insert(0, row++, 2, 1)));
        ICY_ERROR(gui.insert(m_buttons, gui_insert(0, row++, 2, 1)));
        ICY_ERROR(m_window.gui->show(m_window, true));

        return{};
    }
    icy::gui_widget window() const noexcept
    {
        return m_window;
    }
    icy::error_type exec(const icy::event event) noexcept
    {
        using namespace icy;
        if (event->type == icy::event_type::gui_update)
        {
            const auto& event_data = event->data<gui_event>();
            if (event_data.widget == m_save)
            {
                
            }
            else if (event_data.widget == m_exit)
            {

            }
        }
        ICY_ERROR(m_name.exec(event));

        if (m_value)
            ICY_ERROR(m_value->exec(event));

        return {};
    }
private:
    mbox::library& m_library;
    icy::gui_widget_scoped m_window;
    mbox_form_key_value_text m_name;
    mbox_form_key_value_pair m_parent;
    mbox_form_key_value_pair m_type;
    mbox_form_key_value_pair m_index;
    icy::gui_widget_scoped m_tabs;
    icy::unique_ptr<mbox_form_value> m_value;
    icy::gui_widget_scoped m_links;
    icy::gui_widget_scoped m_buttons;
    icy::gui_widget_scoped m_save;
    icy::gui_widget_scoped m_exit;
};
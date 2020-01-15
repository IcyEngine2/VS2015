#pragma once

#include <icy_qtgui/icy_qtgui.hpp>
#include "../icy_mbox_script.hpp"

enum class mbox_form_mode : uint32_t
{
    view,
    edit,
    create,
};

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
            {
                const auto udata = event_data.data.as_node().udata();
                if (udata.type() == icy::gui_variant_type::uinteger)
                    m_select = uint32_t(udata.as_uinteger());
            }
        }
        return {};
    }
    icy::error_type append(const uint32_t value, const icy::string_view text) noexcept
    {
        auto offset = m_data.size();
        ICY_ERROR(model.gui->insert_rows(model, offset, 1));
        const auto node = model.gui->node(model, offset, 0);
        ICY_ERROR(model.gui->text(node, text));
        ICY_ERROR(model.gui->udata(node, uint64_t(value)));
        auto tmp = value;
        ICY_ERROR(m_data.insert(std::move(tmp), uint32_t(m_data.size())));
        return {};
    }
    icy::error_type scroll(const uint32_t value) noexcept
    {
        const auto it = m_data.find(value);
        if (it == m_data.end())
            return icy::make_stdlib_error(std::errc::invalid_argument);

        ICY_ERROR(model.gui->scroll(this->value, model.gui->node(model, it->value, 0)));
        //m_select = value;
        return {};
    }
    uint32_t select() const noexcept
    {
        return m_select;
    }
    template<typename T>
    icy::error_type append(const T value) noexcept
    {
        static_assert(std::is_enum<T>::value, "INVALID TYPE");
        const auto str = to_string(value);
        return append(uint32_t(value), str.empty() ? "None"_s : str);
    }
    template<typename T>
    icy::error_type fill(const bool with_zero) noexcept
    {
        using icy::to_string;
        using mbox::to_string;

        icy::map<string_view, uint32_t> map;
        for (auto k = T(1); k < T::_total; k = T(uint32_t(k) + 1))
            ICY_ERROR(map.insert(to_string(k), uint32_t(k)));

        if (with_zero)
            ICY_ERROR(append(0u, ""_s));

        for (auto&& pair : map)
            ICY_ERROR(append(pair.value, pair.key));
        return {};
    }
public:
    icy::gui_model_scoped model;
private:
    icy::map<uint32_t, uint32_t> m_data;
    uint32_t m_select = 0;
};
class mbox_form_key_value_text : public mbox_form_key_value_pair
{
public:
    using mbox_form_key_value_pair::mbox_form_key_value_pair;
    icy::error_type initialize(const icy::string_view text, const icy::gui_widget parent, const icy::gui_widget_type type = icy::gui_widget_type::line_edit) noexcept
    {
        return mbox_form_key_value_pair::initialize(text, parent, type);
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
class mbox_form_key_value_base : public mbox_form_key_value_pair
{
public:
    mbox_form_key_value_base(icy::gui_queue& gui) noexcept : mbox_form_key_value_pair(gui), m_model(gui)
    {

    }
    icy::error_type initialize(const icy::string_view text, const icy::gui_widget parent) noexcept
    {
        ICY_ERROR(mbox_form_key_value_pair::initialize(text, parent, icy::gui_widget_type::combo_box));
        return {};
    }
    icy::error_type scroll(const icy::guid& index) noexcept
    {
        //if (index == m_select)
        //    return {};
        const auto it = std::find(m_data.begin(), m_data.end(), index);
        if (it == m_data.end())
            return {};// icy::make_stdlib_error(std::errc::invalid_argument);
        ICY_ERROR(m_model.gui->scroll(value, m_model.gui->node(m_model, std::distance(m_data.begin(), it), 0)));
        //m_select = index;
        return {};
    }
    icy::error_type reset(mbox::library& library, const mbox::base& base, const bool no_default_groups = false) noexcept;
    icy::error_type exec(const icy::event event) noexcept
    {
        if (event->type == icy::event_type::gui_select)
        {
            const auto& event_data = event->data<icy::gui_event>();
            if (event_data.widget == value)
            {
                if (event_data.data.as_node().udata().type() == icy::gui_variant_type::guid)
                    m_select = event_data.data.as_node().udata().as_guid();
            }
        }
        return {};
    }
    icy::guid select() const noexcept
    {
        return m_select;
    }
public:
private:
    icy::array<icy::guid> m_data;
    icy::guid m_select;
    icy::gui_model_scoped m_model;
};

class mbox_form_value
{
public:
    mbox_form_value(const mbox_form_mode mode, icy::gui_queue& gui, mbox::library& library) : 
        m_mode(mode), m_library(library), m_widget(gui)
    {

    }
    virtual icy::error_type initialize(const icy::gui_widget parent, const mbox::base& base) noexcept = 0;
    virtual icy::error_type create(mbox::base& base) noexcept = 0;
    virtual icy::error_type exec(const icy::event event) noexcept = 0;
    icy::gui_widget widget() const noexcept
    {
        return m_widget;
    }
    mbox_form_mode mode() const noexcept
    {
        return m_mode;
    }
protected:
    const mbox_form_mode m_mode;
    mbox::library& m_library;
    icy::gui_widget_scoped m_widget;
};
class mbox_form_value_input : public mbox_form_value
{
public:
    mbox_form_value_input(const mbox_form_mode mode, icy::gui_queue& gui, mbox::library& library) noexcept : 
        mbox_form_value(mode, gui, library), m_button(gui), m_event(gui), m_ctrl(gui), m_alt(gui), m_shift(gui), m_macro(gui)
    {

    }
    icy::error_type initialize(const icy::gui_widget parent, const mbox::base& base) noexcept override;
    icy::error_type create(mbox::base& base) noexcept override;
    icy::error_type exec(const icy::event event) noexcept override;
private:
    mbox_form_key_value_combo m_button;
    mbox_form_key_value_combo m_event;
    mbox_form_key_value_combo m_ctrl;
    mbox_form_key_value_combo m_alt;
    mbox_form_key_value_combo m_shift;
    mbox_form_key_value_text m_macro;
};
class mbox_form_value_type : public mbox_form_value
{
public:
    mbox_form_value_type(const mbox_form_mode mode, icy::gui_queue& gui, mbox::library& library, const mbox::type mtype) 
        noexcept : mbox_form_value(mode, gui, library), m_mtype(mtype), m_type(gui), m_group(gui), m_reference_list(gui), 
        m_reference_base(gui), m_secondary_list(gui), m_secondary_base(gui)
    {

    }
    icy::error_type initialize(const icy::gui_widget parent, const mbox::base& base) noexcept override;
    icy::error_type exec(const icy::event event) noexcept override;
private:
    icy::error_type on_select(const uint32_t type) noexcept;
protected:
    const mbox::type m_mtype;
    mbox_form_key_value_combo m_type;
    mbox_form_key_value_base m_group;
    mbox_form_key_value_base m_reference_list;
    mbox_form_key_value_base m_reference_base;
    mbox_form_key_value_base m_secondary_list;
    mbox_form_key_value_base m_secondary_base;
    icy::guid m_reference_cache;
    icy::guid m_secondary_cache;
};
class mbox_form_value_event : public mbox_form_value_type
{
public:
    mbox_form_value_event(const mbox_form_mode mode, icy::gui_queue& gui, mbox::library& library) 
        noexcept : mbox_form_value_type(mode, gui, library, mbox::type::event)
    {

    }
    icy::error_type create(mbox::base& base) noexcept override;
};
class mbox_form_value_action : public mbox_form_value_type
{
public:
    mbox_form_value_action(const mbox_form_mode mode, icy::gui_queue& gui, mbox::library& library, const size_t index)
        noexcept : mbox_form_value_type(mode, gui, library, mbox::type::command), index(index), window(gui), buttons(gui), save(gui), exit(gui)
    {

    }
    icy::error_type create(mbox::base& base) noexcept override;
    icy::error_type initialize(const icy::gui_widget parent, const mbox::base& base) noexcept override;
public:
    const size_t index;
    icy::gui_widget_scoped window;
    icy::gui_widget_scoped buttons;
    icy::gui_widget_scoped save;
    icy::gui_widget_scoped exit;
};

class mbox_form_value_command : public mbox_form_value
{
public:
    mbox_form_value_command(const mbox_form_mode mode, icy::gui_queue& gui, mbox::library& library) noexcept :
        mbox_form_value(mode, gui, library), m_group(gui), m_action(gui), m_create(gui), m_delete(gui), 
        m_modify(gui), m_move_up(gui), m_move_dn(gui), m_model(gui), m_table(gui)
    {

    }
    icy::error_type initialize(const icy::gui_widget parent, const mbox::base& base) noexcept override;
    icy::error_type create(mbox::base& base) noexcept override;
    icy::error_type exec(const icy::event event) noexcept override;
private:
    icy::error_type append() noexcept;
    icy::error_type update(const size_t row) noexcept;
private:
    mbox_form_key_value_base m_group;
    icy::gui_widget_scoped m_action;
    icy::gui_widget_scoped m_create;
    icy::gui_widget_scoped m_delete;
    icy::gui_widget_scoped m_modify;
    icy::gui_widget_scoped m_move_up;
    icy::gui_widget_scoped m_move_dn;
    icy::gui_model_scoped m_model;
    icy::gui_widget_scoped m_table;
    icy::array<mbox::value_command::action> m_data;
    icy::unique_ptr<mbox_form_value_action> m_value;
    size_t m_select = SIZE_MAX;
};

class mbox_form_value_profile : public mbox_form_value
{
public:
    mbox_form_value_profile(const mbox_form_mode mode, icy::gui_queue& gui, mbox::library& library) noexcept :
        mbox_form_value(mode, gui, library), m_command_list(gui), m_command_base(gui)
    {

    }
    icy::error_type initialize(const icy::gui_widget parent, const mbox::base& base) noexcept override;
    icy::error_type create(mbox::base& base) noexcept override;
    icy::error_type exec(const icy::event event) noexcept override;
private:
    mbox_form_key_value_base m_command_list;
    mbox_form_key_value_base m_command_base;
};

class mbox_form_value_binding : public mbox_form_value
{
public:
    mbox_form_value_binding(const mbox_form_mode mode, icy::gui_queue& gui, mbox::library& library) noexcept :
        mbox_form_value(mode, gui, library), m_group(gui), m_event_list(gui), m_event_base(gui), m_command_list(gui), m_command_base(gui)
    {

    }
    icy::error_type initialize(const icy::gui_widget parent, const mbox::base& base) noexcept override;
    icy::error_type create(mbox::base& base) noexcept override;
    icy::error_type exec(const icy::event event) noexcept override;
private:
    mbox_form_key_value_base m_group;
    mbox_form_key_value_base m_event_list;
    mbox_form_key_value_base m_event_base;
    mbox_form_key_value_base m_command_list;
    mbox_form_key_value_base m_command_base;
};

class mbox_form
{
public:
    mbox_form(const mbox_form_mode mode, icy::gui_queue& gui, mbox::library& library) noexcept :
        m_mode(mode), m_library(library), m_window(gui), m_name(gui), m_parent(gui), m_type(gui),
        m_index(gui), m_tabs(gui), m_links(gui), m_buttons(gui), m_save(gui), m_exit(gui)
    {

    }
    icy::gui_widget window() const noexcept
    {
        return m_window;
    }
    icy::error_type initialize(const mbox::base& base) noexcept;
    icy::error_type exec(const icy::event event) noexcept;
private:
    const mbox_form_mode m_mode;
    mbox::library& m_library;
    icy::gui_widget_scoped m_window;
    mbox::base m_base;
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
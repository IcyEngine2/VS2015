#pragma once

#include <icy_engine/core/icy_event.hpp>
#include <icy_qtgui/icy_xqtgui.hpp>
#include "../icy_mbox_script2.hpp"

static constexpr auto mbox_event_type_load_library = icy::event_type(icy::event_type::user << 0x00);
static constexpr auto mbox_event_type_transaction = icy::event_type(icy::event_type::user << 0x01);
static constexpr auto mbox_event_type_view = icy::event_type(icy::event_type::user << 0x02);
static constexpr auto mbox_event_type_edit = icy::event_type(icy::event_type::user << 0x03);
static constexpr auto mbox_event_type_rename = icy::event_type(icy::event_type::user << 0x04);
static constexpr auto mbox_event_type_delete = icy::event_type(icy::event_type::user << 0x05);
static constexpr auto mbox_event_type_create = icy::event_type(icy::event_type::user << 0x06);
static constexpr auto mbox_event_type_save = icy::event_type(icy::event_type::user << 0x07);
static constexpr auto mbox_event_type_changed = icy::event_type(icy::event_type::user << 0x08);
static constexpr auto mbox_event_type_move = icy::event_type(icy::event_type::user << 0x09);


using mbox_event_data_load_library = mbox::library*;
using mbox_event_data_transaction = mbox::transaction;
using mbox_event_data_base = icy::guid;
struct mbox_event_data_create
{
    icy::guid parent;
    mbox::type type;
};

enum class mbox_image
{
    none,
    type_directory,
    type_input,
    //type_variable,
    type_timer,
    type_event,
    type_command,
    //type_binding,
    type_group,
    type_character,

    action_create,
    action_remove,
    action_modify,
    action_move_up,
    action_move_dn,

    _total,
};
namespace icy { struct gui_image; }
icy::gui_image find_image(const mbox_image type) noexcept;

enum class mbox_model_type : uint32_t
{
    none,
    explorer,
    directory,
    view_command,
    edit_command,
};
class mbox_model
{
public:
    static icy::error_type create(const mbox_model_type type, icy::unique_ptr<mbox_model>& model) noexcept;
    mbox_model(const mbox_model_type type) noexcept : m_type(type)
    {

    }
    mbox_model_type type() const noexcept
    {
        return m_type;
    }
    const icy::gui_node root() const noexcept
    {
        return m_model;
    }
    virtual icy::error_type reset(const mbox::library& library, const icy::guid& root, const mbox::type filter = mbox::type::_total) noexcept = 0;
    virtual icy::error_type exec(const icy::event event) noexcept = 0;
    virtual icy::error_type context(const icy::gui_node node) noexcept = 0;
    virtual icy::xgui_node find(const icy::gui_variant& var) noexcept = 0;
    virtual void lock() noexcept
    {

    }
    virtual void unlock() noexcept
    {

    }
    virtual icy::error_type save(mbox::base& base) noexcept
    {
        return {};
    }
    virtual bool is_changed() const noexcept
    {
        return false;
    }
    virtual icy::error_type name(icy::string& str) const noexcept
    {
        return {};
    }
protected:
    mbox_model_type m_type;
    icy::xgui_model m_model;
    icy::guid m_root;
};

class mbox_form_action
{
public:
    static icy::error_type exec(const mbox::library& library, const icy::guid& command, mbox::action& action, bool& saved) noexcept;
protected:
    virtual icy::error_type initialize(const mbox::library& library, const mbox::action& action, icy::xgui_widget& parent) noexcept = 0;
    virtual icy::error_type exec(const icy::event event) noexcept = 0;
    virtual icy::error_type save(mbox::action& action) const noexcept = 0;
private:
    icy::error_type initialize(const icy::string_view command, const mbox::action_type type) noexcept;
protected:
    icy::xgui_widget m_window;
private:
    icy::xgui_widget m_command_label;
    icy::xgui_widget m_command_value;
    icy::xgui_widget m_type_label;
    icy::xgui_widget m_type_value;
    icy::xgui_widget m_widget;
    icy::xgui_widget m_buttons;
    icy::xgui_widget m_save;
    icy::xgui_widget m_exit;
};
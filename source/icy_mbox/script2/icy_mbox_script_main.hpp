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
static constexpr auto mbox_event_type_save_cmd = icy::event_type(icy::event_type::user << 0x06);


struct mbox_event_data_load_library
{
    mbox::library& value;
};
struct mbox_event_data_transaction
{
    mbox::transaction value;
};
struct mbox_event_data_base
{
    icy::guid index;
};
struct mbox_event_data_create
{
    icy::guid parent;
    mbox::type type;
};

uint32_t show_error(const icy::error_type error, const icy::string_view text) noexcept;

enum class mbox_image
{
    none,
    type_directory,
    type_input,
    //type_variable,
    //type_timer,
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
    command,
};
class mbox_model
{
public:
    static icy::error_type create(const mbox_model_type type, icy::unique_ptr<mbox_model>& model) noexcept;
    const icy::gui_node root() const noexcept
    {
        return m_model;
    }
    virtual icy::error_type reset(const mbox::library& library, const icy::guid& root) noexcept = 0;
    virtual icy::error_type execute(const icy::const_array_view<mbox::transaction::operation> oper) noexcept = 0;
    virtual icy::error_type context(const icy::gui_variant& var) noexcept = 0;
    virtual icy::xgui_node find(const icy::gui_variant& var) noexcept = 0;
protected:
    icy::xgui_model m_model;
    icy::guid m_root;
};
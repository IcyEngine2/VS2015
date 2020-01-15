#pragma once

#include <icy_engine/core/icy_event.hpp>
#include "../icy_mbox_script.hpp"

const auto mbox_event_type_create = icy::event_type(icy::event_type::user << 0x00);
const auto mbox_event_type_modify = icy::event_type(icy::event_type::user << 0x01);
const auto mbox_event_type_delete = icy::event_type(icy::event_type::user << 0x02);
using mbox_event_data_create = icy::guid;
struct mbox_event_data_modify
{
    icy::guid index;
    icy::string name;
};
using mbox_event_data_delete = icy::guid;
//const auto mbox_event_type_view = icy::event_type(icy::event_type::user << 0x00);
//const auto mbox_event_type_view_new = icy::event_type(icy::event_type::user << 0x01);
//const auto mbox_event_type_rename = icy::event_type(icy::event_type::user << 0x02);
//const auto mbox_event_type_delete = icy::event_type(icy::event_type::user << 0x03);

uint32_t show_error(const icy::error_type error, const icy::string_view text) noexcept;

enum class mbox_image
{
    none,
    type_directory,
    type_input,
    type_variable,
    type_timer,
    type_event,
    type_command,
    type_binding,
    type_group,
    type_profile,
    
    action_create,
    action_remove,
    action_modify,
    action_move_up,
    action_move_dn,

    _total,
};
namespace icy { struct gui_image; }
icy::gui_image find_image(const mbox_image type) noexcept;
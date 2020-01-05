#pragma once

#include <icy_engine/core/icy_event.hpp>
#include "../icy_mbox_script.hpp"

const auto mbox_event_type_create = icy::event_type(icy::event_type::user << 0x00);
using mbox_event_data_create = icy::guid;
//const auto mbox_event_type_view = icy::event_type(icy::event_type::user << 0x00);
//const auto mbox_event_type_view_new = icy::event_type(icy::event_type::user << 0x01);
//const auto mbox_event_type_rename = icy::event_type(icy::event_type::user << 0x02);
//const auto mbox_event_type_delete = icy::event_type(icy::event_type::user << 0x03);

uint32_t show_error(const icy::error_type error, const icy::string_view text) noexcept;
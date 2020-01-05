#pragma once

#include <icy_engine/core/icy_event.hpp>
#include "../icy_mbox_script.hpp"

const auto mbox_event_type_open = icy::event_type(icy::event_type::user << 0x00);
const auto mbox_event_type_open_new = icy::event_type(icy::event_type::user << 0x01);
const auto mbox_event_type_rename = icy::event_type(icy::event_type::user << 0x02);
const auto mbox_event_type_delete = icy::event_type(icy::event_type::user << 0x03);
const auto mbox_event_type_create = icy::event_type(icy::event_type::user << 0x04);
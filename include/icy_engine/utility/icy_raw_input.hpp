#pragma once

#include <icy_engine/core/icy_input.hpp>
#include <icy_engine/core/icy_event.hpp>

namespace icy
{
    error_type make_raw_input(shared_ptr<event_loop>& loop) noexcept;
}
#pragma once

#include <ecsdb/ecsdb_core.hpp>
#include <icy_engine/core/icy_event.hpp>
#include <icy_engine/utility/icy_imgui.hpp>

struct ecsdb_gui_system : public icy::event_system
{
    virtual const icy::thread& thread() const noexcept = 0;
    icy::thread& thread() noexcept
    {
        return const_cast<icy::thread&>(static_cast<const ecsdb_gui_system*>(this)->thread());
    }
};
icy::error_type create_ecsdb_gui_system(icy::shared_ptr<ecsdb_gui_system>& system, const icy::shared_ptr<icy::imgui_display> display) noexcept;
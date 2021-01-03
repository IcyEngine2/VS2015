#pragma once

#include <icy_engine/core/icy_event.hpp>

namespace icy
{
    class xamarin_system : public event_system
    {
        
    };

    error_type create_event_system(shared_ptr<xamarin_system>& system) noexcept;
}
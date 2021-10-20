#pragma once

#include <icy_engine/core/icy_event.hpp>
#include "icy_render_core.hpp"
#include "icy_gpu.hpp"
#include "icy_window.hpp"

namespace icy
{
    static const auto display_frame_vsync = max_timeout;
    static const auto display_frame_unlim = std::chrono::seconds(0);
    inline auto display_frame_fps(const size_t fps) noexcept
    {
        return fps ? std::chrono::nanoseconds(std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::seconds(1)).count() / fps) : max_timeout;
    }

    class thread;
    struct render_surface;

    struct display_message
    {
        uint64_t index = 0;
        duration_type frame = duration_type(0);
    };
    struct display_system : public event_system
    {
        virtual const icy::thread& thread() const noexcept = 0;
        icy::thread& thread() noexcept
        {
            return const_cast<icy::thread&>(static_cast<const display_system*>(this)->thread());
        }
        virtual error_type repaint(const string_view tag, render_gui_frame& frame) noexcept = 0;
        virtual error_type repaint(const string_view tag, shared_ptr<render_surface> surface) noexcept = 0;
        virtual error_type resize(const window_size size) noexcept = 0;
        virtual error_type frame(const duration_type delta) noexcept = 0;
    };
    error_type create_display_system(shared_ptr<display_system>& system, const shared_ptr<window> window, const shared_ptr<gpu_device> gpu) noexcept;
}
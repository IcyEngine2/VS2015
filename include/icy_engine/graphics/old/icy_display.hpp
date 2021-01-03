#pragma once

#include <icy_engine/core/icy_event.hpp>

struct HWND__;

namespace icy
{
    class adapter;

    enum class display_flags : uint32_t
    {
        none = 0x00,
        triple_buffer = 0x01,
    };
    inline constexpr bool operator&(const display_flags lhs, const display_flags rhs) noexcept
    {
        return !!(uint32_t(lhs) & uint32_t(rhs));
    }
    inline constexpr display_flags operator|(const display_flags lhs, const display_flags rhs) noexcept
    {
        return display_flags(uint32_t(lhs) | uint32_t(rhs));
    }

    static const auto display_frame_vsync = max_timeout;
    static const auto display_frame_unlim = std::chrono::seconds(0);
    inline auto display_frame_fps(const size_t fps) noexcept
    {
        return fps ? std::chrono::nanoseconds(std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::seconds(1)).count() / fps) : max_timeout;
    }

    class render_texture;

    struct display_options
    {
        duration_type vsync = std::chrono::seconds(-1);
        window_size size;
    };
    class display_system : public event_system
    {
    public:
        virtual ~display_system() noexcept = 0 { }
        virtual error_type options(display_options data) noexcept = 0;
        virtual error_type render(render_texture& frame) noexcept = 0;
    };

    inline constexpr uint32_t buffer_count(const display_flags flag) noexcept
    {
        return (flag & display_flags::triple_buffer) ? 3 : 2;
    }

    error_type create_event_system(shared_ptr<display_system>& display, const adapter& adapter, HWND__* const hwnd, const display_flags flags) noexcept;
}
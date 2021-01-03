#pragma once

#include <icy_engine/utility/icy_com.hpp>
#include <icy_engine/graphics/icy_display.hpp>
#include <icy_engine/graphics/icy_adapter.hpp>

struct IDXGISwapChain;
struct IDXGIFactory;
struct IDXGIResource;

class display_data : public icy::display_system
{
public:
    static icy::error_type create_d3d11(icy::shared_ptr<icy::display_system>& system, const icy::adapter& adapter, HWND__* const hwnd, const icy::display_flags flags) noexcept;
    static icy::error_type create_d3d12(icy::shared_ptr<icy::display_system>& system, const icy::adapter& adapter, HWND__* const hwnd, const icy::display_flags flags) noexcept
    {
        return icy::make_stdlib_error(std::errc::function_not_supported);
    }
public:
    display_data(const icy::adapter_flags adapter) : m_adapter(adapter)
    {

    }
    ~display_data() noexcept;
    icy::error_type initialize(IUnknown& device, IDXGIFactory& factory, HWND__* const window, const icy::display_flags flags) noexcept;
protected:
    icy::display_flags flags() const noexcept
    {
        return m_flags;
    }
    virtual icy::error_type resize(IDXGISwapChain& chain, const icy::window_size size) noexcept = 0;
    virtual icy::error_type repaint(IDXGISwapChain& chain, const bool vsync, IDXGIResource* const dxgi) noexcept = 0;
private:
    struct frame_type
    {
        size_t index = 0;
        icy::duration_type delta = icy::display_frame_vsync;
        icy::clock_type::time_point next = {};
        icy::detail::intrusive_mpsc_queue queue;
    };
private:
    icy::error_type options(icy::display_options data) noexcept override;
    icy::error_type exec() noexcept override;
    icy::error_type signal(const icy::event_data& event) noexcept override;
    icy::error_type render(icy::render_texture& frame) noexcept override;
private:
    icy::adapter_flags m_adapter = icy::adapter_flags::none;
    icy::display_flags m_flags = icy::display_flags::none;
    icy::com_ptr<IDXGISwapChain> m_chain;
    void* m_gpu_event = nullptr;
    void* m_cpu_timer = nullptr;
    void* m_update = nullptr;
    bool m_gpu_ready = true;
    bool m_cpu_ready = true;
    frame_type m_frame;
};


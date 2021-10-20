#pragma once

#include <icy_engine/core/icy_array.hpp>
#include <icy_engine/core/icy_string.hpp>
#include <icy_engine/core/icy_smart_pointer.hpp>
#include <icy_engine/core/icy_queue.hpp>
#include <icy_engine/graphics/icy_gpu.hpp>
#include <icy_engine/graphics/icy_display.hpp>
#include <icy_engine/graphics/icy_render.hpp>
#include <icy_engine/utility/icy_com.hpp>
#include <dxgi1_6.h>
#include <dxgidebug.h>
#include <d3d12.h>
#include <d3d11.h>

namespace icy
{
    class directx_gpu_device;
    class directx_system
    {
        friend directx_gpu_device;
    public:
        directx_system(const gpu_flags flags) noexcept : m_flags(flags)
        {

        }
        ~directx_system() noexcept
        {

        }
        error_type initialize() noexcept;
        error_type create_gpu_list(array<shared_ptr<gpu_device>>& list) const noexcept;
    private:
        const gpu_flags m_flags;
        library m_lib_dxgi = "dxgi"_lib;
        library m_lib_d3d12 = "d3d12"_lib;
        library m_lib_d3d11 = "d3d11"_lib;
        library m_lib_debug = "dxgidebug"_lib;
    };
    class directx_gpu_device : public gpu_device
    {
    public:
        directx_gpu_device(const shared_ptr<directx_system> system) noexcept : m_system(system)
        {

        }
        error_type initialize(const DXGI_ADAPTER_DESC1& desc, com_ptr<IDXGIAdapter> handle, const gpu_flags flags) noexcept;
        error_type create_device(com_ptr<ID3D11Device>& device) noexcept;
        void* handle() const noexcept override
        {
            return m_handle.get();
        }
    private:
        string_view name() const noexcept override
        {
            return m_name;
        }
        error_type msaa(render_flags& flags) const noexcept override;

        size_t vid_mem() const noexcept override
        {
            return m_vid_mem;
        }
        size_t sys_mem() const noexcept override
        {
            return m_sys_mem;
        }
        size_t shr_mem() const noexcept override
        {
            return m_shr_mem;
        }
        uint64_t luid() const noexcept override
        {
            return m_luid;
        }
        gpu_flags flags() const noexcept override
        {
            return m_flags;
        }
    private:
        shared_ptr<directx_system> m_system;
        string m_name;
        com_ptr<IDXGIAdapter> m_handle;
        gpu_flags m_flags = gpu_flags::none;
        size_t m_vid_mem = 0;
        size_t m_sys_mem = 0;
        size_t m_shr_mem = 0;
        uint64_t m_luid = 0;
    };
    struct directx_display_internal_event
    {
        duration_type frame = duration_type(-1);
        window_size size;
    };
    struct directx_render_internal_event
    {

    };
    struct directx_display_texture
    {
        window_size offset;
        window_size size;
        com_ptr<ID3D11Texture2D> buffer;
        com_ptr<ID3D11ShaderResourceView> srv;
    };
    class directx_display_system : public display_system
    {
    public:
        directx_display_system(const shared_ptr<gpu_device> adapter, shared_ptr<window> hwnd) : m_adapter(adapter), m_hwnd(hwnd)
        {

        }
        ~directx_display_system() noexcept;
        error_type initialize() noexcept;
        error_type do_resize(const window_size size) noexcept;
        error_type do_repaint(const bool vsync) noexcept;
    private:
        error_type exec() noexcept override;
        error_type signal(const event_data* event) noexcept override;
        const icy::thread& thread() const noexcept override
        {
            return *m_thread;
        }
        error_type repaint(const string_view tag, render_gui_frame& frame) noexcept override;
        error_type repaint(const string_view tag, shared_ptr<render_surface> scene) noexcept override;
        error_type resize(const window_size size) noexcept override
        {
            if (size.x == 0 || size.y == 0)
                return make_stdlib_error(std::errc::invalid_argument);

            directx_display_internal_event msg;
            msg.size = size;
            return event_system::post(nullptr, event_type::system_internal, msg);
        }
        error_type frame(const duration_type duration) noexcept override
        {
            if (duration.count() < 0)
                return make_stdlib_error(std::errc::invalid_argument);

            directx_display_internal_event msg;
            msg.frame = duration;
            return event_system::post(nullptr, event_type::system_internal, msg);
        }
    private:
        struct frame_type
        {
            size_t index = 0;
            duration_type delta = display_frame_vsync;
            clock_type::time_point next = {};
            mpsc_queue<std::pair<string, render_gui_frame>> queue2d;
            mpsc_queue<std::pair<string, directx_display_texture>> queue3d;
            map<string, render_gui_frame> map2d;
            map<string, directx_display_texture> map3d;
        };
    private:
        shared_ptr<directx_gpu_device> m_adapter;
        weak_ptr<window> m_hwnd;
        com_ptr<ID3D11Device> m_device;
        com_ptr<IDXGISwapChain> m_chain;
        com_ptr<ID3D11VertexShader> m_vshader2d;
        com_ptr<ID3D11VertexShader> m_vshader3d;
        com_ptr<ID3D11PixelShader> m_pshader2d;
        com_ptr<ID3D11PixelShader> m_pshader3d;
        com_ptr<ID3D11InputLayout> m_layout2d;
        com_ptr<ID3D11BlendState> m_blend2d;
        com_ptr<ID3D11RasterizerState> m_raster2d;
        com_ptr<ID3D11SamplerState> m_sampler;
        void* m_gpu_event = nullptr;
        void* m_cpu_timer = nullptr;
        sync_handle m_update;
        bool m_gpu_ready = true;
        bool m_cpu_ready = true;
        shared_ptr<event_thread> m_thread;
        frame_type m_frame;
        com_ptr<ID3D11RenderTargetView> m_rtv;
        com_ptr<ID3D11Buffer> m_vbuffer2d;
        com_ptr<ID3D11Buffer> m_ibuffer2d;
        map<guid, com_ptr<ID3D11ShaderResourceView>> m_tex2d;
    };

    class directx_render_system;
    class directx_render_surface : public render_surface
    {
    public:
        directx_render_surface(directx_render_system& system, const uint32_t index) noexcept :
            m_system(make_shared_from_this(&system)), m_index(index)
        {

        }
        shared_ptr<render_system> system() noexcept override;
        uint32_t index() const noexcept override
        {
            return m_index;
        }
        error_type repaint(render_scene& scene) noexcept override;
    private:
        weak_ptr<directx_render_system> m_system;
        uint32_t m_index = 0;
    };
    class directx_render_texture 
    {
    public:
        com_ptr<ID3D11Texture2D> buffer;
        com_ptr<ID3D11RenderTargetView> rtv;
    };
    class directx_render_system : public render_system
    {
    public:
        directx_render_system(const shared_ptr<gpu_device> adapter) : m_adapter(adapter)
        {

        }
        ~directx_render_system() noexcept;
        error_type initialize() noexcept;
    private:
        error_type exec() noexcept override;
        error_type signal(const event_data* event) noexcept override;
        const icy::thread& thread() const noexcept override
        {
            return *m_thread;
        }
        error_type create(const window_size size, shared_ptr<render_surface>& surface) noexcept override;
    private:
        shared_ptr<directx_gpu_device> m_adapter;
        com_ptr<ID3D11Device> m_device;
        sync_handle m_sync;
        shared_ptr<event_thread> m_thread;
        map<uint32_t, directx_render_texture> m_render;
        std::atomic<uint32_t> m_count = 0;
    };
}
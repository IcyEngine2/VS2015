#pragma once

#include <icy_engine/core/icy_string.hpp>
#include <icy_engine/core/icy_queue.hpp>
#include <icy_engine/utility/icy_com.hpp>
#include <icy_engine/graphics/icy_graphics.hpp>

struct IDXGIAdapter;
struct IDXGISwapChain;
struct IDXGIFactory;

struct ID3D11Device;
struct ID3D11DeviceContext;
struct ID3D11VertexShader;
struct ID3D11InputLayout;
struct ID3D11PixelShader;
struct ID3D11RasterizerState;
struct ID3D11DeviceChild;

struct ID3D12Device;
struct ID3D12DeviceChild;
struct ID3D12RootSignature;
struct D3D12_VERSIONED_ROOT_SIGNATURE_DESC;

class d3d11_render_factory;
class d3d11_render_frame;
class d3d11_display_system;

class d3d12_system;
class d3d12_render_factory;
class d3d12_render_frame;
class d3d12_display_system;

enum class render_element_type : uint32_t
{
    none,
    clear,
    svg,
};
struct render_element
{
    render_element(const render_element_type type = render_element_type::none) noexcept : type(type)
    {

    }
    render_element_type type;
    icy::color clear;
    icy::array<icy::render_d2d_vertex> svg;
};
struct render_list_data : public icy::render_list
{
    icy::error_type clear(const icy::color color) noexcept;
    icy::error_type draw(const icy::render_svg_geometry geometry, const icy::render_d2d_matrix& offset, const float layer = 0) noexcept;
    icy::mpsc_queue<render_element> queue;
};
class icy::render_frame::data_type
{
public:
    std::atomic<uint32_t> ref = 1;
    size_t index = 0;
    icy::duration_type time_cpu = {};
    icy::duration_type time_gpu = {};
    icy::window_size size;
    d3d11_render_frame* d3d11 = nullptr;
    d3d12_render_frame* d3d12 = nullptr;
};

using icy::operator""_lib;

class d3d12_system
{
public:
    icy::error_type initialize(const icy::adapter::data_type& adapter) noexcept;
    ID3D12Device& device() const noexcept
    {
        return *m_device;
    }
    icy::error_type rename(ID3D12DeviceChild& resource, const icy::string_view name) noexcept;
    icy::error_type root(icy::com_ptr<ID3D12RootSignature>& output, const D3D12_VERSIONED_ROOT_SIGNATURE_DESC& desc) noexcept;
private:
    icy::library m_lib = "d3d12"_lib;
    icy::com_ptr<ID3D12Device> m_device;
};

class icy::adapter::data_type
{
public:
    icy::error_type initialize(const icy::com_ptr<IDXGIAdapter> adapter, const icy::adapter_flags flags) noexcept;
    icy::com_ptr<IDXGIAdapter> adapter() const noexcept
    {
        return m_adapter;
    }
    icy::string_view name() const noexcept
    {
        return m_name;
    }
    icy::adapter_flags flags() const noexcept
    {
        return m_flags;
    }
    icy::error_type query_msaa(icy::array<uint32_t>& array) const noexcept;
    icy::error_type query_d3d12(icy::shared_ptr<d3d12_system>& system) const noexcept;
    std::atomic<uint32_t> ref = 1;
private:
    icy::library m_lib_dxgi = "dxgi"_lib;
    icy::library m_lib_debug = "dxgidebug"_lib;
    icy::com_ptr<IDXGIAdapter> m_adapter;
    icy::adapter_flags m_flags = icy::adapter_flags::none;
    icy::string m_name;
    mutable icy::mutex m_lock;
    mutable icy::shared_ptr<d3d12_system> m_d3d12;
};

class display_data : public icy::display_system
{
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
    virtual icy::error_type repaint(IDXGISwapChain& chain, const bool vsync, void* const handle) noexcept = 0;
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
    icy::error_type render(const icy::render_frame frame) noexcept override;
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

icy::error_type create_d3d11(icy::shared_ptr<icy::render_factory>& system, const icy::adapter& adapter, const icy::render_flags flags) noexcept;
icy::error_type create_d3d11(d3d11_render_frame*& frame, icy::shared_ptr<d3d11_render_factory> system);
icy::error_type create_d3d11(icy::shared_ptr<icy::display_system>& system, const icy::adapter& adapter, HWND__* const hwnd, const icy::display_flags flags) noexcept;
icy::error_type wait_frame(d3d11_render_frame& frame, void*& handle) noexcept;
void destroy_frame(d3d11_render_frame& frame) noexcept;

icy::error_type create_d3d12(icy::shared_ptr<icy::render_factory>& system, const icy::adapter& adapter, const icy::render_flags flags) noexcept;
icy::error_type create_d3d12(d3d12_render_frame*& frame, icy::shared_ptr<d3d12_render_factory> system);
icy::error_type create_d3d12(icy::shared_ptr<icy::display_system>& system, const icy::adapter& adapter, HWND__* const hwnd, const icy::display_flags flags) noexcept;
icy::error_type wait_frame(d3d12_render_frame& frame, void*& handle) noexcept;
void destroy_frame(d3d12_render_frame& frame) noexcept;

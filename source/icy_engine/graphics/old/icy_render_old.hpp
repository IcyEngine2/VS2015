#pragma once

#include <icy_engine/core/icy_string.hpp>
#include <icy_engine/core/icy_queue.hpp>

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

using icy::operator""_lib;

class render_svg_system
{
public:
    error_type initialize() noexcept;
    ID2D1Factory& d2d1() const noexcept
    {
        return *m_d2d1_factory;
    }
    IDWriteFactory& dwrite() const noexcept
    {
        return *m_dwrite_factory;
    }
    float dpi() const noexcept;
private:
    library m_d2d1_lib = "d2d1"_lib;
    library m_dwrite_lib = "dwrite"_lib;
    com_ptr<ID2D1Factory> m_d2d1_factory;
    com_ptr<IDWriteFactory> m_dwrite_factory;
};

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

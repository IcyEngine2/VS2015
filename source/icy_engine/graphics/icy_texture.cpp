#include <icy_engine/graphics/icy_texture.hpp>
#include <icy_engine/utility/icy_com.hpp>
#include <d3d11.h>
#include <d2d1_3.h>
#include "shaders/draw_simple_vs.hpp"
#include "shaders/draw_simple_ps.hpp"

using namespace icy;

ICY_STATIC_NAMESPACE_BEG
class texture_system_data;
class texture_data : public render_texture
{
    friend texture;
public:
    texture_data(const render_flags flags, const weak_ptr<texture_system_data> system, const window_size size) noexcept;
    error_type initialize() noexcept;
    error_type initialize(const const_matrix_view<color> data) noexcept;
    void* handle() const noexcept override
    {
        return m_texture;
    }
    window_size size() const noexcept override
    {
        return m_size;
    }
    error_type save(array_view<color> colors) const noexcept override;
    error_type clear(const color color) noexcept override;
    error_type render(const render_list& cmd) noexcept override;
private:
    const render_flags m_flags;
    weak_ptr<texture_system> m_system;
    window_size m_size;
    com_ptr<ID3D11Device> m_device;
    com_ptr<ID3D11Texture2D> m_texture;
private:
    com_ptr<ID3D11RenderTargetView> m_view;
    com_ptr<ID3D11VertexShader> m_vertex;
    com_ptr<ID3D11PixelShader> m_pixel;
    com_ptr<ID3D11SamplerState> m_sampler;
    com_ptr<ID3D11InputLayout> m_layout;
    com_ptr<ID3D11Buffer> m_buffer;
};
class texture_system_data : public texture_system
{
public:
    texture_system_data(const adapter adapter) noexcept : m_adapter(adapter)
    {

    }
    error_type initialize() noexcept;
    error_type make_device(com_ptr<ID3D11Device>& ptr) const noexcept;
    error_type make_device(ID3D11Device& d3d11, com_ptr<ID2D1Device>& ptr) const noexcept;
    com_ptr<ID3D11Device> main_device() const noexcept
    {
        return m_device;
    }
    error_type create(const window_size size, const render_flags flags, shared_ptr<render_texture>& texture) const noexcept override;
    error_type create(const const_matrix_view<color> data, shared_ptr<texture>& texture) const noexcept override;
private:
    const adapter m_adapter;
    library m_lib_d3d = "d3d11"_lib;
    library m_lib_d2d = "d2d1"_lib;
    com_ptr<ID3D11Device> m_device;
};
ICY_STATIC_NAMESPACE_END

texture_data::texture_data(const render_flags flags, const weak_ptr<texture_system_data> system, const window_size size) noexcept :
    m_flags(flags), m_system(system), m_size(size)
{

}
error_type texture_data::initialize(const const_matrix_view<color> data) noexcept
{
    auto system = shared_ptr<texture_system>(m_system);
    if (!system)
        return make_stdlib_error(std::errc::invalid_argument);

    m_device = static_cast<texture_system_data*>(system.get())->main_device();

    D3D11_SUBRESOURCE_DATA init = {};
    array<uint32_t> init_data;
    ICY_ERROR(init_data.reserve(m_size.x * m_size.y));
    for (auto row = 0u; row < m_size.y; ++row)
    {
        for (auto col = 0u; col < m_size.x; ++col)
            ICY_ERROR(init_data.push_back(data.at(row, col).to_rgba()));
    }
    init.pSysMem = init_data.data();
    init.SysMemPitch = m_size.x * sizeof(color);

    auto desc = CD3D11_TEXTURE2D_DESC(DXGI_FORMAT_R8G8B8A8_UNORM, 
        m_size.x, m_size.y, 1, 1, D3D11_BIND_SHADER_RESOURCE);
    ICY_COM_ERROR(m_device->CreateTexture2D(&desc, &init, &m_texture));

    return error_type();
}
error_type texture_data::initialize() noexcept
{
    auto system = shared_ptr<texture_system>(m_system);
    if (!system)
        return make_stdlib_error(std::errc::invalid_argument);

    ICY_ERROR(static_cast<texture_system_data*>(system.get())->make_device(m_device));

    auto desc = CD3D11_TEXTURE2D_DESC(DXGI_FORMAT_R8G8B8A8_UNORM, 
        m_size.x, m_size.y, 1, 1, D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET);
    desc.MiscFlags = D3D11_RESOURCE_MISC_SHARED;
    ICY_COM_ERROR(m_device->CreateTexture2D(&desc, nullptr, &m_texture));
    ICY_COM_ERROR(m_device->CreateRenderTargetView(m_texture, 
        &CD3D11_RENDER_TARGET_VIEW_DESC(m_texture, D3D11_RTV_DIMENSION_TEXTURE2D, 
            DXGI_FORMAT_R8G8B8A8_UNORM), &m_view));

    ICY_COM_ERROR(m_device->CreateVertexShader(g_shader_bytes_draw_simple_vs, 
        sizeof(g_shader_bytes_draw_simple_vs), nullptr, &m_vertex));
    ICY_COM_ERROR(m_device->CreatePixelShader(g_shader_bytes_draw_simple_ps,
        sizeof(g_shader_bytes_draw_simple_ps), nullptr, &m_pixel));

    const D3D11_INPUT_ELEMENT_DESC input_desc[] =
    {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT },
        { "COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT },
        { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT },    
    };
    ICY_COM_ERROR(m_device->CreateInputLayout(input_desc, _countof(input_desc), g_shader_bytes_draw_simple_vs,
        sizeof(g_shader_bytes_draw_simple_vs), &m_layout));
    ICY_COM_ERROR(m_device->CreateSamplerState(&CD3D11_SAMPLER_DESC(D3D11_DEFAULT), &m_sampler));

    com_ptr<ID3D11DeviceContext> context;
    m_device->GetImmediateContext(&context);
    
    context->VSSetShader(m_vertex, nullptr, 0);
    context->PSSetShader(m_pixel, nullptr, 0);
    context->PSSetSamplers(0, 1, &m_sampler);

    const D3D11_VIEWPORT viewport = { 0, 0, float(m_size.x), float(m_size.y), 0, 1 };
    context->IASetInputLayout(m_layout);
    context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    context->RSSetViewports(1, &viewport);
    context->OMSetRenderTargets(1, &m_view, nullptr);
    return error_type();
}
error_type texture_data::save(array_view<color> colors) const noexcept
{
    if (colors.size() != m_size.x * m_size.y)
        return make_stdlib_error(std::errc::invalid_argument);

    com_ptr<ID3D11DeviceContext> context;
    m_device->GetImmediateContext(&context);

    com_ptr<ID3D11Texture2D> cpu_texture;
    ICY_COM_ERROR(m_device->CreateTexture2D(&CD3D11_TEXTURE2D_DESC(DXGI_FORMAT_R8G8B8A8_UNORM,
        m_size.x, m_size.y, 1, 1, 0, D3D11_USAGE_STAGING, D3D11_CPU_ACCESS_READ), nullptr, &cpu_texture));

    D3D11_BOX box = { 0, 0, 0, m_size.x, m_size.y, 1 };
    context->CopySubresourceRegion(cpu_texture, 0, 0, 0, 0, m_texture, 0, &box);

    D3D11_MAPPED_SUBRESOURCE map;
    ICY_COM_ERROR(context->Map(cpu_texture, 0, D3D11_MAP_READ, 0, &map));
    ICY_SCOPE_EXIT{ context->Unmap(cpu_texture, 0); };

    auto dst = colors.data();
    for (auto row = 0u; row < m_size.y; ++row)
    {
        auto ptr = reinterpret_cast<const uint32_t*>(static_cast<char*>(map.pData) + row * map.RowPitch);
        for (auto col = 0u; col < m_size.x; ++col, ++dst)
            *dst = color::from_rgba(ptr[col]);
    }
    return error_type();
}
error_type texture_data::clear(const color color) noexcept
{
    com_ptr<ID3D11DeviceContext> context;
    m_device->GetImmediateContext(&context);

    float rgba[4];
    color.to_rgbaf(rgba);
    context->ClearRenderTargetView(m_view, rgba);
    context->Flush();
    return error_type();
}
error_type texture_data::render(const render_list& cmd) noexcept
{
    if (cmd.texture == this)
        return make_stdlib_error(std::errc::invalid_argument);

    auto system = shared_ptr<texture_system>(m_system);
    if (!system)
        return make_stdlib_error(std::errc::invalid_argument);

    D3D11_BUFFER_DESC desc = {};
    if (m_buffer) m_buffer->GetDesc(&desc);

    com_ptr<ID3D11DeviceContext> context;
    m_device->GetImmediateContext(&context);
    const auto bytes = cmd.primitives.size() * sizeof(render_primitive);

    if (desc.ByteWidth >= bytes)
    {
        D3D11_MAPPED_SUBRESOURCE map;
        ICY_COM_ERROR(context->Map(m_buffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &map));
        memcpy(map.pData, cmd.primitives.data(), bytes);
        context->Unmap(m_buffer, 0);
    }
    else
    {
        D3D11_SUBRESOURCE_DATA init = {};
        init.pSysMem = cmd.primitives.data();
        desc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
        desc.ByteWidth = uint32_t(bytes);
        desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
        desc.Usage = D3D11_USAGE_DYNAMIC;
        com_ptr<ID3D11Buffer> buffer;
        ICY_COM_ERROR(m_device->CreateBuffer(&desc, &init, &buffer));
        m_buffer = buffer;
    }

    const uint32_t stride[] = { sizeof(render_vertex) };
    const uint32_t offset[] = { 0 };
    context->IASetVertexBuffers(0, 1, &m_buffer, stride, offset);
    
    context->DrawInstanced(uint32_t(cmd.primitives.size()) * 3, 1, 0, 0);
    //context->DrawInstanced(3, 1, 0, 0);
    context->Flush();

    return error_type();
}

error_type texture_system_data::initialize() noexcept
{
    ICY_ERROR(m_lib_d3d.initialize());
    ICY_ERROR(m_lib_d2d.initialize());
    ICY_ERROR(make_device(m_device));
    return error_type();
}
error_type texture_system_data::make_device(com_ptr<ID3D11Device>& ptr) const noexcept
{
    if (const auto func = ICY_FIND_FUNC(m_lib_d3d, D3D11CreateDevice))
    {
        const auto level = m_adapter.flags() & adapter_flags::d3d10 ? D3D_FEATURE_LEVEL_10_1 : D3D_FEATURE_LEVEL_11_0;
        const auto adapter = static_cast<IDXGIAdapter*>(m_adapter.handle());
        auto flags = 0u;
        if (m_adapter.flags() & adapter_flags::debug)
            flags |= D3D11_CREATE_DEVICE_DEBUG;

        ICY_COM_ERROR(func(adapter, D3D_DRIVER_TYPE_UNKNOWN, nullptr, flags, &level, 1, D3D11_SDK_VERSION, &ptr, nullptr, nullptr));
    }
    else
    {
        return make_stdlib_error(std::errc::function_not_supported);
    }
    return error_type();
}
error_type texture_system_data::make_device(ID3D11Device& d3d11, com_ptr<ID2D1Device>& ptr) const noexcept
{
    if (const auto func = m_lib_d2d.find("D2D1CreateDevice"))
    {
        com_ptr<IDXGIDevice> dxgi_device;
        ICY_COM_ERROR(d3d11.QueryInterface(&dxgi_device));

        auto func_type = reinterpret_cast<HRESULT(WINAPI*)(IDXGIDevice*, const D2D1_CREATION_PROPERTIES*, ID2D1Device**)>(func);
        auto props = D2D1::CreationProperties(D2D1_THREADING_MODE_MULTI_THREADED,
            m_adapter.flags() & adapter_flags::debug ? D2D1_DEBUG_LEVEL_INFORMATION : D2D1_DEBUG_LEVEL_NONE,
            D2D1_DEVICE_CONTEXT_OPTIONS_ENABLE_MULTITHREADED_OPTIMIZATIONS);
        ICY_COM_ERROR(func_type(dxgi_device, &props, &ptr));
    }
    else
    {
        return make_stdlib_error(std::errc::function_not_supported);
    }
    return error_type();
}
error_type texture_system_data::create(const window_size size, const render_flags flags, shared_ptr<render_texture>& texture) const noexcept
{
    shared_ptr<texture_data> ptr;
    ICY_ERROR(make_shared(ptr, flags, make_shared_from_this(this), size));
    ICY_ERROR(ptr->initialize());
    texture = std::move(ptr);
    return error_type();
}
error_type texture_system_data::create(const const_matrix_view<color> data, shared_ptr<texture>& texture) const noexcept
{
    shared_ptr<texture_data> ptr;
    ICY_ERROR(make_shared(ptr, render_flags::none, make_shared_from_this(this), 
        window_size(uint32_t(data.cols()), uint32_t(data.rows()))));
    ICY_ERROR(ptr->initialize(data));
    texture = std::move(ptr);
    return error_type();
}
error_type icy::create_texture_system(const adapter adapter, shared_ptr<texture_system>& system) noexcept
{
    shared_ptr<texture_system_data> ptr;
    ICY_ERROR(make_shared(ptr, adapter));
    ICY_ERROR(ptr->initialize());
    system = std::move(ptr);
    return error_type();
}
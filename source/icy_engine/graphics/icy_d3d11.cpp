#include <d3d11_4.h>
#include <dxgi1_6.h>
#include "icy_shader.hpp"
#include "icy_display.hpp"
#include "icy_adapter.hpp"
#include "icy_render_commands.hpp"
#include "icy_render_factory.hpp"
#include "icy_texture.hpp"

using namespace icy;

using D3D11_SHADER_BYTECODE = const_array_view<uint8_t>;
struct D3D11_GRAPHICS_PIPELINE_STATE_DESC
{
    D3D11_GRAPHICS_PIPELINE_STATE_DESC() noexcept
    {
        DepthStencilState.DepthEnable = FALSE;
    }
    D3D11_SHADER_BYTECODE VS;
    D3D11_SHADER_BYTECODE PS;
    D3D11_SHADER_BYTECODE DS;
    D3D11_SHADER_BYTECODE HS;
    D3D11_BLEND_DESC BlendState = CD3D11_BLEND_DESC(D3D11_DEFAULT);
    D3D11_RASTERIZER_DESC RasterizerState = CD3D11_RASTERIZER_DESC(D3D11_DEFAULT);
    D3D11_DEPTH_STENCIL_DESC DepthStencilState = CD3D11_DEPTH_STENCIL_DESC(D3D11_DEFAULT);
    const_array_view<D3D11_INPUT_ELEMENT_DESC> InputLayout;
};
struct D3D11_GRAPHICS_PIPELINE_STATE
{
    error_type Initialize(const D3D11_GRAPHICS_PIPELINE_STATE_DESC& desc, ID3D11Device& device, const string_view prefix) noexcept;
    void Apply(ID3D11DeviceContext& context) noexcept;
    com_ptr<ID3D11VertexShader> VS;
    com_ptr<ID3D11PixelShader> PS;
    com_ptr<ID3D11HullShader> HS;
    com_ptr<ID3D11DomainShader> DS;
    com_ptr<ID3D11BlendState> BlendState;
    com_ptr<ID3D11RasterizerState> RasterizerState;
    com_ptr<ID3D11DepthStencilState> DepthStencilState;
    com_ptr<ID3D11InputLayout> InputLayout;
    D3D11_PRIMITIVE_TOPOLOGY PrimitiveTopology = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
};

static const auto D3DDebugObjectNameW = _GUID{ 0x4cca5fd8,0x921f,0x42c8,0x85,0x66,0x70,0xca,0xf2,0xa9,0xb7,0x41 };

class d3d11_display_system : public display_data
{
public:
    d3d11_display_system() noexcept : display_data(adapter_flags::d3d11)
    {

    }
    ~d3d11_display_system() noexcept
    {
        filter(0);
    }
    error_type initialize(const adapter& adapter, HWND__* const window, const display_flags flags) noexcept;
private:
    error_type resize(IDXGISwapChain& chain, const window_size size) noexcept override;
    error_type repaint(IDXGISwapChain& chain, const bool vsync, IDXGIResource* const dxgi) noexcept override;
private:
    render_factory_data m_render;
    com_ptr<ID3D11Device> m_device;
    D3D11_GRAPHICS_PIPELINE_STATE m_pipe;
    com_ptr<ID3D11SamplerState> m_sampler;
    com_ptr<ID3D11Texture2D> m_buffer;
    com_ptr<ID3D11RenderTargetView> m_view;
    window_size m_size;
};
class d3d11_texture
{
public:
    error_type resize(ID3D11Device& device, D3D11_TEXTURE2D_DESC new_desc, const render_flags flags) noexcept;
    com_ptr<ID3D11Texture2D> get() const noexcept
    {
        return m_texture;
    }
    com_ptr<ID3D11RenderTargetView> rtv() const noexcept
    {
        return m_rtv;
    }
    com_ptr<ID3D11ShaderResourceView> srv() const noexcept
    {
        return m_srv;
    }
private:
    com_ptr<ID3D11Texture2D> m_texture;
    com_ptr<ID3D11RenderTargetView> m_rtv;
    com_ptr<ID3D11ShaderResourceView> m_srv;
};
class d3d11_buffer
{
public:
    error_type update(ID3D11Device& device, const D3D11_BUFFER_DESC& desc, const void* const data, const size_t size) noexcept;
    com_ptr<ID3D11Buffer> gpu() const noexcept
    {
        return m_gpu_buffer;
    }
private:
    com_ptr<ID3D11Buffer> m_gpu_buffer;
    com_ptr<ID3D11Buffer> m_cpu_buffer;
};
class d3d11_render : public render_commands_3d_data
{
public:
    error_type initialize(render_factory_data& factory) noexcept;
    error_type exec(render_commands_3d::data_type& cmd, const window_size size, render_texture& output) noexcept override;
private:
    struct draw_texture_vertex
    {
        render_d2d_vector pos;
        render_d2d_vector tex;
    };
    com_ptr<ID3D11Device> m_device;
    D3D11_GRAPHICS_PIPELINE_STATE m_msaa;
    D3D11_GRAPHICS_PIPELINE_STATE m_draw_texture_pipe;
    d3d11_buffer m_draw_texture_buffer;
    com_ptr<ID3D11SamplerState> m_sampler; 
};

static error_type rename(ID3D11DeviceChild& resource, const string_view name) noexcept
{
    com_ptr<ID3D11Device> device;
    resource.GetDevice(&device);
    if (device->GetCreationFlags() & D3D11_CREATE_DEVICE_DEBUG)
    {
        array<wchar_t> wname;
        ICY_ERROR(to_utf16(name, wname));
        ICY_COM_ERROR(resource.SetPrivateData(D3DDebugObjectNameW, uint32_t(wname.size() * sizeof(wchar_t)), wname.data()));
    }
    return error_type();
}

error_type d3d11_display_system::initialize(const adapter& adapter, HWND__* const window, const display_flags flags) noexcept
{
    com_ptr<IDXGIFactory> factory;
    ICY_ERROR(m_render.initialize(adapter, render_flags::none));
    ICY_ERROR(m_render.create_device(m_device));
    ICY_COM_ERROR(adapter.data->adapter()->GetParent(IID_PPV_ARGS(&factory)));
    ICY_ERROR(display_data::initialize(*m_device, *factory, window, flags));
    ICY_COM_ERROR(factory->MakeWindowAssociation(window,
        DXGI_MWA_NO_ALT_ENTER | DXGI_MWA_NO_WINDOW_CHANGES | DXGI_MWA_NO_PRINT_SCREEN));

    D3D11_GRAPHICS_PIPELINE_STATE_DESC desc;
    desc.VS = g_shader_bytes_screen_vs;
    desc.PS = g_shader_bytes_draw_texture_ps;
    ICY_ERROR(m_pipe.Initialize(desc, *m_device, "D3D11 Display System"_s));
    ICY_COM_ERROR(m_device->CreateSamplerState(&CD3D11_SAMPLER_DESC(D3D11_DEFAULT), &m_sampler));
    ICY_ERROR(rename(*m_sampler, "D3D11 Render Sampler"_s));

    wchar_t wname[] = L"D3D11 Display System Device";
    ICY_COM_ERROR(m_device->SetPrivateData(D3DDebugObjectNameW, sizeof(wname), wname));
    
    filter(event_type::system_internal);
    return error_type();
}
error_type d3d11_display_system::resize(IDXGISwapChain& chain, const window_size size) noexcept
{
    m_buffer = nullptr;
    m_view = nullptr;

    DXGI_SWAP_CHAIN_DESC chain_desc;
    ICY_COM_ERROR(chain.GetDesc(&chain_desc));
    ICY_COM_ERROR(chain.ResizeBuffers(0, size.x, size.y, DXGI_FORMAT_UNKNOWN, chain_desc.Flags));
    ICY_COM_ERROR(chain.GetBuffer(0, IID_PPV_ARGS(&m_buffer)));
    ICY_COM_ERROR(m_device->CreateRenderTargetView(m_buffer, &CD3D11_RENDER_TARGET_VIEW_DESC(
        D3D11_RTV_DIMENSION_TEXTURE2D, DXGI_FORMAT_R8G8B8A8_UNORM), &m_view));
    ICY_ERROR(rename(*m_buffer, "D3D11 Display Back Buffer Texture"));
    ICY_ERROR(rename(*m_view, "D3D11 Display Back Buffer RTV"));
    m_size = size;
    return error_type();
}
error_type d3d11_display_system::repaint(IDXGISwapChain& chain, const bool vsync, IDXGIResource* const dxgi) noexcept
{
    HANDLE handle = nullptr;
    ICY_COM_ERROR(dxgi->GetSharedHandle(&handle));
    com_ptr<ID3D11Texture2D> texture;
    com_ptr<ID3D11ShaderResourceView> srv;
    ICY_COM_ERROR(m_device->OpenSharedResource(handle, IID_PPV_ARGS(&texture)));   
    ICY_COM_ERROR(m_device->CreateShaderResourceView(texture, nullptr, &srv));

    ICY_ERROR(rename(*texture, "D3D11 Display Frame Texture"_s));
    ICY_ERROR(rename(*srv, "D3D11 Display Frame SRV"_s));

    const auto viewport = D3D11_VIEWPORT{ 0, 0, float(m_size.x), float(m_size.y), 0, 1 };
    com_ptr<ID3D11DeviceContext> context;
    m_device->GetImmediateContext(&context);

    m_pipe.Apply(*context);
    context->PSSetSamplers(0, 1, &m_sampler);
    context->PSSetShaderResources(0, 1, &srv);
    context->RSSetViewports(1, &viewport);
    context->OMSetRenderTargets(1, &m_view, nullptr);
    context->Draw(3, 0);

    const auto hr = chain.Present(vsync, 0);
    if (FAILED(hr))
    {
        if (hr == DXGI_STATUS_OCCLUDED)
            ;
        else
            return make_system_error(hr);
    }
    return error_type();
}

error_type d3d11_texture::resize(ID3D11Device& device, D3D11_TEXTURE2D_DESC new_desc, const render_flags flags) noexcept
{
    D3D11_TEXTURE2D_DESC old_desc;
    if (m_texture)
        m_texture->GetDesc(&old_desc);
    else
        old_desc = {};

    if (new_desc.Width != old_desc.Width || new_desc.Height != old_desc.Height)
    {
        const auto samples = sample_count(flags);
        if (samples > 1)
        {
            new_desc.SampleDesc.Count = samples;
            new_desc.SampleDesc.Quality = D3D11_STANDARD_MULTISAMPLE_PATTERN;
        }

        m_texture = nullptr;
        m_rtv = nullptr;
        m_srv = nullptr;

        ICY_COM_ERROR(device.CreateTexture2D(&new_desc, nullptr, &m_texture));
        ICY_ERROR(rename(*m_texture, "D3D11 Frame Texture"_s));

        if (new_desc.BindFlags & D3D11_BIND_FLAG::D3D11_BIND_RENDER_TARGET)
        {
            const auto rtv_desc = CD3D11_RENDER_TARGET_VIEW_DESC(new_desc.SampleDesc.Count > 1 ?
                D3D11_RTV_DIMENSION_TEXTURE2DMS : D3D11_RTV_DIMENSION_TEXTURE2D, new_desc.Format);
            m_rtv = nullptr;
            ICY_COM_ERROR(device.CreateRenderTargetView(m_texture, &rtv_desc, &m_rtv));
        }
        if (new_desc.BindFlags & D3D11_BIND_FLAG::D3D11_BIND_SHADER_RESOURCE)
        {
            const auto srv_desc = CD3D11_SHADER_RESOURCE_VIEW_DESC(new_desc.SampleDesc.Count > 1 ?
                D3D11_SRV_DIMENSION_TEXTURE2DMS : D3D11_SRV_DIMENSION_TEXTURE2D, new_desc.Format);
            m_srv = nullptr;
            ICY_COM_ERROR(device.CreateShaderResourceView(m_texture, &srv_desc, &m_srv));
        }
    }
    return error_type();
}

error_type d3d11_buffer::update(ID3D11Device& device, const D3D11_BUFFER_DESC& desc, const void* const data, const size_t size) noexcept
{
    if (!size || !data)
        return make_stdlib_error(std::errc::invalid_argument);

    D3D11_BUFFER_DESC cpu_desc = desc;
    D3D11_BUFFER_DESC gpu_desc = desc;
    
    if (m_cpu_buffer) m_cpu_buffer->GetDesc(&cpu_desc);
    if (m_gpu_buffer) m_gpu_buffer->GetDesc(&gpu_desc);

    if (cpu_desc.ByteWidth < size)
    {
        cpu_desc.BindFlags = 0;
        cpu_desc.MiscFlags = 0;
        cpu_desc.Usage = D3D11_USAGE_STAGING;
        cpu_desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
        cpu_desc.ByteWidth = uint32_t(align_up(size, 256_z));
        m_cpu_buffer = nullptr;
        ICY_COM_ERROR(device.CreateBuffer(&cpu_desc, nullptr, &m_cpu_buffer));
        ICY_ERROR(rename(*m_cpu_buffer, "D3D11 CPU Buffer"_s));
    }
    if (gpu_desc.ByteWidth < size)
    {
        gpu_desc.ByteWidth = uint32_t(align_up(size, 256_z));
        m_gpu_buffer = nullptr;
        ICY_COM_ERROR(device.CreateBuffer(&gpu_desc, nullptr, &m_gpu_buffer));
        ICY_ERROR(rename(*m_gpu_buffer, "D3D11 GPU Buffer"_s));
    }
    com_ptr<ID3D11DeviceContext> context;
    device.GetImmediateContext(&context);
    D3D11_MAPPED_SUBRESOURCE map;
    ICY_COM_ERROR(context->Map(m_cpu_buffer, 0, D3D11_MAP_WRITE, 0, &map));
    memcpy(map.pData, data, size);
    context->Unmap(m_cpu_buffer, 0);
    context->CopyResource(m_gpu_buffer, m_cpu_buffer);
    return error_type();
}

error_type d3d11_render::initialize(render_factory_data& factory) noexcept
{
    ICY_ERROR(factory.create_device(m_device));
    com_ptr<ID3D11DeviceContext> context;
    m_device->GetImmediateContext(&context);

    wchar_t wname[] = L"D3D11 Render Device";
    ICY_COM_ERROR(m_device->SetPrivateData(D3DDebugObjectNameW, sizeof(wname), wname));
    ICY_ERROR(rename(*context, "D3D11 Frame Context"_s));

    if (sample_count(factory.flags()) > 1 && !(factory.flags() & render_flags::msaa_hardware))
    {
        const auto count = sample_count(factory.flags());
        D3D11_GRAPHICS_PIPELINE_STATE_DESC desc;
        desc.VS = g_shader_bytes_screen_vs;
        switch (count)
        {
        case 2:
            desc.PS = g_shader_bytes_msaa_ps2;
            break;
        case 4:
            desc.PS = g_shader_bytes_msaa_ps4;
            break;
        case 8:
            desc.PS = g_shader_bytes_msaa_ps8;
            break;
        case 16:
            desc.PS = g_shader_bytes_msaa_ps16;
            break;
        default:
            return make_stdlib_error(std::errc::invalid_argument);
        }
        ICY_ERROR(m_msaa.Initialize(desc, *m_device, "D3D11 Render MSAA"_s));
    }

    {
        D3D11_GRAPHICS_PIPELINE_STATE_DESC desc;
        for (auto&& render : desc.BlendState.RenderTarget)
        {
            render.BlendEnable = TRUE;
            render.SrcBlend = D3D11_BLEND_SRC_ALPHA;
            render.DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
            render.BlendOp = D3D11_BLEND_OP_ADD;
            render.SrcBlendAlpha = D3D11_BLEND_ONE;
            render.DestBlendAlpha = D3D11_BLEND_ZERO;
            render.BlendOpAlpha = D3D11_BLEND_OP_ADD;
            render.RenderTargetWriteMask = 0x0F;
        }
        const D3D11_INPUT_ELEMENT_DESC layout[2] =
        {
            { "POSITION", 0, DXGI_FORMAT_R32G32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
            { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        };
        desc.VS = g_shader_bytes_draw_texture_vs;
        desc.PS = g_shader_bytes_draw_texture_ps;
        desc.InputLayout = layout;
        ICY_ERROR(m_draw_texture_pipe.Initialize(desc, *m_device, "D3D11 Render Draw Texture"_s));
    }

    ICY_COM_ERROR(m_device->CreateSamplerState(&CD3D11_SAMPLER_DESC(D3D11_DEFAULT), &m_sampler));
    ICY_ERROR(rename(*m_sampler, "D3D11 Render Sampler"_s));

    return error_type();
}
error_type d3d11_render::exec(render_commands_3d::data_type& cmds, const window_size size, render_texture& output) noexcept
{
    com_ptr<ID3D11DeviceContext> context;
    m_device->GetImmediateContext(&context);

    auto render = shared_ptr<render_factory_data>(cmds.factory);
    if (!render)
        return make_stdlib_error(std::errc::invalid_argument);

    const auto flags = render->flags();
    const auto flags_msaa = render_flags(uint32_t(flags) & uint32_t(
        render_flags::msaa_x2 |
        render_flags::msaa_x4 |
        render_flags::msaa_x8 |
        render_flags::msaa_x16));

    ICY_ERROR(render->create_texture(*m_device, size, output));
    
    color clear_color = colors::black;
    array<render_command_3d> draw_texture;
    while (true)
    {
        render_command_3d cmd;
        if (!cmds.queue.pop(cmd))
            break;

        switch (cmd.type)
        {
        case render_command_3d::type::clear:
            clear_color = cmd.color;
            break;

        case render_command_3d::type::draw_texture:
            ICY_ERROR(draw_texture.push_back(std::move(cmd)));
            break;
        }
    }
    const auto viewport = D3D11_VIEWPORT{ 0, 0, float(size.x), float(size.y), 0, 1 };
    float rgba[4];
    clear_color.to_rgbaf(rgba);

    HANDLE handle = nullptr;
    ICY_COM_ERROR(output.data->dxgi->GetSharedHandle(&handle));
    com_ptr<ID3D11Texture2D> texture;
    ICY_COM_ERROR(m_device->OpenSharedResource(handle, IID_PPV_ARGS(&texture)));

    com_ptr<ID3D11RenderTargetView> rtv;
    ICY_COM_ERROR(m_device->CreateRenderTargetView(texture, nullptr, &rtv));
    
    context->ClearRenderTargetView(rtv, rgba);
    array<draw_texture_vertex> vertices;
    ICY_ERROR(vertices.reserve(draw_texture.size() * 6));
    for (auto&& cmd : draw_texture)
    {
        draw_texture_vertex v[4];
        v[0].tex = { 0, 0 };
        v[1].tex = { 1, 0 };
        v[2].tex = { 0, 1 };
        v[3].tex = { 1, 1 };

        v[0].pos = { cmd.rect.x + cmd.rect.w * 0, cmd.rect.y + cmd.rect.h * 0 };
        v[1].pos = { cmd.rect.x + cmd.rect.w * 1, cmd.rect.y + cmd.rect.h * 0 };
        v[2].pos = { cmd.rect.x + cmd.rect.w * 0, cmd.rect.y + cmd.rect.h * 1 };
        v[3].pos = { cmd.rect.x + cmd.rect.w * 1, cmd.rect.y + cmd.rect.h * 1 };
        
        for (auto&& vertex : v)
        {
            vertex.pos.x = 2 * vertex.pos.x / size.x - 1;
            vertex.pos.y = 1 - 2 * vertex.pos.y / size.y;
        }

        ICY_ERROR(vertices.push_back(v[0]));
        ICY_ERROR(vertices.push_back(v[1]));
        ICY_ERROR(vertices.push_back(v[2]));

        ICY_ERROR(vertices.push_back(v[0]));
        ICY_ERROR(vertices.push_back(v[2]));
        ICY_ERROR(vertices.push_back(v[1]));
    }
    if (!draw_texture.empty())
    {
        const uint32_t strides[] = { sizeof(draw_texture_vertex) };
        const uint32_t offsets[] = { 0 };

        ICY_ERROR(m_draw_texture_buffer.update(*m_device, CD3D11_BUFFER_DESC(0, D3D11_BIND_VERTEX_BUFFER), vertices.data(), 
            sizeof(draw_texture_vertex) * vertices.size()));
        
        m_draw_texture_pipe.Apply(*context);
        context->PSSetSamplers(0, 1, &m_sampler);
        context->IASetVertexBuffers(0, 1, &m_draw_texture_buffer.gpu(), strides, offsets);
        context->RSSetViewports(1, &viewport);
        context->OMSetRenderTargets(1, &rtv, nullptr);

        auto index = 0u;
        for (auto&& cmd : draw_texture)
        {
            HANDLE handle = nullptr;
            ICY_COM_ERROR(cmd.texture.data->dxgi->GetSharedHandle(&handle));

            com_ptr<ID3D11Texture2D> tex;
            com_ptr<ID3D11ShaderResourceView> srv;
            ICY_COM_ERROR(m_device->OpenSharedResource(handle, IID_PPV_ARGS(&tex)));
            ICY_COM_ERROR(m_device->CreateShaderResourceView(tex, nullptr, &srv));

            string str;
            ICY_ERROR(to_string("D3D11 DrawTexture %1"_s, str, index / 6));
            ICY_ERROR(rename(*tex, str));

            ICY_ERROR(to_string("D3D11 DrawTexture SRV %1"_s, str, index / 6));
            ICY_ERROR(rename(*srv, str));
            
            context->PSSetShaderResources(0, 1, &srv);
            context->Draw(6, index);
            index += 6;
        }
    }

    context->Flush();
    return error_type();
}

error_type display_data::create_d3d11(shared_ptr<display_system>& system, const adapter& adapter, HWND__* const hwnd, const display_flags flags) noexcept
{
    shared_ptr<d3d11_display_system> new_ptr;
    ICY_ERROR(make_shared(new_ptr));
    ICY_ERROR(new_ptr->initialize(adapter, hwnd, flags));
    system = std::move(new_ptr);
    return error_type();
}

error_type render_commands_3d_data::create_d3d11(render_commands_3d_data*& ptr, render_factory_data& factory) noexcept
{
    auto new_ptr = allocator_type::allocate<d3d11_render>(1);
    if (!new_ptr)
        return make_stdlib_error(std::errc::invalid_argument);
    allocator_type::construct(new_ptr);
    if (const auto error = new_ptr->initialize(factory))
    {
        allocator_type::destroy(new_ptr);
        allocator_type::deallocate(new_ptr);
        return error;
    }
    if (ptr)
    {
        allocator_type::destroy(ptr);
        allocator_type::deallocate(ptr);
    }
    ptr = new_ptr;
    return error_type();
}

error_type render_factory_data::create_device(com_ptr<ID3D11Device>& device) const noexcept
{
    if (const auto func = ICY_FIND_FUNC(m_lib_d3d11, D3D11CreateDevice))
    {
        device = nullptr;
        auto level = m_adapter.flags() & adapter_flags::d3d10 ? D3D_FEATURE_LEVEL_10_1 : D3D_FEATURE_LEVEL_11_0;
        auto flags = m_adapter.flags() & adapter_flags::debug ? D3D11_CREATE_DEVICE_DEBUG : 0;
        flags |= D3D11_CREATE_DEVICE_BGRA_SUPPORT;
        ICY_COM_ERROR(func(m_adapter.data->adapter(), D3D_DRIVER_TYPE_UNKNOWN, nullptr,
            flags, &level, 1, D3D11_SDK_VERSION, &device, nullptr, nullptr));

        if (device->GetCreationFlags() & D3D11_CREATE_DEVICE_FLAG::D3D11_CREATE_DEVICE_DEBUG)
        {
            if (auto debug = com_ptr<ID3D11Debug>(device))
            {
                ;
            }
        }
        return error_type();
    }
    return make_stdlib_error(std::errc::function_not_supported);
}
error_type render_factory_data::create_texture(ID3D11Device& device, const window_size size, render_texture& texture) const noexcept
{
    texture = render_texture();
    while (true)
    {
        texture.data = static_cast<render_texture::data_type*>(m_tex.pop());
        if (!texture.data)
            break;

        texture.data->ref.fetch_add(1, std::memory_order_release);
        if (texture.data->size.x == size.x && texture.data->size.y == size.y)
            break;

        texture.data->factory = nullptr;
        texture = render_texture();
    }
    if (!texture.data)
    {
        texture.data = allocator_type::allocate<render_texture::data_type>(1);
        if (!texture.data)
            return make_stdlib_error(std::errc::not_enough_memory);
        allocator_type::construct(texture.data);

        auto desc = CD3D11_TEXTURE2D_DESC(DXGI_FORMAT_R8G8B8A8_UNORM, size.x, size.y,
            1, 1, D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET);
        desc.MiscFlags = D3D11_RESOURCE_MISC_SHARED;

        com_ptr<ID3D11Texture2D> tex2d;
        ICY_COM_ERROR(device.CreateTexture2D(&desc, nullptr, &tex2d));
        ICY_ERROR(rename(*tex2d, "D3D11 Texture2D"_s));

        ICY_COM_ERROR(tex2d->QueryInterface(&texture.data->dxgi));
        texture.data->size = size;
        texture.data->factory = make_shared_from_this(this);
    }
    return error_type();
}

error_type D3D11_GRAPHICS_PIPELINE_STATE::Initialize(const D3D11_GRAPHICS_PIPELINE_STATE_DESC& desc, ID3D11Device& device, const string_view prefix) noexcept
{
    auto default_blend = CD3D11_BLEND_DESC(D3D11_DEFAULT);
    if (memcmp(&default_blend, &desc.BlendState, sizeof(D3D11_BLEND_DESC)) != 0)
    {
        ICY_COM_ERROR(device.CreateBlendState(&desc.BlendState, &BlendState));
        string str;
        ICY_ERROR(to_string("%1 Blend State"_s, str, prefix));
        ICY_ERROR(rename(*BlendState, str));
    }
    if (desc.DepthStencilState.DepthEnable)
    {
        ICY_COM_ERROR(device.CreateDepthStencilState(&desc.DepthStencilState, &DepthStencilState));
        string str;
        ICY_ERROR(to_string("%1 Depth Stencil State"_s, str, prefix));
        ICY_ERROR(rename(*DepthStencilState, str));
    }
    auto default_rasterizer = CD3D11_RASTERIZER_DESC(D3D11_DEFAULT);
    if (memcmp(&default_rasterizer, &desc.RasterizerState, sizeof(D3D11_RASTERIZER_DESC)) != 0)
    {
        ICY_COM_ERROR(device.CreateRasterizerState(&desc.RasterizerState, &RasterizerState));
        string str;
        ICY_ERROR(to_string("%1 Rasterizer State"_s, str, prefix));
        ICY_ERROR(rename(*RasterizerState, str));
    }
    {
        ICY_COM_ERROR(device.CreateVertexShader(desc.VS.data(), desc.VS.size(), nullptr, &VS));
        ICY_COM_ERROR(device.CreatePixelShader(desc.PS.data(), desc.PS.size(), nullptr, &PS));
        string str;
        ICY_ERROR(to_string("%1 Vertex Shader"_s, str, prefix));
        ICY_ERROR(rename(*VS, str));
        ICY_ERROR(to_string("%1 Pixel Shader"_s, str, prefix));
        ICY_ERROR(rename(*PS, str));
    }

    if (!desc.HS.empty() && !desc.DS.empty())
    {
        ICY_COM_ERROR(device.CreateHullShader(desc.HS.data(), desc.HS.size(), nullptr, &HS));
        ICY_COM_ERROR(device.CreateDomainShader(desc.DS.data(), desc.DS.size(), nullptr, &DS));

        string str;
        ICY_ERROR(to_string("%1 Hull Shader"_s, str, prefix));
        ICY_ERROR(rename(*HS, str));
        ICY_ERROR(to_string("%1 Domain Shader"_s, str, prefix));
        ICY_ERROR(rename(*DS, str));
    }
    if (!desc.InputLayout.empty())
    {
        ICY_COM_ERROR(device.CreateInputLayout(desc.InputLayout.data(), 
            uint32_t(desc.InputLayout.size()), desc.VS.data(), desc.VS.size(), &InputLayout));
        string str;
        ICY_ERROR(to_string("%1 Input Layout"_s, str, prefix));
        ICY_ERROR(rename(*InputLayout, str));
    }
    return error_type();
}
void D3D11_GRAPHICS_PIPELINE_STATE::Apply(ID3D11DeviceContext& context) noexcept
{
    context.ClearState();
    context.VSSetShader(VS, nullptr, 0);
    context.PSSetShader(PS, nullptr, 0);
    if (HS && DS)
    {
        context.HSSetShader(HS, nullptr, 0);
        context.DSSetShader(DS, nullptr, 0);
    }
    context.IASetPrimitiveTopology(PrimitiveTopology);
    if (InputLayout)
        context.IASetInputLayout(InputLayout);
    
    if (RasterizerState)
        context.RSSetState(RasterizerState);

    if (BlendState)
    {
        float blend[4] = { 0, 0, 0, 0 };
        context.OMSetBlendState(BlendState, blend, 0xFFFFFFFF);
    }
    if (DepthStencilState)
    {
        context.OMSetDepthStencilState(DepthStencilState, 1);
    }
}
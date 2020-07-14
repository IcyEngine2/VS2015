#include "icy_render.hpp"
#include "icy_shader.hpp"
#include <icy_engine/utility/icy_com.hpp>
#include <d3d11_4.h>
#include <dxgi1_6.h>

using namespace icy;

static const auto D3DDebugObjectNameW = _GUID{ 0x4cca5fd8,0x921f,0x42c8,0x85,0x66,0x70,0xca,0xf2,0xa9,0xb7,0x41 };
//= icy::guid(0x4cca5fd8921f42c8ui64, 0x856670caf2a9b741ui64);
;
static error_type rename(ID3D11DeviceChild& resource, const string_view name) noexcept
{
    com_ptr<ID3D11Device> device;
    resource.GetDevice(&device);
    if (device->GetCreationFlags() & D3D11_CREATE_DEVICE_DEBUG)
    {
        array<wchar_t> wname;
        ICY_ERROR(to_utf16(name, wname));
        ICY_COM_ERROR(resource.SetPrivateData(D3DDebugObjectNameW, uint32_t(wname.size()), wname.data()));
    }
    return error_type();
}

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
    HANDLE handle() const noexcept
    {
        return m_handle;
    }
private:
    com_ptr<ID3D11Texture2D> m_texture;
    com_ptr<ID3D11RenderTargetView> m_rtv;
    com_ptr<ID3D11ShaderResourceView> m_srv;
    com_ptr<IDXGIResource> m_dxgi;
    HANDLE m_handle = nullptr;
};
class d3d11_render_frame
{
public:
    error_type initialize(shared_ptr<d3d11_render_factory> system) noexcept;
    error_type exec(render_list& list) noexcept;
    void* handle() const noexcept
    {
        m_context->Flush();
        return m_msaa.texture.handle();
    }
    void destroy(const bool clear) noexcept;
private:
    struct svg_type
    {
        com_ptr<ID3D11VertexShader> vs;
        com_ptr<ID3D11PixelShader> ps;
        com_ptr<ID3D11InputLayout> layout;
        com_ptr<ID3D11RasterizerState> rasterizer;
        com_ptr<ID3D11Buffer> vertices;
        com_ptr<ID3D11Buffer> buffer;
        d3d11_texture texture;
    };
    struct msaa_type
    {
        com_ptr<ID3D11VertexShader> vs;
        com_ptr<ID3D11PixelShader> ps;
        d3d11_texture texture;
    };
private:
    void* _unused = nullptr;
    weak_ptr<d3d11_render_factory> m_system;
    com_ptr<ID3D11DeviceContext> m_context;
    svg_type m_svg;
    msaa_type m_msaa;
};
class d3d11_render_factory : public render_factory
{
public:
    ~d3d11_render_factory() noexcept;
    error_type initialize(const adapter& adapter, const render_flags flags) noexcept;
    error_type device(com_ptr<ID3D11Device>& device) const noexcept;
    render_flags flags() const noexcept override
    {
        return m_flags;
    }
    void push(d3d11_render_frame* frame) noexcept
    {
        m_frames.push(frame);
    }
    window_size size() const noexcept
    {
        return m_size;
    }
private:
    error_type exec(render_list& list, render_frame& frame) noexcept override;
    error_type resize(const window_size size) noexcept override
    {
        if (!size.x || !size.y)
            return make_stdlib_error(std::errc::invalid_argument);
        m_size = size;
        return error_type();
    }
private:
    render_flags m_flags = render_flags::none;
    adapter m_adapter;
    library m_lib = "d3d11"_lib;
    window_size m_size;
    detail::intrusive_mpsc_queue m_frames;
};
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
    error_type repaint(IDXGISwapChain& chain, const bool vsync, void* const handle) noexcept override;
private:
    d3d11_render_factory m_render;
    com_ptr<ID3D11Device> m_device;
    com_ptr<ID3D11VertexShader> m_vs;
    com_ptr<ID3D11PixelShader> m_ps;
    com_ptr<ID3D11SamplerState> m_sampler;
    com_ptr<ID3D11Texture2D> m_buffer;
    com_ptr<ID3D11RenderTargetView> m_view;
    window_size m_size;
};

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
        m_dxgi = nullptr;
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
        if (new_desc.MiscFlags & D3D11_RESOURCE_MISC_SHARED)
        {
            ICY_COM_ERROR(m_texture->QueryInterface(&m_dxgi));
            ICY_COM_ERROR(m_dxgi->GetSharedHandle(&m_handle));
        }
    }
    return error_type();
}

error_type d3d11_render_frame::initialize(shared_ptr<d3d11_render_factory> system) noexcept
{
    m_system = system;
    com_ptr<ID3D11Device> device;
    ICY_ERROR(system->device(device));
    device->GetImmediateContext(&m_context);
    ICY_ERROR(rename(*m_context, "D3D11 Frame Context"_s));

    ICY_COM_ERROR(device->CreateVertexShader(g_shader_bytes_svg_vs, _countof(g_shader_bytes_svg_vs), nullptr, &m_svg.vs));
    ICY_COM_ERROR(device->CreatePixelShader(g_shader_bytes_svg_ps, _countof(g_shader_bytes_svg_ps), nullptr, &m_svg.ps));
    D3D11_INPUT_ELEMENT_DESC layout[2] =
    {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "COLOR", 0, DXGI_FORMAT_R8G8B8A8_UNORM, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
    };
    ICY_COM_ERROR(device->CreateInputLayout(layout, _countof(layout), g_shader_bytes_svg_vs, _countof(g_shader_bytes_svg_vs), &m_svg.layout));

    if (sample_count(system->flags()) > 1)
    {
        auto rasterizer = CD3D11_RASTERIZER_DESC(D3D11_DEFAULT);
        rasterizer.MultisampleEnable = TRUE;
        rasterizer.AntialiasedLineEnable = TRUE;
        ICY_COM_ERROR(device->CreateRasterizerState(&rasterizer, &m_svg.rasterizer));
    }

    if (sample_count(system->flags()) > 1 && !(system->flags() & render_flags::msaa_hardware))
    {
        const auto count = sample_count(system->flags());
        ICY_COM_ERROR(device->CreateVertexShader(g_shader_bytes_screen_vs, 
            _countof(g_shader_bytes_screen_vs), nullptr, &m_msaa.vs));
        switch (count)
        {
        case 2:
            ICY_COM_ERROR(device->CreatePixelShader(g_shader_bytes_screen_ps2, 
                _countof(g_shader_bytes_screen_ps2), nullptr, &m_msaa.ps));
            break;
        case 4:
            ICY_COM_ERROR(device->CreatePixelShader(g_shader_bytes_screen_ps4, 
                _countof(g_shader_bytes_screen_ps4), nullptr, &m_msaa.ps));
            break;
        case 8:
            ICY_COM_ERROR(device->CreatePixelShader(g_shader_bytes_screen_ps8, 
                _countof(g_shader_bytes_screen_ps8), nullptr, &m_msaa.ps));
            break;
        case 16:
            ICY_COM_ERROR(device->CreatePixelShader(g_shader_bytes_screen_ps16,
                _countof(g_shader_bytes_screen_ps16), nullptr, &m_msaa.ps));
            break;
        default:
            return make_stdlib_error(std::errc::invalid_argument);
        }
        ICY_ERROR(rename(*m_msaa.vs, "D3D11 MSAA Vertex Shader"_s));
        ICY_ERROR(rename(*m_msaa.ps, "D3D11 MSAA Pixel Shader"_s));
    }

    return error_type();
}
error_type d3d11_render_frame::exec(render_list& list) noexcept
{
    auto system = shared_ptr<d3d11_render_factory>(m_system);
    if (!system)
        return make_stdlib_error(std::errc::invalid_argument);

    com_ptr<ID3D11Device> device;
    m_context->GetDevice(&device);
    
    const auto size = system->size();
    const auto flags = system->flags();
    const auto flags_msaa = render_flags(uint32_t(flags) & uint32_t(
        render_flags::msaa_x2 | 
        render_flags::msaa_x4 |
        render_flags::msaa_x8 |
        render_flags::msaa_x16));

    auto svg_tex_desc = CD3D11_TEXTURE2D_DESC(DXGI_FORMAT_R8G8B8A8_UNORM, size.x, size.y,
        1, 1, D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET);

    auto msaa_tex_desc = CD3D11_TEXTURE2D_DESC(DXGI_FORMAT_R8G8B8A8_UNORM, size.x, size.y, 
        1, 1, D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET, D3D11_USAGE_DEFAULT, 0, 1, 0, D3D11_RESOURCE_MISC_SHARED);

    ICY_ERROR(m_svg.texture.resize(*device, svg_tex_desc, flags_msaa));
    ICY_ERROR(m_msaa.texture.resize(*device, msaa_tex_desc, render_flags::none));

    array<render_d2d_vertex> d2d_vertices;
    color clear = colors::black;
    while (true)
    {
        render_element elem;
        if (!static_cast<render_list_data&>(list).queue.pop(elem))
            break;

        switch (elem.type)
        {
        case render_element_type::clear:
        {
            clear = elem.clear;
            break;
        }
        case render_element_type::svg:
        {
            ICY_ERROR(d2d_vertices.append(elem.svg.begin(), elem.svg.end()));
            break;
        }
        }
    }

    const auto make_buffer = [system, this](ID3D11Device& device, com_ptr<ID3D11Buffer>& gpu,
        const void* data, const size_t capacity, D3D11_BIND_FLAG flags, const string_view name)
    {
        D3D11_BUFFER_DESC gpu_desc = {};
        if (gpu) gpu->GetDesc(&gpu_desc);
        if (capacity > gpu_desc.ByteWidth)
        {
            gpu_desc.ByteWidth = align_up(uint32_t(capacity), 256);
            gpu_desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
            gpu_desc.BindFlags = flags;
            gpu_desc.Usage = D3D11_USAGE_DYNAMIC;
            com_ptr<ID3D11Buffer> new_gpu;
            ICY_COM_ERROR(device.CreateBuffer(&gpu_desc, nullptr, &new_gpu));
            ICY_ERROR(rename(*new_gpu, name));
            gpu = new_gpu;
        }
        D3D11_MAPPED_SUBRESOURCE map;
        ICY_COM_ERROR(m_context->Map(gpu, 0, D3D11_MAP::D3D11_MAP_WRITE_DISCARD, 0, &map));
        memcpy(map.pData, data, capacity);
        m_context->Unmap(gpu, 0);
        return error_type();
    };

    float rgba[4];
    clear.to_rgbaf(rgba);
    m_context->ClearRenderTargetView(m_svg.texture.rtv(), rgba);
    const auto viewport = D3D11_VIEWPORT{ 0, 0, float(size.x), float(size.y), 0, 1 };

    if (!d2d_vertices.empty())
    {
        ICY_ERROR(make_buffer(*device, m_svg.vertices, d2d_vertices.data(),
            d2d_vertices.size() * sizeof(render_d2d_vertex), D3D11_BIND_VERTEX_BUFFER,
            "D3D11 Frame SVG Vertex Buffer"_s));

        float gsize[2] = { viewport.Width, viewport.Height };
        ICY_ERROR(make_buffer(*device, m_svg.buffer, gsize, 
            sizeof(gsize), D3D11_BIND_CONSTANT_BUFFER, "D3D11 Frame SVG Constants"_s));

        const uint32_t strides[] = { sizeof(render_d2d_vertex) };
        const uint32_t offsets[] = { 0 };

        m_context->VSSetShader(m_svg.vs, nullptr, 0);
        m_context->VSSetConstantBuffers(0, 1, &m_svg.buffer);

        m_context->PSSetShader(m_svg.ps, nullptr, 0);
        
        m_context->IASetInputLayout(m_svg.layout);
        m_context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        m_context->IASetVertexBuffers(0, 1, &m_svg.vertices, strides, offsets);

        m_context->RSSetState(m_svg.rasterizer);
        m_context->RSSetViewports(1, &viewport);

        m_context->OMSetRenderTargets(1, &m_svg.texture.rtv(), nullptr);
        m_context->Draw(uint32_t(d2d_vertices.size()), 0);
    }

    if (sample_count(flags) == 1)
    {
        m_context->CopyResource(m_msaa.texture.get(), m_svg.texture.get());
    }
    else if (flags & render_flags::msaa_hardware)
    {
        m_context->ResolveSubresource(m_msaa.texture.get(), 0, m_svg.texture.get(), 0, DXGI_FORMAT_R8G8B8A8_UNORM);
    }
    else //if (!(flags & render_flags::msaa_hardware))
    {
        m_context->ClearState();
        m_context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        m_context->VSSetShader(m_msaa.vs, nullptr, 0);
        m_context->PSSetShader(m_msaa.ps, nullptr, 0);
        m_context->PSSetShaderResources(0, 1, &m_svg.texture.srv());
        m_context->RSSetViewports(1, &viewport);
        m_context->OMSetRenderTargets(1, &m_msaa.texture.rtv(), nullptr);
        m_context->Draw(3, 0);
    }
    return error_type();
}
void d3d11_render_frame::destroy(const bool clear) noexcept
{
    auto system = shared_ptr<d3d11_render_factory>(m_system);
    if (system && !clear)
    {
        system->push(this);
    }
    else
    {
        allocator_type::destroy(this);
        allocator_type::deallocate(this);
    }
}

d3d11_render_factory::~d3d11_render_factory() noexcept
{
    while (auto frame = static_cast<d3d11_render_frame*>(m_frames.pop()))
        frame->destroy(true);
}
error_type d3d11_render_factory::initialize(const adapter& adapter, const render_flags flags) noexcept
{
    m_flags = flags;
    m_adapter = adapter;
    ICY_ERROR(m_lib.initialize());
    ICY_ERROR(resize(window_size(1424, 720)));
    return error_type();
}
error_type d3d11_render_factory::device(com_ptr<ID3D11Device>& device) const noexcept
{
    if (const auto func = ICY_FIND_FUNC(m_lib, D3D11CreateDevice))
    {
        auto level = m_adapter.data->flags() & adapter_flags::d3d10 ? D3D_FEATURE_LEVEL_10_1 : D3D_FEATURE_LEVEL_11_0;
        auto flags = m_adapter.data->flags() & adapter_flags::debug ? D3D11_CREATE_DEVICE_DEBUG : 0;
        ICY_COM_ERROR(func(m_adapter.data->adapter(), D3D_DRIVER_TYPE_UNKNOWN, nullptr, 
            flags, &level, 1, D3D11_SDK_VERSION, &device, nullptr, nullptr));
    }
    else
    {
        return make_stdlib_error(std::errc::function_not_supported);
    }

    if (device->GetCreationFlags() & D3D11_CREATE_DEVICE_FLAG::D3D11_CREATE_DEVICE_DEBUG)
    {
        if (auto debug = com_ptr<ID3D11Debug>(device))
        {
            ;
        }
    }
    return error_type();
}
error_type d3d11_render_factory::exec(render_list& list, render_frame& frame) noexcept
{
    if (frame.data)
        return make_stdlib_error(std::errc::invalid_argument);

    frame.data = allocator_type::allocate<render_frame::data_type>(1);
    if (!frame.data)
        return make_stdlib_error(std::errc::not_enough_memory);
    allocator_type::construct(frame.data);

    frame.data->d3d11 = allocator_type::allocate<d3d11_render_frame>(1);
    if (!frame.data->d3d11)
        return make_stdlib_error(std::errc::not_enough_memory);
    allocator_type::construct(frame.data->d3d11);

    if (auto ptr = static_cast<d3d11_render_frame*>(m_frames.pop()))
    {
        *frame.data->d3d11 = std::move(*ptr);
    }
    else
    {
        ICY_ERROR(frame.data->d3d11->initialize(make_shared_from_this(this)));
    }
    ICY_ERROR(frame.data->d3d11->exec(list));
    return error_type();
}

error_type d3d11_display_system::initialize(const adapter& adapter, HWND__* const window, const display_flags flags) noexcept
{
    com_ptr<IDXGIFactory> factory;
    ICY_ERROR(m_render.initialize(adapter, render_flags::none));
    ICY_ERROR(m_render.device(m_device));
    ICY_COM_ERROR(adapter.data->adapter()->GetParent(IID_PPV_ARGS(&factory)));
    ICY_ERROR(display_data::initialize(*m_device, *factory, window, flags));
    ICY_COM_ERROR(factory->MakeWindowAssociation(window, 
        DXGI_MWA_NO_ALT_ENTER | DXGI_MWA_NO_WINDOW_CHANGES | DXGI_MWA_NO_PRINT_SCREEN));

    ICY_COM_ERROR(m_device->CreateVertexShader(g_shader_bytes_screen_vs,
        _countof(g_shader_bytes_screen_vs), nullptr, &m_vs));
    ICY_COM_ERROR(m_device->CreatePixelShader(g_shader_bytes_screen_ps,
        _countof(g_shader_bytes_screen_ps), nullptr, &m_ps));
    ICY_COM_ERROR(m_device->CreateSamplerState(&CD3D11_SAMPLER_DESC(D3D11_DEFAULT), &m_sampler));

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
    ICY_COM_ERROR(m_device->CreateRenderTargetView(m_buffer, 
        &CD3D11_RENDER_TARGET_VIEW_DESC(D3D11_RTV_DIMENSION_TEXTURE2D, flags() & display_flags::sRGB ? 
            DXGI_FORMAT_R8G8B8A8_UNORM_SRGB : DXGI_FORMAT_R8G8B8A8_UNORM), &m_view));
    ICY_ERROR(rename(*m_buffer, "D3D11 Display Back Buffer Texture"));
    ICY_ERROR(rename(*m_view, "D3D11 Display Back Buffer RTV"));
    m_size = size;
    return error_type();
}
error_type d3d11_display_system::repaint(IDXGISwapChain& chain, const bool vsync, void* const handle) noexcept
{
    com_ptr<ID3D11Texture2D> texture;
    ICY_COM_ERROR(m_device->OpenSharedResource(handle, IID_PPV_ARGS(&texture)));

    auto srv_desc = CD3D11_SHADER_RESOURCE_VIEW_DESC(D3D11_SRV_DIMENSION_TEXTURE2D,
        DXGI_FORMAT_R8G8B8A8_UNORM, 0, 1);
    com_ptr<ID3D11ShaderResourceView> srv;
    ICY_COM_ERROR(m_device->CreateShaderResourceView(texture, &srv_desc, &srv));

    const auto viewport = D3D11_VIEWPORT{ 0, 0, float(m_size.x), float(m_size.y), 0, 1 };
    com_ptr<ID3D11DeviceContext> context;
    m_device->GetImmediateContext(&context);

    context->VSSetShader(m_vs, nullptr, 0);
    context->PSSetShader(m_ps, nullptr, 0);
    context->PSSetSamplers(0, 1, &m_sampler);
    context->PSSetShaderResources(0, 1, &srv);
    context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    context->RSSetViewports(1, &viewport);
    context->OMSetRenderTargets(1, &m_view, nullptr);
    context->Draw(3, 0);

    if (const auto error = chain.Present(vsync, 0))
    {
        if (error == DXGI_STATUS_OCCLUDED)
            ;
        else
            return make_system_error(error);
    }
    return error_type();
}

error_type create_d3d11(shared_ptr<render_factory>& system, const adapter& adapter, const render_flags flags) noexcept
{
    shared_ptr<d3d11_render_factory> new_ptr;
    ICY_ERROR(make_shared(new_ptr));
    ICY_ERROR(new_ptr->initialize(adapter, flags));
    system = std::move(new_ptr);
    return error_type();
}
error_type create_d3d11(d3d11_render_frame*& frame, shared_ptr<d3d11_render_factory> system)
{
    auto new_frame = allocator_type::allocate<d3d11_render_frame>(1);
    if (!new_frame)
        return make_stdlib_error(std::errc::not_enough_memory);
    allocator_type::construct(new_frame);

    if (const auto error = new_frame->initialize(system))
    {
        allocator_type::destroy(new_frame);
        allocator_type::deallocate(new_frame);
        return error;
    }

    if (frame) frame->destroy(true);
    frame = new_frame;
    return error_type();
}
error_type create_d3d11(shared_ptr<display_system>& system, const adapter& adapter, HWND__* const hwnd, const display_flags flags) noexcept
{
    shared_ptr<d3d11_display_system> new_ptr;
    ICY_ERROR(make_shared(new_ptr));
    ICY_ERROR(new_ptr->initialize(adapter, hwnd, flags));
    system = std::move(new_ptr);
    return error_type();
}
error_type wait_frame(d3d11_render_frame& frame, void*& handle) noexcept
{
    handle = frame.handle();
    return error_type();
}
void destroy_frame(d3d11_render_frame& frame) noexcept
{
    frame.destroy(false);
}


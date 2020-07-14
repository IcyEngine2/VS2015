#include "icy_render.hpp"
#include "icy_shader.hpp"
#include <icy_engine/utility/icy_com.hpp>
#include <d3d10_1.h>

using namespace icy;

class d3d10_render_frame
{
public:
    icy::error_type initialize(icy::shared_ptr<d3d10_render_factory> system) noexcept;
    icy::error_type exec(icy::render_list& list) noexcept;
    icy::render_flags flags() const noexcept;
    void destroy(const bool clear) noexcept;
    window_size size() const noexcept;
private:
    void* _unused = nullptr;
    icy::weak_ptr<d3d10_render_factory> m_system;
};
class d3d10_render_factory : public render_factory
{
    friend d3d10_render_frame;
public:
    ~d3d10_render_factory() noexcept;
    icy::error_type initialize(const icy::adapter::data_type& adapter, const icy::render_flags flags) noexcept;
private:
    icy::error_type exec(icy::render_list& list, icy::render_frame& frame) noexcept override;
    icy::render_flags flags() const noexcept override
    {
        return m_flags;
    }
    icy::error_type resize(const icy::window_size size) noexcept override
    {
        if (!size.x || !size.y)
            return icy::make_stdlib_error(std::errc::invalid_argument);
        m_size = size;
        return icy::error_type();
    }
private:
    icy::render_flags m_flags = icy::render_flags::none;
    icy::shared_ptr<d3d10_system> m_global;
    icy::window_size m_size;
    icy::detail::intrusive_mpsc_queue m_frames;
};
class d3d10_display_system : public display_data
{
public:
    d3d10_display_system() noexcept : display_data(adapter_flags::d3d10)
    {

    }
    ~d3d10_display_system() noexcept
    {
        filter(0);
    }
    error_type initialize(const adapter::data_type& adapter, HWND__* const window, const display_flags flags) noexcept;
private:
    error_type repaint(IDXGISwapChain& chain, const bool vsync, render_frame& frame) noexcept override;
private:
    shared_ptr<d3d10_system> m_global;
    window_size m_size;
};

error_type d3d10_system::initialize(const adapter::data_type& adapter) noexcept
{
    ICY_ERROR(m_lib.initialize());
    if (const auto func = ICY_FIND_FUNC(m_lib, D3D10CreateDevice1))
    {
        ICY_COM_ERROR(func(&adapter.adapter(), adapter.flags() & adapter_flags::hardware ?
            D3D10_DRIVER_TYPE_HARDWARE : D3D10_DRIVER_TYPE_REFERENCE, nullptr,
            (adapter.flags() & adapter_flags::d3d10 ? D3D10_CREATE_DEVICE_DEBUG : 0) |
            D3D10_CREATE_DEVICE_BGRA_SUPPORT, D3D10_FEATURE_LEVEL_10_1, D3D10_1_SDK_VERSION, &m_device));
        if (adapter.flags() & adapter_flags::debug)
        {
            const wchar_t name[] = L"D3D10 Device";
            ICY_COM_ERROR(m_device->SetPrivateData(reinterpret_cast<const GUID&>(D3DNameGuid), 
                uint32_t(wcslen(name)), name));
        }
    }
    else
    {
        return make_stdlib_error(std::errc::function_not_supported);
    }

    if (m_device->GetCreationFlags() & D3D10_CREATE_DEVICE_DEBUG)
    {        
        if (auto debug = com_ptr<ID3D10Debug>(m_device))
        {
            ;
        }
    }
    return error_type();
}
error_type d3d10_system::rename(ID3D10DeviceChild& resource, const string_view name) const noexcept
{
    if (m_device->GetCreationFlags() & D3D10_CREATE_DEVICE_DEBUG)
    {
        array<wchar_t> wname;
        ICY_ERROR(to_utf16(name, wname));
        ICY_COM_ERROR(resource.SetPrivateData(reinterpret_cast<const GUID&>(D3DNameGuid),
            uint32_t(wname.size()), wname.data()));
    }
    return error_type();
}

error_type d3d10_render_frame::initialize(icy::shared_ptr<d3d10_render_factory> system) noexcept
{
    m_system = system;
    //ICY_COM_ERROR(system->m_global->device().CreateCreateDeferredContext(0, &m_context));
    //ICY_ERROR(system->m_global->rename(*m_context, "D3D11 Frame Deferred Context"_s));
    return error_type();
}
error_type d3d10_render_frame::exec(render_list& list) noexcept
{
    auto system = shared_ptr<d3d10_render_factory>(m_system);
    if (!system)
        return make_stdlib_error(std::errc::invalid_argument);
    /*
    D3D11_TEXTURE2D_DESC desc;
    if (m_texture)
        m_texture->GetDesc(&desc);
    else
        desc = {};

    if (system->m_size.x != desc.Width || system->m_size.y != desc.Height)
    {
        const auto is_msaa = sample_count(system->m_flags) > 1;
        auto tex_desc = CD3D11_TEXTURE2D_DESC(DXGI_FORMAT_R8G8B8A8_TYPELESS,
            system->m_size.x, system->m_size.y, 1, 1,
            D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE);

        auto rtv_desc = CD3D11_RENDER_TARGET_VIEW_DESC(is_msaa ? D3D11_RTV_DIMENSION_TEXTURE2DMS : D3D11_RTV_DIMENSION_TEXTURE2D, DXGI_FORMAT_R8G8B8A8_UNORM);
        auto srv_desc = CD3D11_SHADER_RESOURCE_VIEW_DESC(is_msaa ? D3D11_SRV_DIMENSION_TEXTURE2DMS : D3D11_SRV_DIMENSION_TEXTURE2D, DXGI_FORMAT_R8G8B8A8_UNORM);
        tex_desc.SampleDesc.Count = sample_count(system->m_flags);
        if (is_msaa)
        {
            auto quality = 0u;
            ICY_COM_ERROR(system->m_global->device().CheckMultisampleQualityLevels(DXGI_FORMAT_R8G8B8A8_UNORM, sample_count(system->m_flags), &quality));
            tex_desc.SampleDesc.Quality = quality ? D3D11_STANDARD_MULTISAMPLE_PATTERN : 0;
        }

        com_ptr<ID3D11Texture2D> texture;
        com_ptr<ID3D11RenderTargetView> rtv;
        com_ptr<ID3D11ShaderResourceView> srv;
        ICY_COM_ERROR(system->m_global->device().CreateTexture2D(&tex_desc, nullptr, &texture));
        ICY_COM_ERROR(system->m_global->device().CreateRenderTargetView(texture, &rtv_desc, &rtv));
        ICY_COM_ERROR(system->m_global->device().CreateShaderResourceView(texture, &srv_desc, &srv));
        m_texture = texture;
        m_rtv = rtv;
        m_srv = srv;
        ICY_ERROR(system->m_global->rename(*texture, "D3D11 Frame Texture"_s));
        ICY_ERROR(system->m_global->rename(*rtv, "D3D11 Frame RTV"_s));
        ICY_ERROR(system->m_global->rename(*srv, "D3D11 Frame SRV"_s));
    }

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
            ICY_ERROR(system->m_global->rename(*new_gpu, name));
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
    m_context->ClearRenderTargetView(m_rtv, rgba);

    if (!d2d_vertices.empty())
    {
        ICY_ERROR(make_buffer(system->m_global->device(), m_svg_vertices, d2d_vertices.data(),
            d2d_vertices.size() * sizeof(render_d2d_vertex), D3D11_BIND_VERTEX_BUFFER, "D3D11 Frame SVG Vertex Buffer"_s));

        float gsize[2] = { float(system->m_size.x), float(system->m_size.y) };
        ICY_ERROR(make_buffer(system->m_global->device(), m_svg_buffer, gsize,
            sizeof(gsize), D3D11_BIND_CONSTANT_BUFFER, "D3D11 Frame SVG Constants"_s));

        system->m_global->svg()(*m_context);

        const auto view = D3D11_VIEWPORT{ 0, 0, gsize[0], gsize[1], 0, 1 };
        const uint32_t strides[] = { sizeof(render_d2d_vertex) };
        const uint32_t offsets[] = { 0 };
        m_context->IASetVertexBuffers(0, 1, &m_svg_vertices, strides, offsets);
        m_context->RSSetViewports(1, &view);
        m_context->VSSetConstantBuffers(0, 1, &m_svg_buffer);
        m_context->OMSetRenderTargets(1, &m_rtv, nullptr);
        m_context->Draw(uint32_t(d2d_vertices.size()), 0);
    }
    m_commands = nullptr;
    ICY_COM_ERROR(m_context->FinishCommandList(TRUE, &m_commands));
    ICY_ERROR(system->m_global->rename(*m_commands, "D3D11 Frame CommandList"));*/
    return error_type();
}
render_flags d3d10_render_frame::flags() const noexcept
{
    if (auto system = shared_ptr<d3d10_render_factory>(m_system))
        return system->flags();
    return render_flags::none;
}
void d3d10_render_frame::destroy(const bool clear) noexcept
{
    auto system = shared_ptr<d3d10_render_factory>(m_system);
    if (system && !clear)
    {
        system->m_frames.push(this);
    }
    else
    {
        allocator_type::destroy(this);
        allocator_type::deallocate(this);
    }
}

d3d10_render_factory::~d3d10_render_factory() noexcept
{
    while (auto frame = static_cast<d3d10_render_frame*>(m_frames.pop()))
        frame->destroy(true);
}
error_type d3d10_render_factory::initialize(const adapter::data_type& adapter, const render_flags flags) noexcept
{
    m_flags = flags;
    //ICY_ERROR(adapter.query_d3d10(m_global));
    //ICY_ERROR(resize(window_size(1424, 720)));
    return error_type();
}
error_type d3d10_render_factory::exec(render_list& list, render_frame& frame) noexcept
{
    if (frame.data)
        return make_stdlib_error(std::errc::invalid_argument);

    frame.data = allocator_type::allocate<render_frame::data_type>(1);
    if (!frame.data)
        return make_stdlib_error(std::errc::not_enough_memory);
    allocator_type::construct(frame.data);

    frame.data->d3d10 = allocator_type::allocate<d3d10_render_frame>(1);
    if (!frame.data->d3d10)
        return make_stdlib_error(std::errc::not_enough_memory);
    allocator_type::construct(frame.data->d3d10);

    if (auto ptr = static_cast<d3d10_render_frame*>(m_frames.pop()))
    {
        *frame.data->d3d10 = std::move(*ptr);
    }
    else
    {
        ICY_ERROR(frame.data->d3d10->initialize(make_shared_from_this(this)));
    }
    ICY_ERROR(frame.data->d3d10->exec(list));
    return error_type();
}

error_type d3d10_display_system::initialize(const adapter::data_type& adapter, HWND__* const window, const display_flags flags) noexcept
{
    /*ICY_ERROR(adapter.query_d3d11(m_global));
    com_ptr<IDXGIFactory> factory;
    ICY_COM_ERROR(adapter.adapter().GetParent(IID_PPV_ARGS(&factory)));
    ICY_ERROR(display_data::initialize(m_global->device(), *factory, window, flags));
    ICY_COM_ERROR(factory->MakeWindowAssociation(window, DXGI_MWA_NO_ALT_ENTER | DXGI_MWA_NO_WINDOW_CHANGES | DXGI_MWA_NO_PRINT_SCREEN));

    ICY_COM_ERROR(m_global->device().CreateVertexShader(g_shader_bytes_screen_vs, _countof(g_shader_bytes_screen_vs), nullptr, &m_vs));
    ICY_COM_ERROR(m_global->device().CreatePixelShader(g_shader_bytes_screen_ps1, _countof(g_shader_bytes_screen_ps1), nullptr, &m_ps1));
    ICY_COM_ERROR(m_global->device().CreatePixelShader(g_shader_bytes_screen_ps2, _countof(g_shader_bytes_screen_ps2), nullptr, &m_ps2));
    ICY_COM_ERROR(m_global->device().CreatePixelShader(g_shader_bytes_screen_ps4, _countof(g_shader_bytes_screen_ps4), nullptr, &m_ps4));
    ICY_COM_ERROR(m_global->device().CreatePixelShader(g_shader_bytes_screen_ps8, _countof(g_shader_bytes_screen_ps8), nullptr, &m_ps8));
    ICY_COM_ERROR(m_global->device().CreatePixelShader(g_shader_bytes_screen_ps16, _countof(g_shader_bytes_screen_ps16), nullptr, &m_ps16));

    ICY_ERROR(m_global->rename(*m_vs, "D3D11 Display Sampler"));
    ICY_ERROR(m_global->rename(*m_ps1, "D3D11 Display Pixel Shader"));
    ICY_ERROR(m_global->rename(*m_ps2, "D3D11 Display MSAA[2] Pixel Shader"));
    ICY_ERROR(m_global->rename(*m_ps4, "D3D11 Display MSAA[4] Pixel Shader"));
    ICY_ERROR(m_global->rename(*m_ps8, "D3D11 Display MSAA[8] Pixel Shader"));
    ICY_ERROR(m_global->rename(*m_ps16, "D3D11 Display MSAA[16] Pixel Shader"));

    auto sampler = CD3D11_SAMPLER_DESC(D3D11_DEFAULT);
    ICY_COM_ERROR(m_global->device().CreateSamplerState(&sampler, &m_sampler));
    ICY_ERROR(m_global->rename(*m_sampler, "D3D11 Display Sampler"));
    */
    filter(event_type::system_internal);
    return error_type();
}
/*error_type d3d10_display_system::resize(IDXGISwapChain& chain, const window_size size, const display_flags flags) noexcept
{
    DXGI_SWAP_CHAIN_DESC desc;
    ICY_COM_ERROR(chain.GetDesc(&desc));
    m_view = nullptr;
    ICY_COM_ERROR(chain.ResizeBuffers(0, 0, 0, DXGI_FORMAT_UNKNOWN, desc.Flags));
    com_ptr<ID3D11Resource> texture;
    ICY_COM_ERROR(chain.GetBuffer(0, IID_PPV_ARGS(&texture)));
    ICY_COM_ERROR(m_global->device().CreateRenderTargetView(texture, nullptr, &m_view));
    ICY_ERROR(m_global->rename(*m_view, "D3D11 Display RTV"));

    m_size = size;
    return error_type();
}*/
error_type d3d10_display_system::repaint(IDXGISwapChain& chain, const bool vsync, render_frame& frame) noexcept
{
    const auto now = clock_type::now();
    /*
    com_ptr<ID3D11DeviceContext> context;
    m_global->device().GetImmediateContext(&context);

    com_ptr<ID3D11CommandList> commands;
    com_ptr<ID3D11ShaderResourceView> srv;
    frame.data->d3d11->wait(commands, srv);

    context->ExecuteCommandList(commands, TRUE);

    const auto box = D3D11_VIEWPORT{ 0, 0, float(m_size.x), float(m_size.y), 0, 1 };
    const auto flags = frame.data->d3d11->flags();

    ID3D11PixelShader* ps = nullptr;
    if (flags & render_flags::msaa_x16)
        ps = m_ps16;
    else if (flags & render_flags::msaa_x8)
        ps = m_ps8;
    else if (flags & render_flags::msaa_x4)
        ps = m_ps4;
    else if (flags & render_flags::msaa_x2)
        ps = m_ps2;
    else
        ps = m_ps1;

    context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    context->VSSetShader(m_vs, nullptr, 0);
    context->PSSetShader(ps, nullptr, 0);
    context->PSSetShaderResources(0, 1, &srv);
    context->RSSetViewports(1, &box);
    context->OMSetRenderTargets(1, &m_view, nullptr);
    if (ps == m_ps1)
        context->PSSetSamplers(0, 1, &m_sampler);
    context->Draw(3, 0);*/
    if (const auto error = chain.Present(vsync, 0))
    {
        if (error == DXGI_STATUS_OCCLUDED)
            ;
        else
            return make_system_error(error);
    }
    frame.data->time_cpu = clock_type::now() - now;
    return error_type();
}

error_type create_d3d10(shared_ptr<render_factory>& system, const adapter::data_type& adapter, const render_flags flags) noexcept
{
    shared_ptr<d3d10_render_factory> new_ptr;
    ICY_ERROR(make_shared(new_ptr));
    ICY_ERROR(new_ptr->initialize(adapter, flags));
    system = std::move(new_ptr);
    return error_type();
}
icy::error_type create_d3d10(d3d10_render_frame*& frame, shared_ptr<d3d10_render_factory> system)
{
    auto new_frame = allocator_type::allocate<d3d10_render_frame>(1);
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
error_type create_d3d10(shared_ptr<display_system>& system, const adapter::data_type& adapter, HWND__* const hwnd, const display_flags flags) noexcept
{
    shared_ptr<d3d10_display_system> new_ptr;
    ICY_ERROR(make_shared(new_ptr));
    ICY_ERROR(new_ptr->initialize(adapter, hwnd, flags));
    system = std::move(new_ptr);
    return error_type();
}
void destroy_frame(d3d10_render_frame& frame) noexcept
{
    frame.destroy(false);
}


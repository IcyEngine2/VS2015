#include "icy_d3d11.hpp"
#include "icy_window.hpp"
#include <d3d11.h>
#include "shaders/svg_ps.hpp"
#include "shaders/svg_vs.hpp"

using namespace icy;

error_type d3d11_swap_chain::window(HWND__*& hwnd) const noexcept
{
    DXGI_SWAP_CHAIN_DESC desc = {};
    ICY_COM_ERROR(m_chain->GetDesc(&desc));
    hwnd = desc.OutputWindow;
    return error_type();
}
error_type d3d11_swap_chain::present(const uint32_t vsync) noexcept
{
    ICY_COM_ERROR(m_chain->Present(vsync, 0));
    return error_type();
}
error_type d3d11_swap_chain::size(window_size& output) const noexcept
{
    DXGI_SWAP_CHAIN_DESC desc = {};
    ICY_COM_ERROR(m_chain->GetDesc(&desc));
    output = { desc.BufferDesc.Width, desc.BufferDesc.Height };
    return error_type();
}
error_type d3d11_swap_chain::initialize(IDXGIFactory& factory, ID3D11Device& device, HWND__* const hwnd, const window_flags flags) noexcept
{
    DXGI_SWAP_CHAIN_DESC desc = {};
    desc.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    desc.SampleDesc = { 1, 0 };
    desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    desc.BufferCount = 2;
    desc.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;
    desc.Windowed = TRUE;
    desc.OutputWindow = hwnd;
    com_ptr<IDXGISwapChain> chain;
    ICY_COM_ERROR(factory.CreateSwapChain(&device, &desc, &chain));
    ICY_COM_ERROR(chain->QueryInterface(&m_chain));

    com_ptr<ID3D11Resource> texture;
    ICY_COM_ERROR(m_chain->GetBuffer(0, IID_PPV_ARGS(&texture)));
    ICY_COM_ERROR(device.CreateRenderTargetView(texture, nullptr, &m_view));
    return error_type();
}
error_type d3d11_swap_chain::resize() noexcept
{
    m_view = nullptr;

    com_ptr<ID3D11Device> device;
    ICY_COM_ERROR(m_chain->GetDevice(IID_PPV_ARGS(&device)));
    ICY_COM_ERROR(m_chain->ResizeBuffers(0, 0, 0, DXGI_FORMAT_UNKNOWN, 0));

    com_ptr<ID3D11Resource> texture;
    ICY_COM_ERROR(m_chain->GetBuffer(0, IID_PPV_ARGS(&texture)));
    ICY_COM_ERROR(device->CreateRenderTargetView(texture, nullptr, &m_view));

    return error_type();
}

/*d3d11_display::~d3d11_display() noexcept
{
    filter(0);
    if (m_proc)
    {
        HWND hwnd = nullptr;
        m_chain.window(hwnd);
        if (hwnd)
        {
            SetWindowLongPtrW(hwnd, GWLP_WNDPROC, LONG_PTR(m_proc));
            SetWindowLongPtrW(hwnd, GWLP_USERDATA, LONG_PTR(m_user));
        }
    }
}*/
d3d11_display::~d3d11_display() noexcept
{
    
}
error_type d3d11_display::initialize(const adapter& adapter, const window_flags flags) noexcept
{
    m_adapter = adapter;
    m_flags = flags;
    ICY_ERROR(m_d3d11_lib.initialize());

    if (const auto func = ICY_FIND_FUNC(m_d3d11_lib, D3D11CreateDevice))
    {
        auto level = D3D_FEATURE_LEVEL_11_0;
        ICY_COM_ERROR(func(static_cast<IDXGIAdapter*>(adapter.handle()),
            D3D_DRIVER_TYPE_UNKNOWN, nullptr, (flags & window_flags::debug_layer) ?
            D3D11_CREATE_DEVICE_FLAG::D3D11_CREATE_DEVICE_DEBUG : 0,
            &level, 1, D3D11_SDK_VERSION, &m_device, nullptr, nullptr));
    }
    else
    {
        return make_stdlib_error(std::errc::function_not_supported);
    }

    if (flags & window_flags::debug_layer)
    {
        if (auto debug = com_ptr<ID3D11Debug>(m_device))
        {
            ;
        }
    }
    ICY_ERROR(m_buffers.resize(buffer_count(flags)));
    for (auto&& buffer : m_buffers)
        ICY_ERROR(buffer.initialize(*m_device));
    
    ICY_ERROR(m_render.svg.initialize(*m_device));

    return error_type();
}
error_type d3d11_display::bind(HWND__* const window) noexcept
{
    com_ptr<IDXGIFactory> factory;
    ICY_COM_ERROR(static_cast<IDXGIAdapter*>(m_adapter.handle())->GetParent(IID_PPV_ARGS(&factory)));
    ICY_COM_ERROR(factory->MakeWindowAssociation(window, DXGI_MWA_NO_ALT_ENTER | DXGI_MWA_NO_WINDOW_CHANGES | DXGI_MWA_NO_PRINT_SCREEN));
    ICY_ERROR(m_chain.initialize(*factory, *m_device, window, m_flags));
    
    window_size size = {};
    ICY_ERROR(m_chain.size(size));
    for (auto&& buffer : m_buffers)
        ICY_ERROR(buffer.resize(size, m_flags));
    
    return error_type();
}
error_type d3d11_display::draw(const size_t frame) noexcept
{
    com_ptr<ID3D11DeviceContext> context;
    m_device->GetImmediateContext(&context);

    d3d11_back_buffer* buffer = nullptr;

    if (frame < m_frame)
        buffer = &m_buffers[frame % m_buffers.size()];
    else if (m_frame)
        buffer = &m_buffers[(m_frame + m_buffers.size() - 1) % m_buffers.size()];        
    
    if (!buffer)
    {
        float colors[4] = { 0, 0, 0, 1 };
        context->ClearRenderTargetView(&m_chain.view(), colors);
    }
    else
    {
        ICY_ERROR(buffer->draw(m_chain.view()));
    }
    if (const auto error = m_chain.present(0))
    {
        if (error.source == error_source_system &&
            error.code == DXGI_STATUS_OCCLUDED)
            ;
        else
            return error;
    }
    return error_type();
}
error_type d3d11_display::resize(const window_flags flags) noexcept
{
    ICY_ERROR(m_chain.resize());
    window_size size;
    ICY_ERROR(m_chain.size(size));
    for (auto&& buffer : m_buffers)
        ICY_ERROR(buffer.resize(size, flags));
    return error_type();
}
error_type d3d11_display::update(const render_list& list) noexcept
{
    return m_buffers[((m_frame++) % m_buffers.size())].update(list, m_render);    
}

error_type d3d11_render_svg::initialize(ID3D11Device& device) noexcept
{
    ICY_COM_ERROR(device.CreateVertexShader(g_shader_bytes_svg_vs, _countof(g_shader_bytes_svg_vs), nullptr, &m_vshader));
    ICY_COM_ERROR(device.CreatePixelShader(g_shader_bytes_svg_ps, _countof(g_shader_bytes_svg_ps), nullptr, &m_pshader));
    D3D11_INPUT_ELEMENT_DESC layout[2] =
    {
        { "SV_Position", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "COLOR",       0, DXGI_FORMAT_R8G8B8A8_UNORM, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
    };
    ICY_COM_ERROR(device.CreateInputLayout(layout, _countof(layout), g_shader_bytes_svg_vs, _countof(g_shader_bytes_svg_vs), &m_layout));
    return error_type();
}
void d3d11_render_svg::operator()(ID3D11DeviceContext& context) const noexcept
{
    context.VSSetShader(m_vshader, nullptr, 0);
    context.PSSetShader(m_pshader, nullptr, 0);
    context.IASetInputLayout(m_layout);
    context.IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
}


/*error_type d3d11_render_svg::update(array_view<render_element> data) noexcept
{
    auto change = false;
    for (auto&& elem : data)
    {
        const auto type = render_type(elem.index.type);
        if (type == render_type::svg_geometry)
        {
            auto it = m_geometry.find(elem.index.index);
            if (elem.oper == render_element::oper_type::create)
            {
                if (it != m_geometry.end())
                    return make_render_error(render_error_code::invalid_index);
                ICY_ERROR(m_geometry.insert(elem.index.index, std::move(elem.svg_geometry)));
            }
            else if (elem.oper == render_element::oper_type::clear)
            {
                if (it == m_geometry.end())
                    return make_render_error(render_error_code::invalid_index);
                m_geometry.erase(it);
            }
            else if (elem.oper == render_element::oper_type::update)
            {
                if (it == m_geometry.end())
                    return make_render_error(render_error_code::invalid_index);
                it->value = std::move(elem.svg_geometry);
            }
            else
                continue;
        }
        else if (type == render_type::svg_object)
        {
            auto it = m_objects.find(elem.index.index);
            if (elem.oper == render_element::oper_type::create)
            {
                if (it != m_objects.end())
                    return make_render_error(render_error_code::invalid_index);

                auto key = elem.index.index;
                auto val = elem.svg_object;
                ICY_ERROR(m_objects.insert(key, val));
            }
            else if (elem.oper == render_element::oper_type::clear)
            {
                if (it == m_objects.end())
                    return make_render_error(render_error_code::invalid_index);
                m_objects.erase(it);
            }            
        }
        else
            continue;
        
        change = true;
    }
    if (!change)
        return error_type();

    array<render_d2d_vertex> vertices;
    for (auto&& pair : m_objects)
    {
        auto& obj = pair.value;
        auto& svg = m_geometry[obj.svg.index]->value;
        for (auto vertex : svg.vertices())
        {
            vertex.coord.z += obj.layer;
            vertex.coord.x += obj.offset.x;
            vertex.coord.y += obj.offset.y;
            ICY_ERROR(vertices.push_back(vertex));
        }
    }
    
    auto new_capacity = vertices.size() * sizeof(render_d2d_vertex);
    auto old_capacity = 0_z;

    com_ptr<ID3D11Device> device;
    m_layout->GetDevice(&device);

    D3D11_BUFFER_DESC mem_desc = {};
    D3D11_BUFFER_DESC gpu_desc = {};
    if (m_mem_buffer) m_mem_buffer->GetDesc(&mem_desc);   
    if (m_gpu_buffer) m_gpu_buffer->GetDesc(&gpu_desc);
    
    old_capacity = mem_desc.ByteWidth;
    if (new_capacity > old_capacity)
    {
        gpu_desc.ByteWidth = mem_desc.ByteWidth = new_capacity;
        gpu_desc.BindFlags = mem_desc.BindFlags = D3D11_BIND_VERTEX_BUFFER;

        mem_desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
        mem_desc.Usage = D3D11_USAGE_STAGING;
        
        com_ptr<ID3D11Buffer> mem_buffer;
        com_ptr<ID3D11Buffer> gpu_buffer;
        ICY_COM_ERROR(device->CreateBuffer(&mem_desc, nullptr, &mem_buffer));
        ICY_COM_ERROR(device->CreateBuffer(&gpu_desc, nullptr, &gpu_buffer));
        m_mem_buffer = mem_buffer;
        m_gpu_buffer = gpu_buffer;
    }
    com_ptr<ID3D11DeviceContext> context;
    device->GetImmediateContext(&context);
    D3D11_MAPPED_SUBRESOURCE map;
    ICY_COM_ERROR(context->Map(m_mem_buffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &map));
    memcpy(map.pData, vertices.data(), new_capacity);
    context->Unmap(m_mem_buffer, 0);
    context->CopyResource(m_gpu_buffer, m_mem_buffer);
    return error_type();
}
*/


error_type d3d11_back_buffer::initialize(ID3D11Device& device) noexcept
{
    com_ptr<ID3D11DeviceContext> context;
    ICY_COM_ERROR(device.CreateDeferredContext(0, &context));
    m_context = context;
    return error_type();
}
error_type d3d11_back_buffer::resize(const window_size size, const window_flags flags) noexcept
{
    com_ptr<ID3D11Device> device;
    com_ptr<ID3D11Texture2D> texture;
    com_ptr<ID3D11RenderTargetView> rtv;

    m_context->GetDevice(&device);
    CD3D11_TEXTURE2D_DESC desc(DXGI_FORMAT_R8G8B8A8_TYPELESS, size.x, size.y, 1, 0,
        D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE);
    desc.SampleDesc.Count = sample_count(flags);
    ICY_COM_ERROR(device->CreateTexture2D(&desc, nullptr, &texture));
    
    CD3D11_RENDER_TARGET_VIEW_DESC desc_rtv(desc.SampleDesc.Count > 1 ?
        D3D11_RTV_DIMENSION_TEXTURE2DMS : D3D11_RTV_DIMENSION_TEXTURE2D, DXGI_FORMAT_R8G8B8A8_UNORM);
    ICY_COM_ERROR(device->CreateRenderTargetView(texture, &desc_rtv, &rtv));

    m_texture = texture;
    m_view = rtv;
    return error_type();
}
error_type d3d11_back_buffer::update(const render_list& list, const d3d11_render& render) noexcept
{
    com_ptr<ID3D11Device> device;
    m_context->GetDevice(&device);

    array<render_d2d_vertex> d2d_vertices;
    auto d2d_capacity = 0_z;
    for (auto&& elem : list.data)
    {
        if (elem.type == render_element_type::svg)
            d2d_capacity += elem.svg.geometry.vertices().size();
    }
    ICY_ERROR(d2d_vertices.reserve(d2d_capacity));

    for (auto&& elem : list.data)
    {
        switch (elem.type)
        {
        case render_element_type::clear:
        {
            float rgba[4];
            elem.clear.to_rgbaf(rgba);
            m_context->ClearRenderTargetView(m_view, rgba);
            break;
        }
        case render_element_type::svg:
        {
            const auto& svg = elem.svg;
            for (auto vertex : svg.geometry.vertices())
            {
                vertex.coord.x += svg.offset.x;
                vertex.coord.y += svg.offset.y;
                vertex.coord.z += svg.layer;
                ICY_ERROR(d2d_vertices.push_back(vertex));
            }
            break;
        }
        default:
            break;
        }
    }
    
    const auto copy_buffer = [this](ID3D11Device& device, com_ptr<ID3D11Buffer>& mem, com_ptr<ID3D11Buffer>& gpu, void* data, size_t capacity)
    {
        com_ptr<ID3D11DeviceContext> context;
        device.GetImmediateContext(&context);
        
        D3D11_BUFFER_DESC mem_desc = {};
        D3D11_BUFFER_DESC gpu_desc = {};
        if (mem) mem->GetDesc(&mem_desc);
        if (gpu) gpu->GetDesc(&gpu_desc);

        if (capacity > mem_desc.ByteWidth)
        {
            mem_desc.ByteWidth = capacity;
            mem_desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
            mem_desc.Usage = D3D11_USAGE_STAGING;

            com_ptr<ID3D11Buffer> mem_buffer;
            ICY_COM_ERROR(device.CreateBuffer(&mem_desc, nullptr, &mem_buffer));
            mem = mem_buffer;
        }
        if (capacity > gpu_desc.ByteWidth)
        {
            gpu_desc.ByteWidth = capacity;
            gpu_desc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
            com_ptr<ID3D11Buffer> gpu_buffer;
            ICY_COM_ERROR(device.CreateBuffer(&gpu_desc, nullptr, &gpu_buffer));
            gpu = gpu_buffer;
        }

        D3D11_MAPPED_SUBRESOURCE map;
        ICY_COM_ERROR(context->Map(mem, 0, D3D11_MAP_WRITE_DISCARD, 0, &map));
        memcpy(map.pData, data, capacity);
        context->Unmap(mem, 0);
        m_context->CopyResource(gpu, mem);
        return error_type();
    };
    
    if (!d2d_vertices.empty())
    {
        ICY_ERROR(copy_buffer(*device, m_svg_mem_buffer, m_svg_gpu_buffer,
            d2d_vertices.data(), d2d_vertices.size() * sizeof(render_d2d_vertex)));
        render.svg(*m_context);

        m_context->OMSetRenderTargets(0, &m_view, nullptr);
        m_context->Draw(d2d_vertices.size(), 0);
    }

    com_ptr<ID3D11CommandList> commands;
    ICY_COM_ERROR(m_context->FinishCommandList(TRUE, &commands));
    m_commands = commands;
    return error_type();
}
error_type d3d11_back_buffer::draw(ID3D11RenderTargetView& rtv) noexcept
{
    com_ptr<ID3D11Device> device;
    com_ptr<ID3D11DeviceContext> context;
    com_ptr<ID3D11Resource> resource;
    rtv.GetDevice(&device);
    rtv.GetResource(&resource);
    device->GetImmediateContext(&context);

    context->ExecuteCommandList(m_commands, TRUE);
    context->ResolveSubresource(resource, 0, m_texture, 0, DXGI_FORMAT_R8G8B8A8_UNORM);

    return error_type();
}

error_type icy::make_d3d11_display(unique_ptr<display>& display, const adapter& adapter, const window_flags flags, HWND__* const window) noexcept
{
    auto new_display = make_unique<d3d11_display>();
    if (!new_display)
        return make_stdlib_error(std::errc::not_enough_memory);
    ICY_ERROR(new_display->initialize(adapter, flags));
    ICY_ERROR(new_display->bind(window));
    display = std::move(new_display);
    return error_type();
}

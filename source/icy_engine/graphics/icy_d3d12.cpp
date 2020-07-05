#include "icy_d3d12.hpp"
#include "icy_window.hpp"
#include <dxgi1_6.h>
#include "d3dx12.h"
#include "shaders/svg_ps.hpp"
#include "shaders/svg_vs.hpp"

using namespace icy;

static D3D12_CPU_DESCRIPTOR_HANDLE d3d12_cpu_handle(size_t ptr) noexcept
{
    D3D12_CPU_DESCRIPTOR_HANDLE handle;
    handle.ptr = ptr;
    return handle;
}

d3d12_event::~d3d12_event() noexcept
{
    if (m_handle)
        CloseHandle(m_handle);
}
error_type d3d12_event::initialize() noexcept
{
    return (m_handle = CreateEventW(nullptr, FALSE, FALSE, nullptr)) ? error_type() : last_system_error();
}

error_type d3d12_fence::initialize(ID3D12Device& device) noexcept
{
    ICY_ERROR(m_event.initialize());
    ICY_COM_ERROR(device.CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&m_handle)));
    return error_type();
}
error_type d3d12_fence::wait(const duration_type timeout) noexcept
{
    if (m_handle->GetCompletedValue() < m_value)
    {
        const auto wait = ::WaitForSingleObject(m_event.handle(), ms_timeout(timeout));
        if (wait != WAIT_OBJECT_0)
            return last_system_error();
    }
    return error_type();
}
error_type d3d12_fence::signal(ID3D12CommandQueue& queue) noexcept
{
    ICY_COM_ERROR(queue.Signal(m_handle, m_value + 1));
    ICY_COM_ERROR(m_handle->SetEventOnCompletion(m_value + 1, m_event.handle()));
    ++m_value;
    return error_type();
}
uint64_t d3d12_fence::gpu_value() const noexcept
{
    return m_handle->GetCompletedValue();
}

error_type d3d12_render_svg::initialize(ID3D12Device& device) noexcept
{
    return error_type();
}

error_type d3d12_swap_chain::initialize(IDXGIFactory& factory, ID3D12CommandQueue& queue, HWND__* const hwnd, const window_flags flags) noexcept
{
    com_ptr<IDXGIFactory2> dxgi_factory;
    factory.QueryInterface(&dxgi_factory);

    com_ptr<ID3D12Device> device;
    ICY_COM_ERROR(queue.GetDevice(IID_PPV_ARGS(&device)));

    DXGI_SWAP_CHAIN_DESC1 desc = {};
    desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    desc.SampleDesc = { 1, 0 };
    desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    desc.BufferCount = buffer_count(flags);
    desc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL;
    desc.Flags |= DXGI_SWAP_CHAIN_FLAG_FRAME_LATENCY_WAITABLE_OBJECT;
    com_ptr<IDXGISwapChain1> chain;
    ICY_COM_ERROR(dxgi_factory->CreateSwapChainForHwnd(&queue, hwnd, &desc, nullptr, nullptr, &chain));
    ICY_COM_ERROR(chain->QueryInterface(&m_chain));
    ICY_COM_ERROR(m_chain->SetMaximumFrameLatency(2));
    m_chain_wait = m_chain->GetFrameLatencyWaitableObject();

    /*ICY_ERROR(m_buffers.resize(desc.BufferCount));
    ICY_ERROR(m_views.resize(desc.BufferCount));
    for (auto k = 0u; k < desc.BufferCount; ++k)
    {
        ICY_COM_ERROR(m_chain->GetBuffer(k, IID_PPV_ARGS(&m_buffers[k])));
        ICY_ERROR(heap.push(D3D12_DESCRIPTOR_HEAP_TYPE_RTV, m_views[k]));
        device->CreateRenderTargetView(m_buffers[k], nullptr, d3d12_cpu_handle(m_views[k]));
    }*/
    return {};
}
error_type d3d12_swap_chain::resize() noexcept
{
    com_ptr<ID3D12Device> device;
    ICY_COM_ERROR(m_chain->GetDevice(IID_PPV_ARGS(&device)));
    ICY_COM_ERROR(m_chain->ResizeBuffers(0, 0, 0, 
        DXGI_FORMAT_UNKNOWN, DXGI_SWAP_CHAIN_FLAG_FRAME_LATENCY_WAITABLE_OBJECT));
    return error_type();
}
error_type d3d12_swap_chain::window(HWND__*& hwnd) const noexcept
{
    ICY_COM_ERROR(m_chain->GetHwnd(&hwnd));
    return error_type();
}
error_type d3d12_swap_chain::present(const uint32_t vsync) noexcept
{
    DXGI_PRESENT_PARAMETERS params = {};
    ICY_COM_ERROR(m_chain->Present1(vsync, 0, &params));
    //m_buffer = m_chain->GetCurrentBackBufferIndex();
    return error_type();
}
error_type d3d12_swap_chain::size(window_size& output) const noexcept
{
    DXGI_SWAP_CHAIN_DESC1 desc = {};
    ICY_COM_ERROR(m_chain->GetDesc1(&desc));
    output = { desc.Width, desc.Height };
    return error_type();
}
error_type d3d12_swap_chain::buffer(const size_t index, com_ptr<ID3D12Resource>& texture) const noexcept
{
    ICY_COM_ERROR(m_chain->GetBuffer(index, IID_PPV_ARGS(&texture)));
    return error_type();
}
size_t d3d12_swap_chain::index() const noexcept
{
    return m_chain->GetCurrentBackBufferIndex();
}

error_type d3d12_back_buffer::initialize(ID3D12Device& device) noexcept
{
    const auto type = D3D12_COMMAND_LIST_TYPE_DIRECT;
    ICY_COM_ERROR(device.CreateCommandAllocator(type, IID_PPV_ARGS(&m_alloc)));
    ICY_COM_ERROR(device.CreateCommandList(0, type, m_alloc, nullptr, IID_PPV_ARGS(&m_commands)));
    ICY_COM_ERROR(m_commands->Close());
    
    D3D12_DESCRIPTOR_HEAP_DESC heap_desc = {};
    heap_desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
    heap_desc.NumDescriptors = 2;
    ICY_COM_ERROR(device.CreateDescriptorHeap(&heap_desc, IID_PPV_ARGS(&m_heap)));
    
    const auto rtv_desc_size = device.GetDescriptorHandleIncrementSize(heap_desc.Type);
    m_chain_view = CD3DX12_CPU_DESCRIPTOR_HANDLE(m_heap->GetCPUDescriptorHandleForHeapStart(), 0, rtv_desc_size).ptr;
    m_render_view = CD3DX12_CPU_DESCRIPTOR_HANDLE(m_heap->GetCPUDescriptorHandleForHeapStart(), 1, rtv_desc_size).ptr;
    return error_type();
}
error_type d3d12_back_buffer::resize(const com_ptr<ID3D12Resource> chain_buffer, const window_size size, const window_flags flags) noexcept
{
    m_chain_buffer = chain_buffer;
    if (!chain_buffer)
        return error_type();

    com_ptr<ID3D12Device> device;
    ICY_COM_ERROR(chain_buffer->GetDevice(IID_PPV_ARGS(&device)));
    const auto rtv_chain_desc = D3D12_RENDER_TARGET_VIEW_DESC{ DXGI_FORMAT_R8G8B8A8_UNORM, D3D12_RTV_DIMENSION_TEXTURE2D };
    const auto rtv_render_desc = D3D12_RENDER_TARGET_VIEW_DESC{ DXGI_FORMAT_R8G8B8A8_UNORM, sample_count(flags) > 1 ? D3D12_RTV_DIMENSION_TEXTURE2DMS : D3D12_RTV_DIMENSION_TEXTURE2D };
    const auto tex_render_desc = CD3DX12_RESOURCE_DESC::Tex2D(rtv_render_desc.Format, 
        size.x, size.y, 1, 1, sample_count(flags));

    com_ptr<ID3D12Resource> render;
    ICY_COM_ERROR(device->CreateCommittedResource(nullptr, D3D12_HEAP_FLAG_NONE, &tex_render_desc, 
        D3D12_RESOURCE_STATE_RESOLVE_SOURCE, nullptr, IID_PPV_ARGS(&render)));
    
    m_render_buffer = render;
    device->CreateRenderTargetView(chain_buffer, &rtv_chain_desc, d3d12_cpu_handle(m_chain_view));
    device->CreateRenderTargetView(m_render_buffer, &rtv_render_desc, d3d12_cpu_handle(m_render_view));
    return error_type();
}
error_type d3d12_back_buffer::update(const render_list& list, const d3d12_render& render) noexcept
{
    ICY_COM_ERROR(m_commands->Reset(m_alloc, nullptr));

    array<render_d2d_vertex> d2d_vertices;
    auto d2d_capacity = 0_z;
    for (auto&& elem : list.data)
    {
        if (elem.type == render_element_type::svg)
            d2d_capacity += elem.svg.geometry.vertices().size();
    }
    ICY_ERROR(d2d_vertices.reserve(d2d_capacity));

    float clear[4] = { 0, 0, 0, 1 };
    for (auto&& elem : list.data)
    {
        switch (elem.type)
        {
        case render_element_type::clear:
        {
            elem.clear.to_rgbaf(clear);
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

    const auto barrier_beg = CD3DX12_RESOURCE_BARRIER::Transition(m_render_buffer,
        D3D12_RESOURCE_STATE_RESOLVE_SOURCE, D3D12_RESOURCE_STATE_RENDER_TARGET);
    m_commands->ResourceBarrier(1, &barrier_beg);

    m_commands->ClearRenderTargetView(d3d12_cpu_handle(m_render_view), clear, 0, nullptr);

    const auto barrier_end = CD3DX12_RESOURCE_BARRIER::Transition(m_render_buffer,
        D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_RESOLVE_SOURCE);
    m_commands->ResourceBarrier(1, &barrier_end);
    ICY_COM_ERROR(m_commands->Close());

    ID3D12CommandList* const commands[] = { m_commands };
    render.queue->ExecuteCommandLists(_countof(commands), commands);
    return error_type();
}
void d3d12_back_buffer::draw(ID3D12GraphicsCommandList& commands) noexcept
{
    const auto barrier_beg = CD3DX12_RESOURCE_BARRIER::Transition(m_chain_buffer,
        D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_RESOLVE_DEST);
    const auto barrier_end = CD3DX12_RESOURCE_BARRIER::Transition(m_chain_buffer,
        D3D12_RESOURCE_STATE_RESOLVE_DEST, D3D12_RESOURCE_STATE_RENDER_TARGET);
    commands.ResourceBarrier(1, &barrier_beg);
    commands.ResolveSubresource(m_chain_buffer, 0, m_render_buffer, 0, DXGI_FORMAT_R8G8B8A8_UNORM);
    commands.ResourceBarrier(1, &barrier_end);
}

d3d12_display::~d3d12_display() noexcept
{
    m_fence.signal(*m_queue);
    m_fence.wait();
}
error_type d3d12_display::initialize(const adapter& adapter, const window_flags flags) noexcept
{
    m_adapter = adapter;
    m_flags = flags;
    ICY_ERROR(m_event.initialize());
    ICY_ERROR(m_d3d12_lib.initialize());

    if (flags & window_flags::debug_layer)
    {
        if (const auto func = ICY_FIND_FUNC(m_d3d12_lib, D3D12GetDebugInterface))
        {
            com_ptr<ID3D12Debug> debug;
            func(IID_PPV_ARGS(&debug));
            if (debug)
                debug->EnableDebugLayer();
        }
    }

    if (const auto func = ICY_FIND_FUNC(m_d3d12_lib, D3D12CreateDevice))
    {
        ICY_COM_ERROR(func(static_cast<IDXGIAdapter*>(adapter.handle()), D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&m_device)));
    }
    else
    {
        return make_stdlib_error(std::errc::function_not_supported);
    }

    if (flags & window_flags::debug_layer)
    {
        if (auto debug = com_ptr<ID3D12InfoQueue>(m_device))
        {
            debug->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_CORRUPTION, TRUE);
            debug->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_ERROR, TRUE);
            debug->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_WARNING, TRUE);

            D3D12_MESSAGE_SEVERITY severities[] = { D3D12_MESSAGE_SEVERITY_INFO };
            D3D12_MESSAGE_ID deny[] =
            {
                D3D12_MESSAGE_ID_CLEARRENDERTARGETVIEW_MISMATCHINGCLEARVALUE,
                D3D12_MESSAGE_ID_MAP_INVALID_NULLRANGE,
                D3D12_MESSAGE_ID_UNMAP_INVALID_NULLRANGE,
            };
            D3D12_INFO_QUEUE_FILTER filter = {};
            filter.DenyList.NumSeverities = _countof(severities);
            filter.DenyList.pSeverityList = severities;
            filter.DenyList.NumIDs = _countof(deny);
            filter.DenyList.pIDList = deny;
            ICY_COM_ERROR(debug->PushStorageFilter(&filter));
        }
    }
    D3D12_COMMAND_QUEUE_DESC queue_desc{ D3D12_COMMAND_LIST_TYPE::D3D12_COMMAND_LIST_TYPE_DIRECT };
    ICY_COM_ERROR(m_device->CreateCommandQueue(&queue_desc, IID_PPV_ARGS(&m_queue)));
    ICY_ERROR(m_fence.initialize(*m_device));

    ICY_ERROR(m_buffers.resize(buffer_count(flags)));
    for (auto&& buffer : m_buffers)
        ICY_ERROR(buffer.initialize(*m_device));
    
    return error_type();
}
error_type d3d12_display::bind(HWND__* const window) noexcept
{
    com_ptr<IDXGIFactory2> factory;
    ICY_COM_ERROR(static_cast<IDXGIAdapter*>(m_adapter.handle())->GetParent(IID_PPV_ARGS(&factory)));
    ICY_COM_ERROR(factory->MakeWindowAssociation(window, DXGI_MWA_NO_ALT_ENTER | DXGI_MWA_NO_WINDOW_CHANGES | DXGI_MWA_NO_PRINT_SCREEN));
    ICY_ERROR(m_chain.initialize(*factory, *m_queue, window, m_flags));

    window_size size;
    ICY_ERROR(m_chain.size(size));
    for (auto k = 0u; k < m_buffers.size(); ++k)
    {
        com_ptr<ID3D12Resource> chain_buffer;
        ICY_ERROR(m_chain.buffer(k, chain_buffer));
        ICY_ERROR(m_buffers[k].resize(chain_buffer, size, m_flags));
        ICY_ERROR(m_buffers[k].update(render_list(), m_render));
    }
    return error_type();
}
error_type d3d12_display::draw(const size_t frame, const bool vsync) noexcept
{
    const auto index = m_chain.index();
    m_buffers[index].draw(m_render.queue);

    ICY_COM_ERROR(m_commands->Reset(m_alloc, nullptr));
    ICY_COM_ERROR(m_commands->Close());
    ID3D12CommandList* const commands[] = { m_commands };
    m_queue->ExecuteCommandLists(_countof(commands), commands);

    const auto error = m_chain.present(vsync);
    ICY_ERROR(m_fence.signal(*m_queue));
    if (error)
    {
        if (error.source == error_source_system &&
            error.code == DXGI_STATUS_OCCLUDED)
            ;
        else
            return error;
    }
    return error_type();
}
error_type d3d12_display::resize() noexcept
{
    ICY_ERROR(m_fence.signal(*m_queue));
    ICY_ERROR(m_fence.wait());
    window_size size;
    for (auto&& buffer : m_buffers)
    {
        ICY_ERROR(buffer.resize(nullptr, window_size(), m_flags));
    }
    ICY_ERROR(m_chain.resize());
    ICY_ERROR(m_chain.size(size));
    for (auto k = 0u; k < m_buffers.size(); ++k)
    {
        com_ptr<ID3D12Resource> chain_buffer;
        ICY_ERROR(m_chain.buffer(k, chain_buffer));
        ICY_ERROR(m_buffers[k].resize(chain_buffer, size, m_flags));
    }
    return error_type();
}
error_type d3d12_display::update(const render_list& list) noexcept
{
    return m_buffers[((m_frame++) % m_buffers.size())].update(list, m_render);
}

error_type icy::make_d3d12_display(unique_ptr<display>& display, const adapter& adapter, const window_flags flags, HWND__* const window) noexcept
{
    auto new_display = make_unique<d3d12_display>();
    if (!new_display)
        return make_stdlib_error(std::errc::not_enough_memory);
    ICY_ERROR(new_display->initialize(adapter, flags));
    ICY_ERROR(new_display->bind(window));
    display = std::move(new_display);
    return error_type();
}

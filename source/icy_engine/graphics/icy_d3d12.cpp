#include "icy_render.hpp"
#include "icy_shader.hpp"
#include <dxgi1_6.h>
#include "d3dx12.h"

using namespace icy;

static D3D12_CPU_DESCRIPTOR_HANDLE d3d12_cpu_handle(size_t ptr) noexcept
{
    D3D12_CPU_DESCRIPTOR_HANDLE handle;
    handle.ptr = ptr;
    return handle;
}

class d3d12_fence;
class d3d12_event
{
public:
    d3d12_event() noexcept = default;
    d3d12_event(const d3d12_event&) = delete;
    d3d12_event(d3d12_event&& rhs) noexcept : m_handle(rhs.m_handle)
    {
        rhs.m_handle = nullptr;
    }
    ICY_DEFAULT_MOVE_ASSIGN(d3d12_event);
    ~d3d12_event() noexcept;
    void* handle() const noexcept
    {
        return m_handle;
    }
    icy::error_type initialize() noexcept;
    icy::error_type signal(const icy::const_array_view<const d3d12_fence*> fences) noexcept;
private:
    void* m_handle = nullptr;
};
class d3d12_fence
{
    friend d3d12_event;
public:
    d3d12_fence() noexcept = default;
    d3d12_fence(d3d12_fence&& rhs) noexcept : m_value(rhs.m_value.load(std::memory_order_acquire)), 
        m_event(std::move(rhs.m_event)), m_handle(std::move(rhs.m_handle))
    {
        
    }
    ICY_DEFAULT_MOVE_ASSIGN(d3d12_fence);
    icy::error_type initialize(ID3D12Device& device) noexcept;
    icy::error_type wait_cpu(const icy::duration_type timeout = icy::max_timeout) noexcept;
    icy::error_type wait_gpu(ID3D12CommandQueue& queue) noexcept;
    icy::error_type signal(ID3D12CommandQueue& queue) noexcept;
    uint64_t cpu_value() const noexcept
    {
        return m_value.load(std::memory_order_acquire);
    }
    uint64_t gpu_value() const noexcept;
    void* event() const noexcept
    {
        return m_event.handle();
    }
private:
    std::atomic<uint64_t> m_value = 0;
    d3d12_event m_event;
    icy::com_ptr<ID3D12Fence> m_handle;
};
class d3d12_render_frame
{
public:
    d3d12_render_frame() noexcept = default;
    ~d3d12_render_frame() noexcept;
    d3d12_render_frame(d3d12_render_frame&& rhs) noexcept = default;
    ICY_DEFAULT_MOVE_ASSIGN(d3d12_render_frame);
public:
    icy::error_type initialize(icy::shared_ptr<d3d12_render_factory> system) noexcept;
    icy::error_type exec(icy::render_list& list) noexcept;
    icy::error_type wait(com_ptr<ID3D12Resource>& texture, com_ptr<ID3D12DescriptorHeap>& heap) noexcept;
    icy::render_flags flags() const noexcept;
    void destroy(const bool clear) noexcept;
    window_size size() const noexcept;
private:
    void* _unused = nullptr;
    icy::weak_ptr<d3d12_render_factory> m_system;
    d3d12_fence m_fence;
    icy::com_ptr<ID3D12CommandQueue> m_queue;
    icy::com_ptr<ID3D12CommandAllocator> m_alloc;
    icy::com_ptr<ID3D12GraphicsCommandList> m_commands;
    icy::com_ptr<ID3D12Resource> m_texture;
    icy::com_ptr<ID3D12DescriptorHeap> m_rtv_heap;
    icy::com_ptr<ID3D12DescriptorHeap> m_srv_heap;
    size_t m_rtv = 0;
    size_t m_srv = 0;
};
class d3d12_render_factory : public render_factory
{
    friend d3d12_render_frame;
public:
    ~d3d12_render_factory() noexcept;
    icy::error_type initialize(const icy::adapter::data_type& adapter, const icy::render_flags flags) noexcept;
private:
    icy::error_type exec(icy::render_list& list, icy::render_frame& frame) noexcept override;
    icy::render_flags flags() const noexcept override
    {
        return m_flags;
    }
    icy::error_type resize(const window_size size) noexcept override
    {
        if (!size.x || !size.y)
            return icy::make_stdlib_error(std::errc::invalid_argument);
        m_size = size;
        return icy::error_type();
    }
private:
    icy::render_flags m_flags = icy::render_flags::none;
    icy::shared_ptr<d3d12_system> m_global;
    icy::window_size m_size;
    icy::detail::intrusive_mpsc_queue m_frames;
};
class d3d12_display_system : public display_data
{
public:
    d3d12_display_system() noexcept : display_data(adapter_flags::d3d12)
    {

    }
    ~d3d12_display_system() noexcept
    {
        filter(0);
    }
    error_type initialize(const adapter::data_type& adapter, HWND__* const window, const display_flags flags) noexcept;
private:
    struct buffer_type
    {
        com_ptr<ID3D12Resource> texture;
        size_t view = 0;
    };
private:
    error_type resize(IDXGISwapChain& chain, const window_size size) noexcept override;
    error_type repaint(IDXGISwapChain& chain, const bool vsync, void* const handle) noexcept override;
private:
    shared_ptr<d3d12_system> m_global;
    d3d12_fence m_fence;
    com_ptr<ID3D12CommandQueue> m_queue;
    com_ptr<ID3D12CommandAllocator> m_alloc;
    com_ptr<ID3D12GraphicsCommandList> m_commands;
    com_ptr<ID3D12DescriptorHeap> m_heap;
    buffer_type m_buffers[buffer_count(icy::display_flags::triple_buffer)];
    com_ptr<ID3D12PipelineState> m_pipe1;
    com_ptr<ID3D12PipelineState> m_pipe2;
    com_ptr<ID3D12PipelineState> m_pipe4;
    com_ptr<ID3D12PipelineState> m_pipe8;
    com_ptr<ID3D12PipelineState> m_pipe16;
    com_ptr<ID3D12RootSignature> m_root;
    window_size m_size;
};

error_type adapter::data_type::query_d3d12(shared_ptr<d3d12_system>& system) const noexcept
{
    ICY_LOCK_GUARD(m_lock);
    if (!m_d3d12)
    {
        shared_ptr<d3d12_system> new_ptr;
        ICY_ERROR(make_shared(new_ptr));
        ICY_ERROR(new_ptr->initialize(*this));
        m_d3d12 = std::move(new_ptr);
    }
    system = m_d3d12;
    return error_type();
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
error_type d3d12_event::signal(const const_array_view<const d3d12_fence*> fences) noexcept
{
    if (fences.empty() || !m_handle || fences.size() > MAXIMUM_WAIT_OBJECTS)
        return make_stdlib_error(std::errc::invalid_argument);

    com_ptr<ID3D12Device1> device;
    ICY_COM_ERROR(fences[0]->m_handle->GetDevice(IID_PPV_ARGS(&device)));

    ID3D12Fence* vec[MAXIMUM_WAIT_OBJECTS] = {};
    uint64_t val[MAXIMUM_WAIT_OBJECTS] = {};
    auto len = 0u;
    for (auto&& fence : fences)
    {
        vec[len] = fence->m_handle;
        val[len] = fence->m_value.load(std::memory_order_acquire);
        ++len;
    }
    ICY_COM_ERROR(device->SetEventOnMultipleFenceCompletion(vec, val, len, D3D12_MULTIPLE_FENCE_WAIT_FLAG_ALL, m_handle));
    return error_type();
}

error_type d3d12_fence::initialize(ID3D12Device& device) noexcept
{
    ICY_ERROR(m_event.initialize());
    ICY_COM_ERROR(device.CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&m_handle)));
    ICY_COM_ERROR(device.SetName(L"D3D12 Fence"));
    return error_type();
}
error_type d3d12_fence::wait_cpu(const duration_type timeout) noexcept
{
    auto value = m_value.load(std::memory_order_acquire);
    if (m_handle->GetCompletedValue() < value)
    {
        ICY_COM_ERROR(m_handle->SetEventOnCompletion(value, m_event.handle()));
        const auto wait = ::WaitForSingleObject(m_event.handle(), ms_timeout(timeout));
        if (wait != WAIT_OBJECT_0)
            return last_system_error();
    }
    return error_type();
}
error_type d3d12_fence::wait_gpu(ID3D12CommandQueue& queue) noexcept
{
    ICY_COM_ERROR(queue.Wait(m_handle, m_value.load(std::memory_order_acquire)));
    return error_type();
}
error_type d3d12_fence::signal(ID3D12CommandQueue& queue) noexcept
{
    ICY_COM_ERROR(queue.Signal(m_handle, m_value.fetch_add(1, std::memory_order_acq_rel) + 1));
    return error_type();
}
uint64_t d3d12_fence::gpu_value() const noexcept
{
    return m_handle->GetCompletedValue();
}

error_type d3d12_system::initialize(const adapter::data_type& adapter) noexcept
{
    ICY_ERROR(m_lib.initialize());

    com_ptr<ID3D12Debug> debug;
    if (adapter.flags() & adapter_flags::debug)
    {
        if (const auto func = ICY_FIND_FUNC(m_lib, D3D12GetDebugInterface))
        {
            func(IID_PPV_ARGS(&debug));
            if (debug)
                debug->EnableDebugLayer();
        }
    }

    if (const auto func = ICY_FIND_FUNC(m_lib, D3D12CreateDevice))
    {
        ICY_COM_ERROR(func(adapter.adapter(), D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&m_device)));
        if (debug)
            m_device->SetName(L"D3D12 Device");
    }
    else
    {
        return make_stdlib_error(std::errc::function_not_supported);
    }

    if (debug)
    {
        if (auto debug_queue = com_ptr<ID3D12InfoQueue>(m_device))
        {
            debug_queue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_CORRUPTION, TRUE);
            debug_queue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_ERROR, TRUE);
            debug_queue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_WARNING, TRUE);

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
            ICY_COM_ERROR(debug_queue->PushStorageFilter(&filter));
        }
    }
    return error_type();
}
error_type d3d12_system::rename(ID3D12DeviceChild& resource, const string_view name) noexcept
{
    array<wchar_t> wname;
    ICY_ERROR(to_utf16(name, wname));
    ICY_COM_ERROR(resource.SetName(wname.data()));
    return error_type();
}
icy::error_type d3d12_system::root(icy::com_ptr<ID3D12RootSignature>& output, const D3D12_VERSIONED_ROOT_SIGNATURE_DESC& desc) noexcept
{
    if (const auto func = ICY_FIND_FUNC(m_lib, D3D12SerializeVersionedRootSignature))
    {
        com_ptr<ID3D10Blob> data;
        com_ptr<ID3D10Blob> error;
        const auto hr = func(&desc, &data, &error);
        if (error)
        {
            array<wchar_t> wide;
            ICY_ERROR(to_utf16(string_view(static_cast<const char*>(error->GetBufferPointer()), error->GetBufferSize()), wide));
            OutputDebugStringW(wide.data());
        }
        ICY_COM_ERROR(hr);
        ICY_COM_ERROR(m_device->CreateRootSignature(0, 
            data->GetBufferPointer(), data->GetBufferSize(), IID_PPV_ARGS(&output)));
        return error_type();
    }
    else
    {
        return make_stdlib_error(std::errc::function_not_supported);
    }
}

d3d12_render_frame::~d3d12_render_frame() noexcept
{
    if (m_queue)
    {
        m_fence.signal(*m_queue);
        m_fence.wait_cpu();
    }
}
error_type d3d12_render_frame::initialize(icy::shared_ptr<d3d12_render_factory> system) noexcept
{
    m_system = system;
    ICY_ERROR(m_fence.initialize(system->m_global->device()));

    const auto type = D3D12_COMMAND_LIST_TYPE_DIRECT;
    auto queue_desc = D3D12_COMMAND_QUEUE_DESC{ type };
    ICY_COM_ERROR(system->m_global->device().CreateCommandQueue(&queue_desc, IID_PPV_ARGS(&m_queue)));
    ICY_COM_ERROR(system->m_global->device().CreateCommandAllocator(type, IID_PPV_ARGS(&m_alloc)));
    ICY_COM_ERROR(system->m_global->device().CreateCommandList(0, type, m_alloc, nullptr, IID_PPV_ARGS(&m_commands)));
    ICY_COM_ERROR(m_commands->Close());

    D3D12_DESCRIPTOR_HEAP_DESC rtv_heap_desc = {};
    rtv_heap_desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
    rtv_heap_desc.NumDescriptors = 1;
    ICY_COM_ERROR(system->m_global->device().CreateDescriptorHeap(&rtv_heap_desc, IID_PPV_ARGS(&m_rtv_heap)));

    D3D12_DESCRIPTOR_HEAP_DESC srv_heap_desc = {};
    srv_heap_desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
    srv_heap_desc.NumDescriptors = 1;
    srv_heap_desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAGS::D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
    ICY_COM_ERROR(system->m_global->device().CreateDescriptorHeap(&srv_heap_desc, IID_PPV_ARGS(&m_srv_heap)));

    ICY_ERROR(system->m_global->rename(*m_queue, "D3D12 Render Frame Direct Queue"_s));
    ICY_ERROR(system->m_global->rename(*m_alloc, "D3D12 Render Frame Allocator"_s));
    ICY_ERROR(system->m_global->rename(*m_commands, "D3D12 Render Frame Commands"_s));
    ICY_ERROR(system->m_global->rename(*m_rtv_heap, "D3D12 Render Frame RTV Descriptor Heap"));
    ICY_ERROR(system->m_global->rename(*m_srv_heap, "D3D12 Render Frame SRV Descriptor Heap"));

    const auto rtv_size = system->m_global->device().GetDescriptorHandleIncrementSize(rtv_heap_desc.Type);
    const auto srv_size = system->m_global->device().GetDescriptorHandleIncrementSize(srv_heap_desc.Type);

    m_rtv = CD3DX12_CPU_DESCRIPTOR_HANDLE(m_rtv_heap->GetCPUDescriptorHandleForHeapStart(), 0, rtv_size).ptr;
    m_srv = CD3DX12_CPU_DESCRIPTOR_HANDLE(m_srv_heap->GetCPUDescriptorHandleForHeapStart(), 0, srv_size).ptr;

    return error_type();
}
error_type d3d12_render_frame::exec(render_list& list) noexcept
{
    auto system = shared_ptr<d3d12_render_factory>(m_system);
    if (!system)
        return make_stdlib_error(std::errc::invalid_argument);

    ICY_ERROR(m_fence.wait_cpu());
    const auto msaa = sample_count(system->m_flags) > 1;

    auto desc = m_texture ? m_texture->GetDesc() : D3D12_RESOURCE_DESC{};
    if (system->m_size.x != desc.Width || system->m_size.y != desc.Height)
    {
        const auto rtv_desc = D3D12_RENDER_TARGET_VIEW_DESC{ DXGI_FORMAT_R8G8B8A8_UNORM, 
            msaa ? D3D12_RTV_DIMENSION_TEXTURE2DMS : D3D12_RTV_DIMENSION_TEXTURE2D };
        
        auto srv_desc = D3D12_SHADER_RESOURCE_VIEW_DESC{ DXGI_FORMAT_R8G8B8A8_UNORM,
            msaa ? D3D12_SRV_DIMENSION_TEXTURE2DMS : D3D12_SRV_DIMENSION_TEXTURE2D };
        srv_desc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        if (!msaa) srv_desc.Texture2D.MipLevels = 1;

        auto tex_desc = CD3DX12_RESOURCE_DESC::Tex2D(DXGI_FORMAT_R8G8B8A8_UNORM, system->m_size.x, system->m_size.y, 1, 1);
        tex_desc.SampleDesc.Count = sample_count(system->m_flags);
        tex_desc.Flags |= D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;

        CD3DX12_HEAP_PROPERTIES heap(D3D12_HEAP_TYPE_DEFAULT);
        com_ptr<ID3D12Resource> texture;
        ICY_COM_ERROR(system->m_global->device().CreateCommittedResource(&heap, 
            D3D12_HEAP_FLAG_NONE, &tex_desc, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, nullptr, IID_PPV_ARGS(&texture)));
        
        system->m_global->device().CreateRenderTargetView(texture, &rtv_desc, d3d12_cpu_handle(m_rtv));
        system->m_global->device().CreateShaderResourceView(texture, &srv_desc, d3d12_cpu_handle(m_srv));

        m_texture = texture;

        ICY_ERROR(system->m_global->rename(*m_texture, "D3D12 Render Frame Texture"_s));
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
    /*const auto make_buffer = [this](ID3D11Device& device, com_ptr<ID3D11Buffer>& gpu,
        const void* data, const size_t capacity, D3D11_BIND_FLAG flags)
    {
        D3D11_BUFFER_DESC gpu_desc = {};
        if (gpu) gpu->GetDesc(&gpu_desc);
        if (capacity > gpu_desc.ByteWidth)
        {
            gpu_desc.ByteWidth = align_up(uint32_t(capacity), 256);
            gpu_desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
            gpu_desc.BindFlags = flags;
            gpu_desc.Usage = D3D11_USAGE_DYNAMIC;
            ICY_COM_ERROR(device.CreateBuffer(&gpu_desc, nullptr, &gpu));
        }
        D3D11_MAPPED_SUBRESOURCE map;
        ICY_COM_ERROR(m_context->Map(gpu, 0, D3D11_MAP::D3D11_MAP_WRITE_DISCARD, 0, &map));
        memcpy(map.pData, data, capacity);
        m_context->Unmap(gpu, 0);
        return error_type();
    };

    if (!d2d_vertices.empty())
    {
        ICY_ERROR(make_buffer(m_global->device(), m_svg_vertices, d2d_vertices.data(),
            d2d_vertices.size() * sizeof(render_d2d_vertex), D3D11_BIND_VERTEX_BUFFER));

        float gsize[2] = { float(m_size.x), float(m_size.y) };
        ICY_ERROR(make_buffer(m_global->device(), m_svg_buffer, gsize, sizeof(gsize), D3D11_BIND_CONSTANT_BUFFER));

        m_global->svg()(*m_context);

        const auto view = D3D11_VIEWPORT{ 0, 0, gsize[0], gsize[1], 0, 1 };
        const uint32_t strides[] = { sizeof(render_d2d_vertex) };
        const uint32_t offsets[] = { 0 };
        m_context->IASetVertexBuffers(0, 1, &m_svg_vertices, strides, offsets);
        m_context->RSSetViewports(1, &view);
        m_context->VSSetConstantBuffers(0, 1, &m_svg_buffer);
        m_context->OMSetRenderTargets(1, &m_view, nullptr);
        m_context->Draw(uint32_t(d2d_vertices.size()), 0);
    }*/
    float rgba[4];
    clear.to_rgbaf(rgba);

    const auto barrier_beg = CD3DX12_RESOURCE_BARRIER::Transition(m_texture, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_RENDER_TARGET);
    const auto barrier_end = CD3DX12_RESOURCE_BARRIER::Transition(m_texture, D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

    ICY_COM_ERROR(m_commands->Reset(m_alloc, nullptr));
    m_commands->ResourceBarrier(1, &barrier_beg);
    m_commands->ClearRenderTargetView(d3d12_cpu_handle(m_rtv), rgba, 0, nullptr);
    m_commands->ResourceBarrier(1, &barrier_end);
    ICY_COM_ERROR(m_commands->Close());
    {
        ID3D12CommandList* const commands[] = { m_commands };
        m_queue->ExecuteCommandLists(_countof(commands), commands);
        ICY_ERROR(m_fence.signal(*m_queue));
    }
    return error_type();
}
error_type d3d12_render_frame::wait(com_ptr<ID3D12Resource>& texture, com_ptr<ID3D12DescriptorHeap>& heap) noexcept
{
    ICY_ERROR(m_fence.wait_cpu());
    texture = m_texture;
    heap = m_srv_heap;
    return error_type();
}
render_flags d3d12_render_frame::flags() const noexcept
{
    if (auto system = shared_ptr<d3d12_render_factory>(m_system))
        return system->flags();
    return render_flags::none;
}
void d3d12_render_frame::destroy(const bool clear) noexcept
{
    auto system = shared_ptr<d3d12_render_factory>(m_system);
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

d3d12_render_factory::~d3d12_render_factory() noexcept
{
    while (auto frame = static_cast<d3d12_render_frame*>(m_frames.pop()))
        frame->destroy(true);
}
error_type d3d12_render_factory::initialize(const adapter::data_type& adapter, const render_flags flags) noexcept
{
    m_flags = flags;
    ICY_ERROR(adapter.query_d3d12(m_global));
    ICY_ERROR(resize(window_size(1424, 720)));
    return error_type();
}
error_type d3d12_render_factory::exec(render_list& list, render_frame& frame) noexcept
{
    if (frame.data)
        return make_stdlib_error(std::errc::invalid_argument);

    frame.data = allocator_type::allocate<render_frame::data_type>(1);
    if (!frame.data)
        return make_stdlib_error(std::errc::not_enough_memory);
    allocator_type::construct(frame.data);

    frame.data->d3d12 = allocator_type::allocate<d3d12_render_frame>(1);
    if (!frame.data->d3d12)
        return make_stdlib_error(std::errc::not_enough_memory);
    allocator_type::construct(frame.data->d3d12);

    if (auto ptr = static_cast<d3d12_render_frame*>(m_frames.pop()))
    {
        *frame.data->d3d12 = std::move(*ptr);
    }
    else
    {
        ICY_ERROR(frame.data->d3d12->initialize(make_shared_from_this(this)));
    }

    ICY_ERROR(frame.data->d3d12->exec(list));
    return error_type();
}

error_type d3d12_display_system::initialize(const adapter::data_type& adapter, HWND__* const window, const display_flags flags) noexcept
{
    ICY_ERROR(adapter.query_d3d12(m_global));
    ICY_ERROR(m_fence.initialize(m_global->device()));

    const auto type = D3D12_COMMAND_LIST_TYPE_DIRECT;
    auto queue_desc = D3D12_COMMAND_QUEUE_DESC{ type };
    ICY_COM_ERROR(m_global->device().CreateCommandQueue(&queue_desc, IID_PPV_ARGS(&m_queue)));

    D3D12_DESCRIPTOR_HEAP_DESC heap_desc = {};
    heap_desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
    heap_desc.NumDescriptors = buffer_count(flags);
    ICY_COM_ERROR(m_global->device().CreateDescriptorHeap(&heap_desc, IID_PPV_ARGS(&m_heap)));
    ICY_COM_ERROR(m_global->device().CreateCommandAllocator(type, IID_PPV_ARGS(&m_alloc)));
    ICY_COM_ERROR(m_global->device().CreateCommandList(0, type, m_alloc, nullptr, IID_PPV_ARGS(&m_commands)));
    ICY_COM_ERROR(m_commands->Close());

    for (auto k = 0u; k < buffer_count(flags); ++k)
    {
        auto& buffer = m_buffers[k];
        buffer.view = CD3DX12_CPU_DESCRIPTOR_HANDLE(m_heap->GetCPUDescriptorHandleForHeapStart(), k,
            m_global->device().GetDescriptorHandleIncrementSize(heap_desc.Type)).ptr;
    }
    com_ptr<IDXGIFactory> factory;
    ICY_COM_ERROR(adapter.adapter()->GetParent(IID_PPV_ARGS(&factory)));
    ICY_ERROR(display_data::initialize(*m_queue, *factory, window, flags));
    ICY_COM_ERROR(factory->MakeWindowAssociation(window, DXGI_MWA_NO_ALT_ENTER | DXGI_MWA_NO_WINDOW_CHANGES | DXGI_MWA_NO_PRINT_SCREEN));
    
    ICY_ERROR(m_global->rename(*m_queue, "D3D12 Display Direct Queue"_s));
    ICY_ERROR(m_global->rename(*m_alloc, "D3D12 Display Allocator"_s));
    ICY_ERROR(m_global->rename(*m_commands, "D3D12 Display Commands"_s));
    ICY_ERROR(m_global->rename(*m_heap, "D3D12 Display Descriptor Heap"));

    auto root_desc = CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC(D3D12_DEFAULT);
    root_desc.Desc_1_1.Flags =
        D3D12_ROOT_SIGNATURE_FLAG_DENY_VERTEX_SHADER_ROOT_ACCESS |
        D3D12_ROOT_SIGNATURE_FLAG_DENY_HULL_SHADER_ROOT_ACCESS |
        D3D12_ROOT_SIGNATURE_FLAG_DENY_DOMAIN_SHADER_ROOT_ACCESS |
        D3D12_ROOT_SIGNATURE_FLAG_DENY_GEOMETRY_SHADER_ROOT_ACCESS;
    
    D3D12_DESCRIPTOR_RANGE1 range = {};
    range.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
    range.NumDescriptors = 1;
    range.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

    auto params = D3D12_ROOT_PARAMETER1{};
    params.DescriptorTable.pDescriptorRanges = &range;
    params.DescriptorTable.NumDescriptorRanges = 1;
    params.ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    params.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;   

    auto sampler = CD3DX12_STATIC_SAMPLER_DESC(0);
    sampler.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

    root_desc.Desc_1_1.pParameters = &params; 
    root_desc.Desc_1_1.NumParameters = 1;
    root_desc.Desc_1_1.pStaticSamplers = &sampler;
    root_desc.Desc_1_1.NumStaticSamplers = 1;
    ICY_ERROR(m_global->root(m_root, root_desc));

    const auto make_pipe = [this, &adapter](com_ptr<ID3D12PipelineState>& pipe,
        const const_array_view<uint8_t> pixel, const size_t msaa)
    {  
        const auto make_shader = [](const const_array_view<uint8_t> bytes)
        {
            return D3D12_SHADER_BYTECODE{ bytes.data(), bytes.size() };
        };
        D3D12_GRAPHICS_PIPELINE_STATE_DESC pipe_desc = {};
        pipe_desc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
        if (adapter.flags() & adapter_flags::debug)
            pipe_desc.Flags |= D3D12_PIPELINE_STATE_FLAG_TOOL_DEBUG;
        pipe_desc.VS = make_shader(g_shader_bytes_screen_vs);
        pipe_desc.PS = make_shader(pixel);
        pipe_desc.NumRenderTargets = 1;
        pipe_desc.SampleDesc.Count = 1;
        pipe_desc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
        pipe_desc.pRootSignature = m_root;
        pipe_desc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
        ICY_COM_ERROR(m_global->device().CreateGraphicsPipelineState(&pipe_desc, IID_PPV_ARGS(&pipe)));
        string str;
        ICY_ERROR(to_string("D3D12 Display PSO %1"_s, str, msaa));
        ICY_ERROR(m_global->rename(*pipe, str));
        return error_type();
    };
    ICY_ERROR(make_pipe(m_pipe1, g_shader_bytes_screen_ps, 1));
    ICY_ERROR(make_pipe(m_pipe2, g_shader_bytes_screen_ps2, 2));
    ICY_ERROR(make_pipe(m_pipe4, g_shader_bytes_screen_ps4, 4));
    ICY_ERROR(make_pipe(m_pipe8, g_shader_bytes_screen_ps8, 8));
    ICY_ERROR(make_pipe(m_pipe16, g_shader_bytes_screen_ps16, 16));

    filter(event_type::system_internal);
    return error_type();
}
error_type d3d12_display_system::resize(IDXGISwapChain& chain, const window_size size) noexcept
{
    return error_type();
}
error_type d3d12_display_system::repaint(IDXGISwapChain& chain, const bool vsync, void* const handle) noexcept
{
    /*
      DXGI_SWAP_CHAIN_DESC desc;
    ICY_COM_ERROR(chain.GetDesc(&desc));
    for (auto k = 0u; k < buffer_count(flags); ++k)
    {
        auto& buffer = m_buffers[k];
        buffer.texture = nullptr;
    }
    ICY_COM_ERROR(chain.ResizeBuffers(0, size.x, size.y, DXGI_FORMAT_UNKNOWN, desc.Flags));
    for (auto k = 0u; k < buffer_count(flags); ++k)
    {
        auto& buffer = m_buffers[k];
        ICY_COM_ERROR(chain.GetBuffer(k, IID_PPV_ARGS(&buffer.texture)));
        D3D12_RENDER_TARGET_VIEW_DESC desc = { DXGI_FORMAT_R8G8B8A8_UNORM, D3D12_RTV_DIMENSION_TEXTURE2D };
        m_global->device().CreateRenderTargetView(buffer.texture, &desc, d3d12_cpu_handle(buffer.view));

        string str;
        ICY_ERROR(to_string("D3D12 Display BackBuffer %1", str, k));
        ICY_ERROR(m_global->rename(*buffer.texture, str));
    }
    */
    com_ptr<IDXGISwapChain3> chain3;
    ICY_COM_ERROR(chain.QueryInterface(&chain3));
    com_ptr<ID3D12Resource> texture;
    com_ptr<ID3D12DescriptorHeap> heap;
   // ICY_ERROR(frame.data->d3d12->wait(texture, heap));

    const auto index = chain3->GetCurrentBackBufferIndex();
    auto& buffer = m_buffers[index];
    const auto barrier_beg = CD3DX12_RESOURCE_BARRIER::Transition(buffer.texture, D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET);
    const auto barrier_end = CD3DX12_RESOURCE_BARRIER::Transition(buffer.texture, D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT);
    
    ICY_COM_ERROR(m_commands->Reset(m_alloc, nullptr));

    ID3D12PipelineState* pipe = nullptr;
    /*const auto flags = frame.data->d3d12->flags();
    if (flags & render_flags::msaa_x16)
        pipe = m_pipe16;
    else if (flags & render_flags::msaa_x8)
        pipe = m_pipe8;
    else if (flags & render_flags::msaa_x4)
        pipe = m_pipe4;
    else if (flags & render_flags::msaa_x2)
        pipe = m_pipe2;
    else
        pipe = m_pipe1;*/

    const D3D12_CPU_DESCRIPTOR_HANDLE rtv[1] = { d3d12_cpu_handle(buffer.view) };
    const D3D12_RECT rect = { 0, 0, LONG(m_size.x), LONG(m_size.y) };
    const D3D12_VIEWPORT view = { 0, 0, float(m_size.x), float(m_size.y), 0, 1 };

    m_commands->ResourceBarrier(1, &barrier_beg);
   /* m_commands->SetPipelineState(pipe);
    m_commands->SetGraphicsRootSignature(m_root);
    m_commands->SetDescriptorHeaps(1, &heap);
    m_commands->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    m_commands->OMSetRenderTargets(_countof(rtv), rtv, FALSE, nullptr);
    m_commands->RSSetScissorRects(1, &rect);
    m_commands->RSSetViewports(1, &view);
    m_commands->DrawInstanced(3, 1, 0, 0);*/
   // float rgba[4];
   // color(colors::red).to_rgbaf(rgba);
    //m_commands->ClearRenderTargetView(rtv[0], rgba, 0, nullptr);
    m_commands->ResourceBarrier(1, &barrier_end);
    
    ICY_COM_ERROR(m_commands->Close());
    {
        ID3D12CommandList* const commands[] = { m_commands };
        m_queue->ExecuteCommandLists(_countof(commands), commands);
    }
    DXGI_PRESENT_PARAMETERS params = {};
    //if (const auto error = chain3->Present1(vsync, (vsync ? 0 : DXGI_PRESENT_ALLOW_TEARING), &params))
    if (const auto error = chain3->Present(vsync, 0))

    {
        if (error == DXGI_STATUS_OCCLUDED)
            ;
        else
            return make_system_error(error);
    }
    ICY_ERROR(m_fence.signal(*m_queue));
    ICY_ERROR(m_fence.wait_cpu());
    //frame.data->time_cpu = clock_type::now() - now;
    return error_type();
}

error_type create_d3d12(shared_ptr<render_factory>& system, const adapter& adapter, const render_flags flags) noexcept
{
    shared_ptr<d3d12_render_factory> new_ptr;
    ICY_ERROR(make_shared(new_ptr));
    ICY_ERROR(new_ptr->initialize(*adapter.data, flags));
    system = std::move(new_ptr);
    return error_type();
}
icy::error_type create_d3d12(d3d12_render_frame*& frame, shared_ptr<d3d12_render_factory> system)
{
    auto new_frame = allocator_type::allocate<d3d12_render_frame>(1);
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
error_type create_d3d12(shared_ptr<display_system>& system, const adapter& adapter, HWND__* const hwnd, const display_flags flags) noexcept
{
    shared_ptr<d3d12_display_system> new_ptr;
    ICY_ERROR(make_shared(new_ptr));
    ICY_ERROR(new_ptr->initialize(*adapter.data, hwnd, flags));
    system = std::move(new_ptr);
    return error_type();
}
error_type wait_frame(d3d12_render_frame& frame, void*& handle) noexcept
{
    return make_stdlib_error(std::errc::function_not_supported);
}
void destroy_frame(d3d12_render_frame& frame) noexcept
{
    frame.destroy(false);
}
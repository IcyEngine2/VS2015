#include <icy_engine/graphics/icy_window.hpp>
#include "icy_d3d12.hpp"
#include "d3dx12.h"
#include <dxgi1_6.h>

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
    m_handle = CreateEventW(nullptr, FALSE, FALSE, nullptr);
    if (!m_handle)
        return last_system_error();
    return {};
}

error_type d3d12_fence::initialize(ID3D12Device& device) noexcept
{
    ICY_ERROR(m_event.initialize());
    ICY_COM_ERROR(device.CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&m_handle)));
    return {};
}
error_type d3d12_fence::wait(const duration_type timeout) noexcept
{
    if (m_handle->GetCompletedValue() < m_value)
    {
        const auto wait = ::WaitForSingleObject(m_event.handle(), ms_timeout(timeout));
        if (wait != WAIT_OBJECT_0)
            return last_system_error();
    }
    return {};
}
error_type d3d12_fence::signal(ID3D12CommandQueue& queue) noexcept
{
    ICY_COM_ERROR(queue.Signal(m_handle, m_value + 1));
    ICY_COM_ERROR(m_handle->SetEventOnCompletion(m_value + 1, m_event.handle()));
    ++m_value;
    return {};
}
uint64_t d3d12_fence::gpu_value() const noexcept
{
    return m_handle->GetCompletedValue();
}

error_type d3d12_command_list::initialize(ID3D12Device& device, const D3D12_COMMAND_LIST_TYPE type) noexcept
{
    ICY_COM_ERROR(device.CreateCommandAllocator(type, IID_PPV_ARGS(&m_alloc)));
    ICY_COM_ERROR(device.CreateCommandList(0, type, m_alloc, nullptr, IID_PPV_ARGS(&m_commands)));
    ICY_COM_ERROR(m_commands->Close());
    return {};
}
error_type d3d12_command_list::reset() noexcept
{
    ICY_COM_ERROR(m_alloc->Reset());
    ICY_COM_ERROR(m_commands->Reset(m_alloc, nullptr));
    return {};
}

error_type d3d12_view_heap::initialize(com_ptr<ID3D12Device> device, const size_type& capacity) noexcept
{
    m_device = device;

    decltype(m_heaps) heaps = {};
    for (auto k = 0u; k < D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES; ++k)
    {
        if (!capacity[k])
            continue;

        D3D12_DESCRIPTOR_HEAP_DESC heap_desc = {};
        heap_desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE(k);
        heap_desc.NumDescriptors = capacity[k];
        ICY_COM_ERROR(device->CreateDescriptorHeap(&heap_desc, IID_PPV_ARGS(&heaps[k])));
    }
    m_capacity = capacity;
    m_heaps = std::move(heaps);
    m_size = {};
    return {};
}
error_type d3d12_view_heap::push(const D3D12_DESCRIPTOR_HEAP_TYPE type, size_t& view) noexcept
{
    if (m_size[type] >= m_capacity[type])
        return make_stdlib_error(std::errc::not_enough_memory);

    const auto view_size = device().GetDescriptorHandleIncrementSize(type);
    view = CD3DX12_CPU_DESCRIPTOR_HANDLE(m_heaps[type]->GetCPUDescriptorHandleForHeapStart(), m_size[type], view_size).ptr;
    m_size[type]++;
    return {};
}

error_type d3d12_texture::initialize(d3d12_view_heap& heap, const D3D12_RESOURCE_FLAGS flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET) noexcept
{
    if (flags & D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL) 
    {
        ICY_ERROR(heap.push(D3D12_DESCRIPTOR_HEAP_TYPE_DSV, m_dsv));
    }
    if (flags & D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET) 
    {
        ICY_ERROR(heap.push(D3D12_DESCRIPTOR_HEAP_TYPE_RTV, m_rtv)); 
    }
    if (!(flags & D3D12_RESOURCE_FLAG_DENY_SHADER_RESOURCE))
    {
        ICY_ERROR(heap.push(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, m_srv));
    }
    m_flags = flags;
    m_device = com_ptr<ID3D12Device>(&heap.device());
    return {};
}
error_type d3d12_texture::resize(const window_size size, const DXGI_FORMAT format) noexcept
{
    const auto dsv_format = DXGI_FORMAT_R24_UNORM_X8_TYPELESS;
    const auto rtv_format = format;
    const auto srv_format = format;

    const auto heap_prop = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
    auto resource_desc = CD3DX12_RESOURCE_DESC::Tex2D(format, size.w, size.h, 1, 1);
    resource_desc.Flags = m_flags;

    auto state = D3D12_RESOURCE_STATE_COMMON;
    if (m_flags & D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL)
        state = D3D12_RESOURCE_STATE_DEPTH_WRITE;
    else if (m_flags & D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET)
        state = D3D12_RESOURCE_STATE_RENDER_TARGET;
    else
        return make_stdlib_error(std::errc::invalid_argument);

    com_ptr<ID3D12Resource> buffer;
    ICY_COM_ERROR(m_device->CreateCommittedResource(&heap_prop, D3D12_HEAP_FLAG_NONE,
        &resource_desc, state, nullptr, IID_PPV_ARGS(&buffer)));
    m_buffer = buffer;

    if (m_dsv)
    {
        auto dsv_desc = D3D12_DEPTH_STENCIL_VIEW_DESC{};
        dsv_desc.Format = dsv_format;
        dsv_desc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
        m_device->CreateDepthStencilView(buffer, &dsv_desc, d3d12_cpu_handle(m_dsv));
    }
    if (m_rtv)
    {
        auto rtv_desc = D3D12_RENDER_TARGET_VIEW_DESC{};
        rtv_desc.Format = rtv_format;
        rtv_desc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
        m_device->CreateRenderTargetView(buffer, &rtv_desc, d3d12_cpu_handle(m_rtv));
    }
    if (m_srv)
    {
        auto srv_desc = D3D12_SHADER_RESOURCE_VIEW_DESC{};
        srv_desc.Format = srv_format;
        srv_desc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
        m_device->CreateShaderResourceView(buffer, &srv_desc, d3d12_cpu_handle(m_srv));
    }
    return {};
}

error_type d3d12_swap_chain::window(HWND__*& hwnd) const noexcept
{
    ICY_COM_ERROR(m_chain->GetHwnd(&hwnd));
    return {};
}
error_type d3d12_swap_chain::present(const uint32_t vsync) noexcept
{
    DXGI_PRESENT_PARAMETERS params = {};
    ICY_COM_ERROR(m_chain->Present1(vsync, 0, &params));
    m_buffer = m_chain->GetCurrentBackBufferIndex();
    return {};
}
error_type d3d12_swap_chain::size(window_size& output) const noexcept
{
    DXGI_SWAP_CHAIN_DESC1 desc = {};
    ICY_COM_ERROR(m_chain->GetDesc1(&desc));
    output = { desc.Width, desc.Height };
    return {};
}
error_type d3d12_swap_chain::initialize(IDXGIFactory2& factory, ID3D12CommandQueue& queue, HWND__* const hwnd, d3d12_view_heap& heap, const display_flag flags) noexcept
{
    com_ptr<ID3D12Device> device;
    ICY_COM_ERROR(queue.GetDevice(IID_PPV_ARGS(&device)));

    DXGI_SWAP_CHAIN_DESC1 desc = {};
    desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    desc.SampleDesc = { 1, 0 };
    desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    desc.BufferCount = d3d12_buffer_count(flags);
    desc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL;
    desc.Flags |= DXGI_SWAP_CHAIN_FLAG_FRAME_LATENCY_WAITABLE_OBJECT;
    com_ptr<IDXGISwapChain1> chain;
    ICY_COM_ERROR(factory.CreateSwapChainForHwnd(&queue, hwnd, &desc, nullptr, nullptr, &chain));
    ICY_COM_ERROR(chain->QueryInterface(&m_chain));


    ICY_ERROR(m_buffers.resize(desc.BufferCount));
    ICY_ERROR(m_views.resize(desc.BufferCount));
    for (auto k = 0u; k < desc.BufferCount; ++k)
    {
        ICY_COM_ERROR(m_chain->GetBuffer(k, IID_PPV_ARGS(&m_buffers[k])));
        ICY_ERROR(heap.push(D3D12_DESCRIPTOR_HEAP_TYPE_RTV, m_views[k]));
        device->CreateRenderTargetView(m_buffers[k], nullptr, d3d12_cpu_handle(m_views[k]));
    }
    m_chain_wait = m_chain->GetFrameLatencyWaitableObject();
    ICY_COM_ERROR(m_chain->SetMaximumFrameLatency(2));
    return {};
}
error_type d3d12_swap_chain::resize() noexcept
{
    for (auto&& buffer : m_buffers)
        buffer = nullptr;

    com_ptr<ID3D12Device> device;
    ICY_COM_ERROR(m_chain->GetDevice(IID_PPV_ARGS(&device)));
    ICY_COM_ERROR(m_chain->ResizeBuffers(uint32_t(m_buffers.size()), 0, 0,
        DXGI_FORMAT_UNKNOWN, DXGI_SWAP_CHAIN_FLAG_FRAME_LATENCY_WAITABLE_OBJECT));

    for (auto k = 0u; k < m_buffers.size(); ++k)
    {
        ICY_COM_ERROR(m_chain->GetBuffer(k, IID_PPV_ARGS(&m_buffers[k])));
        device->CreateRenderTargetView(m_buffers[k], nullptr, d3d12_cpu_handle(m_views[k]));
    }
    return {};
}

d3d12_display::~d3d12_display() noexcept
{
    m_fence.signal(*m_queue);
    m_fence.wait();
}
error_type d3d12_display::initialize(const adapter& adapter, const display_flag flags) noexcept
{
    m_adapter = adapter;
    m_flags = flags;
    ICY_ERROR(m_event_wait.initialize());

    ICY_ERROR(m_d3d12_lib.initialize());

    if (flags & display_flag::debug_layer)
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

    if (flags & display_flag::debug_layer)
    {
        if (auto debug = com_ptr<ID3D12InfoQueue>(m_device))
        {
            debug->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_CORRUPTION, TRUE);
            debug->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_ERROR, TRUE);
            debug->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_WARNING, TRUE);

            D3D12_MESSAGE_SEVERITY severities[] = { D3D12_MESSAGE_SEVERITY_INFO };
            // Suppress individual messages by their ID
            D3D12_MESSAGE_ID deny[] =
            {
                D3D12_MESSAGE_ID_CLEARRENDERTARGETVIEW_MISMATCHINGCLEARVALUE,   // I'm really not sure how to avoid this message.
                D3D12_MESSAGE_ID_MAP_INVALID_NULLRANGE,                         // This warning occurs when using capture frame while graphics debugging.
                D3D12_MESSAGE_ID_UNMAP_INVALID_NULLRANGE,                       // This warning occurs when using capture frame while graphics debugging.
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

    d3d12_view_heap::size_type heap_size = {};
    heap_size[D3D12_DESCRIPTOR_HEAP_TYPE_RTV] += d3d12_buffer_count(flags);
    /*for (auto&& render : m_renders)
    {
        const auto render_heap = render->heap_size();
        for (auto k = 0u; k < D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES; ++k)
            heap_size[k] += render_heap[k];

        if (render->type() == d3d12_render_type::shading ||
            render->type() == d3d12_render_type::postproc)
            m_render = render.get();
    }*/
    ICY_ERROR(m_heap.initialize(m_device, heap_size));

  //  for (auto&& render : m_renders)
  //      ICY_ERROR(render->initialize(m_heap));

    ICY_ERROR(m_commands.resize(d3d12_buffer_count(flags)));
    for (auto&& command : m_commands)
        ICY_ERROR(command.initialize(*m_device, D3D12_COMMAND_LIST_TYPE_DIRECT));

    filter(event_type::none
        | event_type::window_resize
        | event_type::window_active
        | event_type::window_minimized
        | event_type::window_close);
    return {};
}
error_type d3d12_display::bind(HWND__* const window) noexcept
{
    com_ptr<IDXGIFactory2> factory;
    ICY_COM_ERROR(static_cast<IDXGIAdapter*>(m_adapter.handle())->GetParent(IID_PPV_ARGS(&factory)));
    ICY_COM_ERROR(factory->MakeWindowAssociation(window, DXGI_MWA_NO_ALT_ENTER | DXGI_MWA_NO_WINDOW_CHANGES | DXGI_MWA_NO_PRINT_SCREEN));
    ICY_ERROR(m_chain.initialize(*factory, *m_queue, window, m_heap, m_flags));

    window_size size = {};
    ICY_ERROR(m_chain.size(size));

    //for (auto&& render : m_renders)
    //    ICY_ERROR(render->resize(size));

    for (auto i = 0u; i < d3d12_buffer_count(m_flags); ++i)
    {
        //auto render_count = m_renders.size() + 1;
        //if (!m_render)
        //    render_count += 1;

        //for (auto j = 0u; j < render_count; ++j)
        //    ICY_ERROR(m_state.push_back(2 * i + j));
    }

    return {};
}
error_type d3d12_display::loop(const duration_type timeout) noexcept
{
    while (true)
    {
        while (auto event = pop())
        {
            if (event->type == event_type::global_quit)
            {
                return error_type{};
            }
            else if (event->type == event_type::window_close && event->source && event->data<bool>())
            {
                HWND__* hwnd = nullptr;
                ICY_ERROR(m_chain.window(hwnd));

                auto src = shared_ptr<event_queue>(event->source);
                if (src && hwnd == static_cast<const window*>(src.get())->handle())
                    return error_type{};
            }
        }
        //ICY_ERROR(m_render_system.loop());

        //  0: event, 1: fence, 2? swap chain
        std::array<HANDLE, 3> handles = { };
        auto handles_count = 0u;
        handles[handles_count++] = m_event_wait.handle();

        if (!m_ready)
            handles[handles_count++] = m_chain.event();

        if (m_fence.gpu_value() < m_fence.cpu_value())
        {
            handles[handles_count++] = m_fence.event();
        }
        else // ready for new list
        {
            const auto render_count = m_state.size() / d3d12_buffer_count(m_flags);
            array<std::pair<size_t, size_t>> states;

            auto present = false;
            for (auto k = 0u; k < m_state.size(); ++k)
            {
                if (m_state[k] != m_fence.cpu_value())
                    continue;

                const auto i = k / render_count;
                const auto j = k % render_count;
                if (j + 1 == render_count)
                {
                    if (!m_ready)
                    {
                        states.clear();
                        break;
                    }
                    if (i == m_chain.buffer())
                        present = true;
                    else
                    {
                        ICY_ASSERT(false, "INVALID RENDER STATE");
                    }
                }
                ICY_ERROR(states.push_back({ i, j }));
            }

            if (!states.empty())
            {
                if (present)
                {
                    ICY_ERROR(m_chain.present(0));
                    m_frame.fetch_add(1, std::memory_order_release);
                }
                auto command_index = 0u;
                std::array<ID3D12CommandList*, d3d12_buffer_count(display_flag::triple_buffer)> command_list = {};
                for (auto&& pair : states)
                {
                    const auto i = pair.first;
                    const auto j = pair.second;
                    if (j + 1 == render_count)
                        continue;   //  'present'

                    auto& command = m_commands[command_index];
                    command_list[command_index] = command.get();
                    ICY_ERROR(command.reset());

                    if (true) //if (!m_render && j + 2 == render_count)
                    {
                        const auto barrier_beg = CD3DX12_RESOURCE_BARRIER::Transition(m_chain.resource(),
                            D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET);
                        command->ResourceBarrier(1, &barrier_beg);
                        float color[] = { 0, (frame() % 256) / 255.0F, 0, 1 };
                        command->ClearRenderTargetView(d3d12_cpu_handle(m_chain.view()), color, 0, nullptr);
                        const auto barrier_end = CD3DX12_RESOURCE_BARRIER::Transition(m_chain.resource(),
                            D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT);
                        command->ResourceBarrier(1, &barrier_end);
                    }
                    else
                    {
                        //ICY_ERROR(m_renders[j]->render(j ? m_renders[j - 1].get() : nullptr, *command.get()));
                    }
                    ++command_index;
                    ICY_COM_ERROR(command->Close());
                }

                if (command_index)
                    m_queue->ExecuteCommandLists(command_index, command_list.data());

                ICY_ERROR(m_fence.signal(*m_queue));
                for (auto&& pair : states)
                    m_state[pair.first * render_count + pair.second] += m_state.size();
            }
            continue;
        }

        const auto wait = WaitForMultipleObjectsEx(handles_count, handles.data(), FALSE, ms_timeout(timeout), TRUE);
        if (wait == WAIT_TIMEOUT)
        {
            ICY_ERROR(event::post(this, event_type::global_timeout));
            continue;
        }
        else if (wait == WAIT_FAILED)
        {
            return last_system_error();
        }
        const auto handle = handles[wait - WAIT_OBJECT_0];
        if (handle == m_chain.event())
            m_ready = true;
    }
}
error_type d3d12_display::signal(const event_data&) noexcept
{
    if (!SetEvent(m_event_wait.handle()))
        return last_system_error();
    return {};
}

namespace icy
{
    error_type display_create_d3d12(const adapter& adapter, const display_flag flags, shared_ptr<display>& display) noexcept
    {
        shared_ptr<d3d12_display> d3d12;
        ICY_ERROR(make_shared(d3d12));
        ICY_ERROR(d3d12->initialize(adapter, flags));
        display = d3d12;
        return {};
    }
}
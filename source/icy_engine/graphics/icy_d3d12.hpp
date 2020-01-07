#pragma once

#include <icy_engine/core/icy_array.hpp>
#include <icy_engine/graphics/icy_graphics.hpp>
#include <icy_engine/utility/icy_com.hpp>
#include <array>

struct ID3D12Device;
struct ID3D12CommandQueue;
struct ID3D12CommandAllocator;
struct ID3D12Resource;
struct IDXGISwapChain3;
struct ID3D12Fence;
struct IDXGIFactory2;
struct ID3D12GraphicsCommandList;
struct ID3D12DescriptorHeap;
enum D3D12_COMMAND_LIST_TYPE;
enum DXGI_FORMAT;
enum D3D12_RESOURCE_FLAGS;
enum D3D12_DESCRIPTOR_HEAP_TYPE;

namespace icy
{
    class d3d12_event;
    class d3d12_fence;
    class d3d12_command_list;
    class d3d12_view_heap;
    class d3d12_texture;
    class d3d12_swap_chain;
    class d3d12_display;

    inline constexpr uint32_t d3d12_buffer_count(const display_flag flag) noexcept
    {
        return flag & display_flag::triple_buffer ? 3 : 2;
    }
    
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
        error_type initialize() noexcept;
    private:
        void* m_handle = nullptr;
    };
    class d3d12_fence
    {
    public:
        error_type initialize(ID3D12Device& device) noexcept;
        error_type wait(const duration_type timeout = max_timeout) noexcept;
        error_type signal(ID3D12CommandQueue& queue) noexcept;
        uint64_t cpu_value() const noexcept
        {
            return m_value;
        }
        uint64_t gpu_value() const noexcept;
        void* event() const noexcept
        {
            return m_event.handle();
        }
    private:
        uint64_t m_value = 0;
        d3d12_event m_event;
        com_ptr<ID3D12Fence> m_handle;
    };
    class d3d12_command_list
    {
    public:
        error_type initialize(ID3D12Device& device, const D3D12_COMMAND_LIST_TYPE type) noexcept;
        error_type reset() noexcept;
        ID3D12GraphicsCommandList* operator->() const noexcept
        {
            return m_commands;
        }
        ID3D12GraphicsCommandList* get() const noexcept
        {
            return m_commands;
        }
    private:
        com_ptr<ID3D12CommandAllocator> m_alloc;
        com_ptr<ID3D12GraphicsCommandList> m_commands;
    };
    class d3d12_view_heap
    {
    public:
        using size_type = std::array<uint32_t, 4>;
        error_type initialize(com_ptr<ID3D12Device> device, const size_type& capacity) noexcept;
        error_type push(const D3D12_DESCRIPTOR_HEAP_TYPE type, size_t& view) noexcept;
        void clear() noexcept
        {
            m_size = {};
        }
        ID3D12Device& device() const noexcept
        {
            return *m_device;
        }
    private:
        com_ptr<ID3D12Device> m_device;
        std::array<com_ptr<ID3D12DescriptorHeap>, 4> m_heaps;
        size_type m_capacity = {};
        size_type m_size = {};
    };
    class d3d12_texture
    {
    public:
        error_type initialize(d3d12_view_heap& heap, const D3D12_RESOURCE_FLAGS flags) noexcept;
        error_type resize(const window_size size, const DXGI_FORMAT format) noexcept;
        ID3D12Resource& resource() const noexcept { return *m_buffer; }
        size_t dsv() const noexcept { return m_dsv; }
        size_t rtv() const noexcept { return m_rtv; }
        size_t srv() const noexcept { return m_srv; }
    private:
        com_ptr<ID3D12Device> m_device;
        D3D12_RESOURCE_FLAGS m_flags = D3D12_RESOURCE_FLAGS(0);
        com_ptr<ID3D12Resource> m_buffer;
        size_t m_dsv = {};
        size_t m_rtv = {};
        size_t m_srv = {};
    };
    class d3d12_swap_chain
    {
    public:
        error_type initialize(IDXGIFactory2& factory, ID3D12CommandQueue& queue, HWND__* const hwnd, d3d12_view_heap& heap, const display_flag flags) noexcept;
        error_type resize() noexcept;
        uint32_t buffer() const noexcept
        {
            return m_buffer;
        }
        void* event() const noexcept
        {
            return m_chain_wait;
        }
        error_type window(HWND__*& hwnd) const noexcept;
        error_type present(const uint32_t vsync = 1) noexcept;
        size_t view() const noexcept
        {
            return m_views[buffer()];
        }
        com_ptr<ID3D12Resource> resource() const noexcept
        {
            return m_buffers[buffer()];
        }
        error_type size(window_size& output) const noexcept;
    private:
        com_ptr<IDXGISwapChain3> m_chain;
        void* m_chain_wait = nullptr;
        uint32_t m_buffer = 0;
        array<com_ptr<ID3D12Resource>> m_buffers;
        array<size_t> m_views = {};
    };
    class d3d12_display : public display
    {
    public:
        ~d3d12_display() noexcept;
        error_type initialize(const adapter& adapter, const display_flag flags) noexcept;
        error_type bind(HWND__* const window) noexcept override;
        error_type bind(window& window) noexcept override;
        error_type draw() noexcept override;
        uint64_t frame() const noexcept
        {
            return m_frame.load(std::memory_order_acquire);
        }
        ID3D12Device& device() const noexcept
        {
            return *m_device;
        }
        display_flag flags() const noexcept override
        {
            return m_flags;
        }
    private:
        adapter m_adapter;
        display_flag m_flags = display_flag::none;
        d3d12_event m_event_wait;
        library m_d3d12_lib = "d3d12"_lib;
        com_ptr<ID3D12Device> m_device;
        com_ptr<ID3D12CommandQueue> m_queue;
        d3d12_fence m_fence;
        std::atomic<uint64_t> m_frame = 0;
        d3d12_view_heap m_heap;
        d3d12_swap_chain m_chain;
        array<d3d12_command_list> m_commands;
        array<uint64_t> m_state;
        bool m_ready = false; //  can call swap chain 'Present'
    };
}
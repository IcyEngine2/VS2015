#pragma once

#include <icy_engine/core/icy_array.hpp>
#include <icy_engine/core/icy_map.hpp>
#include <icy_engine/graphics/icy_render_svg.hpp>
#include <icy_engine/graphics/icy_render.hpp>
#include "icy_graphics.hpp"

struct ID3D12Device;
struct ID3D12CommandQueue;
struct ID3D12Fence;
struct ID3D12CommandAllocator;
struct ID3D12GraphicsCommandList;
struct IDXGIFactory;
struct IDXGISwapChain3;
struct ID3D12RootSignature;
struct ID3D12Resource;
struct ID3D12DescriptorHeap;

namespace icy
{
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
    class d3d12_swap_chain
    {
    public:
        error_type initialize(IDXGIFactory& factory, ID3D12CommandQueue& queue, HWND__* const hwnd, const window_flags flags) noexcept;
        error_type resize() noexcept;
        error_type window(HWND__*& hwnd) const noexcept;
        error_type present(const uint32_t vsync = 1) noexcept;
        error_type size(window_size& output) const noexcept;
        void* event() const noexcept
        {
            return m_chain_wait;
        }
        error_type buffer(const size_t index, com_ptr<ID3D12Resource>& texture) const noexcept;
        size_t index() const noexcept;
    private:
        com_ptr<IDXGISwapChain3> m_chain;
        void* m_chain_wait = nullptr;
    };

    class d3d12_render_svg
    {
    public:
        error_type initialize(ID3D12Device& device) noexcept;
        //void operator()(ID3D11DeviceContext& context) const noexcept;
    private:
        com_ptr<ID3D12RootSignature> m_root;
    };
    struct d3d12_render
    {
        d3d12_render_svg svg;
    };

    class d3d12_back_buffer
    {
    public:
        d3d12_back_buffer() noexcept = default;
        ~d3d12_back_buffer() noexcept;
        d3d12_back_buffer(d3d12_back_buffer&&) noexcept;
        ICY_DEFAULT_MOVE_ASSIGN(d3d12_back_buffer);
        error_type initialize(const com_ptr<ID3D12CommandQueue> queue, const com_ptr<ID3D12Resource> chain_buffer, const window_flags flags) noexcept;
        error_type update(const render_list& list, const d3d12_render& render) noexcept;
        error_type draw() noexcept;
        //error_type draw(ID3D12RenderTargetView& rtv) noexcept;
    private:
        struct data_type
        {
            d3d12_fence fence;
            com_ptr<ID3D12CommandAllocator> alloc;
            com_ptr<ID3D12GraphicsCommandList> commands;
            com_ptr<ID3D12Resource> buffer;
            size_t view = 0;
        };
        window_size m_size;
        com_ptr<ID3D12CommandQueue> m_queue;
        com_ptr<ID3D12DescriptorHeap> m_heap;
        data_type m_chain;
        data_type m_render;
    };
    
    class d3d12_display : public display
    {
    public:
        ~d3d12_display() noexcept;
        error_type initialize(const adapter& adapter, const window_flags flags) noexcept;
        error_type bind(HWND__* const window) noexcept;
        error_type draw(const size_t frame, const bool vsync) noexcept override;
        error_type resize() noexcept override;
        void* event() noexcept override
        {
            return m_chain.event();
        }
        error_type update(const render_list& list) noexcept override;
    private:
        adapter m_adapter;
        window_flags m_flags = window_flags::none;
        library m_d3d12_lib = "d3d12"_lib;
        com_ptr<ID3D12Device> m_device;
        com_ptr<ID3D12CommandQueue> m_queue;
        d3d12_render m_render;
        d3d12_fence m_fence;
        d3d12_swap_chain m_chain;
        array<d3d12_back_buffer> m_buffers;
        size_t m_frame = 0;
    };
}
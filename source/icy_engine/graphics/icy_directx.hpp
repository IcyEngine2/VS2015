#pragma once

#include <icy_engine/core/icy_array.hpp>
#include <icy_engine/core/icy_string.hpp>
#include <icy_engine/core/icy_smart_pointer.hpp>
#include <icy_engine/core/icy_queue.hpp>
#include <icy_engine/core/icy_set.hpp>
#include <icy_engine/graphics/icy_gpu.hpp>
#include <icy_engine/graphics/icy_display.hpp>
#include <icy_engine/graphics/icy_render.hpp>
#include <icy_engine/utility/icy_com.hpp>
#include <dxgi1_6.h>
#include <dxgidebug.h>
#include <d3d12.h>
#include <d3d11.h>

namespace icy
{
    enum class directx_render_internal_event_type : uint32_t
    {
        none,
        repaint,
    };
    enum directx_material_state : uint32_t
    {
        directx_material_none,
        directx_material_ready_vtx,
        directx_material_ready_obj,
    };
    using directx_vec4 = __m128;
    using directx_vec2 = render_vec2;
    static const uint32_t directx_max_bone_weights = 64;
    struct directx_transform
    {
        directx_vec4 world;
        directx_vec4 scale;
        directx_vec4 angle;
    };

    class directx_gpu_device;
    class directx_system
    {
        friend directx_gpu_device;
    public:
        directx_system(const gpu_flags flags) noexcept : m_flags(flags)
        {

        }
        ~directx_system() noexcept
        {

        }
        error_type initialize() noexcept;
        error_type create_gpu_list(array<shared_ptr<gpu_device>>& list) const noexcept;
    private:
        const gpu_flags m_flags;
        library m_lib_dxgi = "dxgi"_lib;
        library m_lib_d3d12 = "d3d12"_lib;
        library m_lib_d3d11 = "d3d11"_lib;
        library m_lib_debug = "dxgidebug"_lib;
    };
    class directx_gpu_device : public gpu_device
    {
    public:
        directx_gpu_device(const shared_ptr<directx_system> system) noexcept : m_system(system)
        {

        }
        error_type initialize(const DXGI_ADAPTER_DESC1& desc, com_ptr<IDXGIAdapter> handle, const gpu_flags flags) noexcept;
        error_type create_device(com_ptr<ID3D11Device>& device) noexcept;
        void* handle() const noexcept override
        {
            return m_handle.get();
        }
    private:
        string_view name() const noexcept override
        {
            return m_name;
        }
        error_type msaa(render_flags& flags) const noexcept override;

        size_t vid_mem() const noexcept override
        {
            return m_vid_mem;
        }
        size_t sys_mem() const noexcept override
        {
            return m_sys_mem;
        }
        size_t shr_mem() const noexcept override
        {
            return m_shr_mem;
        }
        uint64_t luid() const noexcept override
        {
            return m_luid;
        }
        gpu_flags flags() const noexcept override
        {
            return m_flags;
        }
    private:
        shared_ptr<directx_system> m_system;
        string m_name;
        com_ptr<IDXGIAdapter> m_handle;
        gpu_flags m_flags = gpu_flags::none;
        size_t m_vid_mem = 0;
        size_t m_sys_mem = 0;
        size_t m_shr_mem = 0;
        uint64_t m_luid = 0;
    };
    struct directx_display_internal_event
    {
        duration_type frame = duration_type(-1);
        window_size size;
    };
    struct directx_display_texture
    {
        window_size offset;
        window_size size;
        com_ptr<ID3D11Texture2D> buffer;
        com_ptr<ID3D11ShaderResourceView> srv;
    };
    class directx_display_system : public display_system
    {
    public:
        directx_display_system(const shared_ptr<gpu_device> adapter, shared_ptr<window> hwnd) : m_adapter(adapter), m_hwnd(hwnd)
        {

        }
        ~directx_display_system() noexcept;
        error_type initialize() noexcept;
        error_type do_resize(const window_size size) noexcept;
        error_type do_repaint(const bool vsync) noexcept;
    private:
        error_type exec() noexcept override;
        error_type signal(const event_data* event) noexcept override;
        const icy::thread& thread() const noexcept override
        {
            return *m_thread;
        }
        error_type repaint(const string_view tag, render_gui_frame& frame) noexcept override;
        error_type repaint(const string_view tag, shared_ptr<render_surface> scene) noexcept override;
        error_type resize(const window_size size) noexcept override
        {
            if (size.x == 0 || size.y == 0)
                return make_stdlib_error(std::errc::invalid_argument);

            directx_display_internal_event msg;
            msg.size = size;
            return event_system::post(nullptr, event_type::system_internal, msg);
        }
        error_type frame(const duration_type duration) noexcept override
        {
            if (duration.count() < 0)
                return make_stdlib_error(std::errc::invalid_argument);

            directx_display_internal_event msg;
            msg.frame = duration;
            return event_system::post(nullptr, event_type::system_internal, msg);
        }
    private:
        struct frame_type
        {
            size_t index = 0;
            duration_type delta = display_frame_vsync;
            clock_type::time_point next = {};
            mpsc_queue<std::pair<string, render_gui_frame>> queue2d;
            mpsc_queue<std::pair<string, directx_display_texture>> queue3d;
            map<string, render_gui_frame> map2d;
            map<string, directx_display_texture> map3d;
        };
    private:
        shared_ptr<directx_gpu_device> m_adapter;
        weak_ptr<window> m_hwnd;
        com_ptr<ID3D11Device> m_device;
        com_ptr<IDXGISwapChain> m_chain;
        com_ptr<ID3D11VertexShader> m_vshader2d;
        com_ptr<ID3D11VertexShader> m_vshader3d;
        com_ptr<ID3D11PixelShader> m_pshader2d;
        com_ptr<ID3D11PixelShader> m_pshader3d;
        com_ptr<ID3D11InputLayout> m_layout2d;
        com_ptr<ID3D11BlendState> m_blend2d;
        com_ptr<ID3D11RasterizerState> m_raster2d;
        com_ptr<ID3D11SamplerState> m_sampler;
        void* m_gpu_event = nullptr;
        void* m_cpu_timer = nullptr;
        sync_handle m_update;
        bool m_gpu_ready = true;
        bool m_cpu_ready = true;
        shared_ptr<event_thread> m_thread;
        frame_type m_frame;
        com_ptr<ID3D11RenderTargetView> m_rtv;
        com_ptr<ID3D11Buffer> m_vbuffer2d;
        com_ptr<ID3D11Buffer> m_ibuffer2d;
        map<guid, com_ptr<ID3D11ShaderResourceView>> m_tex2d;
    };

    struct directx_render_node;
    struct directx_render_mesh;
    struct directx_render_material;
    class directx_render_system;
    class directx_render_surface : public render_surface
    {
        friend directx_render_system;
    public:
        directx_render_surface(directx_render_system& system, const uint32_t index) noexcept :
            m_system(make_shared_from_this(&system)), m_index(index)
        {

        }
        shared_ptr<render_system> system() noexcept override;
        uint32_t index() const noexcept override
        {
            return m_index;
        }
    private:
        weak_ptr<directx_render_system> m_system;
        uint32_t m_index = 0;
        com_ptr<ID3D11Texture2D> m_buffer;
        com_ptr<ID3D11RenderTargetView> m_rtv;
        com_ptr<ID3D11ShaderResourceView> m_srv;
    };
    struct directx_render_texture 
    {
        com_ptr<ID3D11Texture2D> buffer;
        com_ptr<ID3D11RenderTargetView> rtv;
    };
    struct directx_render_internal_event
    {
        directx_render_internal_event_type type = directx_render_internal_event_type::none;
        window_size size;
    };
    struct directx_render_node
    {
        directx_transform transform;
        set<directx_render_mesh*> meshes;
        set<directx_render_material*> materials;
    };
    struct directx_render_material : public render_material
    {
        directx_render_material() noexcept = default;
        directx_render_material(directx_render_material&& rhs) noexcept = default;
        ICY_DEFAULT_MOVE_ASSIGN(directx_render_material);
        uint32_t state = 0;
        com_ptr<ID3D11Buffer> vtx_world;
        com_ptr<ID3D11Buffer> vtx_toobj;
        com_ptr<ID3D11Buffer> vtx_angle;
        com_ptr<ID3D11Buffer> vtx_color[render_channel_count];
        com_ptr<ID3D11Buffer> vtx_texuv[render_channel_count];
        com_ptr<ID3D11Buffer> idx_array;
        com_ptr<ID3D11Buffer> obj_world;
        com_ptr<ID3D11Buffer> obj_angle;
        com_ptr<ID3D11Buffer> obj_scale;

        set<directx_render_mesh*> meshes;
        set<directx_render_node*> nodes;
    };
    struct directx_render_weight
    {
        uint32_t vertex = 0;
        float value = 0;
    };
    struct directx_render_bone
    {
        directx_render_weight weights[directx_max_bone_weights];
        directx_transform transform;
    };
    struct directx_render_mesh
    {
        directx_render_mesh() noexcept = default;
        directx_render_mesh(directx_render_mesh&& rhs) noexcept = default;
        ICY_DEFAULT_MOVE_ASSIGN(directx_render_mesh);
        render_mesh_type type = render_mesh_type::none;
        array<directx_vec4> world;
        array<directx_vec4> angle;
        array<directx_vec4> color[render_channel_count];
        array<directx_vec2> texuv[render_channel_count];
        array<uint32_t> indices;
        array<directx_render_bone> bones;
        directx_render_material* material = nullptr;
        set<directx_render_node*> nodes;
    };
    
    class directx_render_system : public render_system
    {
    public:
        directx_render_system(const shared_ptr<gpu_device> adapter) : m_adapter(adapter)
        {

        }
        ~directx_render_system() noexcept;
        error_type initialize() noexcept;
    private:
        struct cpu_buffer 
        {
            com_ptr<ID3D11Buffer> vtx_world;    //  
            com_ptr<ID3D11Buffer> vtx_angle;    //  
            com_ptr<ID3D11Buffer> vtx_color;    //  
            com_ptr<ID3D11Buffer> vtx_texuv;    //  
            com_ptr<ID3D11Buffer> idx_array;    //  
            com_ptr<ID3D11Buffer> obj_world;    //  
            com_ptr<ID3D11Buffer> obj_angle;    //  
            com_ptr<ID3D11Buffer> obj_scale;    // 
        };
    private:
        error_type exec() noexcept override;
        error_type signal(const event_data* event) noexcept override;
        const icy::thread& thread() const noexcept override
        {
            return *m_thread;
        }
        error_type repaint(const window_size size) noexcept override;
        error_type load_mesh(const guid index, const render_mesh& mesh) noexcept;
        error_type load_material(const guid index, const render_material& material) noexcept;
        error_type do_repaint(const window_size size) noexcept;
        error_type do_repaint(directx_render_surface& surface, const window_size size) noexcept;
    private:
        shared_ptr<directx_gpu_device> m_adapter;
        com_ptr<ID3D11Device> m_device;
        sync_handle m_sync;
        shared_ptr<event_thread> m_thread;
        std::atomic<uint32_t> m_index = 0;
        map<guid, unique_ptr<directx_render_material>> m_materials;
        map<guid, unique_ptr<directx_render_mesh>> m_meshes;
        map<uint64_t, unique_ptr<directx_render_node>> m_nodes;
        cpu_buffer m_cpu_buffer;
    };
}
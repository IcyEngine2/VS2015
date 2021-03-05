#include <icy_engine/core/icy_thread.hpp>
#include <icy_engine/utility/icy_com.hpp>
#include <icy_engine/graphics/icy_render.hpp>
#include <d3d11.h>
#include "shaders/draw_simple_vs.hpp"
#include "shaders/draw_simple_ps.hpp"

using namespace icy;

ICY_STATIC_NAMESPACE_BEG
struct vertex_type
{
    render_vertex::vec4 coord;
    render_vertex::vec4 color;
    render_vertex::vec2 texuv;    
};
class render_query_data : public render_query
{
public:
    render_query_data(render_texture& source) noexcept : m_source(source)
    {

    }
private:
    error_type exec(uint32_t& oper) noexcept override;
    void clear(const color color) noexcept override
    {
        m_clear = color;
    }
    void set_index_buffer(array<render_vertex::index>&& data) noexcept override
    {
        m_index_buffer = std::move(data);
    }
    void set_coord_buffer(array<render_vertex::vec4>&& data) noexcept override
    {
        m_coord_buffer = std::move(data);
    }
    void set_color_buffer(array<render_vertex::vec4>&& data) noexcept override
    {
        m_color_buffer = std::move(data);
    }
    void set_texuv_buffer(array<render_vertex::vec2>&& data) noexcept override
    {
        m_texuv_buffer = std::move(data);
    }
    void set_texture(const shared_ptr<texture> texture) noexcept override
    {
        m_texture = texture;
    }
    error_type draw_primitive(const render_vertex::index offset, const render_vertex::index size) noexcept override
    {
        return m_primitives.push_back({ offset, size });
    }
public:
    render_texture& m_source;
    color m_clear;
    array<render_vertex::index> m_index_buffer;
    array<render_vertex::vec4> m_coord_buffer;
    array<render_vertex::vec4> m_color_buffer;
    array<render_vertex::vec2> m_texuv_buffer;
    array<std::pair<render_vertex::index, render_vertex::index>> m_primitives;
    shared_ptr<texture> m_texture;
};
class render_texture_data : public render_texture
{
public:
    render_texture_data(const weak_ptr<render_system> system, const render_flags flags, const window_size size) noexcept :
        m_flags(flags), m_system(system), m_size(size)
    {

    }
    error_type initialize() noexcept;
    error_type initialize(const const_array_view<uint32_t> data) noexcept;
    error_type exec(uint32_t& oper, render_query_data& query) noexcept;
    window_size size() const noexcept override
    {
        return m_size;
    }
private:
    void* handle() const noexcept override
    {
        void* ptr = nullptr;
        com_ptr<IDXGIResource> dxgi;
        m_texture->QueryInterface(IID_PPV_ARGS(&dxgi));
        dxgi->GetSharedHandle(&ptr);
        return ptr;
    }
    icy::adapter adapter() const noexcept override
    {
        if (const auto system = shared_ptr<render_system>(m_system))
            return system->adapter();
        return icy::adapter();
    }
    shared_ptr<render_system> system() const noexcept override
    {
        return shared_ptr<render_system>(m_system);
    }
    error_type save(uint32_t& oper, const window_size offset, const window_size size) const noexcept override;
    error_type query(shared_ptr<render_query>& data) noexcept override;
private:
    weak_ptr<render_system> m_system;
    const render_flags m_flags;
    const window_size m_size;
public:
    com_ptr<ID3D11Texture2D> m_texture;
    com_ptr<ID3D11Texture2D> m_depth;
    com_ptr<ID3D11RenderTargetView> m_rtv;
    com_ptr<ID3D11DepthStencilView> m_dsv;
    com_ptr<ID3D11Buffer> m_vbuffer;
    com_ptr<ID3D11Buffer> m_ibuffer;
};
struct internal_event
{
    enum class type
    {
        none,
        save,
        query,
    } type = type::none;
    uint32_t oper = 0;
    weak_ptr<render_texture_data> texture;
    window_size offset;
    window_size size;
    unique_ptr<render_query_data> query;
};
class render_system_thread : public thread
{
public:
    render_system* system = nullptr;
    void cancel() noexcept
    {
        system->post(nullptr, event_type::global_quit);
    }
    error_type run() noexcept
    {
        if (auto error = system->exec())
            return event::post(system, event_type::system_error, std::move(error));
        return error_type();
    }
};
class render_system_data : public render_system
{
public:
    ~render_system_data() noexcept;
    render_system_data(const icy::adapter adapter) noexcept : m_adapter(adapter)
    {

    }
    error_type initialize() noexcept;
    ID3D11Device& device() const noexcept
    {
        return *m_device;
    }
    error_type make_device(com_ptr<ID3D11Device>& ptr) const noexcept;
    error_type save(const uint32_t oper, weak_ptr<render_texture_data> texture, const window_size offset, const window_size size) noexcept;
    uint32_t next_oper() noexcept
    {
        return m_oper.fetch_add(1, std::memory_order_acq_rel) + 1;
    }
private:
    error_type exec() noexcept override;
    error_type signal(const event_data& event) noexcept override
    {
        m_cvar.wake();
        return error_type();
    }
    const icy::thread& thread() const noexcept override
    {
        return *m_thread;
    }
    icy::adapter adapter() const noexcept override
    {
        return m_adapter;
    }
    error_type create(const window_size size, const render_flags flags, shared_ptr<render_texture>& texture) noexcept override;
    error_type create(const const_matrix_view<color> data, shared_ptr<texture>& texture) noexcept override;
    error_type render(render_texture_data& texture, const render_query_data& query) noexcept;
    error_type print(const render_texture_data& texture, const window_size offset, const window_size size, matrix<color>& colors) noexcept;
private:
    const icy::adapter m_adapter;
    mutex m_lock;
    cvar m_cvar;
    library m_lib_d3d = "d3d11"_lib;
    com_ptr<ID3D11Device> m_device;
    shared_ptr<render_system_thread> m_thread;   
    std::atomic<uint32_t> m_oper = 0;
    com_ptr<ID3D11VertexShader> m_vshader;
    com_ptr<ID3D11PixelShader> m_pshader;
    com_ptr<ID3D11InputLayout> m_layout;
    com_ptr<ID3D11SamplerState> m_sampler;
    com_ptr<ID3D11DepthStencilState> m_depth;
};
ICY_STATIC_NAMESPACE_END

error_type render_query_data::exec(uint32_t& oper) noexcept
{
    return static_cast<render_texture_data&>(m_source).exec(oper, *this);
}

error_type render_texture_data::initialize(const const_array_view<uint32_t> data) noexcept
{
    auto system = shared_ptr<render_system>(m_system);
    if (!system)
        return make_stdlib_error(std::errc::invalid_argument);

    auto& device = static_cast<render_system_data*>(system.get())->device();

    D3D11_SUBRESOURCE_DATA init = {};
    init.pSysMem = data.data();
    init.SysMemPitch = m_size.x * sizeof(uint32_t);

    auto desc = CD3D11_TEXTURE2D_DESC(DXGI_FORMAT_R8G8B8A8_UNORM,
        m_size.x, m_size.y, 1, 1, D3D11_BIND_SHADER_RESOURCE);
    desc.MiscFlags = D3D11_RESOURCE_MISC_SHARED;

    ICY_COM_ERROR(device.CreateTexture2D(&desc, &init, &m_texture));
    return error_type();
}
error_type render_texture_data::initialize() noexcept
{
    auto system = shared_ptr<render_system>(m_system);
    if (!system)
        return make_stdlib_error(std::errc::invalid_argument);

    auto& device = static_cast<render_system_data*>(system.get())->device();

    auto color_desc = CD3D11_TEXTURE2D_DESC(DXGI_FORMAT_R8G8B8A8_UNORM, m_size.x, m_size.y, 1, 1, D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET);
    color_desc.MiscFlags = D3D11_RESOURCE_MISC_SHARED;
    ICY_COM_ERROR(device.CreateTexture2D(&color_desc, nullptr, &m_texture));
    
    auto rtv_desc = CD3D11_RENDER_TARGET_VIEW_DESC(m_texture, D3D11_RTV_DIMENSION_TEXTURE2D);
    ICY_COM_ERROR(device.CreateRenderTargetView(m_texture, &rtv_desc, &m_rtv));

    auto depth_desc = CD3D11_TEXTURE2D_DESC(DXGI_FORMAT_D24_UNORM_S8_UINT, m_size.x, m_size.y, 1, 1, D3D11_BIND_DEPTH_STENCIL);
    ICY_COM_ERROR(device.CreateTexture2D(&depth_desc, nullptr, &m_depth));

    auto dsv_desc = CD3D11_DEPTH_STENCIL_VIEW_DESC(m_depth, D3D11_DSV_DIMENSION_TEXTURE2D);
    ICY_COM_ERROR(device.CreateDepthStencilView(m_depth, &dsv_desc, &m_dsv));

    return error_type();
}
error_type render_texture_data::exec(uint32_t& oper, render_query_data& query) noexcept
{
    auto system = shared_ptr<render_system>(m_system);
    if (!system)
        return make_stdlib_error(std::errc::invalid_argument);

    internal_event msg;
    msg.type = internal_event::type::query;
    msg.oper = oper = static_cast<render_system_data*>(system.get())->next_oper();
    msg.query = make_unique<render_query_data>(std::move(query));
    if (!msg.query)
        return make_stdlib_error(std::errc::not_enough_memory);
    msg.texture = make_shared_from_this(this);
    return system->post(nullptr, event_type::system_internal, std::move(msg));
}
error_type render_texture_data::save(uint32_t& oper, const window_size offset, const window_size size) const noexcept
{
    if (size.x == 0 || size.y == 0 || offset.x + size.x > m_size.x || offset.y + m_size.y)
        return make_stdlib_error(std::errc::invalid_argument);

    auto system = shared_ptr<render_system>(m_system);
    if (!system)
        return make_stdlib_error(std::errc::invalid_argument);

    const auto ptr = static_cast<render_system_data*>(system.get());
    return ptr->save(oper = ptr->next_oper(), make_shared_from_this(this), offset, size);
}
error_type render_texture_data::query(shared_ptr<render_query>& data) noexcept
{
    shared_ptr<render_query_data> ptr;
    ICY_ERROR(make_shared(ptr, *this));
    data = std::move(ptr);
    return error_type();
}
/*error_type render_texture_data::save(array_view<color> colors) const noexcept
{
    if (colors.size() != m_size.x * m_size.y)
        return make_stdlib_error(std::errc::invalid_argument);

    com_ptr<ID3D11DeviceContext> context;
    m_device->GetImmediateContext(&context);

    com_ptr<ID3D11Texture2D> cpu_texture;
    ICY_COM_ERROR(m_device->CreateTexture2D(&CD3D11_TEXTURE2D_DESC(DXGI_FORMAT_R8G8B8A8_UNORM,
        m_size.x, m_size.y, 1, 1, 0, D3D11_USAGE_STAGING, D3D11_CPU_ACCESS_READ), nullptr, &cpu_texture));

    D3D11_BOX box = { 0, 0, 0, m_size.x, m_size.y, 1 };
    context->CopySubresourceRegion(cpu_texture, 0, 0, 0, 0, m_texture, 0, &box);

    D3D11_MAPPED_SUBRESOURCE map;
    ICY_COM_ERROR(context->Map(cpu_texture, 0, D3D11_MAP_READ, 0, &map));
    ICY_SCOPE_EXIT{ context->Unmap(cpu_texture, 0); };

    auto dst = colors.data();
    for (auto row = 0u; row < m_size.y; ++row)
    {
        auto ptr = reinterpret_cast<const uint32_t*>(static_cast<char*>(map.pData) + row * map.RowPitch);
        for (auto col = 0u; col < m_size.x; ++col, ++dst)
            *dst = color::from_rgba(ptr[col]);
    }
    return error_type();
}
error_type texture_data::render(const color clear, const render_list& list) noexcept
{
   
}
*/

render_system_data::~render_system_data() noexcept
{
    if (m_thread) 
        m_thread->wait();
    filter(0);
}
error_type render_system_data::initialize() noexcept
{
    ICY_ERROR(m_lock.initialize());
    ICY_ERROR(m_lib_d3d.initialize());
    ICY_ERROR(make_device(m_device));

    ICY_COM_ERROR(m_device->CreateVertexShader(g_shader_bytes_draw_simple_vs, sizeof(g_shader_bytes_draw_simple_vs), nullptr, &m_vshader));
    ICY_COM_ERROR(m_device->CreatePixelShader(g_shader_bytes_draw_simple_ps, sizeof(g_shader_bytes_draw_simple_ps), nullptr, &m_pshader));

    const D3D11_INPUT_ELEMENT_DESC input_desc[] =
    {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT },
        { "COLOR", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT },
        { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT },
    };
    ICY_COM_ERROR(m_device->CreateInputLayout(input_desc, _countof(input_desc), g_shader_bytes_draw_simple_vs, sizeof(g_shader_bytes_draw_simple_vs), &m_layout));
    ICY_COM_ERROR(m_device->CreateSamplerState(&CD3D11_SAMPLER_DESC(D3D11_DEFAULT), &m_sampler));
    ICY_COM_ERROR(m_device->CreateDepthStencilState(&CD3D11_DEPTH_STENCIL_DESC(D3D11_DEFAULT), &m_depth));

    ICY_ERROR(make_shared(m_thread));
    m_thread->system = this;
    filter(event_type::system_internal);
    return error_type();
}
error_type render_system_data::make_device(com_ptr<ID3D11Device>& ptr) const noexcept
{
    if (const auto func = ICY_FIND_FUNC(m_lib_d3d, D3D11CreateDevice))
    {
        const auto level = m_adapter.flags() & adapter_flags::d3d10 ? D3D_FEATURE_LEVEL_10_1 : D3D_FEATURE_LEVEL_11_0;
        const auto adapter = static_cast<IDXGIAdapter*>(m_adapter.handle());
        auto flags = 0u;
        if (m_adapter.flags() & adapter_flags::debug)
            flags |= D3D11_CREATE_DEVICE_DEBUG;

        ICY_COM_ERROR(func(adapter, D3D_DRIVER_TYPE_UNKNOWN, nullptr, flags, &level, 1, D3D11_SDK_VERSION, &ptr, nullptr, nullptr));
    }
    else
    {
        return make_stdlib_error(std::errc::function_not_supported);
    }
    return error_type();
}
error_type render_system_data::save(const uint32_t oper, weak_ptr<render_texture_data> texture, const window_size offset, const window_size size) noexcept
{
    internal_event msg;
    msg.type = internal_event::type::save;
    msg.oper = oper;
    msg.texture = texture;
    msg.offset = offset;
    msg.size = size;
    return event_system::post(nullptr, event_type::system_internal, std::move(msg));
}
error_type render_system_data::exec() noexcept
{
    while (true)
    {
        while (auto event = pop())
        {
            if (event->type == event_type::global_quit)
                return error_type();
            if (event->type == event_type::system_internal)
            {
                const auto& event_data = event->data<internal_event>();
                auto texture = shared_ptr<render_texture_data>(event_data.texture);
                if (!texture)
                    continue;

                switch (event_data.type)
                {
                case internal_event::type::save:
                {
                    render_message msg;
                    msg.oper = event_data.oper;
                    msg.texture = texture;
                    ICY_ERROR(print(*texture, event_data.size, event_data.offset, msg.colors));
                    ICY_ERROR(event::post(this, event_type::render_save, std::move(msg)));
                    break;
                }

                case internal_event::type::query:
                {
                    render_message msg;
                    msg.oper = event_data.oper;
                    msg.texture = texture;
                    ICY_ERROR(render(*texture, *event_data.query));
                    ICY_ERROR(event::post(this, event_type::render_update, std::move(msg)));
                    break;
                }
                }
            }
        }
        ICY_ERROR(m_cvar.wait(m_lock));
    }
    return error_type();
}
error_type render_system_data::create(const window_size size, const render_flags flags, shared_ptr<render_texture>& texture) noexcept
{
    shared_ptr<render_texture_data> ptr;
    ICY_ERROR(make_shared(ptr, make_shared_from_this(this), flags, size));
    ICY_ERROR(ptr->initialize());
    texture = std::move(ptr);
    return error_type();
}
error_type render_system_data::create(const const_matrix_view<color> data, shared_ptr<texture>& texture) noexcept
{
    const auto size = window_size(uint32_t(data.cols()), uint32_t(data.rows()));
    shared_ptr<render_texture_data> ptr;
    ICY_ERROR(make_shared(ptr, make_shared_from_this(this), render_flags::none, size));
    array<uint32_t> init;
    ICY_ERROR(init.reserve(data.size()));
    for (auto row = 0u; row < data.rows(); ++row)
    {
        for (auto col = 0u; col < data.cols(); ++col)
        {
            ICY_ERROR(init.push_back(data.at(row, col).to_rgba()));
        }
    }
    ICY_ERROR(ptr->initialize(init));
    texture = std::move(ptr);
    return error_type();
}
error_type render_system_data::render(render_texture_data& texture, const render_query_data& query) noexcept
{
    com_ptr<ID3D11DeviceContext> context;
    m_device->GetImmediateContext(&context);

    float rgba[4];
    query.m_clear.to_rgbaf(rgba);
    context->ClearRenderTargetView(texture.m_rtv, rgba);
    context->ClearDepthStencilView(texture.m_dsv, D3D11_CLEAR_DEPTH, 1.0, 0);

    auto vsize_bytes = query.m_coord_buffer.size() * sizeof(vertex_type);
    auto isize_bytes = query.m_index_buffer.size() * sizeof(render_vertex::index);
    
    if (isize_bytes == 0 || vsize_bytes == 0)
    {
        context->Flush();
        return error_type();
    }

    D3D11_BUFFER_DESC vdesc = {};
    D3D11_BUFFER_DESC idesc = {};
    if (texture.m_vbuffer) texture.m_vbuffer->GetDesc(&vdesc);
    if (texture.m_ibuffer) texture.m_ibuffer->GetDesc(&idesc);

    if (vdesc.ByteWidth < vsize_bytes)
    {
        vdesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
        vdesc.ByteWidth = uint32_t(vsize_bytes);
        vdesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
        vdesc.Usage = D3D11_USAGE_DYNAMIC;
        com_ptr<ID3D11Buffer> buffer;
        ICY_COM_ERROR(m_device->CreateBuffer(&vdesc, nullptr, &buffer));
        texture.m_vbuffer = buffer;
    }
    if (idesc.ByteWidth < isize_bytes)
    {
        idesc.BindFlags = D3D11_BIND_INDEX_BUFFER;
        idesc.ByteWidth = uint32_t(isize_bytes);
        idesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
        idesc.Usage = D3D11_USAGE_DYNAMIC;
        com_ptr<ID3D11Buffer> buffer;
        ICY_COM_ERROR(m_device->CreateBuffer(&idesc, nullptr, &buffer));
        texture.m_ibuffer = buffer;
    }

    const auto vcount = query.m_coord_buffer.size();

    D3D11_MAPPED_SUBRESOURCE map;
    ICY_COM_ERROR(context->Map(texture.m_vbuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &map));
    auto vptr = static_cast<vertex_type*>(map.pData);

    for (auto k = 0u; k < vcount; ++k)
        memcpy(&vptr[k].coord, &query.m_coord_buffer[k], sizeof(render_vertex::vec4));

    if (query.m_color_buffer.empty())
    {
        const float white[4] = { 1, 1, 1, 1 };
        for (auto k = 0u; k < vcount; ++k)
            memcpy(&vptr[k].color, white, sizeof(render_vertex::vec4));
    }
    else
    {
        for (auto k = 0u; k < vcount; ++k)
            memcpy(&vptr[k].color, &query.m_color_buffer[k], sizeof(render_vertex::vec4));
    }
    if (query.m_texuv_buffer.empty())
    {
        const float null[2] = { 1, 1 };
        for (auto k = 0u; k < vcount; ++k)
            memcpy(&vptr[k].texuv, null, sizeof(render_vertex::vec2));
    }
    else
    {
        for (auto k = 0u; k < vcount; ++k)
            memcpy(&vptr[k].texuv, &query.m_texuv_buffer[k], sizeof(render_vertex::vec2));
    }
    context->Unmap(texture.m_vbuffer, 0);

    ICY_COM_ERROR(context->Map(texture.m_ibuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &map));
    memcpy(map.pData, query.m_index_buffer.data(), query.m_index_buffer.size() * sizeof(render_vertex::index));
    context->Unmap(texture.m_ibuffer, 0);

    const uint32_t stride[] = { sizeof(vertex_type) };
    const uint32_t offset[] = { 0 };
    const D3D11_VIEWPORT viewport = { 0, 0, float(texture.size().x), float(texture.size().y), 0, 1 };
    
    context->IASetVertexBuffers(0, 1, &texture.m_vbuffer, stride, offset);
    context->IASetIndexBuffer(texture.m_ibuffer, DXGI_FORMAT_R32_UINT, 0);
    context->IASetInputLayout(m_layout);
    context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    context->RSSetViewports(1, &viewport);
    context->OMSetRenderTargets(1, &texture.m_rtv, texture.m_dsv);
    context->OMSetDepthStencilState(m_depth, 1);

    context->VSSetShader(m_vshader, nullptr, 0);
    context->PSSetShader(m_pshader, nullptr, 0);
    context->PSSetSamplers(0, 1, &m_sampler);
    
    for (auto&& element : query.m_primitives)
        context->DrawIndexedInstanced(element.second, 1, element.first, 0, 0);
    
    context->Flush();
    return error_type();
}
error_type render_system_data::print(const render_texture_data& texture, const window_size offset, const window_size size, matrix<color>& colors) noexcept
{
    return error_type();
}


error_type icy::create_render_system(shared_ptr<render_system>& system, const adapter adapter) noexcept
{
    shared_ptr<render_system_data> ptr;
    ICY_ERROR(make_shared(ptr, adapter));
    ICY_ERROR(ptr->initialize());
    system = std::move(ptr);
    return error_type();
}
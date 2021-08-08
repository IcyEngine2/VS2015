#include <icy_engine/graphics/icy_render.hpp>
#include <icy_engine/graphics/icy_adapter.hpp>
#include <icy_engine/core/icy_map.hpp>
#include <icy_engine/utility/icy_com.hpp>
#include <d3d11.h>

using namespace icy;

ICY_STATIC_NAMESPACE_BEG
class render_system_data;
class render_surface_data;
enum class internal_message_type : uint32_t
{
    none,
    render,
};
struct internal_message
{
    internal_message_type type = internal_message_type::none;
    weak_ptr<render_surface_data> surface;
    render_data render;
};
class render_surface_data : public render_surface
{
public:
    render_surface_data(shared_ptr<render_system_data> system) noexcept : m_system(system)
    {

    }
    error_type initialize(const window_size size) noexcept;
    void lock() noexcept
    {
        m_lock.lock();
    }
    void unlock() noexcept
    {
        m_lock.unlock();
    }
    void* handle() noexcept
    {
        return m_dxgi.get();
    }
    error_type do_render(render_data& data) noexcept;
private:
    error_type resize(const window_size size) noexcept override;
    error_type render(render_data& data) noexcept override;
private:
    shared_ptr<render_system_data> m_system;
    mutex m_lock;
    com_ptr<ID3D11Texture2D> m_texture;
    com_ptr<IDXGISurface> m_dxgi;
    com_ptr<ID3D11RenderTargetView> m_rtv;
};
class render_system_data : public render_system
{
public:
    error_type initialize(const icy::adapter adapter) noexcept;
    ~render_system_data() noexcept
    {
        if (m_thread)
            m_thread->wait();
        filter(0);
    }
    ID3D11Device& device() const noexcept
    {
        return *m_device;
    }
private:
    error_type exec() noexcept override;
    error_type signal(const icy::event_data* event) noexcept override
    {
        return m_sync.wake();
    }
    const icy::thread& thread() const noexcept override
    {
        return *m_thread;
    }
    icy::adapter adapter() const noexcept override
    {
        return m_adapter;
    }
    error_type create(shared_ptr<render_surface>& surface, const window_size size) noexcept override;
private:
    sync_handle m_sync;
    shared_ptr<event_thread> m_thread;
    icy::adapter m_adapter;
    library m_lib_d3d = "d3d11"_lib;
    com_ptr<ID3D11Device> m_device;
};
ICY_STATIC_NAMESPACE_END

render_lock::render_lock(render_surface& surface) noexcept : surface(surface)
{
    static_cast<render_surface_data&>(surface).lock();
}
render_lock::~render_lock() noexcept
{
    static_cast<render_surface_data&>(surface).lock();
}
void* render_lock::handle() noexcept
{
    return static_cast<render_surface_data&>(surface).handle();
}

error_type render_surface_data::initialize(const window_size size) noexcept
{
    ICY_ERROR(m_lock.initialize());
    ICY_ERROR(resize(size));
    return error_type();
}
error_type render_surface_data::resize(const window_size size) noexcept
{
    auto& device = m_system->device();

    com_ptr<ID3D11Texture2D> texture;
    com_ptr<ID3D11RenderTargetView> rtv;
    com_ptr<IDXGISurface> dxgi;

    auto desc = CD3D11_TEXTURE2D_DESC(DXGI_FORMAT_R8G8B8A8_UNORM, size.x, size.y, 1, 1, D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET);
    desc.MiscFlags = D3D11_RESOURCE_MISC_SHARED;
    ICY_COM_ERROR(device.CreateTexture2D(&desc, nullptr, &texture));
    ICY_COM_ERROR(texture->QueryInterface(&dxgi));

    auto rtv_desc = CD3D11_RENDER_TARGET_VIEW_DESC(texture, D3D11_RTV_DIMENSION_TEXTURE2D);
    ICY_COM_ERROR(device.CreateRenderTargetView(texture, &rtv_desc, &rtv));

    ICY_LOCK_GUARD(m_lock);
    m_texture = texture;
    m_dxgi = dxgi;
    m_rtv = rtv;

    return error_type();
}
error_type render_surface_data::render(render_data& data) noexcept
{
    auto system = shared_ptr<render_system_data>(m_system);
    if (!system)
        return make_stdlib_error(std::errc::invalid_argument);

    internal_message msg;
    msg.type = internal_message_type::render;
    msg.surface = make_shared_from_this(this);
    msg.render = std::move(data);
    return system->post(nullptr, event_type::system_internal, std::move(msg));
}
error_type render_surface_data::do_render(render_data& data) noexcept
{
    {
        ICY_LOCK_GUARD(m_lock);
        auto& device = m_system->device();
        com_ptr<ID3D11DeviceContext> context;
        device.GetImmediateContext(&context);

        float rgba[4] = { 1, 0, 0, 1 };
        context->ClearRenderTargetView(m_rtv, rgba);
        context->Flush();
    }

    render_event new_event;
    new_event.surface = make_shared_from_this(this);
    ICY_ERROR(event::post(m_system.get(), event_type::render_event, std::move(new_event)));
    return error_type();
}

error_type render_system_data::initialize(icy::adapter adapter) noexcept
{
    ICY_ERROR(m_sync.initialize());
    ICY_ERROR(make_shared(m_thread));
    m_thread->system = this;
    m_adapter = adapter;

    ICY_ERROR(m_lib_d3d.initialize());
    if (const auto func = ICY_FIND_FUNC(m_lib_d3d, D3D11CreateDevice))
    {
        const auto level = m_adapter.flags() & adapter_flags::d3d10 ? D3D_FEATURE_LEVEL_10_1 : D3D_FEATURE_LEVEL_11_0;
        const auto adapter = static_cast<IDXGIAdapter*>(m_adapter.handle());
        auto flags = 0u;
        if (m_adapter.flags() & adapter_flags::debug)
            flags |= D3D11_CREATE_DEVICE_DEBUG;

        ICY_COM_ERROR(func(adapter, D3D_DRIVER_TYPE_UNKNOWN, nullptr, flags, &level, 1, D3D11_SDK_VERSION, &m_device, nullptr, nullptr));
    }
    else
    {
        return make_stdlib_error(std::errc::function_not_supported);
    }

    const uint64_t event_types = 0
        | event_type::system_internal;

    filter(event_types);
    return error_type();
}
error_type render_system_data::exec() noexcept
{
    while (*this)
    {
        while (auto event = pop())
        {
            if (event->type == event_type::system_internal)
            {
                auto& event_data = event->data<internal_message>();
                if (auto surface = shared_ptr<render_surface_data>(event_data.surface))
                {
                    ICY_ERROR(surface->do_render(event_data.render));
                }
            }
        }
        ICY_ERROR(m_sync.wait());
    }
    return error_type();
}
error_type render_system_data::create(shared_ptr<render_surface>& surface, const window_size size) noexcept
{
    shared_ptr<render_surface_data> ptr;
    ICY_ERROR(make_shared(ptr, make_shared_from_this(this)));
    ICY_ERROR(ptr->initialize(size));
    surface = std::move(ptr);
    return error_type();
}

error_type icy::create_render_system(shared_ptr<render_system>& system, const adapter adapter) noexcept
{
    shared_ptr<render_system_data> new_system;
    ICY_ERROR(make_shared(new_system));
    ICY_ERROR(new_system->initialize(adapter));
    system = std::move(new_system);
    return error_type();
}
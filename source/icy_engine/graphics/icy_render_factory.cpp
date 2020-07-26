#include "icy_render_factory.hpp"
#include "icy_render_commands.hpp"
#include "icy_render_svg.hpp"
#include "icy_adapter.hpp"
#include "icy_texture.hpp"
#include "icy_window.hpp"
#include <dxgi1_6.h>
#include <dwrite_3.h>
#include <d2d1_3.h>
#include <d3d11_4.h>

using namespace icy;

render_factory_data::~render_factory_data() noexcept
{
    if (m_write)
        m_write->Release();
    while (auto ptr = static_cast<render_texture::data_type*>(m_tex.pop()))
    {
        allocator_type::destroy(ptr);
        allocator_type::deallocate(ptr);
    }
    while (auto ptr = static_cast<render_commands_2d::data_type*>(m_cmd_2d.pop()))
    {
        allocator_type::destroy(ptr);
        allocator_type::deallocate(ptr);
    }
    while (auto ptr = static_cast<render_commands_3d::data_type*>(m_cmd_3d.pop()))
    {
        allocator_type::destroy(ptr);
        allocator_type::deallocate(ptr);
    }
}
error_type render_factory_data::initialize(const icy::adapter& adapter, const icy::render_flags flags) noexcept
{
    m_adapter = adapter;
    m_flags = flags;

    ICY_ERROR(m_lib_dwrite.initialize());
    ICY_ERROR(m_lib_d2d1.initialize());
    ICY_ERROR(m_lib_d3d11.initialize());

    if (const auto func = ICY_FIND_FUNC(m_lib_dwrite, DWriteCreateFactory))
    {
        com_ptr<IUnknown> any_factory;
        ICY_COM_ERROR(func(DWRITE_FACTORY_TYPE_SHARED, __uuidof(IDWriteFactory), &any_factory));
        ICY_COM_ERROR(any_factory->QueryInterface(&m_write));
    }
    else
    {
        return make_stdlib_error(std::errc::function_not_supported);
    }

    if (adapter.data->flags() & adapter_flags::d3d12)
    {
        ICY_ERROR(m_lib_d3d12.initialize());
    }
    return error_type();
}
icy::adapter_flags render_factory_data::adapter() const noexcept
{
    return m_adapter.flags();
}
error_type render_factory_data::create_texture(const icy::const_array_view<uint8_t> bytes, icy::render_texture& texture) const noexcept
{
    return make_stdlib_error(std::errc::function_not_supported);
}
error_type render_factory_data::create_texture(const icy::const_matrix_view<icy::color> bytes, icy::render_texture& texture) const noexcept
{
    return make_stdlib_error(std::errc::function_not_supported);
}   
error_type render_factory_data::init_commands_2d(render_commands_2d& commands, map<float, array<render_command_2d>>& map) const noexcept
{
    auto& d3d11 = commands.data->d3d11;
    auto& device = commands.data->device;
    auto& context = commands.data->context;

    if (!d3d11)
        ICY_ERROR(create_device(d3d11));
    if (!device)
        ICY_ERROR(create_device(*d3d11, device));
    if (!context)
        ICY_COM_ERROR(device->CreateDeviceContext(D2D1_DEVICE_CONTEXT_OPTIONS_ENABLE_MULTITHREADED_OPTIMIZATIONS, &context));

    while (true)
    {
        render_command_2d cmd;
        if (!commands.data->queue.pop(cmd))
            break;

        auto layer = map.find(cmd.layer);
        if (layer == map.end())
            ICY_ERROR(map.insert(cmd.layer, array<render_command_2d>(), &layer));

        ICY_ERROR(layer->value.push_back(std::move(cmd)));
    }
    return error_type();
}
error_type render_factory_data::close_commands_2d(render_commands_2d& commands, const window_size& size, render_texture& texture) const noexcept
{
    if (!commands.data || !size.x || !size.y)
        return make_stdlib_error(std::errc::function_not_supported);

    auto& d3d11 = commands.data->d3d11;
    auto& context = commands.data->context;

    map<float, array<render_command_2d>> map;
    ICY_ERROR(init_commands_2d(commands, map));
    ICY_ERROR(create_texture(*d3d11, size, texture));

    HANDLE handle = nullptr;
    ICY_COM_ERROR(texture.data->dxgi->GetSharedHandle(&handle));
    com_ptr<ID3D11Texture2D> d3d11_texture;
    ICY_COM_ERROR(d3d11->OpenSharedResource(handle, IID_PPV_ARGS(&d3d11_texture)));

    com_ptr<IDXGISurface> dxgi;
    ICY_COM_ERROR(d3d11_texture->QueryInterface(&dxgi));

    auto pixel = D2D1::PixelFormat(DXGI_FORMAT_R8G8B8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED);
    auto props = D2D1::BitmapProperties1(D2D1_BITMAP_OPTIONS_TARGET, pixel);

    com_ptr<ID2D1Bitmap1> bitmap;
    ICY_COM_ERROR(context->CreateBitmapFromDxgiSurface(dxgi, props, &bitmap));

    context->SetAntialiasMode(D2D1_ANTIALIAS_MODE_PER_PRIMITIVE);
    context->SetTextAntialiasMode(D2D1_TEXT_ANTIALIAS_MODE_GRAYSCALE);
    context->SetTarget(bitmap);
    context->BeginDraw();
    context->Clear(D2D1::ColorF(0, 0, 0, 0));

    for (const auto& layer : map)
    {
        for (auto&& cmd : layer.value)
        {
            context->SetTransform(reinterpret_cast<const D2D1_MATRIX_3X2_F*>(&cmd.transform));
            if (cmd.geometry.data)
            {
                ICY_ERROR(cmd.geometry.data->render(*context));
            }
            if (cmd.text)
            {
                com_ptr<ID2D1SolidColorBrush> brush;
                ICY_COM_ERROR(context->CreateSolidColorBrush(D2D1::ColorF(cmd.color.to_rgb(), cmd.color.a / 255.0f), &brush));
                context->DrawTextLayout(D2D1::Point2F(), cmd.text, brush, D2D1_DRAW_TEXT_OPTIONS_ENABLE_COLOR_FONT);
            }
        }
    }
    ICY_COM_ERROR(context->EndDraw());
    context->SetTarget(nullptr);

    com_ptr<ID3D11DeviceContext> d3d11_context;
    d3d11->GetImmediateContext(&d3d11_context);
    d3d11_context->Flush();
    return error_type();
}
error_type render_factory_data::close_commands_2d(render_commands_2d& commands, window_system& window) const noexcept
{
    window_data::repaint_type repaint;
    if (commands.data)
    {
        ICY_ERROR(init_commands_2d(commands, repaint.map));
        repaint.render = make_shared_from_this(this);
        repaint.device = commands.data->device;
    }
    commands = render_commands_2d();
    return static_cast<window_data&>(window).repaint(std::move(repaint));
}

error_type icy::create_render_factory(shared_ptr<render_factory>& system, const adapter& adapter, const render_flags flags) noexcept
{
    shared_ptr<render_factory_data> new_ptr;
    ICY_ERROR(make_shared(new_ptr));
    ICY_ERROR(new_ptr->initialize(adapter, flags));
    system = std::move(new_ptr);
    return error_type();
}
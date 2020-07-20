#include "icy_render_factory.hpp"
#include "icy_render_commands.hpp"
#include "icy_adapter.hpp"
#include "icy_texture.hpp"
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

error_type icy::create_render_factory(shared_ptr<render_factory>& system, const adapter& adapter, const render_flags flags) noexcept
{
    shared_ptr<render_factory_data> new_ptr;
    ICY_ERROR(make_shared(new_ptr));
    ICY_ERROR(new_ptr->initialize(adapter, flags));
    system = std::move(new_ptr);
    return error_type();
}
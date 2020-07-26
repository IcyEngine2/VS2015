#pragma once

#include <icy_engine/core/icy_map.hpp>
#include <icy_engine/utility/icy_com.hpp>
#include <icy_engine/graphics/icy_render.hpp>
#include <icy_engine/graphics/icy_adapter.hpp>

struct ID2D1Device;
struct ID3D11Device;
struct ID3D12Device;
struct IDWriteFactory;
struct render_command_2d;

class render_factory_data : public icy::render_factory
{
public:
    render_factory_data() noexcept = default;
    render_factory_data(const render_factory_data&) = delete;
    ~render_factory_data() noexcept;
    icy::error_type initialize(const icy::adapter& adapter, const icy::render_flags flags) noexcept;
    icy::adapter_flags adapter() const noexcept;
    icy::render_flags flags() const noexcept override
    {
        return m_flags;
    }
    IDWriteFactory& write() noexcept
    {
        return *m_write;
    }
    icy::error_type create_device(ID3D11Device& d3d11, icy::com_ptr<ID2D1Device>& device) const noexcept;
    icy::error_type create_device(icy::com_ptr<ID3D11Device>& device) const noexcept;
    icy::error_type create_device(icy::com_ptr<ID3D12Device>& device) const noexcept;
    void destroy(icy::render_texture::data_type& ref) noexcept
    {
        m_tex.push(&ref);
    }
    void destroy(icy::render_commands_2d::data_type& ref) noexcept
    {
        m_cmd_2d.push(&ref);
    }
    void destroy(icy::render_commands_3d::data_type& ref) noexcept
    {
        m_cmd_3d.push(&ref);
    }
    icy::error_type create_texture(ID3D11Device& device, const icy::window_size size, icy::render_texture& texture) const noexcept;
private:
    icy::error_type enum_font_names(icy::array<icy::string>& fonts) const noexcept override;
    icy::error_type enum_font_sizes(icy::array<uint32_t>& sizes) const noexcept override;
    icy::error_type create_svg_font(const icy::render_svg_font_data& data, icy::render_svg_font& font) const noexcept override;
    icy::error_type create_svg_text(const icy::string_view str, const icy::color color, const icy::window_size size, const icy::render_svg_font& font, icy::render_svg_text& text) const noexcept override;
    icy::error_type create_svg_geometry(const icy::const_array_view<char> xml, icy::render_svg_geometry& geometry) const noexcept override;
    icy::error_type open_commands_2d(icy::render_commands_2d& commands) noexcept override;
    icy::error_type open_commands_3d(icy::render_commands_3d& commands) noexcept override;
    icy::error_type create_texture(const icy::const_array_view<uint8_t> bytes, icy::render_texture& texture) const noexcept override;
    icy::error_type create_texture(const icy::const_matrix_view<icy::color> bytes, icy::render_texture& texture) const noexcept override;
    icy::error_type close_commands_2d(icy::render_commands_2d& commands, const icy::window_size& size, icy::render_texture& texture) const noexcept override;
    icy::error_type close_commands_2d(icy::render_commands_2d& commands, icy::window_system& window) const noexcept override;
    icy::error_type init_commands_2d(icy::render_commands_2d& commands, icy::map<float, icy::array<render_command_2d>>& map) const noexcept;
private:
    icy::adapter m_adapter;
    icy::render_flags m_flags = icy::render_flags::none;
    icy::library m_lib_dwrite = icy::library("dwrite");
    icy::library m_lib_d2d1 = icy::library("d2d1");
    icy::library m_lib_d3d11 = icy::library("d3d11");
    icy::library m_lib_d3d12 = icy::library("d3d12");
    IDWriteFactory* m_write = nullptr;
    mutable icy::detail::intrusive_mpsc_queue m_tex;
    icy::detail::intrusive_mpsc_queue m_cmd_2d;
    icy::detail::intrusive_mpsc_queue m_cmd_3d;
};
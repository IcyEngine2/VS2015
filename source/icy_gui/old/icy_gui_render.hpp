#pragma once

#include <icy_engine/core/icy_color.hpp>
#include <icy_engine/core/icy_string.hpp>
#include <icy_engine/core/icy_smart_pointer.hpp>
#include <icy_engine/utility/icy_com.hpp>
#include <icy_gui/icy_gui.hpp>

struct ID2D1Device;
struct ID3D11Device;

inline constexpr uint64_t gui_style_flag(const icy::gui_style_prop prop) noexcept
{
    return prop != icy::gui_style_prop::none ? (1 << (uint64_t(prop) - 1)) : 0;
}

enum gui_system_font_type
{
    gui_system_font_type_default,
    gui_system_font_type_menu,
    gui_system_font_type_caption,
    gui_system_font_type_caption_small,
    gui_system_font_type_status,
    gui_system_font_type_count,
};
struct gui_font_data
{
    gui_font_data() noexcept : flags(0)
    {

    }
    uint32_t size       = 0u;
    uint32_t stretch    = 0u;
    uint32_t spacing    = 0u;
    uint32_t weight     = 0u;
    union
    {
        struct
        {
            int32_t italic      : 1;
            int32_t oblique     : 1;
            int32_t strike      : 1;
            int32_t underline   : 1;
            int32_t align_text  : 2;
            int32_t align_par   : 2;
            int32_t trimming    : 2;
        };
        int32_t flags;
    };
    icy::color color;
};
struct gui_system_font : gui_font_data
{
    icy::string name;
    uint32_t size_x = 0;
    uint32_t size_y = 0;
};
class gui_render_system;
class gui_texture : public icy::texture
{
public:
    struct rect_type
    {
        float min_x = 0;
        float min_y = 0;
        float max_x = 0;
        float max_y = 0;
    };
public:
    gui_texture(const icy::shared_ptr<gui_render_system> system, const icy::window_size size, const icy::render_flags flags) noexcept :
        m_system(system), m_size(size), m_flags(flags)
    {

    }
    icy::error_type initialize() noexcept;
    void* handle() const noexcept override;
    icy::window_size size() const noexcept override
    {
        return m_size;
    }
    icy::adapter adapter() const noexcept override;
    void draw_begin(const icy::color color) noexcept;
    icy::error_type draw_end() noexcept;
    icy::error_type draw_text(const rect_type& rect, const icy::color color, icy::com_ptr<IUnknown> text) noexcept;
    icy::error_type fill_rect(const rect_type& rect, const icy::color color) noexcept;
private:
    icy::shared_ptr<gui_render_system> m_system;
    const icy::window_size m_size;
    const icy::render_flags m_flags;
    icy::com_ptr<IUnknown> m_texture_d3d;
    icy::com_ptr<IUnknown> m_texture_d2d;
    icy::com_ptr<IUnknown> m_context;
};
class gui_render_system
{
public:
    friend gui_texture;
public:    
    gui_render_system(const icy::adapter adapter) noexcept : m_adapter(adapter)
    {

    }
    icy::error_type initialize(HWND__* hwnd) noexcept;
    icy::error_type enum_font_names(icy::array<icy::string>& fonts) const noexcept;
    icy::error_type enum_font_sizes(const icy::string_view font, icy::array<uint32_t>& sizes) const noexcept;
    icy::error_type create_texture(icy::shared_ptr<gui_texture>& texture, 
        const icy::window_size size, const icy::render_flags flags) const noexcept;
    icy::error_type create_text(icy::com_ptr<IUnknown>& text, const gui_texture::rect_type& rect,
        const gui_font_data& font_data, const icy::string_view font_name, 
        const icy::string_view str, const uint64_t flags) const noexcept;
public:
    gui_system_font system_fonts[gui_system_font_type_count] = {};
    uint32_t dpi = 0;
private:
    icy::adapter m_adapter;
    icy::library m_lib_dwrite = icy::library("dwrite");
    icy::library m_lib_d3d = icy::library("d3d11");
    icy::library m_lib_d2d = icy::library("d2d1");
    icy::com_ptr<IUnknown> m_factory_dwrite;
    icy::com_ptr<IUnknown> m_device_d3d;
    icy::com_ptr<IUnknown> m_device_d2d;
    icy::com_ptr<IUnknown> m_factory_d2d;
    icy::com_ptr<IUnknown> m_text_format;
};
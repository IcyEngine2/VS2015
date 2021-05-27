#pragma once

#include <icy_engine/core/icy_color.hpp>
#include <icy_engine/core/icy_string.hpp>
#include <icy_engine/core/icy_smart_pointer.hpp>
#include <icy_engine/graphics/icy_adapter.hpp>
#include <icy_engine/utility/icy_com.hpp>
#include <icy_gui/icy_gui.hpp>

//struct ID2D1Bitmap;
struct ID2D1Device;
struct ID3D11Device;
struct IDWriteTextLayout;

enum gui_system_font_type
{
    gui_system_font_type_default,
    gui_system_font_type_menu,
    gui_system_font_type_caption,
    gui_system_font_type_caption_small,
    gui_system_font_type_status,
    gui_system_font_type_count,
};
struct gui_font
{
    icy::com_ptr<IUnknown> value;
};
struct gui_text 
{
    explicit operator bool() const noexcept
    {
        return !!value;
    }
    IDWriteTextLayout* operator->() const noexcept;
    icy::com_ptr<IUnknown> value;
};
/*struct blob
{
    explicit operator bool() const noexcept
    {
        return !!value;
    }
    ID2D1Bitmap* operator->() const noexcept;
    icy::window_size size() const noexcept;
    icy::com_ptr<IUnknown> value;
};*/

class gui_render_system;
/*class gui_texture : public icy::texture
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
    icy::error_type draw_text(const float x, const float y, const icy::color color, icy::com_ptr<IUnknown> text) noexcept;
    icy::error_type fill_rect(const rect_type& rect, const icy::color color) noexcept;
    void draw_image(const rect_type& rect, icy::com_ptr<IUnknown> image) noexcept;
    void push_clip(const rect_type& rect) noexcept;
    void pop_clip() noexcept;
private:
    icy::shared_ptr<gui_render_system> m_system;
    const icy::window_size m_size;
    const icy::render_flags m_flags;
    icy::com_ptr<IUnknown> m_texture_d3d;
    icy::com_ptr<IUnknown> m_texture_d2d;
    icy::com_ptr<IUnknown> m_context;
};*/
class gui_render_system
{
public:    
    /*gui_render_system(const icy::adapter adapter) noexcept : m_adapter(adapter)
    {

    }*/
    icy::error_type initialize() noexcept;
    icy::error_type enum_font_names(icy::array<icy::string>& fonts) const noexcept;
    icy::error_type create_font(gui_font& font, icy::shared_ptr<icy::window> hwnd, const gui_system_font_type type) const noexcept;
    icy::error_type create_text(gui_text& text, const gui_font& font, const icy::string_view str) const noexcept;
    //icy::error_type create_image(blob& image, const icy::const_array_view<uint8_t> bytes) const noexcept;
    //icy::error_type create_texture(icy::shared_ptr<gui_texture>& texture, const icy::window_size size, const icy::render_flags flags) const noexcept;
private:
    //icy::adapter m_adapter;
    icy::library m_lib_user32 = icy::library("user32");
    icy::library m_lib_dwrite = icy::library("dwrite");
    //icy::library m_lib_d3d = icy::library("d3d11");
    //icy::library m_lib_d2d = icy::library("d2d1");
    icy::com_ptr<IUnknown> m_factory_dwrite;
    //icy::com_ptr<IUnknown> m_device_d3d;
    //icy::com_ptr<IUnknown> m_device_d2d;
    //icy::com_ptr<IUnknown> m_factory_d2d;
};
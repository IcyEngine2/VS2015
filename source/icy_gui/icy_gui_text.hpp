#pragma once

#include <icy_engine/core/icy_string.hpp>
#include <icy_engine/core/icy_array.hpp>
#include <icy_engine/utility/icy_com.hpp>

class gui_text_system
{
public:
    icy::error_type initialize() noexcept;    
    icy::error_type enum_font_names(icy::array<icy::string>& fonts) const noexcept;
    icy::error_type enum_font_sizes(const icy::string_view font, icy::array<uint32_t>& sizes) const noexcept;
private:
    icy::library m_lib_dwrite = icy::library("dwrite");
    icy::library m_lib_user = icy::library("user32");
    icy::library m_lib_gdi = icy::library("gdi32");
    icy::com_ptr<IUnknown> m_factory;
    icy::array<icy::string> m_fonts;
};
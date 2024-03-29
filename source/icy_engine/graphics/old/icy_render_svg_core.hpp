#pragma once

#include <icy_engine/graphics/icy_render_svg.hpp>
#include <icy_engine/utility/icy_com.hpp>
#include "icy_system.hpp"

struct ID2D1Factory;
struct IDWriteFactory;
struct ID2D1Geometry;
struct ID2D1StrokeStyle;
struct IDWriteTextFormat;
struct IDWriteTextLayout;
struct NSVGpaint;
struct HDC__;

namespace icy 
{
	
	class render_svg_font::data_type
	{
	public:
		error_type initialize(const render_svg_font_data& data) noexcept;
		IDWriteTextFormat& format() const noexcept
		{
			return *m_format;
		}
		std::atomic<uint32_t> ref = 1;
	private:
		any_system_acqrel_pointer<render_svg_system> m_render;
		com_ptr<IDWriteTextFormat> m_format;
	};
	class render_svg_geometry::data_type
	{
	public:
        error_type initialize(const render_d2d_matrix& transform, const const_array_view<char> text) noexcept;
        error_type initialize(const render_d2d_matrix& transform, const color color, const float width, const const_array_view<render_d2d_vector> shape) noexcept;
        error_type initialize(const render_d2d_matrix& transform, const color color, const render_svg_font font, const string_view text) noexcept;
		const_array_view<render_d2d_vertex> vertices() const noexcept
		{
			return m_vertices;
		}
        array<render_d2d_vertex>& take_vertices() noexcept
        {
            return m_vertices;
        }
		std::atomic<uint32_t> ref = 1;
	private:
		any_system_acqrel_pointer<render_svg_system> m_render;
		array<render_d2d_vertex> m_vertices;
        com_ptr<IDWriteTextLayout> m_layout;
    };
	error_type render_svg_tesselate(const render_d2d_matrix& matrix, ID2D1Geometry& geometry, array<render_d2d_triangle>& mesh) noexcept;
	error_type render_svg_rasterize(const_array_view<render_d2d_triangle> mesh, const NSVGpaint& paint, const float opacity, array<render_d2d_vertex>& vertices) noexcept;
}
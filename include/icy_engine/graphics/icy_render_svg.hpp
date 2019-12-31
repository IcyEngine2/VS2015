#pragma once

#include <icy_engine/core/icy_array.hpp>
#include <icy_engine/core/icy_color.hpp>
#include <icy_engine/core/icy_string.hpp>
#include "icy_render_math.hpp"

namespace icy
{
	struct render_svg_text_flag
    {
		enum _enum : uint32_t
		{
			none = 0,

			font_min_stretch = 0,
			font_max_stretch = 9,
			font_min_weight = 0,
			font_max_weight = 999,

			//  applied at string level (can be inline)

			font_size = 0,
			font_stretch,
			font_normal,
			font_italic,
			font_oblique,
			font_weight,
			strikethrough,
			underline,

			//  applied at paragraph level
			line_spacing,   //  0   (default)   -   non uniform
			par_align_near,
			par_align_far,
			par_align_center,
			trimming_none,
			trimming_word,
			trimming_char,
			align_leading,
			align_trailing,
			align_center,

			_total,
		};
		render_svg_text_flag(const _enum type = none, const uint32_t value = 0) noexcept : type(type), value(value)
		{

		}
		const _enum type;
        const uint32_t value;
    };
	struct render_svg_font_data 
	{
		array<render_svg_text_flag> flags;
		string name;
	};
    struct render_svg_properties
    {
        render_svg_properties() noexcept : offset({}), layer(0), enabled(true)
        {

        }
        render_svg_properties(const render_d2d_vector offset, const float layer, const bool enabled) noexcept :
            offset(offset), layer(layer), enabled(enabled)
        {

        }
        render_d2d_vector offset;
        float layer;
        bool enabled;
    };

    class render_svg_font
	{
	public:
		static error_type enumerate(array<string>& fonts) noexcept;
		render_svg_font() noexcept = default;
		render_svg_font(const render_svg_font& rhs) noexcept;
		~render_svg_font() noexcept
		{
			shutdown();
		}
		void shutdown() noexcept;
		error_type initialize(const render_svg_font_data& data) noexcept;
	public:
		class data_type;
		data_type* data = nullptr;
	};
	class render_svg_geometry
	{
	public:
		render_svg_geometry() noexcept = default;
		render_svg_geometry(const render_svg_geometry& rhs) noexcept;
		~render_svg_geometry() noexcept
		{
			shutdown();
		}
		void shutdown() noexcept;
		error_type initialize(const render_d2d_matrix& transform, const string_view text) noexcept;
        error_type initialize(const render_d2d_matrix& transform, const color color, const float width, const render_d2d_line& shape) noexcept
        {
            const render_d2d_vector points[] = { shape.v0, shape.v1 };
            return initialize(transform, color, width, points);
        }
        error_type initialize(const render_d2d_matrix& transform, const color color, const float width, const render_d2d_triangle& shape) noexcept
        {
            return initialize(transform, color, width, shape.vec);
        }
        error_type initialize(const render_d2d_matrix& transform, const color color, const float width, const render_d2d_rectangle& shape) noexcept
        {
            const render_d2d_vector points[] =
            {
                { shape.x + 0 * shape.w, shape.y + 0 * shape.h },
                { shape.x + 0 * shape.w, shape.y + 1 * shape.h },
                { shape.x + 1 * shape.w, shape.y + 1 * shape.h },
                { shape.x + 1 * shape.w, shape.y + 0 * shape.h },
                { shape.x + 0 * shape.w, shape.y + 0 * shape.h },
            };
            return initialize(transform, color, width, points);
        }
        error_type initialize(const render_d2d_matrix& transform, const color color, const float width, const const_array_view<render_d2d_vector> shape) noexcept;
        error_type initialize(const render_d2d_matrix& transform, const color color, const render_svg_font font, const string_view text) noexcept;
	public:
		class data_type;
		data_type* data = nullptr;
	};
}
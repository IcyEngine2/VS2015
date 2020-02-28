#pragma once

#include "icy_string_view.hpp"

#pragma warning(push)
#pragma warning(disable:4201) // nameless struct/union

namespace icy
{
	enum class colors : uint32_t
	{
		none,
		alice_blue,
		antique_white,
		aqua,
		aquamarine,
		azure,
		beige,
		bisque,
		black,
		blanched_almond,
		blue,
		blue_violet,
		brown,
		burly_wood,
		cadet_blue,
		chartreuse,
		chocolate,
		coral,
		cornflower_blue,
		cornsilk,
		crimson,
		cyan,
		dark_blue,
		dark_cyan,
		dark_goldenrod,
		dark_gray,
		dark_green,
		dark_khaki,
		dark_magenta,
		dark_olive_green,
		dark_orange,
		dark_orchid,
		dark_red,
		dark_salmon,
		dark_sea_green,
		dark_slate_blue,
		dark_slate_gray,
		dark_turquoise,
		dark_violet,
		deep_pink,
		deep_sky_blue,
		dim_gray,
		dodger_blue,
		firebrick,
		floral_white,
		forest_green,
		fuchsia,
		gainsboro,
		ghost_white,
		gold,
		goldenrod,
		gray,
		green,
		green_yellow,
		honeydew,
		hot_pink,
		indian_red,
		indigo,
		ivory,
		khaki,
		lavender,
		lavender_blush,
		lawn_green,
		lemon_chiffon,
		light_blue,
		light_coral,
		light_cyan,
		light_goldenrod_yellow,
		light_green,
		light_gray,
		light_pink,
		light_salmon,
		light_sea_green,
		light_sky_blue,
		light_slate_gray,
		light_steel_blue,
		light_yellow,
		lime,
		lime_green,
		linen,
		magenta,
		maroon,
		medium_aquamarine,
		medium_blue,
		medium_orchid,
		medium_purple,
		medium_sea_green,
		medium_slate_blue,
		medium_spring_green,
		medium_turquoise,
		medium_violet_red,
		midnight_blue,
		mint_cream,
		misty_rose,
		moccasin,
		navajo_white,
		navy,
		old_lace,
		olive,
		olive_drab,
		orange,
		orange_red,
		orchid,
		pale_goldenrod,
		pale_green,
		pale_turquoise,
		pale_violet_red,
		papaya_whip,
		peach_puff,
		peru,
		pink,
		plum,
		powder_blue,
		purple,
		red,
		rosy_brown,
		royal_blue,
		saddle_brown,
		salmon,
		sandy_brown,
		sea_green,
		sea_shell,
		sienna,
		silver,
		sky_blue,
		slate_blue,
		slate_gray,
		snow,
		spring_green,
		steel_blue,
		tan,
		teal,
		thistle,
		tomato,
		turquoise,
		violet,
		wheat,
		white,
		white_smoke,
		yellow,
		yellow_green,
		_count,
	};
	class color;
	class color_hsv;

    int compare(const color& lhs, const color& rhs) noexcept;
    class color
    {
    public:
		struct float_array
		{
			float_array(float* ptr) noexcept : ptr(ptr)
			{

			}
			float* const ptr;
		};
        union
        {
            uint8_t array[4];
            struct
            {
                uint8_t b;
                uint8_t g;
                uint8_t r;
                uint8_t a;
            };
            uint32_t bgra;
        };
    public:
        rel_ops(color);
		color() noexcept : bgra(0)
		{

		}
		color(const colors type, const uint8_t alpha = 255) noexcept;
		static color from_bgra(const uint32_t bgra) noexcept
		{
			color value;
			value.bgra = bgra;
			return value;
		}
		static color from_bgra(const uint8_t b, const uint8_t g, const uint8_t r, const uint8_t a = 0xFF) noexcept
		{
			color value;
			value.r = r;
			value.g = g;
			value.b = b;
			value.a = a;
			return value;
		}
		static color from_rgba(const uint8_t r, const uint8_t g, const uint8_t b, const uint8_t a = 0xFF) noexcept
		{
			color value;
			value.r = r;
			value.g = g;
			value.b = b;
			value.a = a;
			return value;
		}
		static color from_rgbaf(const float r, const float g, const float b, const float a = 1) noexcept
        {
            color value;
            value.r	= uint8_t(r * 0xFF);
            value.g = uint8_t(g * 0xFF);
            value.b = uint8_t(b * 0xFF);
            value.a = uint8_t(a * 0xFF);
            return value;
        }
		static color from_bgraf(const float b, const float g, const float r, const float a = 1) noexcept
		{
			color value;
			value.r = uint8_t(r * 0xFF);
			value.g = uint8_t(g * 0xFF);
			value.b = uint8_t(b * 0xFF);
			value.a = uint8_t(a * 0xFF);
			return value;
		}
		static color from_hsv(const color_hsv& hsv, const uint8_t alpha = 0xFF) noexcept;
        uint32_t to_rgb() const noexcept
        {
            return uint32_t((r << shift_red) + (g << shift_green) + (b << shift_blue));
        }
		void to_rgbaf(float_array rgba_array) const noexcept
		{
			const auto k = 1.0F / 255.0F;
			rgba_array.ptr[0] = array[2] * k;
			rgba_array.ptr[1] = array[1] * k;
			rgba_array.ptr[2] = array[0] * k;
			rgba_array.ptr[3] = array[3] * k;
		}
        void to_rgbaf(float(&rgba_array)[4]) const noexcept
        {
			to_rgbaf(float_array(rgba_array));
        }
		float operator-(const color& rhs) const noexcept;
    private:
        enum
        {
            shift_blue  = 0x00,
            shift_green = 0x08,
            shift_red   = 0x10,
            shift_alpha = 0x18,
        };
    };
    class color_hsv
    {
    public:
        float hue;
        float sat;
        float val;
    public:
		color_hsv() noexcept : hue(0), sat(0), val(0)
		{

		}
		color_hsv(const float hue, const float sat, const float val) noexcept : hue(hue), sat(sat), val(val)
        {

        }
		color_hsv(const color& color) noexcept;
    };

    inline int compare(const color& lhs, const color& rhs) noexcept
    {
        return icy::compare(lhs.to_rgb(), rhs.to_rgb());
    }

	string_view to_string(const colors value) noexcept;
	error_type to_string(const color value, string& str) noexcept;
}

#pragma warning(pop)
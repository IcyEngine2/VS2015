#pragma once

#include <icy_engine/core/icy_core.hpp>

namespace icy
{
    enum class key : uint32_t
	{
		none = 0,

        mouse_left = 0x01,
        mouse_right = 0x02,
        mouse_mid = 0x03,
        mouse_x1 = 0x04,
        mouse_x2 = 0x05,

		back = 0x08,
		tab = 0x09,
		enter = 0x0D,
		pause = 0x13,
		caps_lock = 0x14,
		esc = 0x1B,
		space = 0x20,
		page_up = 0x21,
		page_down = 0x22,
		end = 0x23,
		home = 0x24,
		left = 0x25,
		up = 0x26,
		right = 0x27,
		down = 0x28,
		print_screen = 0x2A,
		insert = 0x2D,
		del = 0x2E,

		_0 = '0',
		_1 = '1',
		_2 = '2',
		_3 = '3',
		_4 = '4',
		_5 = '5',
		_6 = '6',
		_7 = '7',
		_8 = '8',
		_9 = '9',

		a = 'A',
		b = 'B',
		c = 'C',
		d = 'D',
		e = 'E',
		f = 'F',
		g = 'G',
		h = 'H',
		i = 'I',
		j = 'J',
		k = 'K',
		l = 'L',
		m = 'M',
		n = 'N',
		o = 'O',
		p = 'P',
		q = 'Q',
		r = 'R',
		s = 'S',
		t = 'T',
		u = 'U',
		v = 'V',
		w = 'W',
		x = 'X',
		y = 'Y',
		z = 'Z',

		left_win = 0x5B,
		right_win = 0x5C,

		num_0 = 0x60,
		num_1 = 0x61,
		num_2 = 0x62,
		num_3 = 0x63,
		num_4 = 0x64,
		num_5 = 0x65,
		num_6 = 0x66,
		num_7 = 0x67,
		num_8 = 0x68,
		num_9 = 0x69,

		num_mul = 0x6A,
		num_add = 0x6B,
		num_separator = 0x6C,
		num_sub = 0x6D,
		num_decimal = 0x6E,
		num_div = 0x6F,
		num_enter = 255,

		F1 = 0x70,
		F2 = 0x71,
		F3 = 0x72,
		F4 = 0x73,
		F5 = 0x74,
		F6 = 0x75,
		F7 = 0x76,
		F8 = 0x77,
		F9 = 0x78,
		F10 = 0x79,
		F11 = 0x7A,
		F12 = 0x7B,

		num_lock = 0x90,
		scr_lock = 0x91,

		left_shift = 0xA0,
		right_shift = 0xA1,

		left_ctrl = 0xA2,
		right_ctrl = 0xA3,

		left_alt = 0xA4,
		right_alt = 0xA5,

		colon = 0xBA,
		plus = 0xBB,
		comma = 0xBC,
		minus = 0xBD,
		period = 0xBE,
		slash = 0xBF,
		tilde = 0xC0,

		bracket_open = 0xDB,
		back_slash = 0xDC,
		bracket_close = 0xDD,
		quote = 0xDE,
	};

	template<>
	inline int compare<key>(const key& lhs, const key& rhs) noexcept
	{
		return int32_t(lhs) - int32_t(rhs);
	}
}
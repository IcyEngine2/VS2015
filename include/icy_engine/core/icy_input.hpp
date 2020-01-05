#pragma once

#include "icy_core.hpp"
#include "icy_key.hpp"
#include "icy_string_view.hpp"
#include <bitset>

#pragma warning(push)
#pragma warning(disable:4201) // nameless struct/union
#pragma warning(disable:4582)

struct tagMSG;

namespace icy
{
	class window;

    enum class key_event : uint32_t
    {
        none,
        release,
        hold,
        press,
        _total,
    };
	struct mouse_event_enum
	{
		enum : uint32_t
		{
			none,
			move,
			wheel,
			btn_release,
			btn_press,
			btn_hold,
			btn_double,
			_total,
		};
	};
	struct mouse_button_enum
	{
		enum : uint32_t
		{
            none,
			left,
			right,
			mid,
			x1,
			x2,
			_total,
		};
	};
	enum class input_type : uint32_t
	{
		none,
		key,
		mouse,
		text,
	};
	using mouse_event = decltype(mouse_event_enum::_total);
	using mouse_button = decltype(mouse_button_enum::_total);
		
    struct key_message
    {
        key_message
        (
            const key key = key::none, 
            const key_event event = key_event::none, 
            const uint32_t ctrl = 0,
            const uint32_t alt = 0, 
            const uint32_t shift = 0
        ) noexcept : key(key), event(event), ctrl(ctrl), alt(alt), shift(shift)
        {

        }
        ICY_DEFAULT_COPY_ASSIGN(key_message);
        explicit operator bool() const noexcept
        {
            return event != key_event::none && key != key::none;
        }
        const key key;
        const key_event event;
        const uint32_t ctrl;
        const uint32_t alt;
        const uint32_t shift;
    };
    class key_mod
    {
    public:
        //  pass in key_message .control or .alt or .shift
        key_mod(const uint32_t mod) noexcept : m_value(mod)
        {

        }
		key_mod(const key_mod& rhs) noexcept : m_value(rhs.m_value)
		{

		}
		ICY_DEFAULT_COPY_ASSIGN(key_mod);
        explicit operator bool() const noexcept
        {
            return !!m_value;
        }
        auto left() const noexcept
        {
            return !!(m_value & 0x01);
        }
        auto right() const noexcept
        {
            return !!(m_value & 0x02);
        }
        explicit operator uint32_t() const noexcept
        {
            return m_value;
        }
    private:
        const uint32_t m_value;
    };
    struct mouse_message
    {
        mouse_message() noexcept
        {
            std::memset(this, 0, sizeof(*this));
        }
        explicit operator bool() const noexcept
        {
            return event != mouse_event::none;
        }
        mouse_event event;
		struct
		{
			double x;
			double y;
		} point;
        union
        {
			struct
			{
				double x;
				double y;
			} delta;
            double wheel;
            mouse_button button;
        };
        struct
        {
            uint32_t ctrl : 2;
            uint32_t alt : 2;
            uint32_t shift : 2;
            union
            {
                struct
                {
                    uint32_t left : 1;
                    uint32_t right : 1;
                    uint32_t mid : 1;
                    uint32_t x1 : 1;
                    uint32_t x2 : 1;
                };
                uint32_t buttons : 5;
            };
        };
    };
	struct input_message
	{
		enum
		{
			u16_max = 2,
		};
		explicit operator bool() const noexcept
		{
			return type != input_type::none;
		}
		input_message() : type(input_type::none)
		{

		}
		input_message(const key_message& key) noexcept : type(input_type::key), key(key)
		{

		}
		input_message(const mouse_message& mouse) noexcept : type(input_type::mouse), mouse(mouse)
		{

		}
		input_message(const wchar_t* const wstring) noexcept : type(input_type::text), text{}
		{
			for (auto k = 0_z; k < u16_max && wstring && wstring[k]; ++k)
				text[k] = wstring[k];
		}
		ICY_DEFAULT_COPY_ASSIGN(input_message);
		const union
		{
			key_message key;
			mouse_message mouse;
			wchar_t text[u16_max + 1];
		};
		const input_type type;
	};
    namespace detail
    {
        inline auto fill_key_mod(key_message& msg, const std::bitset<256>& buffer) noexcept
        {
            msg = 
            { 
                msg.key, 
                msg.event, 
                uint32_t((buffer[size_t(key::left_ctrl)] << 0)  | (buffer[size_t(key::right_ctrl)] << 1)),
                uint32_t((buffer[size_t(key::left_alt)] << 0)   | (buffer[size_t(key::right_alt)] << 1)),
                uint32_t((buffer[size_t(key::left_shift)] << 0) | (buffer[size_t(key::right_shift)] << 1))
            };
        }
		inline auto fill_key_mod(mouse_message& msg, const std::bitset<256>& buffer) noexcept
        {
            msg.ctrl = uint32_t((buffer[size_t(key::left_ctrl)] << 0) | (buffer[size_t(key::right_ctrl)] << 1));
            msg.alt = uint32_t((buffer[size_t(key::left_alt)] << 0) | (buffer[size_t(key::right_alt)] << 1));
            msg.shift = uint32_t((buffer[size_t(key::left_shift)] << 0) | (buffer[size_t(key::right_shift)] << 1));
        }
		key scan_vk_to_key(uint16_t vkey, uint16_t scan, const bool isE0) noexcept;
		uint16_t from_winapi(key_message& msg, const size_t wParam, const ptrdiff_t lParam, const std::bitset<256>& buffer) noexcept;
		uint16_t from_winapi(key_message& key, const tagMSG& msg, const std::bitset<256> & buffer) noexcept;
		void from_winapi(mouse_message& mouse, const uint32_t offset_x, const uint32_t offset_y, const uint32_t msg, const size_t wParam, const ptrdiff_t lParam, const std::bitset<256> & buffer) noexcept;
		void from_winapi(mouse_message& mouse, const uint32_t offset_x, const uint32_t offset_y, const tagMSG& msg, const std::bitset<256>& buffer) noexcept;
		tagMSG to_winapi(const key_message& key) noexcept;
		tagMSG to_winapi(const mouse_message& mouse) noexcept;
		tagMSG to_winapi(const input_message& input) noexcept;
    }
	inline auto key_mod_and(const uint32_t lhs, const uint32_t rhs) noexcept
	{
		return lhs ? (lhs & rhs) != 0 : true;
	}
	error_type to_string(const key_message& key, string& str) noexcept;
    string_view to_string(const key key) noexcept;
}

#pragma warning(pop)
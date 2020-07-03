#pragma once

#include "icy_string_view.hpp"
#include "icy_memory.hpp"
#include <tuple>

#if !defined ICY_DEFAULT_STRING_BUFFER
#define ICY_DEFAULT_STRING_BUFFER (64 - sizeof(void*) * 3)
#endif

namespace icy
{
    template<typename T> class array;
    enum float_type
    {
        float_type_fixed,
        float_type_exp,
    };
	class string : public string_view
	{
		template<typename T> friend error_type to_string_unsigned(const T _value, const uint64_t base, string& str) noexcept;
		friend error_type to_string(int64_t value, string& str) noexcept;
	public:
		using type = string;
	public:
		string() noexcept
		{
			m_ptr = m_buffer;
		}
		string(const string&) = delete;
		string& operator=(const string&) = delete;
		string(string&& rhs) noexcept;
		~string() noexcept;
		ICY_DEFAULT_MOVE_ASSIGN(type);
	public:
		size_type capacity() const noexcept
		{
			return is_dynamic() ? m_capacity : _countof(m_buffer);
		}
		bool is_dynamic() const noexcept
		{
			return m_ptr != m_buffer;
		}
		error_type append(const string_view rhs) noexcept;
        template<typename... arg_types>
        error_type appendf(const string_view format, arg_types&& ... args) noexcept;
		void clear() noexcept
		{
			m_ptr[m_size = 0] = '\0';
		}
		error_type insert(const iterator where, const string_view rhs) noexcept;
		void pop_back(const size_type count = 1) noexcept;
		error_type replace(const string_view find, const string_view replace) noexcept;
		error_type reserve(const size_type capacity) noexcept;
		error_type resize(const size_type size, const char symb = '\0') noexcept;
	private:
		char m_buffer[ICY_DEFAULT_STRING_BUFFER] = {};
        size_t m_capacity = 0;
	};
	template<typename T> class array;

	namespace detail
	{
		inline error_type to_string_arg(std::integral_constant<size_t, 0>, size_t, const void*, string&) noexcept
		{
			return make_stdlib_error(std::errc::invalid_argument);
		}
		template<size_t N, typename tuple_type, typename = std::enable_if_t<(N > 0)>>
		inline error_type to_string_arg(std::integral_constant<size_t, N>, size_t index, const tuple_type * tuple, string & str) noexcept
		{
			if (index == N)
				return icy::to_string(std::get<N - 1>(*tuple), str);
			else
				return to_string_arg(std::integral_constant<size_t, N - 1>{}, index, tuple, str);
		}
	}
	inline error_type to_string(const string_view rhs, string& str) noexcept
	{
		str.clear();
		return str.append(rhs);
	}
	inline error_type to_string(const char* const value, string& str) noexcept
	{
		return to_string(string_view(value, strlen(value)), str);
	}
	inline error_type to_string(const string_iterator first, const string_iterator last, string& str) noexcept
	{
		return to_string(string_view(first, last), str);
	}
	error_type to_string(const const_array_view<wchar_t> src, string& str) noexcept;
	template<typename T> inline error_type to_string_unsigned(const T _value, const uint64_t base, string& str) noexcept
	{
		static_assert(std::is_unsigned<T>::value, "INVALID TYPE");
		char buffer[8 * sizeof(T) + sizeof("0b")];
		auto length = 0_z;
		ICY_ERROR(str.reserve(sizeof(buffer)));

		uint64_t value = _value;

		buffer[length++] = '\0';
		do
		{
			buffer[length++] = "0123456789ABCDEF"[value % base];
			value /= base;
		} while (value);

		//constexpr max_digits_base_10[] = { sizeof("255"),
		//sizeof("65536"), sizeof("4294967296"),
		//sizeof("18446744073709551615") };
		//constexpr max_digits_base_8[] = { sizeof("377"),
		//sizeof("377777"), sizeof("37777777777"),
		//sizeof("1777777777777777777777") };
		// 4 6 11 21
		// 4 7 12 25
		//(remember offset by one) -> { 3, 5, 10, 20 } and { 3, 6, 11, 24 }
		const auto max_length = 1 + (base == 2 ?
			sizeof(T) * 8 : base == 16 ?
			sizeof(T) * 2 : base == 8 ?
			("DGLY"[detail::constexpr_log2(sizeof(T))] - 'A') :
			("DFKU"[detail::constexpr_log2(sizeof(T))] - 'A'));

		if (base != 10)
		{
			std::fill(buffer + length, buffer + max_length, '0');
			length = max_length;

			switch (base)
			{
			case 8:
				buffer[length++] = '0';
				break;
			case 2:
			case 16:
				buffer[length++] = "bx"[base > 2];
				buffer[length++] = '0';
				break;
			case 10:
				break;
			default:
				ICY_ASSERT(false, "INVALID TO_STRING UNSIGNED BASE");
			}
		}
		char rbuffer[sizeof(buffer)];
		std::reverse_copy(buffer, buffer + length, rbuffer);
		memcpy(str.m_ptr, rbuffer, length - 1);
		str.m_size = length - 1;
		return error_type();
	}
	error_type to_string(const double value, const float_type type, size_t prec, string& str) noexcept;
	inline error_type to_string(const double value, const float_type type, string& str) noexcept
	{
		return icy::to_string(value, type, FLT_DIG, str);
	}
	inline error_type to_string(const double value, string& str) noexcept
	{
		return icy::to_string(value, float_type::float_type_fixed, FLT_DIG, str);
	}
	error_type to_string(const int64_t value, string& str) noexcept;
	inline error_type to_string(const int8_t value, string& str) noexcept
	{
		return to_string(int64_t(value), str);
	}
	inline error_type to_string(const int16_t value, string& str) noexcept
	{
		return to_string(int64_t(value), str);
	}
	inline error_type to_string(const int32_t value, string& str) noexcept
	{
		return to_string(int64_t(value), str);
	}
	inline error_type to_string(const uint8_t value, const uint64_t base, string& str) noexcept
	{
		return to_string_unsigned(value, base, str);
	}
	inline error_type to_string(const uint16_t value, const uint64_t base, string& str) noexcept
	{
		return to_string_unsigned(value, base, str);
	}
	inline error_type to_string(const uint32_t value, const uint64_t base, string& str) noexcept
	{
		return to_string_unsigned(value, base, str);
	}
	inline error_type to_string(const uint64_t value, const uint64_t base, string& str) noexcept
	{
		return to_string_unsigned(value, base, str);
	}
	inline error_type to_string(const uint8_t value, string& str) noexcept
	{
		return to_string_unsigned(value, 10, str);
	}
	inline error_type to_string(const uint16_t value, string& str) noexcept
	{
		return to_string_unsigned(value, 10, str);
	}
	inline error_type to_string(const uint32_t value, string& str) noexcept
	{
		return to_string_unsigned(value, 10, str);
	}
	inline error_type to_string(const uint64_t value, string& str) noexcept
	{
		return to_string_unsigned(value, 10, str);
	}
	inline error_type to_string(const long value, string& str) noexcept
	{
		return to_string_unsigned((unsigned long)(value), 16, str);
	}
	error_type to_string(const guid& value, string& str) noexcept;
    error_type to_string(const std::chrono::steady_clock::time_point time, string& str, const bool local = true) noexcept;
    error_type to_string(const std::chrono::system_clock::time_point time, string& str, const bool local = true) noexcept;
	template<typename... arg_types>
	inline error_type string::appendf(const string_view format, arg_types&& ... args) noexcept
	{
		ICY_ERROR(reserve(bytes().size() + format.bytes().size()));

		auto ptr = format.bytes().data();
		auto len = format.bytes().size();
		const auto tuple = std::make_tuple(std::forward<arg_types>(args)...);
		for (auto k = 0u; k < len; ++k)
		{
			if (ptr[k] == '%')
			{
				if (k + 1 >= len)
					return make_stdlib_error(std::errc::invalid_argument);

				if (ptr[k + 1] == '%')
				{
					if (const auto error = append("%"_s))
						return error;
					++k;
				}
				else if (ptr[k + 1] >= '0' && ptr[k] <= '9')
				{
					const auto beg = ptr + k + 1;
					k += 1;
					while (k < len && ptr[k] >= '0' && ptr[k] <= '9')
						++k;
					size_t index = 0;
					if (const auto error = to_value_uint(string_view{ beg, ptr + k }, index))
						return error;
					if (index > sizeof...(args))
						return make_stdlib_error(std::errc::invalid_argument);
					string arg;
					ICY_ERROR(detail::to_string_arg(std::integral_constant<size_t, sizeof...(args)>{}, index, & tuple, arg));
					ICY_ERROR(append(arg));
					if (k < len)
						--k;
				}
				else
					return make_stdlib_error(std::errc::invalid_argument);
			}
			else
			{
				ICY_ERROR(append(string_view{ &ptr[k], 1 }));
			}
		}
		return error_type();
	}
    template<typename... arg_types>
    inline error_type to_string(const string_view format, string& str, arg_types&& ... args) noexcept
    {
        str.clear();
        ICY_ERROR(str.appendf(format, std::forward<arg_types>(args)...));
        return error_type();
    }
	error_type to_utf16(const string_view value, array<wchar_t>& wide) noexcept;

    template<typename T> struct is_string : public std::false_type 
    {
    
    };
    template<> struct is_string<string> : public std::true_type
    {

    };
    template<> struct is_string<string_view> : public std::true_type
    {

	};
    
    template<typename func_type>
    inline error_type transform(const string_view str, string& output, func_type&& func) noexcept
    {
        ICY_ERROR(output.reserve(str.bytes().size()));
        for (auto it = str.begin(); it != str.end(); ++it)
        {
            char32_t chr = 0;
            ICY_ERROR(it.to_char(chr));
            func(chr);
            if (!chr)
                continue;

            char c[5] = { '\0', '\0', '\0', '\0', '\0' };
            if (0 == chr)
            {
                ;
            }
            else if (0 == (char32_t(0xffffff80) & chr))
            {
                // 1-byte/7-bit ascii
                // (0b0xxxxxxx)
                c[0] = (char)chr;
            }
            else if (0 == (char32_t(0xfffff800) & chr))
            {
                // 2-byte/11-bit utf8 code point
                // (0b110xxxxx 0b10xxxxxx)
                c[0] = 0xc0 | (char)(chr >> 6);
                c[1] = 0x80 | (char)(chr & 0x3f);
            }
            else if (0 == (char32_t(0xffff0000) & chr))
            {
                // 3-byte/16-bit utf8 code point
                // (0b1110xxxx 0b10xxxxxx 0b10xxxxxx)
                c[0] = 0xe0 | (char)(chr >> 12);
                c[1] = 0x80 | (char)((chr >> 6) & 0x3f);
                c[2] = 0x80 | (char)(chr & 0x3f);
            }
            else if (0 == ((char32_t)0xffe00000 & chr))
            {
                // 4-byte/21-bit utf8 code point
                // (0b11110xxx 0b10xxxxxx 0b10xxxxxx 0b10xxxxxx)
                c[0] = 0xf0 | (char)(chr >> 18);
                c[1] = 0x80 | (char)((chr >> 12) & 0x3f);
                c[2] = 0x80 | (char)((chr >> 6) & 0x3f);
                c[3] = 0x80 | (char)(chr & 0x3f);
            }
            ICY_ERROR(output.append(string_view(c)));
        }
        return error_type();
    }
    error_type to_lower(const string_view str, string& output) noexcept;
    error_type to_upper(const string_view str, string& output) noexcept;
    error_type copy(const string_view src, string& dst) noexcept;
    error_type copy(const string& src, string& dst) noexcept;

    template<typename key_type, typename value_type> class map;
    struct locale
    {
        static error_type enumerate(array<locale>& locales) noexcept;
        string display_lang;    //  UI language name
        string display_ctry;    //  UI country
        string global_lang;     //  international language name
        string global_ctry;     //  international country
        string local_lang;      //  localized language name
        string local_ctry;      //  localized country
        string short_name;      //  short name (en-US)
        uint32_t code = 0;      //  lcid
    };    
 }
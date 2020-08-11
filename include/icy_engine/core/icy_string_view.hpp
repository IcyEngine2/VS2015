#pragma once

#include "icy_array_view.hpp"

extern "C" __declspec(dllimport) int __stdcall MultiByteToWideChar(unsigned CodePage, unsigned long dwFlags,
	const char* lpMultiByteStr, int cbMultiByte, wchar_t* lpWideCharStr, int cchWideChar);

namespace icy
{
    template<typename T> class array;
	class string_iterator;
	class string_view;
	constexpr string_view operator""_s(const char*, size_t) noexcept;
	inline constexpr uint32_t constexpr_hash(const char* const str, const uint32_t last = 0x811C9DC5) noexcept
	{
		return !*str ? last : constexpr_hash(str + 1,
			uint32_t((last ^ *str) * 0x01000193ui64));
	}
	inline constexpr auto constexpr_hash(const char* const begin, const char* const end, const uint32_t last = 0x811C9DC5) noexcept
	{
		return begin == end ? last : constexpr_hash(begin + 1,
			uint32_t((last ^ *begin) * 0x01000193ui64));
	}
	inline constexpr auto operator""_hash(const char* const str, size_t) noexcept
	{
		return constexpr_hash(str);
	}

	class string;
    class string_iterator;

	template<> inline int compare<string_iterator>(const string_iterator& lhs, const string_iterator& rhs) noexcept;
	template<> inline int compare<string_view>(const string_view& lhs, const string_view& rhs) noexcept;

	class string_iterator
	{
		friend string;
		enum : uint8_t
		{
			mask_0 = 0x80,
			mask_1 = 0x40,
			mask_2 = 0x20,
			mask_3 = 0x10,
		};
		friend string_view;
	public:
		using type = string_iterator;
		using iterator_category = std::bidirectional_iterator_tag;
		using value_type = char;
		using reference = const char&;
		using pointer = const char*;
		using size_type = size_t;
		using difference_type = ptrdiff_t;
		ICY_DEFAULT_COPY_ASSIGN(string_iterator);
	private:
		string_iterator(const pointer begin, const pointer end, const pointer ptr) noexcept :
			m_ptr{ ptr }, m_begin{ begin }, m_end{ end }
		{

		}
	public:
        rel_ops(string_iterator);
        string_iterator& operator++() noexcept;
		auto operator++(int) noexcept
		{
			auto tmp = *this;
			++* this;
			return tmp;
		}
		auto& operator+=(const size_t size) noexcept
		{
			for (auto k = 0_z; k < size; ++k)
				++ * this;
			return *this;
		}
		auto operator+(const size_t size) const noexcept
		{
			auto tmp = *this;
			return tmp += size;
		}
        string_iterator& operator--() noexcept;
		auto operator--(int) noexcept
		{
			auto tmp = *this;
			--* this;
			return tmp;
		}
		auto& operator-=(const size_t size) noexcept
		{
			for (auto k = 0_z; k < size; ++k)
				-- * this;
			return *this;
		}
		auto operator-(const size_t size) const noexcept
		{
			auto tmp = *this;
			return tmp -= size;
		}
        error_type to_char(char32_t& chr) const noexcept;
		explicit operator pointer() const noexcept
		{
			return m_ptr;
		}
        char32_t operator*() const noexcept
        {
            char32_t chr = 0;
            to_char(chr);
            return chr;
        }
	private:
		pointer m_ptr;
		const pointer m_begin;
		const pointer m_end;
	};
	class string_view
	{
		friend constexpr string_view operator""_s(const char*, size_t) noexcept;
		friend string;
	public:
		using type = string_view;
		using value_type = char;
		using pointer = value_type *;
		using const_pointer = const value_type*;
		using reference = value_type &;
		using const_reference = const value_type &;
		using size_type = size_t;
		using difference_type = ptrdiff_t;
		using const_iterator = string_iterator;
		using iterator = const_iterator;
		using const_reverse_iterator = std::reverse_iterator<const_iterator>;
		using reverse_iterator = std::reverse_iterator<iterator>;
	public:
        rel_ops(string_view);
		string_view() noexcept : m_ptr{}, m_size{}
		{

		}
        string_view(const string_view& rhs) noexcept : m_ptr(rhs.m_ptr), m_size(rhs.m_size)
        {

        }
		string_view(const string_iterator first, const string_iterator last) noexcept :
			string_view{ first.m_ptr, last.m_ptr }
		{

		}
		constexpr string_view(const const_pointer ptr, const size_type size) noexcept :
			m_ptr{ const_cast<pointer>(ptr) }, m_size{ size }
		{

		}
		/*string_view(const_pointer ptr) noexcept :
			m_ptr{ const_cast<pointer>(ptr) }, m_size{ 0 }
		{
			while (ptr && *ptr)
			{
				++ptr;
				++m_size;
			}
		}*/
		string_view(const const_pointer begin, const const_pointer end) noexcept :
			m_ptr{ const_cast<pointer>(begin) }, m_size{ size_type(end - begin) }
		{
			ICY_ASSERT(begin <= end, "INVALID ITERATOR RANGE (BEGIN > END)");
		}
		bool empty() const noexcept
		{
			return !m_size;
		}
		const_iterator begin() const noexcept
		{
			return const_iterator{ m_ptr, m_ptr + m_size, m_ptr };
		}
        const_iterator cbegin() const noexcept
		{
			return begin();
		}
        const_iterator end() const noexcept
		{
			return const_iterator{ m_ptr, m_ptr + m_size, m_ptr + m_size };
		}
        const_iterator cend() const noexcept
		{
			return end();
		}
		auto rbegin() const noexcept
		{
			return std::make_reverse_iterator(end());
		}
		auto crbegin() const noexcept
		{
			return rbegin();
		}
		auto rend() const noexcept
		{
			return std::make_reverse_iterator(begin());
		}
		auto crend() const noexcept
		{
			return rend();
		}
        const_array_view<char> bytes() const noexcept
		{
			return const_array_view<char>{ m_ptr, m_size };
		}
        const_array_view<uint8_t> ubytes() const noexcept
		{
			return const_array_view<uint8_t>{ reinterpret_cast<uint8_t*>(m_ptr), m_size };
		}
        const_iterator find(const string_view& rhs) const noexcept;
        const_iterator rfind(const string_view& rhs) const noexcept;
        error_type to_utf16(wchar_t* const data, size_t* const size) const noexcept;        
	protected:
		pointer m_ptr;
		size_type m_size;
	};

	template<> inline int compare<string_iterator>(const string_iterator& lhs, const string_iterator& rhs) noexcept
	{
		return compare(static_cast<const char*>(lhs), static_cast<const char*>(rhs));
	}
	template<> inline int compare<string_view>(const string_view& lhs, const string_view& rhs) noexcept
	{
		return compare(lhs.bytes(), rhs.bytes());
	}

	inline constexpr string_view operator""_s(const char* const ptr, const size_t size) noexcept
	{
		return string_view(ptr, size);
	}
    uint32_t hash(const string_view string) noexcept;
    uint64_t hash64(const string_view string) noexcept;

    //  split string by specific delimiter
	error_type split(const string_view str, array<string_view>& words, const string_view delims) noexcept;
	inline error_type split(const string_view str, array<string_view>& words, const char delim) noexcept
	{
		return split(str, words, string_view(&delim, 1));
	}
	//  split string by whitespace
	error_type split(const string_view str, array<string_view>& words) noexcept;
    
    static const char base64_alpha[] =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
        "abcdefghijklmnopqrstuvwxyz"
        "0123456789+/";
    inline constexpr size_t base64_encode_size(const size_t input_size) noexcept
    {
        return round_up(input_size, 3) / 3 * 4;
    }
    inline constexpr size_t base64_decode_size(const size_t input_size) noexcept
    {
        return round_up(input_size, 4) / 4 * 3;
    }
    error_type base64_encode(const const_array_view<uint8_t> input, array_view<char> output) noexcept;
    error_type base64_decode(const const_array_view<char> input, array_view<uint8_t> output) noexcept;
    error_type base64_decode(const string_view input, array_view<uint8_t> output) noexcept;

    template<typename T> inline error_type base64_encode(const T& input, array_view<char> output)
    {
        static_assert(std::is_trivially_destructible<T>::value, "INVALID TYPE");
        union
        {
            const uint8_t* as_bytes;
            const T* as_type;
        };
        as_type = &input;
        return base64_encode({ as_bytes, sizeof(input) }, output);
    }
    template<typename T> inline error_type base64_decode(const string_view input, T& output) noexcept
    {
        static_assert(std::is_trivially_destructible<T>::value, "INVALID TYPE");
        union
        {
            uint8_t* as_bytes;
            T* as_type;
        };
        as_type = &output;
        return base64_decode(input.bytes(), array_view<uint8_t>{ as_bytes, sizeof(T) });
    }

    size_t strcopy(const string_view src, array_view<char> dst) noexcept;
    inline size_t strcopy(const string_view src, array_view<uint8_t> dst) noexcept
    {
        return strcopy(src, array_view<char>(reinterpret_cast<char*>(dst.data()), dst.size()));
    }
    template<size_t N> inline size_t strcopy(const string_view src, char(&dst)[N]) noexcept
    {
        return strcopy(src, array_view<char>(dst));
    }
    template<size_t N> inline size_t strcopy(const string_view src, uint8_t(&dst)[N]) noexcept
    {
        return strcopy(src, array_view<uint8_t>(dst));
    }
    
    error_type to_value(const string_view str, uint64_t& value) noexcept;
    error_type to_value(const string_view str, int64_t& value) noexcept;
    template<typename T>
    error_type to_value_int(const string_view str, T& value) noexcept
    {
        auto integer = 0i64;
        ICY_ERROR(to_value(str, integer));
        if (integer < std::numeric_limits<T>::min() || integer > std::numeric_limits<T>::max())
            return make_stdlib_error(std::errc::illegal_byte_sequence);
        value = T(integer);
        return error_type();
    }
    template<typename T>
    error_type to_value_uint(const string_view str, T& value) noexcept
    {
        auto integer = 0ui64;
        ICY_ERROR(to_value(str, integer));
        if (integer > std::numeric_limits<T>::max())
            return make_stdlib_error(std::errc::illegal_byte_sequence);
        value = T(integer);
        return error_type();
    }
    error_type to_value(const string_view str, float& value) noexcept;
    error_type to_value(const string_view str, double& value) noexcept;
    error_type to_value(const string_view str, bool& value) noexcept;
    inline error_type to_value(const string_view str, int32_t& value) noexcept
    {
        return to_value_int(str, value);
    }
    inline error_type to_value(const string_view str, int16_t& value) noexcept
    {
        return to_value_int(str, value);
    }
    inline error_type to_value(const string_view str, int8_t& value) noexcept
    {
        return to_value_int(str, value);
    }
    inline error_type to_value(const string_view str, uint32_t& value) noexcept
    {
        return to_value_uint(str, value);
    }
    inline error_type to_value(const string_view str, uint16_t& value) noexcept
    {
        return to_value_uint(str, value);
    }
    inline error_type to_value(const string_view str, uint8_t& value) noexcept
    {
        return to_value_uint(str, value);
    }
    error_type to_value(const string_view str, std::chrono::system_clock::time_point& time, const bool local = true) noexcept;
    error_type to_value(const string_view str, clock_type::time_point& time, const bool local = true) noexcept;
    error_type to_value(const string_view str, guid& guid) noexcept;

}
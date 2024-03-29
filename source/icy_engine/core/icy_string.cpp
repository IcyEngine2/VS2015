#include <icy_engine/core/icy_string.hpp>
#include <icy_engine/core/icy_array.hpp>
#include <guiddef.h>
#include <Windows.h>
#define snscanf _snscanf_s

#pragma warning(disable:4774)

using namespace icy;

static constexpr auto guid_string_length = sizeof("12345678-1234-1234-1234-1234567890AB");
static constexpr auto guid_string_format = "%08lX-%04hX-%04hX-%02hhX%02hhX-%02hhX%02hhX%02hhX%02hhX%02hhX%02hhX";
static constexpr auto time_string_format = "%d-%d-%d %d:%d:%d";

string_iterator& string_iterator::operator++() noexcept
{
    ICY_ASSERT(m_ptr < m_end, "INVALID ITERATOR INCREMENT: PAST END OF STRING");
    auto chr = *m_ptr;
    auto offset = 1;
    if (chr & mask_0)
    {
        ++offset;
        if (chr & mask_2)
            offset += (chr & mask_3) ? 2 : 1;
    }
    m_ptr += offset;
    return *this;
}
string_iterator& string_iterator::operator--() noexcept
{
    ICY_ASSERT(m_ptr > m_begin, "INVALID ITERATOR DECREMENT: BEFORE STRING START");
    --m_ptr;
    if (*m_ptr & mask_0)
    {
        --m_ptr;
        if ((*m_ptr & mask_1) == 0)
        {
            --m_ptr;
            if ((*m_ptr & mask_2) == 0)
                --m_ptr;
        }
    }
    return *this;
}
size_t string_iterator::to_utf16(wchar_t(&wchr)[2]) const noexcept
{
    wchr[0] = wchr[1] = 0;

    const auto capacity = detail::distance(m_ptr, m_end);
    if (capacity == 0)
        return 0;

    if (*m_ptr <= 0x80)
    {
        wchr[0] = *m_ptr;
        return 1;
    }
    else if (*m_ptr <= 0xC0)
    {
        // reserved for utf8?
    }
    else if (*m_ptr <= 0xE0)
    {
        if (capacity != 2)
            return 0;

        auto hsur = char16_t(m_ptr[0] & 0x1F);
        auto lsur = char16_t(m_ptr[1] & 0x3F);
        const auto unicode = (hsur << 8) | lsur;
        if ((0 <= unicode && unicode <= 0xD7FF) || (0xE000 <= unicode && unicode <= 0xFFFF))
        {
            wchr[0] = unicode;
            return 1;
        }
    }
    else if (*m_ptr <= 0xF0)
    {
        if (capacity != 3)
            return 0;

        const auto chr4 = char16_t(m_ptr[0] & 0xF);
        const auto chr3 = char16_t((m_ptr[1] & 0x3C) >> 2);
        const auto chr2_1 = char16_t((m_ptr[1] & 0x3));
        const auto chr2_0 = char16_t((m_ptr[2] & 0x30) >> 4);
        const auto chr1 = char16_t((m_ptr[2] & 0xF));

        const char32_t unicode = 0
            | (chr4 << 12) 
            | (chr3 << 8)
            | (chr2_1 << 6) 
            | (chr2_0 << 4)
            | (chr1);

        if ((0 <= unicode && unicode <= 0xD7FF) || (0xE000 <= unicode && unicode <= 0xFFFF))
        {
            wchr[0] = wchar_t(unicode);
            return 1;
        }
    }
    else if (*m_ptr < 0xF8)
    {
        if (capacity != 3)
            return 0;

        const auto chr_6 = char16_t((m_ptr[0] & 0x4) >> 2);
        const auto chr_5_1 = char16_t(m_ptr[0] & 0x3);
        const auto chr_5_0 = char16_t((m_ptr[1] & 0x30) >> 4);
        const auto chr_4 = char16_t(m_ptr[1] & 0xF);
        const auto chr_3 = char16_t((m_ptr[2] & 0x3C) >> 2);
        const auto chr_2_1 = char16_t(m_ptr[2] & 0x3);
        const auto chr_2_0 = char16_t((m_ptr[3] & 0x30) >> 4);
        const auto chr_1 = char16_t(m_ptr[3] & 0xF);
        
        const char32_t unicode = 0
            | (chr_6 << 4) 
            | (chr_5_1 << 2) 
            | (chr_5_0)
            | (chr_4 << 12)
            | (chr_3 << 8) 
            | (chr_2_1 << 6) 
            | (chr_2_0 << 4) 
            | chr_1;

        const auto hsur = (unicode - 0x10000) / 0x400 + 0xD800;
        const auto lsur = (unicode - 0x10000) % 0x400 + 0xDC00;
        wchr[0] = wchar_t(hsur);
        wchr[1] = wchar_t(lsur);
        return 2;
    }
    return 0;
}
error_type string_iterator::to_char(char32_t& chr) const noexcept
{
    const auto capacity = detail::distance(m_ptr, m_end);
    if (capacity == 0)
        return make_stdlib_error(std::errc::invalid_argument);

    const auto c1 = static_cast<unsigned char>(m_ptr[0]);
    if (c1 < 0x80)
    {
        chr = c1;
    }
    else if (c1 < 0xC2) // continuation or overlong 2-byte sequence
    {
        return make_stdlib_error(std::errc::illegal_byte_sequence);
    }
    else if (c1 < 0xE0) // 2-byte sequence
    {
        if (capacity < 2)
            return make_stdlib_error(std::errc::illegal_byte_sequence);

        const auto c2 = static_cast<unsigned char>(m_ptr[1]);
        if ((c2 & 0xC0) != 0x80)
            return make_stdlib_error(std::errc::illegal_byte_sequence);

        chr = (c1 << 6) + c2 - 0x3080;
    }
    else if (c1 < 0xF0) // 3-byte sequence
    {
        if (capacity < 3)
            return make_stdlib_error(std::errc::illegal_byte_sequence);

        const auto c2 = static_cast<unsigned char>(m_ptr[1]);
        if ((c2 & 0xC0) != 0x80)
            return make_stdlib_error(std::errc::illegal_byte_sequence);
        if (c1 == 0xE0 && c2 < 0xA0) // overlong
            return make_stdlib_error(std::errc::illegal_byte_sequence);

        const auto c3 = static_cast<unsigned char>(m_ptr[2]);
        if ((c3 & 0xC0) != 0x80)
            return make_stdlib_error(std::errc::illegal_byte_sequence);

        chr = (c1 << 12) + (c2 << 6) + c3 - 0xE2080;
    }
    else if (c1 < 0xF5) // 4-byte sequence
    {
        if (capacity < 4)
            return make_stdlib_error(std::errc::illegal_byte_sequence);

        const auto c2 = static_cast<unsigned char>(m_ptr[1]);
        if ((c2 & 0xC0) != 0x80)
            return make_stdlib_error(std::errc::illegal_byte_sequence);
        if (c1 == 0xF0 && c2 < 0x90) // overlong
            return make_stdlib_error(std::errc::illegal_byte_sequence);
        if (c1 == 0xF4 && c2 >= 0x90) // > U+10FFFF
            return make_stdlib_error(std::errc::illegal_byte_sequence);

        const auto c3 = static_cast<unsigned char>(m_ptr[2]);
        if ((c3 & 0xC0) != 0x80)
            return make_stdlib_error(std::errc::illegal_byte_sequence);

        const auto c4 = static_cast<unsigned char>(m_ptr[3]);
        if ((c4 & 0xC0) != 0x80)
            return make_stdlib_error(std::errc::illegal_byte_sequence);

        chr = (c1 << 18) + (c2 << 12) + (c3 << 6) + c4 - 0x3C82080;
    }
    else // > U+10FFFF
    {
        return make_stdlib_error(std::errc::illegal_byte_sequence);
    }
    return error_type();
}
string_view::const_iterator string_view::find(const string_view& rhs) const noexcept
{
    return const_iterator{ m_ptr, m_ptr + m_size,
        std::search(m_ptr, m_ptr + m_size, rhs.m_ptr, rhs.m_ptr + rhs.m_size) };
}
string_view::const_iterator string_view::rfind(const string_view& rhs) const noexcept
{
    auto it = std::search(
        std::make_reverse_iterator(m_ptr + m_size),
        std::make_reverse_iterator(m_ptr),
        std::make_reverse_iterator(rhs.m_ptr + rhs.m_size),
        std::make_reverse_iterator(rhs.m_ptr));
    if (it == std::make_reverse_iterator(rhs.m_ptr))
        return end();
    std::advance(it, rhs.m_size);
    return const_iterator{ m_ptr, m_ptr + m_size, it.base() };
}
error_type string_view::to_utf16(wchar_t* const data, size_t* const size) const noexcept
{
    if (!size)
        return make_stdlib_error(std::errc::invalid_argument);

    const auto length = MultiByteToWideChar(CP_UTF8, 0, m_ptr, static_cast<int>(m_size), *size ? data : nullptr, static_cast<int>(*size));
    if (length < 0)
        return last_system_error();

    *size = size_t(length);
    return error_type();
}
error_type icy::to_value(const string_view str, float& value) noexcept
{
    return str.bytes().size() && snscanf(str.bytes().data(), str.bytes().size(), "%f", &value) == 1 ? error_type{} : make_stdlib_error(std::errc::illegal_byte_sequence);
}
error_type icy::to_value(const string_view str, double& value) noexcept
{
    return str.bytes().size() && snscanf(str.bytes().data(), str.bytes().size(), "%lf", &value) == 1 ? error_type{} : make_stdlib_error(std::errc::illegal_byte_sequence);
}
error_type icy::to_value(const string_view str, bool& value)  noexcept
{
    if (str == "true"_s || str == "1"_s)
        value = true;
    else if (str == "false"_s || str == "0"_s)
        value = false;
    else
        return make_stdlib_error(std::errc::illegal_byte_sequence);
    return error_type();
}
error_type icy::to_value(const string_view str, int64_t& value) noexcept
{
    return str.bytes().size() && snscanf(str.bytes().data(), str.bytes().size(), "%lli", &value) == 1 ? error_type{} : make_stdlib_error(std::errc::illegal_byte_sequence);
}
error_type icy::to_value(const string_view str, uint64_t& value) noexcept
{
    return str.bytes().size() && snscanf(str.bytes().data(), str.bytes().size(), "%llu", &value) == 1 ? error_type{} : make_stdlib_error(std::errc::illegal_byte_sequence);
}
error_type icy::to_value(const string_view str, std::chrono::system_clock::time_point& time, const bool local) noexcept
{
    ::tm std_time = {};
    const auto count = str.bytes().size() ? snscanf(str.bytes().data(), str.bytes().size(), time_string_format,
        &std_time.tm_year, &std_time.tm_mon, &std_time.tm_mday,
        &std_time.tm_hour, &std_time.tm_min, &std_time.tm_sec) : 0;
    if (count != 6)
        return make_stdlib_error(std::errc::illegal_byte_sequence);

    std_time.tm_year -= 1900;
    std_time.tm_mon -= 1;
    const auto t = local ? mktime(&std_time) : _mkgmtime(&std_time);
    if (t == -1)
        return make_stdlib_error(std::errc::illegal_byte_sequence);

    time = std::chrono::system_clock::from_time_t(t);
    return error_type();
}
error_type icy::to_value(const string_view str, clock_type::time_point& time, const bool local)  noexcept
{
    std::chrono::system_clock::time_point sys_time = {};
    ICY_ERROR(to_value(str, sys_time, local));

    const auto now_system = std::chrono::system_clock::now();
    const auto now_steady = std::chrono::steady_clock::now();
    const auto delta = std::chrono::duration_cast<std::chrono::system_clock::duration>(now_system - sys_time);
    time = now_steady - delta;
    return error_type();
}
error_type icy::to_value(const string_view str, guid& guid) noexcept
{
    auto& rguid = reinterpret_cast<GUID&>(guid);
    if (str.bytes().size() + sizeof("") < guid_string_length)
        return make_stdlib_error(std::errc::illegal_byte_sequence);

    const auto count = str.bytes().size() ? snscanf(str.bytes().data(), str.bytes().size(), guid_string_format,
        &rguid.Data1, &rguid.Data2, &rguid.Data3,
        &rguid.Data4[0], &rguid.Data4[1], &rguid.Data4[2], &rguid.Data4[3],
        &rguid.Data4[4], &rguid.Data4[5], &rguid.Data4[6], &rguid.Data4[7]) : 0;
    if (count != 11)
        return make_stdlib_error(std::errc::illegal_byte_sequence);
    return error_type();
}

string::string(string&& rhs) noexcept : m_realloc(rhs.m_realloc), m_user(rhs.m_user)
{
	if (rhs.is_dynamic())
	{
		m_ptr = rhs.m_ptr;
	}
	else
	{
		m_ptr = m_buffer;
		memcpy(m_buffer, rhs.m_buffer, sizeof(m_buffer));
	}
	m_size = rhs.m_size;
	new (&rhs) type;
}
string::~string() noexcept
{
	if (is_dynamic())
		m_realloc(m_ptr, 0, m_user);
}
error_type icy::to_string(const_array_view<wchar_t> src, string& str) noexcept
{
	while (!src.empty() && src.back() == L'\0')
		src = { src.data(), src.size() - 1 };

	if (src.empty())
	{
		str.clear();
		return error_type();
	}

	array<wchar_t> dst;
	constexpr auto type = NormalizationC;
	if (!IsNormalizedString(type, src.data(), static_cast<int>(src.size())))
	{
		auto try_normalize = [&](int& size, bool& try_again)
		{
			size = NormalizeString(type,
				src.data(), static_cast<int>(src.size()),
				dst.data(), static_cast<int>(dst.size()));
            if (size <= 0)
            {
                if (GetLastError() == ERROR_INSUFFICIENT_BUFFER)
                {
                    size = -size;
                    try_again = true;
                }
                else
                    return last_system_error();
            }
            return error_type();
		};
		auto size = 0;
        auto try_again = true;
        ICY_ERROR(try_normalize(size, try_again));
		ICY_ERROR(dst.resize(size_t(size)));
		while (try_again)
		{
            try_again = false;
            ICY_ERROR(try_normalize(size, try_again));
            ICY_ERROR(dst.resize(size_t(size)));
		}
		src = dst;
	}
	const auto size = WideCharToMultiByte(CP_UTF8, 0, src.data(), int(src.size()), nullptr, 0, nullptr, nullptr);
	if (size < 0)
		return last_system_error();

	ICY_ERROR(str.resize(size_t(size)));
	WideCharToMultiByte(CP_UTF8, 0, src.data(), int(src.size()), const_cast<char*>(str.bytes().data()), size, nullptr, nullptr);
	return error_type();
}
error_type icy::to_string(int64_t value, string& str) noexcept
{
	constexpr auto max_length = ("DFKU"[detail::constexpr_log2(sizeof(int64_t))] - 'A');
	char buffer[max_length + sizeof("+")];
	auto length = 0u;

	ICY_ERROR(str.reserve(sizeof(buffer)));

	const bool is_min = value == std::numeric_limits<int64_t>::min();
	if (is_min) ++value;

	const auto sign = value < 0 ? (value = -value, true) : false;
	
	buffer[length++] = '\0';
	do
	{
		buffer[length++] = "0123456789"[value % 10];
		value /= 10;
	} while (value);
	if (sign) 
		buffer[length++] = '-';

	char rbuffer[sizeof(buffer)];
	std::reverse_copy(buffer, buffer + length, rbuffer);
	if (is_min) 
		rbuffer[length - 2] = '8';
	
	memcpy(str.m_ptr, rbuffer, str.m_size = length - 1);
	return error_type();
}
error_type icy::to_string(const guid& value, string& str) noexcept
{
    auto& rguid = reinterpret_cast<const GUID&>(value);
	char buffer[guid_string_length];
	const auto length = sprintf_s(buffer, guid_string_format,
		rguid.Data1, rguid.Data2, rguid.Data3,
        rguid.Data4[0], rguid.Data4[1], rguid.Data4[2], rguid.Data4[3],
        rguid.Data4[4], rguid.Data4[5], rguid.Data4[6], rguid.Data4[7]);
    ICY_ERROR(to_string(const_array_view<char>(buffer, length), str));
    return error_type();
}
error_type icy::to_string(const double value, const float_type type, size_t precision, string& str) noexcept
{
	if (isnan(value) || isinf(value))
		return make_stdlib_error(std::errc::invalid_argument);

	char precision_fmt[3]{};
	precision = DBL_DIG < precision ? DBL_DIG : precision;
	auto digits = size_t(sprintf_s(precision_fmt, "%zu", precision));

	char format[sizeof("%.##e")]{};
	auto length = 0_z;
	format[length++] = '%';
	format[length++] = '.';
	std::memcpy(format + length, precision_fmt, digits);
	length += digits;
	if (type == float_type_exp)
		format[length++] = 'e';
	else //if (type == float_type_fixed)
		format[length++] = 'f';

	char buf[512] = {};
	digits = size_t(snprintf(nullptr, 0, format, value));
	snprintf(buf, digits + 2, format, value);
    string_view tmp;
    ICY_ERROR(to_string(const_array_view<char>(buf, digits), tmp));
    ICY_ERROR(copy(tmp, str));
    return error_type();
}
error_type icy::to_string(const std::chrono::steady_clock::time_point time, string& str, const bool local) noexcept
{   
    const auto now_system = std::chrono::system_clock::now();
    const auto now_steady = std::chrono::steady_clock::now();
    const auto delta = std::chrono::duration_cast<std::chrono::system_clock::duration>(now_steady - time);
    return to_string(std::chrono::system_clock::time_point(now_system - delta), str, local);
}
error_type icy::to_string(const std::chrono::system_clock::time_point time, string& str, const bool local) noexcept
{
    const auto time_t = std::chrono::system_clock::to_time_t(time);
    ::tm tm = {};
    if (local)
    {
        ICY_ERROR(make_stdlib_error(static_cast<std::errc>(localtime_s(&tm, &time_t))));
    }
    else
    {
        ICY_ERROR(make_stdlib_error(static_cast<std::errc>(gmtime_s(&tm, &time_t))));
    }
    char buffer[64];
    const auto length = strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", &tm);
    string_view tmp;
    ICY_ERROR(to_string(const_array_view<char>(buffer, length), tmp));
    ICY_ERROR(copy(tmp, str));
    return error_type();
}

error_type string::reserve(const size_type capacity) noexcept
{
	if (capacity > this->capacity())
	{
		const auto new_capacity = align_size(capacity);
		char* new_ptr = nullptr;
		if (is_dynamic())
		{
			new_ptr = static_cast<char*>(m_realloc(m_ptr, new_capacity, m_user));			
		}
		else
		{
			new_ptr = static_cast<char*>(m_realloc(nullptr, new_capacity, m_user));
			if (new_ptr)
				memcpy(new_ptr, m_ptr, m_size + 1);
		}
		if (!new_ptr)
			return make_stdlib_error(std::errc::not_enough_memory);

        m_capacity = new_capacity;
		m_ptr = new_ptr;
	}
	return error_type();
}
error_type string::resize(const size_type size, const char symb) noexcept
{
	if (symb < 0)
		return make_stdlib_error(std::errc::invalid_argument);

	ICY_ERROR(reserve(size + 1));
	memset(m_ptr, symb, size);
	m_ptr[m_size = size] = '\0';
	return error_type();
}
error_type icy::split(const string_view str, array<string_view>& substr) noexcept
{
    substr.clear();
    auto beg = str.end();
    for (auto it = str.begin(); it != str.end(); ++it)
    {
        char32_t chr = 0;
        ICY_ERROR(it.to_char(chr));
        const auto is_space = chr > 0 && isspace(chr);
        if (beg == str.end()) //  searching for first word (letter)
        {
            if (is_space)
                continue;
            else
                beg = it;
        }
        else // searching for last letter (before space)
        {
            if (is_space)
            {
                ICY_ERROR(substr.push_back(string_view(beg, it)));
                beg = str.end();
            }
        }
    }
    if (beg != str.end())
        ICY_ERROR(substr.push_back(string_view(beg, str.end())));
    return error_type();
}
error_type icy::split(const string_view str, array<string_view>& substr, const string_view delims) noexcept
{
    substr.clear();
    auto beg = str.end();
    for (auto it = str.begin(); it != str.end(); ++it)
    {
        char32_t chr = 0;
        ICY_ERROR(it.to_char(chr));

        auto is_delim = false;
        for (auto jt = delims.begin(); jt != delims.end(); ++jt)
        {
            char32_t delim = 0;
            ICY_ERROR(jt.to_char(delim));
            if (chr == delim)
            {
                is_delim = true;
                break;
            }
        }
        if (beg == str.end()) //  searching for first word (letter)
        {
            if (is_delim)
                continue;
            else
                beg = it;
        }
        else // searching for last letter (before space)
        {
            if (is_delim)
            {
                ICY_ERROR(substr.push_back(string_view(beg, it)));
                beg = str.end();
            }
        }
    }
    if (beg != str.end())
        ICY_ERROR(substr.push_back(string_view(beg, str.end())));
    return error_type();
}
error_type string::append(const string_view rhs) noexcept
{
	const auto new_size = m_size + rhs.bytes().size();
	if (new_size >= capacity())
	{
		string str;
		ICY_ERROR(str.reserve(new_size + 1));
		memcpy(str.m_ptr, m_ptr, m_size);
		memcpy(str.m_ptr + m_size, rhs.m_ptr, rhs.m_size);
		str.m_ptr[str.m_size = new_size] = '\0';
		*this = std::move(str);
	}
	else
	{
		memcpy(m_ptr + m_size, rhs.m_ptr, rhs.m_size);
		m_ptr[m_size += rhs.m_size] = '\0';
	}
	return error_type();
}
error_type string::insert(const iterator where, const string_view rhs) noexcept
{
	string str;
	ICY_ERROR(str.reserve(capacity()));
	ICY_ERROR(str.append(string_view(begin(), where)));
	ICY_ERROR(str.append(rhs));
	ICY_ERROR(str.append(string_view(where, end())));
	//const auto index = where.m_ptr - m_ptr;
	*this = std::move(str);
	return error_type();
}
void string::pop_back(const size_type count) noexcept
{
	auto it = end();
	for (auto k = 0_z; k < count; ++k)
		--it;
	m_ptr[m_size = static_cast<size_type>(it.m_ptr - m_ptr)] = '\0';
}
error_type string::replace(const string_view find, const string_view replace) noexcept
{
	struct node
	{
		static node* create() noexcept
		{
			return static_cast<node*>(icy::realloc(nullptr, sizeof(node)));
		}
		node* prev = nullptr;
		difference_type offset = 0;
	};
	node* list = nullptr;
	ICY_SCOPE_EXIT
	{
		auto prev = list;
		while (prev)
		{
			const auto ptr = prev->prev;
            icy::realloc(prev, 0);
			prev = ptr;
		}
	};
	auto offset = begin();
	auto count = 0u;
	while (true)
	{
		const auto it = string_view(offset, end()).find(find);
		if (it != end())
		{
			auto next = node::create();
			if (!next)
				return make_stdlib_error(std::errc::not_enough_memory);
			next->prev = list;
			next->offset = it.m_ptr - m_ptr;
			list = next;
			offset.m_ptr = const_cast<char*>(it.m_ptr + find.m_size);
			++count;
		}
		else break;
	}
	if (list)
	{
		string str;
		ICY_ERROR(str.reserve(m_size - count * find.m_size + count * replace.m_size));

		auto vec = static_cast<difference_type*>(icy::realloc(nullptr, sizeof(difference_type) * count));
		if (!vec)
			return make_stdlib_error(std::errc::not_enough_memory);

        ICY_SCOPE_EXIT{ icy::realloc(vec, 0); };
		auto prev = list;
		auto k = count;
		while (prev)
		{
			vec[--k] = prev->offset;
			prev = prev->prev;
		}

		offset = begin();
		for (k = 0; k < count; ++k)
		{
            auto tmp = begin();
            tmp.m_ptr += vec[k];
			str.append(string_view(offset, tmp));
			offset.m_ptr = m_ptr + vec[k] + find.m_size;
			str.append(replace);
		}
		str.append(string_view(offset, end()));
		*this = std::move(str);
	}
	return error_type();
}
/*error_type icy::emplace(map<string, string>& map, const string_view key, const string_view val) noexcept
{
	string new_key;
	string new_val;
	ICY_ERROR(to_string(key, new_key));
	ICY_ERROR(to_string(val, new_val));
	ICY_ERROR(map.find_or_insert(std::move(new_key), std::move(new_val)));
	return error_type();
}
*/
error_type icy::to_utf16(const string_view value, array<wchar_t>& wide) noexcept
{
	auto size = 0_z;
	ICY_ERROR(value.to_utf16(nullptr, &size));
	ICY_ERROR(wide.resize(size + 1));
	ICY_ERROR(value.to_utf16(wide.data(), &size));
	return error_type();
}
uint32_t icy::hash(const string_view string) noexcept
{
    auto value = 0x811C9DC5;//14695981039346656037ui64;
    for (auto&& chr : string.bytes())
    {
        value = uint32_t((value ^ uint64_t(chr)) * 0x01000193ui64);
    }
    return value;
}
uint64_t icy::hash64(const void* const data, const size_t size) noexcept
{
    auto value = 14695981039346656037ui64;
    for (auto k = 0u; k < size; ++k)
    {
        value = uint64_t((value ^ uint64_t(static_cast<const uint8_t*>(data)[k])) * 1099511628211ui64);
    }
    return value;
}
error_type icy::base64_encode(const const_array_view<uint8_t> input, array_view<char> output) noexcept
{
    if (output.size() != base64_encode_size(input.size()))
        return make_stdlib_error(std::errc::message_size);

    auto t0 = +0;
    auto t1 = -6;
    auto k = 0_z;
    for (auto&& c : input)
    {
        t0 = (t0 << 8) + c;
        t1 += 8;
        while (t1 >= 0)
        {
            output[k++] = base64_alpha[(t0 >> t1) & 0x3F];
            t1 -= 6;
        }
    }
    if (t1 > -6)
        output[k++] = base64_alpha[((t0 << 8) >> (t1 + 8)) & 0x3F];

    while (k % 4)
        output[k++] = '=';

    return error_type();
}
error_type icy::base64_decode(const const_array_view<char> input, array_view<uint8_t> output) noexcept
{
    if (input.size() != round_up(output.size(), 3) / 3 * 4)
        return make_stdlib_error(std::errc::message_size);

    char array[256];
    std::fill_n(array, sizeof(array), -1);
    for (auto k = '\0'; k < 64; ++k)
        array[base64_alpha[k]] = k;

    auto t0 = +0;
    auto t1 = -8;
    auto k = 0_z;
    for (auto&& c : input)
    {
        if (array[c] == -1)
        {
            if (c != '=')
                return make_stdlib_error(std::errc::illegal_byte_sequence);
            return error_type();
        }
        t0 = (t0 << 6) + array[c];
        t1 += 6;
        if (t1 >= 0)
        {
            output[k++] = static_cast<uint8_t>((t0 >> t1) & 0xFF);
            t1 -= 8;
        }
    }
    return error_type();
}
error_type icy::base64_decode(const string_view input, array_view<uint8_t> output) noexcept
{
    return base64_decode(input.bytes(), output);
}
size_t icy::strcopy(const string_view src, array_view<char> dst) noexcept
{
    auto it = src.begin();
    while (it != src.end())
    {
        auto next = it++;
        if (detail::distance(src.bytes().data(), static_cast<const char*>(next)) >= dst.size())
            break;
    }
    const auto length = detail::distance(src.bytes().data(), static_cast<const char*>(it));
    memcpy(dst.data(), src.bytes().data(), length);
    return length;
}

error_type icy::to_lower(const string_view str, string& output) noexcept
{
    auto lib = "user32"_lib;
    ICY_ERROR(lib.initialize());
    if (const auto func = ICY_FIND_FUNC(lib, CharLowerBuffW))
    {
        array<wchar_t> wide;
        ICY_ERROR(to_utf16(str, wide));
        const auto length = func(wide.data(), uint32_t(wide.size()));
        return to_string(const_array_view<wchar_t>(wide.data(), length), output);
    }
    return make_stdlib_error(std::errc::function_not_supported);
}
error_type icy::to_upper(const string_view str, string& output) noexcept
{
    auto lib = "user32"_lib;
    ICY_ERROR(lib.initialize());
    if (const auto func = ICY_FIND_FUNC(lib, CharUpperBuffW))
    {
        array<wchar_t> wide;
        ICY_ERROR(to_utf16(str, wide));
        const auto length = func(wide.data(), uint32_t(wide.size()));
        return to_string(const_array_view<wchar_t>(wide.data(), length), output);
    }
    return make_stdlib_error(std::errc::function_not_supported);
}
error_type icy::copy(const string_view src, string& dst) noexcept
{
    string tmp;
    ICY_ERROR(tmp.insert(tmp.begin(), src));
    dst = std::move(tmp);
    return error_type();
}
error_type icy::copy(const string& src, string& dst) noexcept
{
    return copy(string_view(src), dst);
}

error_type icy::locale::enumerate(array<locale>& vec) noexcept
{
    struct data_type
    {
        array<locale>* vec = nullptr;
        error_type error;
    };
    data_type data;
    data.vec = &vec;
    const auto proc = [](wchar_t* str, DWORD, LPARAM ptr) -> BOOL
    {
        if (!str || *str == L'\0' || wcsstr(str, L"-") == nullptr)
            return TRUE;

        locale new_locale;
        const auto data = reinterpret_cast<data_type*>(ptr);
        const auto func = [str, data](const LCTYPE type, string& ref)
        {
            wchar_t buf[256] = {};
            auto len = GetLocaleInfoEx(str, type, buf, _countof(buf));
            if (len <= 0)
                return last_system_error();
            ICY_ERROR(to_string(const_array_view<wchar_t>(buf, len), ref));
            return error_type();
        };
#define LOCALE_ERROR(X) if (data->error = (X)) return FALSE;
        new_locale.code = LocaleNameToLCID(str, LOCALE_ALLOW_NEUTRAL_NAMES);
        if (new_locale.code == 0x1000) return TRUE;
        if (new_locale.code == 0)
        {
            data->error == last_system_error();
            return FALSE;
        }

        LOCALE_ERROR(to_string(const_array_view<wchar_t>(str, wcslen(str)), new_locale.short_name));
        LOCALE_ERROR(func(LOCALE_SLOCALIZEDLANGUAGENAME, new_locale.display_lang));
        LOCALE_ERROR(func(LOCALE_SENGLISHLANGUAGENAME, new_locale.global_lang));
        LOCALE_ERROR(func(LOCALE_SNATIVELANGUAGENAME, new_locale.local_lang));
        LOCALE_ERROR(func(LOCALE_SLOCALIZEDCOUNTRYNAME, new_locale.display_ctry));
        LOCALE_ERROR(func(LOCALE_SENGLISHCOUNTRYNAME, new_locale.global_ctry));
        LOCALE_ERROR(func(LOCALE_SNATIVECOUNTRYNAME, new_locale.local_ctry));
        LOCALE_ERROR(data->vec->push_back(std::move(new_locale)));
#undef LOCALE_ERROR
        return TRUE;
    };
    auto success = EnumSystemLocalesEx(proc, LOCALE_ALL, reinterpret_cast<LPARAM>(&data), nullptr);
    ICY_ERROR(data.error);
    if (!success)
        return last_system_error();
    std::sort(vec.begin(), vec.end(), [](const locale& lhs, const locale& rhs) { return lhs.short_name < rhs.short_name; });
    
    const auto it = std::unique(vec.begin(), vec.end(),
        [](const locale& lhs, const locale& rhs) { return lhs.short_name == rhs.short_name; });
    vec.pop_back(std::distance(it, vec.end()));
    
    const auto jt = std::unique(vec.begin(), vec.end(), 
        [](const locale& lhs, const locale& rhs) { return lhs.global_ctry == rhs.global_ctry && lhs.global_lang == rhs.global_lang; });
    vec.pop_back(std::distance(jt, vec.end()));

    return error_type();
}


error_type icy::to_string(const const_array_view<char> input, string& output) noexcept
{
    string_view tmp;
    ICY_ERROR(to_string(input, tmp));
    ICY_ERROR(copy(tmp, output));
    return error_type();
}
error_type icy::to_string(const const_array_view<uint8_t> input, string& output) noexcept
{
    string_view tmp;
    ICY_ERROR(to_string(input, tmp));
    ICY_ERROR(copy(tmp, output));
    return error_type();
}
error_type icy::to_string(const const_array_view<uint8_t> input, string_view& output) noexcept
{
    auto i = 0_z;
    const auto len = input.size();
    while (i < len)
    {
        if (input[i] == 0)
        {
            break;
        }
        else if (input[i] <= 0x7F) /* 00..7F */
        {
            i += 1;
        }
        else if (input[i] >= 0xC2 && input[i] <= 0xDF) /* C2..DF 80..BF */
        {
            if (i + 1 < len) /* Expect a 2nd byte */
            {
                if (input[i + 1] < 0x80 || input[i + 1] > 0xBF)
                {
                    // "After a first byte between C2 and DF, expecting a 2nd byte between 80 and BF";
                    break;
                }
            }
            else
            {
                //  "After a first byte between C2 and DF, expecting a 2nd byte.";
                break;
            }
            i += 2;
        }
        else if (input[i] == 0xE0) /* E0 A0..BF 80..BF */
        {
            if (i + 2 < len) /* Expect a 2nd and 3rd byte */
            {
                if (input[i + 1] < 0xA0 || input[i + 1] > 0xBF)
                {
                    // "After a first byte of E0, expecting a 2nd byte between A0 and BF.";
                    break;
                }
                if (input[i + 2] < 0x80 || input[i + 2] > 0xBF)
                {
                    //"After a first byte of E0, expecting a 3nd byte between 80 and BF.";
                    break;
                }
            }
            else
            {
                //"After a first byte of E0, expecting two following bytes.";
                break;
            }
            i += 3;
        }
        else if (input[i] >= 0xE1 && input[i] <= 0xEC) /* E1..EC 80..BF 80..BF */
        {
            if (i + 2 < len) /* Expect a 2nd and 3rd byte */
            {
                if (input[i + 1] < 0x80 || input[i + 1] > 0xBF)
                {
                    // "After a first byte between E1 and EC, expecting the 2nd byte between 80 and BF.";
                    break;
                }
                if (input[i + 2] < 0x80 || input[i + 2] > 0xBF)
                {
                    // "After a first byte between E1 and EC, expecting the 3rd byte between 80 and BF.";
                    break;
                }
            }
            else
            {
                // "After a first byte between E1 and EC, expecting two following bytes.";
                break;
            }
            i += 3;
        }
        else if (input[i] == 0xED) /* ED 80..9F 80..BF */
        {
            if (i + 2 < len) /* Expect a 2nd and 3rd byte */
            {
                if (input[i + 1] < 0x80 || input[i + 1] > 0x9F)
                {
                    // "After a first byte of ED, expecting 2nd byte between 80 and 9F.";
                    break;
                }
                if (input[i + 2] < 0x80 || input[i + 2] > 0xBF)
                {
                    // "After a first byte of ED, expecting 3rd byte between 80 and BF.";
                    break;
                }
            }
            else
            {
                // "After a first byte of ED, expecting two following bytes.";
                break;
            }
            i += 3;
        }
        else if (input[i] >= 0xEE && input[i] <= 0xEF) /* EE..EF 80..BF 80..BF */
        {
            if (i + 2 < len) /* Expect a 2nd and 3rd byte */
            {
                if (input[i + 1] < 0x80 || input[i + 1] > 0xBF)
                {
                    // "After a first byte between EE and EF, expecting 2nd byte between 80 and BF.";
                    break;
                }
                if (input[i + 2] < 0x80 || input[i + 2] > 0xBF)
                {
                    // "After a first byte between EE and EF, expecting 3rd byte between 80 and BF.";
                    break;
                }
            }
            else
            {
                // "After a first byte between EE and EF, two following bytes.";
                break;
            }
            i += 3;
        }
        else if (input[i] == 0xF0) /* F0 90..BF 80..BF 80..BF */
        {
            if (i + 3 < len) /* Expect a 2nd, 3rd 3th byte */
            {
                if (input[i + 1] < 0x90 || input[i + 1] > 0xBF)
                {
                    // "After a first byte of F0, expecting 2nd byte between 90 and BF.";
                    break;
                }
                if (input[i + 2] < 0x80 || input[i + 2] > 0xBF)
                {
                    // "After a first byte of F0, expecting 3rd byte between 80 and BF.";
                    break;
                }
                if (input[i + 3] < 0x80 || input[i + 3] > 0xBF)
                {
                    // "After a first byte of F0, expecting 4th byte between 80 and BF.";
                    break;
                }
            }
            else
            {
                // "After a first byte of F0, expecting three following bytes.";
                break;
            }
            i += 4;
        }
        else if (input[i] >= 0xF1 && input[i] <= 0xF3) /* F1..F3 80..BF 80..BF 80..BF */
        {
            if (i + 3 < len) /* Expect a 2nd, 3rd 3th byte */
            {
                if (input[i + 1] < 0x80 || input[i + 1] > 0xBF)
                {
                    // "After a first byte of F1, F2, or F3, expecting a 2nd byte between 80 and BF.";
                    break;
                }
                if (input[i + 2] < 0x80 || input[i + 2] > 0xBF)
                {
                    //*message = "After a first byte of F1, F2, or F3, expecting a 3rd byte between 80 and BF.";
                    break;
                }
                if (input[i + 3] < 0x80 || input[i + 3] > 0xBF)
                {
                    //*message = "After a first byte of F1, F2, or F3, expecting a 4th byte between 80 and BF.";
                    break;
                }
            }
            else
            {
                // "After a first byte of F1, F2, or F3, expecting three following bytes.";
                break;
            }
            i += 4;
        }
        else if (input[i] == 0xF4) /* F4 80..8F 80..BF 80..BF */
        {
            if (i + 3 < len) /* Expect a 2nd, 3rd 3th byte */
            {
                if (input[i + 1] < 0x80 || input[i + 1] > 0x8F)
                {
                    // "After a first byte of F4, expecting 2nd byte between 80 and 8F.";
                    break;
                }
                if (input[i + 2] < 0x80 || input[i + 2] > 0xBF)
                {
                    // "After a first byte of F4, expecting 3rd byte between 80 and BF.";
                    break;
                }
                if (input[i + 3] < 0x80 || input[i + 3] > 0xBF)
                {
                    // "After a first byte of F4, expecting 4th byte between 80 and BF.";
                    break;
                }
            }
            else
            {
                // "After a first byte of F4, expecting three following bytes.";
                break;
            }
            i += 4;
        }
        else
        {
            // "Expecting bytes in the following ranges: 00..7F C2..F4.";
            break;
        }
    }
    if (i != len)
    {
        if (input[i] != 0)
            return make_stdlib_error(std::errc::illegal_byte_sequence);
    }
    output = string_view(reinterpret_cast<const char*>(input.data()), i, string_view::constexpr_tag());
    return error_type();
}

/*error_type icy::jaro_winkler_distance(const string_view utf8lhs, const string_view utf8rhs, double& distance) noexcept
{
    // Exit early if either are empty
    if (utf8lhs.empty() || utf8rhs.empty())
    {
        distance = 0;
        return error_type();
    }

    // Exit early if they're an exact match.
    if (utf8lhs == utf8rhs) 
    {
        distance = 1;
        return error_type();
    }

    array<char32_t> lhs;
    array<char32_t> rhs;
    for (auto&& chr : utf8lhs) ICY_ERROR(lhs.push_back(chr));
    for (auto&& chr : utf8rhs) ICY_ERROR(rhs.push_back(chr));

    const auto lhs_size = static_cast<int>(lhs.size());
    const auto rhs_size = static_cast<int>(rhs.size());
    const auto range = std::max(lhs_size, rhs_size) / 2 - 1;
    
    array<bool> lhs_matches;
    array<bool> rhs_matches;
    ICY_ERROR(lhs_matches.resize(lhs.size()));
    ICY_ERROR(rhs_matches.resize(rhs.size()));

    auto num_match = 0.0;
    auto num_trans = 0.0;

    for (auto i = 0; i < lhs_size; ++i) 
    {
        const auto lower = std::max(i - range, 0);
        const auto upper = std::min(i + range, rhs_size);
        for (auto j = lower; j < upper; ++j) 
        {
            if (!lhs_matches[i] && !rhs_matches[j] && lhs[i] == rhs[j]) 
            {
                num_match += 1;
                lhs_matches[i] = true;
                rhs_matches[j] = true;
                break;
            }
        }
    }

    // Exit early if no matches were found
    if (num_match == 0) 
    {
        distance = 0;
        return error_type();
    }
    
    for (auto i = 0, j = 0; i < lhs_size; ++i)
    {
        if (!lhs_matches[i])
            continue;
        
        for (; j < rhs_size; ++j)
        {
            if (!rhs_matches[j])
                continue;
        }
        if (j >= rhs_size)
            break;

        num_trans += lhs[i] != rhs[j];
    }
    num_trans /= 2;

    distance = 0;
    distance += num_match / lhs_size;
    distance += num_match / rhs_size;
    distance += (num_match - num_trans) / num_match;
    distance /= 3;
    return error_type();
}*/
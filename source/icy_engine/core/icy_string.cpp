#include <icy_engine/core/icy_string.hpp>
#include <icy_engine/core/icy_array.hpp>
#include <icy_engine/core/icy_map.hpp>
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
    return {};
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
    return {};
}
error_type string_view::to_value(float& value) const noexcept
{
    return m_size && snscanf(m_ptr, m_size, "%f", &value) == 1 ? error_type{} : make_stdlib_error(std::errc::illegal_byte_sequence);
}
error_type string_view::to_value(double& value) const noexcept
{
    return m_size && snscanf(m_ptr, m_size, "%lf", &value) == 1 ? error_type{} : make_stdlib_error(std::errc::illegal_byte_sequence);
}
error_type string_view::to_value(bool& value) const noexcept
{
    if (*this == "true"_s || *this == "1"_s)
        value = true;
    else if (*this == "false"_s || *this == "0"_s)
        value = false;
    else
        return make_stdlib_error(std::errc::illegal_byte_sequence);
    return {};
}
error_type string_view::to_value(int64_t& value) const noexcept
{
    return m_size && snscanf(m_ptr, m_size, "%lli", &value) == 1 ? error_type{} : make_stdlib_error(std::errc::illegal_byte_sequence);
}
error_type string_view::to_value(uint64_t& value) const noexcept
{
    return m_size && snscanf(m_ptr, m_size, "%llu", &value) == 1 ? error_type{} : make_stdlib_error(std::errc::illegal_byte_sequence);
}
error_type string_view::to_value(std::chrono::system_clock::time_point& time, const bool local) const noexcept
{
    ::tm std_time = {};
    const auto count = m_size ? snscanf(m_ptr, m_size, time_string_format,
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
    return {};
}
error_type string_view::to_value(clock_type::time_point& time, const bool local) const noexcept
{
    std::chrono::system_clock::time_point sys_time = {};
    ICY_ERROR(to_value(sys_time, local));

    const auto now_system = std::chrono::system_clock::now();
    const auto now_steady = std::chrono::steady_clock::now();
    const auto delta = std::chrono::duration_cast<std::chrono::system_clock::duration>(now_system - sys_time);
    time = now_steady - delta;
    return {};
}
error_type string_view::to_value(guid& guid) const noexcept
{
    auto& rguid = reinterpret_cast<GUID&>(guid);
    if (m_size + sizeof("") < guid_string_length)
        return make_stdlib_error(std::errc::illegal_byte_sequence);

    const auto count = m_size ? snscanf(m_ptr, m_size, guid_string_format,
        &rguid.Data1, &rguid.Data2, &rguid.Data3,
        &rguid.Data4[0], &rguid.Data4[1], &rguid.Data4[2], &rguid.Data4[3],
        &rguid.Data4[4], &rguid.Data4[5], &rguid.Data4[6], &rguid.Data4[7]) : 0;
    if (count != 11)
        return make_stdlib_error(std::errc::illegal_byte_sequence);
    return {};
}

string::string(string&& rhs) noexcept
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
		icy::realloc(m_ptr, 0);
}
error_type icy::to_string(const_array_view<wchar_t> src, string& str) noexcept
{
	while (!src.empty() && src.back() == L'\0')
		src = { src.data(), src.size() - 1 };

	if (src.empty())
	{
		str.clear();
		return {};
	}

	array<wchar_t> dst;
	constexpr auto type = NormalizationD;
	if (!IsNormalizedString(type, src.data(), static_cast<int>(src.size())))
	{
		auto try_normalize = [&](int& size)
		{
			size = NormalizeString(type,
				src.data(), static_cast<int>(src.size()),
				dst.data(), static_cast<int>(dst.size()));
			if (size <= 0 && GetLastError() == ERROR_INSUFFICIENT_BUFFER)
				size = -size;
			return icy::last_system_error();
		};
		auto size = 0;
		ICY_ERROR(try_normalize(size));
		ICY_ERROR(dst.resize(size_t(size)));
		while (true)
		{
			ICY_ERROR(try_normalize(size));
			ICY_ERROR(dst.resize(size_t(size)));
			if (last_system_error() == error_type{})
				break;
		}
		src = dst;
	}
	const auto size = WideCharToMultiByte(CP_UTF8, 0, src.data(), int(src.size()), nullptr, 0, nullptr, nullptr);
	if (size < 0)
		return last_system_error();

	ICY_ERROR(str.resize(size_t(size)));
	WideCharToMultiByte(CP_UTF8, 0, src.data(), int(src.size()), const_cast<char*>(str.bytes().data()), size, nullptr, nullptr);
	return {};
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
	return {};
}
error_type icy::to_string(const guid& value, string& str) noexcept
{
    auto& rguid = reinterpret_cast<const GUID&>(value);
	char buffer[guid_string_length];
	const auto length = sprintf_s(buffer, guid_string_format,
		rguid.Data1, rguid.Data2, rguid.Data3,
        rguid.Data4[0], rguid.Data4[1], rguid.Data4[2], rguid.Data4[3],
        rguid.Data4[4], rguid.Data4[5], rguid.Data4[6], rguid.Data4[7]);
	return to_string(string_view{ buffer, size_t(length) }, str);
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
	return to_string(string_view{ buf, digits }, str);
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
    char buffer[64] = {};
    const auto length = strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", &tm);
    return to_string(string_view(buffer, buffer + length), str);
}

error_type string::reserve(const size_type capacity) noexcept
{
	if (capacity > this->capacity())
	{
		const auto new_capacity = align_size(capacity);
		char* new_ptr = nullptr;
		if (is_dynamic())
		{
			new_ptr = static_cast<char*>(icy::realloc(m_ptr, new_capacity));			
		}
		else
		{
			new_ptr = static_cast<char*>(icy::realloc(nullptr, new_capacity));
			if (new_ptr)
				memcpy(new_ptr, m_ptr, m_size + 1);
		}
		if (!new_ptr)
			return make_stdlib_error(std::errc::not_enough_memory);

        m_capacity = new_capacity;
		m_ptr = new_ptr;
	}
	return {};
}
error_type string::resize(const size_type size, const char symb) noexcept
{
	if (symb < 0)
		return make_stdlib_error(std::errc::invalid_argument);

	ICY_ERROR(reserve(size + 1));
	memset(m_ptr, symb, size);
	m_ptr[m_size = size] = '\0';
	return {};
}
error_type icy::split(const string_view str, array<string_view>& substr) noexcept
{
    substr.clear();
    const char* ptr = str.bytes().data();
    const char* beg = nullptr;
    for (auto&& chr : str.bytes())
    {
        const auto is_space = chr > 0 && isspace(chr);
        if (!beg) //  searching for first word (letter)
        {
            if (is_space)
                continue;
            else
                beg = &chr;
        }
        else // searching for last letter (before space)
        {
            if (is_space)
            {
                ICY_ERROR(substr.push_back(string_view(beg, &chr)));
                beg = nullptr;
            }
        }
    }
    if (beg)
        ICY_ERROR(substr.push_back(string_view(beg, ptr + str.bytes().size())));
    return {};
}
error_type icy::split(const string_view str, array<string_view>& substr, const char delim) noexcept
{
    if (delim < 0)
        return make_stdlib_error(std::errc::invalid_argument);

    substr.clear();
    const char* ptr = str.bytes().data();
    for (auto&& chr : str.bytes())
    {
        if (chr == delim)
        {
            ICY_ERROR(substr.push_back(string_view(ptr, &chr)));
            ptr = &chr + 1;
        }
    }
    ICY_ERROR(substr.push_back(string_view(ptr, str.bytes().data() + str.bytes().size())));
    return {};
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
	return {};
}
error_type string::insert(const iterator where, const string_view rhs) noexcept
{
	string str;
	ICY_ERROR(str.reserve(capacity()));
	ICY_ERROR(str.append(string_view{ m_ptr, where.m_ptr }));
	ICY_ERROR(str.append(rhs));
	ICY_ERROR(str.append(string_view{ where.m_ptr, m_ptr + m_size }));
	const auto index = where.m_ptr - m_ptr;
	*this = std::move(str);
	return {};
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
	auto offset = m_ptr;
	auto count = 0u;
	while (true)
	{
		const auto it = string_view{ offset, m_ptr + m_size }.find(find);
		if (it != end())
		{
			auto next = node::create();
			if (!next)
				return make_stdlib_error(std::errc::not_enough_memory);
			next->prev = list;
			next->offset = it.m_ptr - m_ptr;
			list = next;
			offset = const_cast<char*>(it.m_ptr + find.m_size);
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

		offset = m_ptr;
		for (k = 0; k < count; ++k)
		{
			str.append(string_view{ offset, m_ptr + vec[k] });
			offset = m_ptr + vec[k] + find.m_size;
			str.append(replace);
		}
		str.append({ offset, m_ptr + m_size });
		*this = std::move(str);
	}
	return {};
}
error_type icy::emplace(map<string, string>& map, const string_view key, const string_view val) noexcept
{
	string new_key;
	string new_val;
	ICY_ERROR(to_string(key, new_key));
	ICY_ERROR(to_string(val, new_val));
	ICY_ERROR(map.find_or_insert(std::move(new_key), std::move(new_val)));
	return {};
}
error_type icy::to_utf16(const string_view value, array<wchar_t>& wide) noexcept
{
	auto size = 0_z;
	ICY_ERROR(value.to_utf16(nullptr, &size));
	ICY_ERROR(wide.resize(size + 1));
	ICY_ERROR(value.to_utf16(wide.data(), &size));
	return {};
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
uint64_t icy::hash64(const string_view string) noexcept
{
    auto value = 14695981039346656037ui64;
    for (auto&& chr : string.bytes())
    {
        value = uint64_t((value ^ uint64_t(chr)) * 1099511628211ui64);
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

    return {};
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
            return {};
        }
        t0 = (t0 << 6) + array[c];
        t1 += 6;
        if (t1 >= 0)
        {
            output[k++] = static_cast<uint8_t>((t0 >> t1) & 0xFF);
            t1 -= 8;
        }
    }
    return {};
}
error_type icy::base64_decode(const string_view input, array_view<uint8_t> output) noexcept
{
    return base64_decode(input.bytes(), output);
}
size_t icy::copy(const string_view src, array_view<char> dst) noexcept
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

/*error_type icy::jaro_winkler_distance(const string_view utf8lhs, const string_view utf8rhs, double& distance) noexcept
{
    // Exit early if either are empty
    if (utf8lhs.empty() || utf8rhs.empty())
    {
        distance = 0;
        return {};
    }

    // Exit early if they're an exact match.
    if (utf8lhs == utf8rhs) 
    {
        distance = 1;
        return {};
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
        return {};
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
    return {};
}*/
#include <icy_engine/network/icy_http.hpp>
#include <chrono>

using namespace icy;

static const auto http_key_content_size = "Content-Length"_s;
static const auto http_key_content_type = "Content-Type"_s;
static const auto http_key_cookie_r = "Cookie"_s;
static const auto http_key_cookie_w = "Set-Cookie"_s;
static const auto http_key_host = "Host"_s;

static string_view to_string(const http_content_type type) noexcept
{
	string_view str;
	switch (type)
	{
	case http_content_type::application_javascript: return "application/javascript"_s;
	case http_content_type::application_octet_stream: return "application/octet-stream"_s;
	case http_content_type::application_pdf: return "application/pdf"_s;
	case http_content_type::application_json: return "application/json"_s;
	case http_content_type::application_dll: return "application/x-msdownload"_s;
	case http_content_type::application_xml: return "application/xml"_s;
	case http_content_type::application_x_www_form_urlencoded: return "application/x-www-form-urlencoded"_s;
	case http_content_type::application_wasm: return "application/wasm"_s;
	case http_content_type::application_zip: return "application/zip"_s;

	case http_content_type::audio_mpeg: return "audio/mpeg"_s;
	case http_content_type::audio_xwav: return "audio/x-wav"_s;

	case http_content_type::image_gif: return "image/gif"_s;
	case http_content_type::image_icon: return "image/x-icon"_s;
	case http_content_type::image_jpeg: return "image/jpeg"_s;
	case http_content_type::image_png: return "image/png"_s;
	case http_content_type::image_svg_xml: return "image/svg+xml"_s;
	case http_content_type::image_tiff: return "image/tiff"_s;

	case http_content_type::text_css: return "text/css"_s;
	case http_content_type::text_csv: return "text/csv"_s;
	case http_content_type::text_html: return "text/html"_s;
	case http_content_type::text_plain: return "text/plain"_s;

	case http_content_type::video_mpeg: return "video/mpeg"_s;
	case http_content_type::video_mp4: return "video/mp4"_s;
	}
	return {};
}
static string_view to_string(const http_error error) noexcept
{
	switch (error)
	{
	case http_error::success: return "200 OK"_s;
	case http_error::not_found: return "404 Not found"_s;
	case http_error::bad_request: return "400 Bad request"_s;
	case http_error::internal: return "500 Internal"_s;
	}
	return {};
}
static error_type decode(const string_view input, string& output) noexcept
{
	const auto len = input.bytes().size();
	const auto ptr = input.bytes().data();

	if (const auto error = output.reserve(len))
		return error;

	const auto hex = "0123456789ABCDEF"_s;

	for (auto k = 0_z; k < len; )
	{
		if (ptr[k] == '%')
		{
			if (k + 2 >= len)
				return make_stdlib_error(std::errc::illegal_byte_sequence);
			const auto it = hex.find(string_view{ &ptr[k + 1], 1 });
			const auto jt = hex.find(string_view{ &ptr[k + 2], 1 });
			if (it == hex.end() || jt == hex.end())
				return make_stdlib_error(std::errc::illegal_byte_sequence);

			const auto h0 = static_cast<const char*>(it) - hex.bytes().data();
			const auto h1 = static_cast<const char*>(jt) - hex.bytes().data();
			const auto chr = char(h0 * 0x10 + h1);
			if (const auto error = output.append(string_view{ &chr, 1 }))
				return error;
			k += 3;
		}
		else
		{
			if (const auto error = output.append(string_view{ &ptr[k], 1 }))
				return error;
			++k;
		}
	}
	return {};
}
static error_type parse(const string_view input, const char delim, map<string, string>& output) noexcept
{
	const auto ptr = input.bytes().data();
	const auto len = input.bytes().size();
	auto str_key = string_view{};
	auto beg = ptr;
	enum
	{
		parse_key,
		parse_val,
	} state = parse_key;

	const auto next = [&str_key, &beg, ptr, &output](const size_t k)
	{
		string key;
		string val;
		if (const auto error = decode(str_key, key))
			return error;
		if (const auto error = decode(string_view{ beg, ptr + k }, val))
			return error;
		if (key.empty() || val.empty())
			return make_stdlib_error(std::errc::illegal_byte_sequence);
		if (const auto error = output.insert(std::move(key), std::move(val)))
		{
			if (error == make_stdlib_error(std::errc::invalid_argument))
				;
			else
				return error;
		}
		return error_type{};
	};

	for (auto k = 0_z; k < len; ++k)
	{
		switch (state)
		{
		case parse_key:
		{
			if (ptr[k] == '=')
			{
				state = parse_val;
				str_key = string_view{ beg, ptr + k };
				beg = ptr + k + 1;
			}
			break;
		}
		case parse_val:
		{
			if (ptr[k] == delim)
				state = parse_key;
			else
				break;
			if (k + 1 >= len || ptr[k + 1] != ' ')
				return make_stdlib_error(std::errc::illegal_byte_sequence);

			if (const auto error = next(k))
				return error;
			beg = ptr + k + 2;
			break;
		}
		default:
			return make_stdlib_error(std::errc::illegal_byte_sequence);
		}
	}
	if (beg != ptr + len)
		return next(len);
	else
		return {};
}

error_type icy::to_value(const string_view buffer, http_request& request) noexcept
{
	enum
	{
		none,
		parse_type,
		parse_url_path,
		parse_url_arg_key,
		parse_url_arg_val,
		parse_http_version,
		parse_header_key,
		parse_header_val,
	} state = parse_type;

	const auto len = buffer.bytes().size();
	const auto ptr = buffer.bytes().data();

	auto beg = ptr;
	auto url_arg_key = string_view{};
	auto header_key = string_view{};

	for (auto k = 0_z; k < len && state; ++k)
	{
		switch (state)
		{
		case parse_type:
		{
			if (ptr[k] == ' ')
			{
				switch (hash(string_view{ beg, ptr + k }))
				{
				case "GET"_hash: 
                    request.type = http_request_type::get;
					break;
				case "POST"_hash: 
                    request.type = http_request_type::post;
					break;
				case "OPTIONS"_hash:
                    request.type = http_request_type::options;
					break;
				default:
					return make_stdlib_error(std::errc::illegal_byte_sequence);
				}
				if (k + 1 >= len || ptr[k + 1] != '/')
					return make_stdlib_error(std::errc::illegal_byte_sequence);
				state = parse_url_path;
				beg = ptr + k + 2;
				k += 1;
			}
			break;
		}
		case parse_url_path:
		{
			if (ptr[k] == '?')
			{
				state = parse_url_arg_key;
				beg = ptr + k + 1;
				break;
			}
			else if (ptr[k] == ' ')
				state = parse_http_version;
			else if (ptr[k] == '/')
				;
			else
				break;

			string path;
            ICY_ERROR(decode(string_view{ beg, ptr + k }, path));
            ICY_ERROR(request.url_sub.push_back(std::move(path)));
			beg = ptr + k + 1;
			break;
		}
		case parse_url_arg_key:
		{
			if (ptr[k] == '=')
			{
				state = parse_url_arg_val;
				url_arg_key = string_view{ beg, ptr + k };
				beg = ptr + k + 1;
			}
			break;
		}
		case parse_url_arg_val:
		{
			if (ptr[k] == '&')
				state = parse_url_arg_key;
			else if (ptr[k] == ' ')
				state = parse_http_version;
			else
				break;

			string key;
			string val;
            ICY_ERROR(decode(url_arg_key, key));
            ICY_ERROR(decode(string_view{ beg, ptr + k }, val));
            ICY_ERROR(request.url_args.insert(std::move(key), std::move(val)));
			beg = ptr + k + 1;
			break;
		}

		case parse_http_version:
		{
			if (ptr[k] == '\r')
			{
				if (string_view(beg, ptr + k) != "HTTP/1.1"_s || k + 1 >= len || ptr[k + 1] != '\n')
					return make_stdlib_error(std::errc::illegal_byte_sequence);
				state = parse_header_key;
				beg = ptr + k + 2;
				k += 1;		
			}
			break;
		}
		case parse_header_key:
		{
			if (ptr[k] == ':')
			{
				if (k + 1 >= len || ptr[k + 1] != ' ')
					return make_stdlib_error(std::errc::illegal_byte_sequence);
				header_key = string_view(beg, ptr + k);
				state = parse_header_val;
				beg = ptr + k + 2;
				k += 1;
			}
			break;
		}
		case parse_header_val:
		{
			if (ptr[k] == '\r')
				;
			else
				break;

			if (k + 3 >= len || ptr[k + 1] != '\n')
				return make_stdlib_error(std::errc::illegal_byte_sequence);

			if (header_key == http_key_cookie_r)
			{
				ICY_ERROR(parse(string_view{ beg, ptr + k }, ';', request.cookies));
			}
			else if (header_key == http_key_cookie_w)
			{
				return make_stdlib_error(std::errc::illegal_byte_sequence);
			}
			else
			{
				string key;
				string val;
				ICY_ERROR(decode(header_key, key));
				ICY_ERROR(decode(string_view{ beg, ptr + k }, val));
				ICY_ERROR(request.headers.insert(std::move(key), std::move(val)));
			}

			if (ptr[k + 2] == '\r' && ptr[k + 3] == '\n')
			{
				beg = ptr + k + 4;
				k += 3;
				state = none;
			}
			else
			{
				beg = ptr + k + 2;
				k += 1;
				state = parse_header_key;
			}
			break;
		}
		default:
			return make_stdlib_error(std::errc::illegal_byte_sequence);
		}
	}
	const auto keys = request.headers.keys();
	const auto key_content_size = binary_search(keys.begin(), keys.end(), http_key_content_size);
	const auto key_content_type = binary_search(keys.begin(), keys.end(), http_key_content_type);
	const auto key_host = binary_search(keys.begin(), keys.end(), http_key_host);

    string_view body_str;
	if (key_content_size != keys.end())
	{
		const auto val_content_size = string_view{ request.headers.vals()[key_content_size - keys.begin()] };
		auto size = 0u;
		if (to_value(val_content_size, size) || beg + size != ptr + len)
			return make_stdlib_error(std::errc::illegal_byte_sequence);

        body_str = string_view(beg, size);
        ICY_ERROR(request.body.append(body_str.ubytes()));
	}
	else if (beg != ptr + len)
	{
		return make_stdlib_error(std::errc::illegal_byte_sequence);
	}

	if (key_host != keys.end())
	{
		ICY_ERROR(copy(request.headers.vals()[key_host - keys.begin()], request.host));
	}

	if (key_content_type != keys.end())
	{
		const auto& val = request.headers.vals()[key_content_type - keys.begin()];
		for (auto k = http_content_type::none; k < http_content_type::_count; )
		{
			if (::to_string(k) == val)
			{
                request.content = k;
				break;
			}
			k = http_content_type(uint32_t(k) + 1);
		}
		if (request.content == http_content_type::application_x_www_form_urlencoded)
			ICY_ERROR(parse(body_str, '&', request.body_args));
	}
	return {};
}
error_type icy::to_value(const string_view buffer, http_response& response) noexcept
{
	enum
	{
		none,
		parse_http_error,
		parse_http_version,
		parse_header_key,
		parse_header_val,
	} state = parse_http_version;

	const auto len = buffer.bytes().size();
	const auto ptr = buffer.bytes().data();

	auto beg = ptr;
	auto header_key = string_view{};

	for (auto k = 0_z; k < len && state; ++k)
	{
		switch (state)
		{
		case parse_http_version:
		{
			if (ptr[k] == ' ')
			{
				if (string_view(beg, ptr + k) != "HTTP/1.1"_s)
					return make_stdlib_error(std::errc::illegal_byte_sequence);
				state = parse_http_error;
				beg = ptr + k + 1;
				k += 1;
			}
			break;
		}
		case parse_http_error:
		{
			if (ptr[k] == '\r')
			{
				if (to_value(string_view(beg, ptr + k), reinterpret_cast<uint32_t&>(response.herror)) != error_type{} || k + 1 >= len || ptr[k + 1] != '\n')
					return make_stdlib_error(std::errc::illegal_byte_sequence);
				state = parse_header_key;
				beg = ptr + k + 2;
				k += 1;
			}
			break;
		}
		case parse_header_key:
		{
			if (ptr[k] == ':')
			{
				if (k + 1 >= len || ptr[k + 1] != ' ')
					return make_stdlib_error(std::errc::illegal_byte_sequence);
				header_key = string_view(beg, ptr + k);
				state = parse_header_val;
				beg = ptr + k + 2;
				k += 1;
			}
			break;
		}
		case parse_header_val:
		{
			if (ptr[k] == '\r')
				;
			else
				break;

			if (k + 3 >= len || ptr[k + 1] != '\n')
				return make_stdlib_error(std::errc::illegal_byte_sequence);

			if (header_key == http_key_cookie_r)
			{
				return make_stdlib_error(std::errc::illegal_byte_sequence);
			}
			else if (header_key == http_key_cookie_w)
			{
				auto cookie_beg = beg;
				for (; cookie_beg != ptr + k; ++cookie_beg)
				{
					if (*cookie_beg == '=')
						break;
				}
				const auto cookie_key = string_view{ beg, cookie_beg };

				if (cookie_beg != ptr + k && *cookie_beg == '=')
					++cookie_beg;

				auto cookie_end = cookie_beg;
				for (; cookie_end != ptr + k; ++cookie_end)
				{
					if (*cookie_end == ';')
						break;
				}
				const auto cookie_val = string_view{ cookie_beg, cookie_end };
				string key;
				string val;
				ICY_ERROR(decode(cookie_key, key));
				ICY_ERROR(decode(cookie_val, val));
				ICY_ERROR(response.cookies.insert(std::move(key), std::move(val)));
			}
			else
			{
				string key;
				string val;
				ICY_ERROR(decode(header_key, key));
				ICY_ERROR(decode(string_view{ beg, ptr + k }, val));
				ICY_ERROR(response.headers.insert(std::move(key), std::move(val)));
			}

			if (ptr[k + 2] == '\r' && ptr[k + 3] == '\n')
			{
				beg = ptr + k + 4;
				k += 3;
				state = none;
			}
			else
			{
				beg = ptr + k + 2;
				k += 1;
				state = parse_header_key;
			}
			break;
		}
		default:
			return make_stdlib_error(std::errc::illegal_byte_sequence);
		}
	}
	const auto keys = response.headers.keys();
	const auto key_content_size = binary_search(keys.begin(), keys.end(), http_key_content_size);
	//const auto key_content_type = binary_search(keys.begin(), keys.end(), http_key_content_type);

	if (key_content_size != keys.end())
	{
		const auto val_content_size = string_view{ response.headers.vals()[key_content_size - keys.begin()] };
		auto size = 0u;
		if (to_value(val_content_size, size) || beg + size != ptr + len)
			return make_stdlib_error(std::errc::illegal_byte_sequence);

        ICY_ERROR(copy({ reinterpret_cast<const uint8_t*>(beg), size }, response.body));
	}
	else if (beg != ptr + len)
	{
		return make_stdlib_error(std::errc::illegal_byte_sequence);
	}

	return {};
}

http_content_type icy::is_http_content_type(const string_view filename) noexcept
{
	auto ptr = filename.bytes().data();
	auto len = filename.bytes().size();
	const char* last_dot = nullptr;
	for (auto k = len; k; --k)
	{
		const auto chr = ptr[k - 1];
		if (chr == '.')
			last_dot = ptr + k - 1;
		if (chr == '\\' || chr == '/')
			break;
	}
	const auto ext = last_dot ? string_view(last_dot, ptr + len) : string_view();

	if (ext.find(".js"_s) != ext.end())
		return http_content_type::application_javascript;
	else if (ext.find(".map"_s) != ext.end())
		return http_content_type::application_javascript;
	else if (ext.find(".css"_s) != ext.end())
		return http_content_type::text_css;
	else if (ext.find(".dll"_s) != ext.end())
		return http_content_type::application_dll;

	switch (hash(ext))
	{
	case ".gif"_hash:
		return http_content_type::image_gif;
	case ".ico"_hash:
		return http_content_type::image_icon;
	case ".jpg"_hash:
	case ".jpeg"_hash:
		return http_content_type::image_jpeg;
	case ".png"_hash:
		return http_content_type::image_png;
	case ".tiff"_hash:
		return http_content_type::image_tiff;

	case ".json"_hash:
		return http_content_type::application_javascript;
	case ".pdf"_hash:
		return http_content_type::application_pdf;

	case ".xml"_hash:
		return http_content_type::application_xml;

	case ".zip"_hash:
		return http_content_type::application_zip;

	case ".wav"_hash:
		return http_content_type::audio_xwav;

	case ".mp4"_hash:
		return http_content_type::video_mp4;

	case ".csv"_hash:
		return http_content_type::text_csv;

	case ".html"_hash:
		return http_content_type::text_html;

	case ".txt"_hash:
		return http_content_type::text_plain;

	case ".wasm"_hash:
		return http_content_type::application_wasm;

	}
	
	return http_content_type::application_octet_stream;
}

error_type icy::to_string(const http_request& request, string& str) noexcept
{
	switch (request.type)
	{
	case http_request_type::get:
		ICY_ERROR(str.append("GET"_s));
		break;
	case http_request_type::post:
		ICY_ERROR(str.append("POST"_s));
		break;
	case http_request_type::options:
		ICY_ERROR(str.append("OPTIONS"_s));
		break;
	}
	ICY_ERROR(str.append(" "_s));

	if (request.url_sub.empty())
	{
		ICY_ERROR(str.append("/"_s));
	}
	else
	{
		for (auto&& url : request.url_sub)
		{
			ICY_ERROR(str.append("/"_s));
			ICY_ERROR(str.append(url));
		}
	}
	if (!request.url_args.empty())
	{
		ICY_ERROR(str.append("?"_s));
		auto first_time = true;
		for (auto&& arg : request.url_args)
		{
			if (!first_time)
				ICY_ERROR(str.append("&"_s));
			ICY_ERROR(str.append(arg.key));
			ICY_ERROR(str.append("="_s));
			ICY_ERROR(str.append(arg.value));
			first_time = false;
		}
	}

	ICY_ERROR(str.append(" HTTP/1.1"_s));
	for (auto&& pair : request.headers)
	{
		if (pair.key == http_key_content_size ||
			pair.key == http_key_content_type ||
			pair.key == http_key_host)
			continue;

		ICY_ERROR(str.append("\r\n"_s));
		ICY_ERROR(str.append(pair.key));
		ICY_ERROR(str.append(": "_s));
		ICY_ERROR(str.append(pair.value));
	}

	string body_args;
	if (!request.body_args.empty())
	{
		auto first_time = true;
		for (auto&& arg : request.body_args)
		{
			if (!first_time)
				ICY_ERROR(body_args.append("&"_s));
			ICY_ERROR(body_args.append(arg.key));
			ICY_ERROR(body_args.append("="_s));
			ICY_ERROR(body_args.append(arg.value));
			first_time = false;
		}
	}

	if (!request.body.empty() || !body_args.empty())
	{
		ICY_ERROR(str.append("\r\n"_s));
		ICY_ERROR(str.append(http_key_content_size));
		ICY_ERROR(str.append(": "_s));
		string content_size;
		if (body_args.empty())
		{
			ICY_ERROR(to_string(request.body.size(), content_size));
		}
		else
		{
			ICY_ERROR(to_string(body_args.bytes().size(), content_size));
		}
		ICY_ERROR(str.append(content_size));
	}
	

	ICY_ERROR(str.append("\r\n"_s));
	ICY_ERROR(str.append(http_key_content_type));
	ICY_ERROR(str.append(": "_s));
	ICY_ERROR(str.append(::to_string(request.content)));

	if (!request.host.empty())
	{
		ICY_ERROR(str.append("\r\n"_s));
		ICY_ERROR(str.append(http_key_host));
		ICY_ERROR(str.append(": "_s));
		ICY_ERROR(str.append(request.host));
	}

	if (!request.cookies.empty())
	{
		ICY_ERROR(str.append("\r\n"_s));
		ICY_ERROR(str.append(http_key_cookie_r));
		ICY_ERROR(str.append(": "_s));
		auto first_time = true;
		for (auto&& cookie : request.cookies)
		{
			if (!first_time)
				ICY_ERROR(str.append("; "_s));
			ICY_ERROR(str.append(cookie.key));
			ICY_ERROR(str.append("="_s));
			ICY_ERROR(str.append(cookie.value));
			first_time = false;
		}
	}

	ICY_ERROR(str.append("\r\n\r\n"_s));

	if (body_args.empty())
	{
		//ICY_ERROR(str.append(request.body));
	}
	else
	{
		ICY_ERROR(str.append(body_args));
	}
	return {};
}
error_type icy::to_string(const http_response& response, string& str) noexcept
{
	const auto type_str = ::to_string(response.type);
	const auto error_str = ::to_string(response.herror);
	string length_str;

	if (error_str.empty())
		return make_stdlib_error(std::errc::invalid_argument);

	ICY_ERROR(to_string(response.body.size(), length_str));

	
	ICY_ERROR(str.append("HTTP/1.1 "_s));
	ICY_ERROR(str.append(error_str));
	ICY_ERROR(str.append("\r\n"_s));
	ICY_ERROR(str.append(http_key_content_size));
	ICY_ERROR(str.append(": "_s));
	ICY_ERROR(str.append(length_str));

	if (!type_str.empty())
	{
		ICY_ERROR(str.append("\r\n"_s));
		ICY_ERROR(str.append(http_key_content_type));
		ICY_ERROR(str.append(": "_s));
		ICY_ERROR(str.append(type_str));
		ICY_ERROR(str.append(";charset=UTF-8"_s));
	}

	for (auto&& pair : response.headers)
	{
        if (pair.key == http_key_content_size ||
            pair.key == http_key_content_type ||
            pair.key == http_key_host ||
            pair.key == http_key_cookie_r)
            continue;

		ICY_ERROR(str.append("\r\n"_s));
		ICY_ERROR(str.append(pair.key));
		ICY_ERROR(str.append(": "_s));
		ICY_ERROR(str.append(pair.value));;
	}

	for (auto&& cookie : response.cookies)
	{
		ICY_ERROR(str.append("\r\n"_s));
		ICY_ERROR(str.append(http_key_cookie_w));
		ICY_ERROR(str.append(": "_s));
		ICY_ERROR(str.append(cookie.key));
		ICY_ERROR(str.append("="_s));
		ICY_ERROR(str.append(cookie.value));
	}
	ICY_ERROR(str.append("\r\n\r\n"_s));
	return {};
}

error_type icy::copy(const http_request& src, http_request& dst) noexcept
{
    dst.type = src.type;
    dst.content = src.content;
    ICY_ERROR(copy(src.url_path, dst.url_path));
    ICY_ERROR(copy(src.url_sub, dst.url_sub));
    ICY_ERROR(copy(src.url_args, dst.url_args));
    ICY_ERROR(copy(src.headers, dst.headers));
    ICY_ERROR(copy(src.cookies, dst.cookies));
    ICY_ERROR(copy(src.host, dst.host));
    ICY_ERROR(copy(src.body, dst.body));
    ICY_ERROR(copy(src.body_args, dst.body_args));
    return {};
}
error_type icy::copy(const http_response& src, http_response& dst) noexcept
{
    dst.type = src.type;
    dst.herror = src.herror;
    ICY_ERROR(copy(src.headers, dst.headers));
    ICY_ERROR(copy(src.cookies, dst.cookies));
    ICY_ERROR(copy(src.body, dst.body));
    return {};
}
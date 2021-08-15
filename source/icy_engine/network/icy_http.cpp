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
	//const auto len = input.bytes().size();
	//const auto ptr = input.bytes().data();

	ICY_ERROR(output.reserve(input.bytes().size()));
	const auto hex = "0123456789ABCDEF"_s;

	for (auto it = input.begin(); it != input.end(); ++it)
	{
		char32_t chr = 0;
		ICY_ERROR(it.to_char(chr));
		if (chr == '%')
		{
			if (it + 2 >= input.end())
				return make_stdlib_error(std::errc::illegal_byte_sequence);
			const auto jt = hex.find(string_view(it + 1, it + 2));
			const auto kt = hex.find(string_view(it + 2, it + 3));
			if (jt == hex.end() || kt == hex.end())
				return make_stdlib_error(std::errc::illegal_byte_sequence);

			const auto h0 = static_cast<const char*>(it) - hex.bytes().data();
			const auto h1 = static_cast<const char*>(jt) - hex.bytes().data();
			char buffer[] = { char(h0 * 0x10 + h1), 0 };
			string tmp;
			ICY_ERROR(to_string(const_array_view<char>(buffer, 1), tmp));
			ICY_ERROR(output.append(tmp));
			it += 2;
		}
		else
		{
			ICY_ERROR(output.append(string_view(it, it + 1)));
		}
	}
	return error_type();
}
static error_type parse(const string_view input, const char32_t delim, map<string, string>& output) noexcept
{
	string_view str_key;
	auto beg = input.begin();
	enum
	{
		parse_key,
		parse_val,
	} state = parse_key;

	const auto func_next = [&str_key, &beg, &output](const string_view::iterator it)
	{
		string key;
		string val;
		if (const auto error = decode(str_key, key))
			return error;
		if (const auto error = decode(string_view{ beg, it }, val))
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
		return error_type();
	};

	for (auto it = input.begin(); it != input.end(); ++it)
	{
		char32_t chr = 0;
		ICY_ERROR(it.to_char(chr));
		switch (state)
		{
		case parse_key:
		{
			if (chr == '=')
			{
				state = parse_val;
				str_key = string_view(beg, it);
				beg = it + 1;
			}
			break;
		}
		case parse_val:
		{
			if (chr == delim)
				state = parse_key;
			else
				break;
			if (it + 1 >= input.end())
				return make_stdlib_error(std::errc::illegal_byte_sequence);

			char32_t next = 0;
			ICY_ERROR((it + 1).to_char(next));
			if (next != ' ')
				return make_stdlib_error(std::errc::illegal_byte_sequence);

			if (const auto error = func_next(it))
				return error;
			beg = it + 2;
			break;
		}
		default:
			return make_stdlib_error(std::errc::illegal_byte_sequence);
		}
	}
	if (beg != input.end())
		return func_next(input.end());
	else
		return error_type();
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

	
	auto beg = buffer.begin();
	string_view url_arg_key;
	string_view header_key;

	for (auto it = buffer.begin(); it != buffer.end() && state; ++it)
	{
		char32_t chr = 0;
		ICY_ERROR(it.to_char(chr));
		switch (state)
		{
		case parse_type:
		{
			if (chr == ' ')
			{
				switch (hash(string_view(beg, it)))
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
				for (++it; it != buffer.end(); ++it)
				{
					char32_t next = 0;
					ICY_ERROR(it.to_char(next));
					if (next == ' ')
						continue;
					else if (next == '/')
						break;
					else
						return make_stdlib_error(std::errc::illegal_byte_sequence);
				}
				state = parse_url_path;
				beg = it;
			}
			break;
		}
		case parse_url_path:
		{
			if (chr == '?')
			{
				state = parse_url_arg_key;
				beg = it + 1;
				break;
			}
			else if (chr == ' ')
				state = parse_http_version;
			else if (chr == '/')
				;
			else
				break;

			string path;
            ICY_ERROR(decode(string_view(beg, it), path));
            ICY_ERROR(request.url_sub.push_back(std::move(path)));
			beg = it + 1;
			break;
		}
		case parse_url_arg_key:
		{
			if (chr == '=')
			{
				state = parse_url_arg_val;
				url_arg_key = string_view{ beg, it };
				beg = it + 1;
			}
			break;
		}
		case parse_url_arg_val:
		{
			if (chr == '&')
				state = parse_url_arg_key;
			else if (chr == ' ')
				state = parse_http_version;
			else
				break;

			string key;
			string val;
            ICY_ERROR(decode(url_arg_key, key));
            ICY_ERROR(decode(string_view(beg, it), val));
            ICY_ERROR(request.url_args.insert(std::move(key), std::move(val)));
			beg = it + 1;
			break;
		}

		case parse_http_version:
		{
			if (chr == '\r')
			{
				if (string_view(beg, it) != "HTTP/1.1"_s || it + 1 >= buffer.end())
					return make_stdlib_error(std::errc::illegal_byte_sequence);

				char32_t next = 0;
				ICY_ERROR((it + 1).to_char(next));
				if (next != '\n')
					return make_stdlib_error(std::errc::illegal_byte_sequence);

				state = parse_header_key;
				beg = it + 2;
				it += 1;
			}
			break;
		}
		case parse_header_key:
		{
			if (chr == '\r')
			{
				state = none;
				beg = buffer.end();
				break;
			}
			if (chr == ':')
			{
				if (it + 1 >= buffer.end())
					return make_stdlib_error(std::errc::illegal_byte_sequence);

				char32_t next = 0;
				ICY_ERROR((it + 1).to_char(next));
				if (next != ' ')
					return make_stdlib_error(std::errc::illegal_byte_sequence);

				header_key = string_view(beg, it);
				state = parse_header_val;
				beg = it + 2;
				it += 1;
			}
			break;
		}
		case parse_header_val:
		{
			if (chr == '\r')
				;
			else
				break;

			if (it + 3 >= buffer.end())
				return make_stdlib_error(std::errc::illegal_byte_sequence);

			char32_t next = 0;
			ICY_ERROR((it + 1).to_char(next));
			if (next != '\n')
				return make_stdlib_error(std::errc::illegal_byte_sequence);

			if (header_key == http_key_cookie_r)
			{
				ICY_ERROR(parse(string_view(beg, it), ';', request.cookies));
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
				ICY_ERROR(decode(string_view(beg, it), val));
				ICY_ERROR(request.headers.insert(std::move(key), std::move(val)));
			}


			char32_t next2 = 0;
			char32_t next3 = 0;
			ICY_ERROR((it + 2).to_char(next2));
			ICY_ERROR((it + 3).to_char(next3));

			if (next2 == '\r' && next3 == '\n')
			{
				beg = it + 4;
				it += 3;
				state = none;
			}
			else
			{
				beg = it + 2;
				it += 1;
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

	if (key_host != keys.end())
	{
		ICY_ERROR(copy(request.headers.vals()[key_host - keys.begin()], request.host));
	}

	string_view body_str;
	if (key_content_size != keys.end())
	{
		const auto val_content_size = string_view(request.headers.vals()[key_content_size - keys.begin()]);
		auto size = 0u;
		if (to_value(val_content_size, size))
			return make_stdlib_error(std::errc::illegal_byte_sequence);

		const auto ptr = static_cast<const char*>(beg);
		if (ptr + size != buffer.bytes().data() + buffer.bytes().size())
			return make_stdlib_error(std::errc::illegal_byte_sequence);

		body_str = string_view(beg, buffer.end());
		ICY_ERROR(request.body.append(body_str.ubytes()));
	}
	else if (beg != buffer.end())
	{
		return make_stdlib_error(std::errc::illegal_byte_sequence);
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
	return error_type();
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

	//const auto len = buffer.bytes().size();
	//const auto ptr = buffer.bytes().data();

	auto beg = buffer.begin();
	string_view header_key;

	for (auto it = buffer.begin(); it != buffer.end() && state; ++it)
	{
		char32_t chr = 0;
		ICY_ERROR(it.to_char(chr));

		switch (state)
		{
		case parse_http_version:
		{
			if (chr == ' ')
			{
				if (string_view(beg, it) != "HTTP/1.1"_s)
					return make_stdlib_error(std::errc::illegal_byte_sequence);
				state = parse_http_error;
				beg = it + 1;
				it += 1;
			}
			break;
		}
		case parse_http_error:
		{
			if (chr == '\r')
			{
				if (to_value(string_view(beg, it), reinterpret_cast<uint32_t&>(response.herror)) != error_type())
					return make_stdlib_error(std::errc::illegal_byte_sequence);

				if (it + 1 >= buffer.end())
					return make_stdlib_error(std::errc::illegal_byte_sequence);

				char32_t next = 0;
				ICY_ERROR((it + 1).to_char(next));
				if (next != '\n')
					return make_stdlib_error(std::errc::illegal_byte_sequence);

				state = parse_header_key;
				beg = it + 2;
				it += 1;
			}
			break;
		}
		case parse_header_key:
		{
			if (chr == ':')
			{
				if (it + 1 >= buffer.end())
					return make_stdlib_error(std::errc::illegal_byte_sequence);

				char32_t next = 0;
				ICY_ERROR((it + 1).to_char(next));
				if (next != ' ')
					return make_stdlib_error(std::errc::illegal_byte_sequence);

				header_key = string_view(beg, it);
				state = parse_header_val;
				beg = it + 2;
				it += 1;
			}
			break;
		}
		case parse_header_val:
		{
			if (chr == '\r')
				;
			else
				break;

			if (it + 3 >= buffer.end())
				return make_stdlib_error(std::errc::illegal_byte_sequence);

			char32_t next = 0;
			ICY_ERROR((it + 1).to_char(next));
			if (next != '\n')
				return make_stdlib_error(std::errc::illegal_byte_sequence);
			
			if (header_key == http_key_cookie_r)
			{
				return make_stdlib_error(std::errc::illegal_byte_sequence);
			}
			else if (header_key == http_key_cookie_w)
			{
				auto cookie_beg = beg;
				for (; cookie_beg != it; ++cookie_beg)
				{
					char32_t tmp = 0;
					ICY_ERROR(cookie_beg.to_char(tmp));
					if (tmp == '=')
						break;
				}
				const auto cookie_key = string_view(beg, cookie_beg);

				if (cookie_beg != it)
				{
					char32_t tmp = 0;
					ICY_ERROR(cookie_beg.to_char(tmp));
					if (tmp == '=')
						++cookie_beg;
				}
				auto cookie_end = cookie_beg;
				for (; cookie_end != it; ++cookie_end)
				{
					char32_t tmp = 0;
					ICY_ERROR(cookie_end.to_char(tmp));
					if (tmp == ';')
						break;
				}
				const auto cookie_val = string_view(cookie_beg, cookie_end);
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
				ICY_ERROR(decode(string_view(beg, it), val));
				ICY_ERROR(response.headers.insert(std::move(key), std::move(val)));
			}

			char32_t next2 = 0;
			char32_t next3 = 0;
			ICY_ERROR((it + 2).to_char(next2));
			ICY_ERROR((it + 3).to_char(next3));
			if (next2 == '\r' && next3 == '\n')
			{
				beg = it + 4;
				it += 3;
				state = none;
			}
			else
			{
				beg = it + 2;
				it += 1;
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
		const auto val_content_size = string_view(response.headers.vals()[key_content_size - keys.begin()]);
		auto size = 0u;
		if (to_value(val_content_size, size))
			return make_stdlib_error(std::errc::illegal_byte_sequence);

		const auto ptr = static_cast<const char*>(beg);
		if (ptr + size != buffer.bytes().data() + buffer.bytes().size())
			return make_stdlib_error(std::errc::illegal_byte_sequence);

		auto body_str = string_view(beg, buffer.end());
		ICY_ERROR(response.body.append(body_str.ubytes()));
	}
	else if (beg != buffer.end())
	{
		return make_stdlib_error(std::errc::illegal_byte_sequence);
	}

	return error_type();
}

http_content_type icy::is_http_content_type(const string_view filename) noexcept
{
	/*const char* last_dot = nullptr;
	for (auto k = len; k; --k)
	{
		const auto chr = ptr[k - 1];
		if (chr == '.')
			last_dot = ptr + k - 1;
		if (chr == '\\' || chr == '/')
			break;
	}*/
	auto last_dot = filename.end();
	for (auto it = filename.end(); it != filename.begin(); --it)
	{
		char32_t chr = 0;
		if ((it - 1).to_char(chr))
			return http_content_type::none;

		if (chr == '.')
		{
			last_dot = it - 1;
			break;
		}
		else if (chr == '\\' || chr == '/')
		{
			break;
		}
	}

	const auto ext = last_dot != filename.end() ? string_view(last_dot, filename.end()) : string_view();

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
	
	if (request.type == http_request_type::post)
	{
		ICY_ERROR(str.append("\r\n"_s));
		ICY_ERROR(str.append(http_key_content_type));
		ICY_ERROR(str.append(": "_s));
		ICY_ERROR(str.append(::to_string(request.content)));
	}

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
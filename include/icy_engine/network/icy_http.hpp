#pragma once

#include <icy_engine/core/icy_string.hpp>
#include <icy_engine/core/icy_map.hpp>

namespace icy
{
	enum class http_request_type : uint32_t
	{
		none,
		get,
		post,
		options,
	};
	enum class http_content_type : uint32_t
	{
		none,

		application_dll,
		application_javascript,
		application_octet_stream,
		application_pdf,
		application_json,
		application_wasm,
		application_xml,
		application_x_www_form_urlencoded,
		application_zip,

		audio_mpeg,
		audio_xwav,

		image_gif,
		image_icon,
		image_jpeg,
		image_png,
		image_tiff,
		image_svg_xml,

		text_css,
		text_csv,
		text_html,
		text_plain,

		video_mpeg,
		video_mp4,

		_count,
	};
	enum class http_error : uint32_t
	{
		success		= 200,
		
		bad_request = 400,
		forbidden	= 403,
		not_found	= 404,

		internal	= 500,
	};
	struct http_request
	{
		error_type initialize(const string_view buffer) noexcept;		
		http_request_type type = http_request_type::none;
		http_content_type content = http_content_type::none;
		string url_path;
		array<string> url_sub;
		map<string, string> url_args;
		map<string, string> headers;
		map<string, string> cookies;
		string host;
		string body;
		map<string, string> body_args;
	};
	struct http_response
	{
		error_type initialize(const string_view buffer, const_array_view<uint8_t>& body) noexcept;
		http_error herror = http_error::success;
		http_content_type type = http_content_type::none;
		map<string, string> headers;
		map<string, string> cookies;
	};

	http_content_type is_http_content_type(const string_view filename) noexcept;
	error_type to_string(const http_request& request, string& str) noexcept;
    error_type to_string(const http_response& response, string& str, const const_array_view<uint8_t> body = {}) noexcept;
}
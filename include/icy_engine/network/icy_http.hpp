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
		http_request_type type = http_request_type::none;
		http_content_type content = http_content_type::none;
		string url_path;
		array<string> url_sub;
        dictionary<string> url_args;
        dictionary<string> headers;
        dictionary<string> cookies;
		string host;
		array<uint8_t> body;
		dictionary<string> body_args;
	};
	struct http_response
	{
		http_error herror = http_error::success;
		http_content_type type = http_content_type::none;
        dictionary<string> headers;
        dictionary<string> cookies;
        array<uint8_t> body;
	};

	http_content_type is_http_content_type(const string_view filename) noexcept;
	error_type to_string(const http_request& request, string& str) noexcept;
    error_type to_string(const http_response& response, string& str) noexcept;

    error_type to_value(const string_view buffer, http_request& request) noexcept;
    error_type to_value(const string_view buffer, http_response& response) noexcept;

    error_type copy(const http_request& src, http_request& dst) noexcept;
    error_type copy(const http_response& src, http_response& dst) noexcept;
}
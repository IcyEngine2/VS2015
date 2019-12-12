#pragma once

#include "icy_engine/icy_array.hpp"
#include "icy_engine/icy_string_view.hpp"

struct Curl_easy;

namespace icy
{
    extern const error_source error_source_curl;
    class curl_socket
    {
    public:
        curl_socket() noexcept = default;
        curl_socket(const curl_socket&) noexcept = default;
        ~curl_socket() noexcept
        {
            shutdown();
        }
        void shutdown() noexcept;
        error_type initialize(const string_view url) noexcept;
        error_type exec() noexcept;
        const void* data() const noexcept
        {
            return m_data;
        }
        void* data() noexcept
        {
            return m_data;
        }
        size_t size() const noexcept
        {
            return m_size;
        }
    private:
        Curl_easy* m_handle = nullptr;
        void* m_data = nullptr;
        size_t m_size = 0;
    };
    error_type curl_initialize(const global_heap_type alloc = detail::global_heap) noexcept;
    error_type curl_encode(const string_view url, string& encoded) noexcept;
    void curl_shutdown() noexcept;
}
#define CURL_STATICLIB 1
#define CURL_STRICTER 1

#include <icy_engine/utility/icy_curl.hpp>
#include <icy_engine/core/icy_string.hpp>
#include "../../libs/curl/curl/curl.h"

#pragma comment(lib, "ws2_32")
#pragma comment(lib, "Advapi32")
#pragma comment(lib, "Crypt32")
#if _DEBUG
#pragma comment(lib, "libcurld")
#else
#pragma comment(lib, "libcurl")
#endif

using namespace icy;

ICY_STATIC_NAMESPACE_BEG
global_heap_type g_heap;
error_type curl_error_to_string(const unsigned code, const string_view locale, string& str) noexcept
{
    return icy::to_string(curl_easy_strerror(static_cast<CURLcode>(code)), str);
}
inline error_type make_curl_error(const CURLcode code) noexcept
{
    return error_type(code, error_source_curl);
}
ICY_STATIC_NAMESPACE_END

void curl_socket::shutdown() noexcept
{
    if (m_handle)
    {
        curl_easy_cleanup(m_handle);
        m_handle = nullptr;
    }
    if (m_data)
    {
        g_heap.realloc(m_data, 0, g_heap.user);
        m_data = nullptr;
    }
    m_size = 0;
}
error_type curl_socket::exec() noexcept
{
    g_heap.realloc(m_data, 0, g_heap.user);
    m_data = nullptr;
    m_size = 0;

    if (const auto error = curl_easy_perform(m_handle))
        return make_curl_error(error);
    return {};
}
error_type curl_socket::initialize(const string_view url) noexcept
{
    auto handle = curl_easy_init();
    if (!handle)
        return make_curl_error(CURLcode::CURLE_OUT_OF_MEMORY);
    ICY_SCOPE_EXIT{ curl_easy_cleanup(handle); };

    string str;
    ICY_ERROR(to_string(url, str));
    if (const auto error = curl_easy_setopt(handle, CURLoption::CURLOPT_URL, str.bytes().data()))
        return make_curl_error(error);

#if _DEBUG
    const auto trace = [](CURL* handle, curl_infotype type, char* data, size_t size, void* userp) -> int
    {
        if (type == curl_infotype::CURLINFO_TEXT)
        {
            array<wchar_t> wide;
            to_utf16(string_view(data, size), wide);
            OutputDebugStringW(wide.data());
        }
        return 0;
    };
    if (const auto error = curl_easy_setopt(handle, CURLOPT_VERBOSE, 1L))
        return make_curl_error(error);
    if (const auto error = curl_easy_setopt(handle, CURLOPT_DEBUGFUNCTION,
        static_cast<int(*)(CURL*, curl_infotype, char*, size_t, void*)>(trace)))
        return make_curl_error(error);
#endif
    const auto curl_write = [](void const* const data, const size_t csize, const size_t count, FILE* user)
    {
        const auto size = csize * count;
        const auto self = reinterpret_cast<curl_socket*>(user);
        const auto new_size = self->m_size + size;
        const auto new_data = g_heap.realloc(self->m_data, new_size, g_heap.user);
        memcpy(static_cast<char*>(new_data) + self->m_size, data, size);
        self->m_data = new_data;
        self->m_size = new_size;
        return new_data ? size : 0;
    };

    if (const auto error = curl_easy_setopt(handle, CURLOPT_WRITEFUNCTION, static_cast<decltype(&fwrite)>(curl_write)))
        return make_curl_error(error);
    if (const auto error = curl_easy_setopt(handle, CURLOPT_WRITEDATA, this))
        return make_curl_error(error);

    shutdown();
    std::swap(handle, m_handle);
    return {};
}
error_type icy::curl_initialize(global_heap_type alloc) noexcept
{
    icy::curl_shutdown();

    const auto curl_malloc = [](const size_t size)
    {
        return g_heap.realloc(nullptr, size, g_heap.user); 
    };
    const auto curl_free = [](void* const ptr) 
    {
        return g_heap.realloc(ptr, 0, g_heap.user);
    };
    const auto curl_realloc = [](void* const ptr, const size_t size)
    {
        return g_heap.realloc(ptr, size, g_heap.user);
    };
    const auto curl_strdup = [](const char* const ptr) -> char*
    {
        if (!ptr)
            return nullptr;

        const auto len = strlen(ptr);
        const auto new_ptr = static_cast<char*>(g_heap.realloc(nullptr, len, g_heap.user));
        if (new_ptr)
            memcpy(new_ptr, ptr, len);

        return new_ptr;
    };
    const auto curl_calloc = [](const size_t count, const size_t size)
    {
        const auto new_size = count * size;
        const auto ptr = g_heap.realloc(nullptr, new_size, g_heap.user);
        if (ptr)
            memset(ptr, 0, new_size);
        return ptr;
    };
    if (!alloc.realloc)
        return make_stdlib_error(std::errc::invalid_argument);

    g_heap = alloc;
    if (const auto error = curl_global_init(CURL_GLOBAL_ALL))
    //if (const auto error = curl_global_init_mem(CURL_GLOBAL_ALL,
    //    curl_malloc, curl_free, curl_realloc, curl_strdup, curl_calloc))
    {
        g_heap = {};
        return make_curl_error(error);
    }
    
    return {};
}
error_type icy::curl_encode(const string_view url, string& encoded) noexcept
{
    if (url.empty())
    {
        encoded.clear();
        return {};
    }

    const auto ptr = curl_easy_escape(nullptr, url.bytes().data(), static_cast<int>(url.bytes().size()));
    if (!ptr)
        return make_curl_error(CURLcode::CURLE_OUT_OF_MEMORY);

    ICY_SCOPE_EXIT{ curl_free(ptr); };
    ICY_ERROR(to_string(ptr, encoded));
    return {};
}
void icy::curl_shutdown() noexcept
{
    if (g_heap.realloc)
        curl_global_cleanup();
    g_heap = {};
}
const error_source icy::error_source_curl = register_error_source("curl"_s, curl_error_to_string);
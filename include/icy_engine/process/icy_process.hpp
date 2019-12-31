#pragma once

#include "../core/icy_array.hpp"

namespace icy
{
    class process;
    class remote_library;

    class process
    {
        friend remote_library;
    public:
        static error_type enumerate(array<std::pair<string, uint32_t>>& proc) noexcept;
        ~process() noexcept
        {
            wait(nullptr, {});
        }
        error_type wait(uint32_t* const code, const duration_type timeout = max_timeout) noexcept;
        error_type open(const uint32_t index, const bool read = false) noexcept;
        error_type launch(const string_view path) noexcept;
        error_type inject(const string_view library) noexcept;
        uint32_t index() const noexcept
        {
            return m_index;
        }
        explicit operator bool() const noexcept
        {
            return m_handle && m_index;
        }
    private:
        void* m_handle = nullptr;
        uint32_t m_index = 0;
    };
    class remote_library
    {
    public:
        error_type open(const process& process, const string_view path) noexcept;
        error_type close() noexcept;
        error_type call(const char* const name, const string_view args, error_type& error) noexcept;
    private:
        void* m_process = nullptr;
        void* m_handle = nullptr;
        array<wchar_t> m_wpath;
    };
}
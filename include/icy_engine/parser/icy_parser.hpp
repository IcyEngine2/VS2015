#pragma once

#include <icy_engine/core/icy_string.hpp>
#include <icy_engine/core/icy_map.hpp>
#include <icy_engine/core/icy_file.hpp>


namespace icy
{
    static const auto default_parser_capacity = 0x10000;
    class file;
    class parser_base
    {
    public: 
        parser_base(const size_t max_string = default_parser_capacity) noexcept : m_capacity(max_string)
        {

        }
        explicit operator bool() const noexcept
        {
            return !m_stop;
        }
        size_t offset() const noexcept
        {
            return m_offset;
        }
        void stop() noexcept
        {
            m_stop = true;
        }
        virtual error_type operator()(const const_array_view<char> buffer) noexcept = 0;
    protected:
        const size_t m_capacity;
        size_t m_offset = 0;
        bool m_stop = false;
        array<char> m_buffer;
    };

    inline error_type parse(parser_base& parser, file& file, const size_t buffer) noexcept
    {
        array<char> bytes;
        ICY_ERROR(bytes.resize(buffer));
        while (parser)
        {
            auto size = 0_z;
            ICY_ERROR(file.read(bytes.data(), size));
            ICY_ERROR(parser({ bytes.data(), size }));
        }
        return {};
    }
    inline error_type parse(parser_base& parser, const string_view file_path, const size_t buffer) noexcept
    {
        file input;
        ICY_ERROR(input.open(file_path, file_access::read, file_open::open_existing, file_share::read));
        return parse(parser, input, buffer);
    }

}
#pragma once

#include "icy_parser.hpp"

namespace icy
{
    class csv_parser : public parser_base
    {
    public:
        csv_parser(const char delim = '\t', const size_t max_string = default_parser_capacity) noexcept : parser_base(max_string), m_delim(delim)
        {

        }
        error_type operator()(const const_array_view<char> buffer) noexcept override;
        virtual error_type callback(const const_array_view<string_view> tabs) noexcept = 0;
    private:
        enum class state
        {
            build_tabs,
            wait_for_endl_1,
        };
    private:
        const char m_delim;
        state m_state = state(0);
    };

    inline error_type csv_parser::operator()(const const_array_view<char> buffer) noexcept
    {
        static constexpr auto csv_chr_endl_0 = '\r';
        static constexpr auto csv_chr_endl_1 = '\n';

        const auto process = [this]
        {
            array<string_view> tabs;
            auto beg = m_buffer.data();
            for (auto&& chr : m_buffer)
            {
                if (chr == m_delim)
                {
                    ICY_ERROR(tabs.push_back(string_view(beg, &chr)));
                    beg = &chr + 1;
                }
            }
            ICY_ERROR(tabs.push_back(string_view(beg, m_buffer.data() + m_buffer.size())));
            ICY_ERROR(callback(tabs));
            m_buffer.clear();
            return error_type();
        };
        for (auto&& chr : buffer)
        {
            switch (m_state)
            {
            case state::build_tabs:
                if (chr == csv_chr_endl_0)
                {
                    ICY_ERROR(process());
                    m_state = state::wait_for_endl_1;
                }
                else if (chr == csv_chr_endl_1)
                {
                    ICY_ERROR(process());
                    m_state = state::build_tabs;
                }
                else
                {
                    if (m_buffer.size() <= m_capacity)
                        ICY_ERROR(m_buffer.push_back(chr));
                }
                break;

            case state::wait_for_endl_1:
                if (chr == csv_chr_endl_1)
                {
                    ;
                }
                else if (chr == csv_chr_endl_0)
                {
                    ICY_ERROR(process());
                }
                else
                {
                    if (m_buffer.size() <= m_capacity)
                        ICY_ERROR(m_buffer.push_back(chr));
                }
                m_state = state::build_tabs;
                break;

            default:
                ICY_ASSERT(false, "INVALID CSV PARSER STATE");
            }
            ++m_offset;
        }
        if (buffer.empty() && m_offset)
            ICY_ERROR(process());
        if (buffer.empty())
            m_stop = true;
        return {};
    }

}
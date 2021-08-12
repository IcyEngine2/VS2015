#pragma once

#include "icy_parser.hpp"
#include <cctype>

namespace icy
{
    class cmd_parser : public parser_base
    {
    public:
        error_type operator()(const const_array_view<char> buffer) noexcept;
        string_view cmd() const noexcept
        {
            return m_cmd;
        }
        const map<string, string>& args() const noexcept
        {
            return m_args;
        }
    private:
        enum class state : uint32_t
        {
            find_cmd,
            find_key,
            find_val,
            build_cmd,
            build_key,
            build_val_plain,
            build_val_quote,
        };
    private:
        state m_state = state(0);
        string m_cmd;
        string m_key;
        map<string, string> m_args;
    };
    inline error_type cmd_parser::operator()(const const_array_view<char> buffer) noexcept
    {
        static constexpr auto cmd_chr_quote = '\"';
        static constexpr auto cmd_chr_assign = '=';

        const auto save_arg = [this]() -> error_type
        {
            m_state = state::find_key;
            string val;
            ICY_ERROR(to_string(string_view(m_buffer.data(), m_buffer.size()), val));
            m_buffer.clear();
            auto it = m_args.find(m_key);
            if (it == m_args.end())
            {
                ICY_ERROR(m_args.insert(std::move(m_key), string(), &it));
            }
            it->value = std::move(val);
            return error_type();
        };
        const auto save_cmd = [this]() -> error_type
        {
            m_state = state::find_key;
            ICY_ERROR(to_string(string_view(m_buffer.data(), m_buffer.size()), m_cmd));
            m_buffer.clear();
            return error_type();
        };

        for (auto&& chr : buffer)
        {
            const auto is_space = chr > 0 && isspace(chr);
            switch (m_state)
            {
            case state::find_cmd:
                if (is_space)
                    break;

                m_state = state::build_cmd;
                ICY_ERROR(m_buffer.push_back(chr));
                break;

            case state::find_key:
                if (!is_space)
                {
                    m_state = state::build_key;
                    ICY_ERROR(m_buffer.push_back(chr));
                }
                break;

            case state::find_val:
                if (chr == cmd_chr_quote)
                {
                    m_state = state::build_val_quote;
                }
                else if (!is_space)
                {
                    m_state = state::build_val_plain;
                    ICY_ERROR(m_buffer.push_back(chr));
                }
                else
                {
                    return make_stdlib_error(std::errc::illegal_byte_sequence);
                }
                break;

            case state::build_cmd:
                if (is_space)
                {
                    ICY_ERROR(save_cmd());
                }
                else
                {
                    ICY_ERROR(m_buffer.push_back(chr));
                }
                break;

            case state::build_key:
                if (is_space)
                {
                    return make_stdlib_error(std::errc::illegal_byte_sequence);
                }
                else
                {
                    if (chr == cmd_chr_assign)
                    {
                        m_state = state::find_val;
                        ICY_ERROR(to_string(string_view(m_buffer.data(), m_buffer.size()), m_key));
                        m_buffer.clear();
                    }
                    else
                    {
                        ICY_ERROR(m_buffer.push_back(chr));
                    }
                }
                break;

            case state::build_val_plain:
            case state::build_val_quote:
                if (m_state == state::build_val_plain && is_space ||
                    m_state == state::build_val_quote && chr == cmd_chr_quote)
                {
                    ICY_ERROR(save_arg());
                }
                else
                {
                    ICY_ERROR(m_buffer.push_back(chr));
                }
                break;
            }
        }
        if (buffer.empty())
        {
            if (m_state == state::find_key || m_state == state::find_cmd)
            {
                ;
            }
            else if (m_state == state::build_cmd)
            {
                ICY_ERROR(save_cmd());
            }
            else if (m_state == state::build_val_plain)
            {
                ICY_ERROR(save_arg());
            }
            else
            {
                return make_stdlib_error(std::errc::illegal_byte_sequence);
            }
            m_stop = true;
        }
        return error_type();
    }
}
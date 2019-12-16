#pragma once

#include "icy_parser.hpp"

namespace icy
{
    enum class xml_error_code : uint32_t
    {
        success,
        string_overflow,    //  max node length reached
        invalid_header,     //  expected "<?xml version="1.0" encoding="UTF-8"?>
        invalid_tag_beg,    //  expected whitespace or <   
        invalid_tag_end,    //  expected >
        invalid_node_end,   //  expected </previous_name> 
        invalid_name,       //  expected node name
        invalid_key,        //  expected key name
        invalid_assign,     //  expected =
        invalid_value,      //  expected "abc1234" (quoted text)
        unexpected_eof,     //  root node not closed
    };
    extern const error_source error_source_xml_parser;
    inline error_type make_xml_error(const xml_error_code code) noexcept
    {
        return error_type(unsigned(code), error_source_xml_parser);
    }

    struct xml_node
    {
        const xml_node* parent = nullptr;
        string name;
        string value;
        map<string, string> attributes;
        bool done = false;
    };
    class xml_parser : public parser_base
    {
    public:
        error_type operator()(const const_array_view<char> buffer) noexcept override;
        virtual error_type callback(const xml_node& node) noexcept = 0;
    private:
        enum class state : uint32_t
        {
            none,
            skip_to_key,
            skip_to_assign,
            skip_to_value,
            build_name,
            build_header,
            build_key,
            build_value,
            build_end,
            wait_end,
        };
    private:
        state m_state = state(0);
        array<unique_ptr<xml_node>> m_nodes;
        array<char> m_key;
    };

    static error_type xml_error_to_string(const unsigned code, const string_view, string& str) noexcept
    {
        string_view msg;
        switch (static_cast<xml_error_code>(code))
        {
        case xml_error_code::string_overflow:
            msg = "max <node> string length reached";
            break;

        case xml_error_code::invalid_header:
            msg = "expected header <?xml version='1.0' encoding='UTF-8'?>";
            break;

        case xml_error_code::invalid_tag_beg:
            msg = "invalid tag begin: expected '<'";
            break;

        case xml_error_code::invalid_tag_end:
            msg = "invalid tag end: expected '>'";
            break;

        case xml_error_code::invalid_node_end:
            msg = "invalid node end: expected </previous node name>";
            break;

        case xml_error_code::invalid_name:
            msg = "invalid <node> structure: expected string name";
            break;

        case xml_error_code::invalid_key:
            msg = "invalid attribute string: expected key (name)";
            break;

        case xml_error_code::invalid_assign:
            msg = "invalid attribute string: expected assign '='";
            break;

        case xml_error_code::invalid_value:
            msg = "invalid attribute string: expected \"value\"";
            break;

        case xml_error_code::unexpected_eof:
            msg = "unexpected end of stream: <root> node not closed";
            break;

        default:
            ICY_ERROR(icy::to_string("Unknown error code"_s, str));
            return make_stdlib_error(std::errc::invalid_argument);
        }
        return icy::to_string(msg, str);
    }
    
    inline error_type xml_parser::operator()(const const_array_view<char> buffer) noexcept
    {
        static constexpr auto xml_chr_node_beg = '<';
        static constexpr auto xml_chr_node_end = '>';
        static constexpr auto xml_chr_node_fin = '/';
        static constexpr auto xml_chr_header_beg = '?';
        static constexpr auto xml_chr_assign = '=';
        static constexpr auto xml_chr_quote = '\"';
        static constexpr auto xml_default_header_0 = "?xml version=\'1.0\' encoding=\'UTF-8\'?"_s;
        static constexpr auto xml_default_header_1 = "?xml version=\"1.0\" encoding=\"UTF-8\"?"_s;

        const auto str_buffer = string_view(buffer.data(), buffer.size());
        const auto is_name_start_char = [](const char32_t chr) noexcept
        {
            return false
                || chr == ':'
                || chr == '_'
                || chr >= 'A' && chr <= 'Z'
                || chr >= 'a' && chr <= 'z'
                || chr >= 0xC0 && chr <= 0xD6
                || chr >= 0xD8 && chr <= 0xF6
                || chr >= 0xF8 && chr <= 0x2FF
                || chr >= 0x370 && chr <= 0x37D
                || chr >= 0x37F && chr <= 0x1FFF
                || chr >= 0x200C && chr <= 0x200D
                || chr >= 0x2070 && chr <= 0x218F
                || chr >= 0x2C00 && chr <= 0x2FEF
                || chr >= 0x3001 && chr <= 0xD7FF
                || chr >= 0xF900 && chr <= 0xFDCF
                || chr >= 0xFDF0 && chr <= 0xFFFD
                || chr >= 0x10000 && chr <= 0xEFFFF;
        };
        const auto is_name_char = [is_name_start_char](const char32_t chr) noexcept
        {
            return is_name_start_char(chr)
                || chr == '-'
                || chr == '.'
                || chr >= '0' && chr <= '9'
                || chr == 0xB7
                || chr >= 0x0300 && chr <= 0x036F
                || chr >= 0x203F && chr <= 0x2040;
        };
        const auto is_space = [](const char chr)
        {
            switch (chr)
            {
            case ' ':
            case '\t':
            case '\r':
            case '\n':
            case '\f':
            case '\v':
                return true;
            default:
                return false;
            }
        };
        const auto reset = [this](const state state)
        {
            m_state = state;
            m_buffer.clear();
        };

        for (auto&& chr : buffer)
        {
            if (m_stop)
                return {};

            const auto str_mbuffer = string_view(m_buffer.data(), m_buffer.size());
            const auto validate_name = [this, is_name_start_char, is_name_char, str_mbuffer](const xml_error_code code)
            {
                auto name = str_mbuffer;
                for (auto it = name.begin(); it != name.end(); ++it)
                {
                    char32_t utf8 = 0;
                    ICY_ERROR(it.to_char(utf8));
                    if (it == name.begin())
                    {
                        if (!is_name_start_char(utf8))
                            return make_xml_error(code);
                    }
                    else
                    {
                        if (!is_name_char(utf8))
                            return make_xml_error(code);
                    }
                }
                return error_type();
            };
            const auto push_back = [this, chr]()
            {
                if (m_buffer.size() >= m_capacity)
                    return make_xml_error(xml_error_code::string_overflow);
                ICY_ERROR(m_buffer.push_back(chr));
                return error_type();
            };

            switch (m_state)
            {
            case state::none:
                if (chr == xml_chr_node_beg)
                {
                    while (!m_buffer.empty() && is_space(m_buffer.back()))
                        m_buffer.pop_back();

                    if (!m_nodes.empty())
                        ICY_ERROR(to_string(string_view(m_buffer.data(), m_buffer.size()), m_nodes.back()->value));

                    reset(state::build_name);
                }
                else if (!m_nodes.empty())
                {
                    if (is_space(chr) && m_buffer.empty())
                        break;
                    ICY_ERROR(push_back());
                }
                else if (!is_space(chr))
                {
                    return make_xml_error(xml_error_code::invalid_tag_beg);
                }
                break;

            case state::skip_to_key:
                if (chr == xml_chr_node_fin)
                {
                    reset(state::wait_end);
                }
                else if (chr == xml_chr_node_end)
                {
                    if (m_nodes.empty())
                        return make_unexpected_error();
                    ICY_ERROR(callback(*m_nodes.back()));
                    reset(state::none);
                }
                else if (!is_space(chr))
                {
                    reset(state::build_key);
                    ICY_ERROR(push_back());
                }
                break;

            case state::skip_to_assign:
                if (chr == xml_chr_assign)
                    reset(state::skip_to_value);
                else if (!is_space(chr))
                    return make_xml_error(xml_error_code::invalid_assign);
                break;

            case state::skip_to_value:
                if (chr == xml_chr_quote)
                    reset(state::build_value);
                else if (!is_space(chr))
                    return make_xml_error(xml_error_code::invalid_value);
                break;

            case state::build_name:
                if (chr == xml_chr_header_beg)
                {
                    reset(state::build_header);
                    push_back();
                }
                else if (chr == xml_chr_node_fin && m_buffer.empty())
                {
                    reset(state::build_end);
                }
                else if (is_space(chr) || chr == xml_chr_node_end || chr == xml_chr_node_fin)
                {
                    if (m_buffer.empty())
                        return make_xml_error(xml_error_code::invalid_name);

                    ICY_ERROR(validate_name(xml_error_code::invalid_name));
                    auto new_node = make_unique<xml_node>();
                    if (!new_node)
                        return make_stdlib_error(std::errc::not_enough_memory);
                    if (!m_nodes.empty())
                        new_node->parent = m_nodes.back().get();

                    ICY_ERROR(to_string(str_mbuffer, new_node->name));
                    ICY_ERROR(m_nodes.push_back(std::move(new_node)));

                    if (chr == xml_chr_node_end)
                    {
                        ICY_ERROR(callback(*m_nodes.back()));
                        reset(state::none);
                    }
                    else if (chr == xml_chr_node_fin)
                    {
                        reset(state::wait_end);
                    }
                    else
                    {
                        reset(state::skip_to_key);
                    }
                }
                else
                {
                    ICY_ERROR(push_back());
                }
                break;

            case state::build_header:
                if (chr == xml_chr_node_end)
                {
                    if (str_mbuffer == xml_default_header_0 ||
                        str_mbuffer == xml_default_header_1)
                    {
                        reset(state::none);
                    }
                    else
                    {
                        return make_xml_error(xml_error_code::invalid_header);
                    }
                }
                else
                {
                    ICY_ERROR(push_back());
                }
                break;

            case state::build_key:
                if (is_space(chr) || chr == xml_chr_assign)
                {
                    ICY_ERROR(validate_name(xml_error_code::invalid_key));
                    m_key = std::move(m_buffer);
                    reset(chr == xml_chr_assign ? state::skip_to_value : state::skip_to_assign);
                }
                else
                {
                    ICY_ERROR(push_back());
                }
                break;

            case state::build_value:
                if (chr == xml_chr_quote)
                {
                    if (m_nodes.empty())
                        return make_unexpected_error();

                    ICY_ERROR(emplace(m_nodes.back()->attributes, string_view(m_key.data(), m_key.size()), str_mbuffer));
                    reset(state::skip_to_key);
                }
                else
                {
                    ICY_ERROR(push_back());
                }
                break;

            case state::build_end:
                if (chr == xml_chr_node_end)
                {
                    if (m_nodes.empty())
                        return make_unexpected_error();

                    if (str_mbuffer != m_nodes.back()->name)
                        return make_xml_error(xml_error_code::invalid_node_end);

                    m_nodes.back()->done = true;
                    ICY_ERROR(callback(*m_nodes.back()));
                    m_nodes.pop_back();
                    reset(state::none);
                }
                else if (is_space(chr))
                {
                    return make_xml_error(xml_error_code::invalid_node_end);
                }
                else
                {
                    ICY_ERROR(push_back());
                }
                break;

            case state::wait_end:
                if (chr == xml_chr_node_end)
                {
                    if (m_nodes.empty())
                        return make_unexpected_error();

                    m_nodes.back()->done = true;
                    ICY_ERROR(callback(*m_nodes.back()));
                    m_nodes.pop_back();
                    reset(state::none);
                }
                else
                    return make_xml_error(xml_error_code::invalid_tag_end);
                break;

            default:
                ICY_ASSERT(false, "INVALID STATE VALUE");
            }
            ++m_offset;
        }
        if (buffer.empty())
        {
            if (!m_nodes.empty())
                return make_xml_error(xml_error_code::unexpected_eof);
        }
        if (buffer.empty())
            m_stop = true;
        return {};
    }
}
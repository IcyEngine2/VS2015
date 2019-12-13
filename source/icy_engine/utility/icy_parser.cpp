#include <icy_engine/utility/icy_parser.hpp>
#include <icy_engine/core/icy_file.hpp>
#include <cctype>
using namespace icy;

enum class xml_parser::state : uint32_t
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
enum class csv_parser::state : uint32_t
{
    build_tabs,
    wait_for_endl_1,
};
enum class cmd_parser::state : uint32_t
{
    find_cmd,       
    find_key,
    find_val,
    build_cmd,
    build_key,
    build_val_plain,
    build_val_quote,
};
static constexpr auto xml_chr_node_beg = '<';
static constexpr auto xml_chr_node_end = '>';
static constexpr auto xml_chr_node_fin = '/';
static constexpr auto xml_chr_header_beg = '?';
static constexpr auto xml_chr_assign = '=';
static constexpr auto xml_chr_quote = '\"';
static constexpr auto xml_default_header_0 = "?xml version=\'1.0\' encoding=\'UTF-8\'?"_s;
static constexpr auto xml_default_header_1 = "?xml version=\"1.0\" encoding=\"UTF-8\"?"_s;

static constexpr auto csv_chr_endl_0 = '\r';
static constexpr auto csv_chr_endl_1 = '\n';

static constexpr auto cmd_chr_quote = '\"';
static constexpr auto cmd_chr_assign = '=';

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
static error_type osm_error_to_string(const unsigned code, const string_view, string& str) noexcept
{
    string_view msg;
    switch (static_cast<osm_error_code>(code))
    {
    case osm_error_code::index_not_found:
        msg = "index not found"_s;
        break;

    case osm_error_code::index_invalid_format:
        msg = "index is not a valid integer"_s;
        break;

    case osm_error_code::index_is_not_positive:
        msg = "index is not positive"_s;
        break;

    case osm_error_code::invalid_tag_parent:
        msg = "invalid <tag> parent"_s;
        break;

    case osm_error_code::invalid_tag_name:
        msg = "invalid <tag> name"_s;
        break;

    case osm_error_code::tag_key_not_found:
        msg = "<tag> key 'k' not found"_s;
        break;

    case osm_error_code::tag_val_not_found:
        msg = "<tag> value 'v' not found"_s;
        break;

    case osm_error_code::member_type_not_found:
        msg = "<member> 'type' not found"_s;
        break;

    case osm_error_code::member_type_unknown:
        msg = "member 'type' unknown"_s;
        break;
    }
    return icy::to_string(msg, str);
}

error_type icy::parse(parser_base& parser, file& file, const size_t buffer) noexcept
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
error_type icy::parse(parser_base& parser, const string_view file_path, const size_t buffer) noexcept
{
    file input;
    ICY_ERROR(input.open(file_path, file_access::read, file_open::open_existing, file_share::read));
    return parse(parser, input, buffer);
}

error_type xml_parser::operator()(const const_array_view<char> buffer) noexcept
{
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
error_type csv_parser::operator()(const const_array_view<char> buffer) noexcept
{
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
error_type osm_parser::callback(const xml_node& node) noexcept
{
    const auto get_type = [](const string_view str)
    {
        switch (hash(str))
        {
        case "node"_hash: return osm_type::node;
        case "way"_hash: return osm_type::way; 
        case "relation"_hash:
        case "rel"_hash: return osm_type::rel;
        }
        return osm_type::null;
    };
    const auto check_index = [](const string* ptr, osm_index& value)
    {
        if (!ptr)
            return make_osm_error(osm_error_code::index_not_found);

        auto index = 0i64;
        if (ptr->to_value(index))
            return make_osm_error(osm_error_code::index_invalid_format);

        if (index <= 0)
            return make_osm_error(osm_error_code::index_is_not_positive);

        value.index = uint64_t(index);
        return error_type();
    };
    const auto reset = [this]()
    {
        m_root = nullptr;
        m_base.type = 0;
        m_base.refs.clear();
        m_base.tags.clear();
        m_base.roles.clear();
    };

    if (&node == m_root && node.done)
    {
        ICY_ERROR(callback(m_base));
        reset();
        return {};
    }

    const auto type = get_type(node.name);   

    if (type)
    {
        ICY_ERROR(check_index(node.attributes.try_find("id"_s), m_base));
        reset();
        m_root = &node;
        m_base.type = type;
        for (auto&& pair : node.attributes)
            ICY_ERROR(emplace(m_base.tags, pair.key, pair.value));
        if (node.done)
        {
            ICY_ERROR(callback(m_base));
            reset();
            return {};
        }
    }
    else
    {
        if (node.parent != m_root)
            return make_osm_error(osm_error_code::invalid_tag_parent);
    }

    if (node.name == "tag"_s)
    {
        const auto str_key = node.attributes.try_find("k"_s);
        const auto str_val = node.attributes.try_find("v"_s);
        if (!str_key)
            return make_osm_error(osm_error_code::tag_key_not_found);
        if (!str_val)
            return make_osm_error(osm_error_code::tag_val_not_found);
        ICY_ERROR(emplace(m_base.tags, *str_key, *str_val));
    }
    else if (node.name == "nd"_s)
    {
        osm_index new_index;
        new_index.type = osm_type::node;
        ICY_ERROR(check_index(node.attributes.try_find("ref"_s), new_index));
        ICY_ERROR(m_base.refs.push_back(new_index));
    }
    else if (node.name == "member"_s)
    {
        const auto str_type = node.attributes.try_find("type"_s);
        if (!str_type)
            return make_osm_error(osm_error_code::member_type_not_found);

        osm_index new_index;
        new_index.type = get_type(*str_type);
        if (!new_index.type)
            return make_osm_error(osm_error_code::member_type_unknown);

        ICY_ERROR(check_index(node.attributes.try_find("ref"_s), new_index));
        ICY_ERROR(m_base.refs.push_back(new_index));
        const auto str_role = node.attributes.try_find("role"_s);
        if (str_role)
        {
            string new_role;
            ICY_ERROR(to_string(*str_role, new_role));
            ICY_ERROR(m_base.roles.push_back(std::move(new_role)));
        }
    }
    else if (node.name == "osm"_s)
    {
        //  skip
    }
    else if (!type)
    {
        return make_osm_error(osm_error_code::invalid_tag_name);
    }
    return {};
}
error_type cmd_parser::operator()(const const_array_view<char> buffer) noexcept
{
    const auto save_arg = [this]() -> error_type
    {
        m_state = state::find_key;
        string val;
        ICY_ERROR(to_string(string_view(m_buffer.data(), m_buffer.size()), val));
        m_buffer.clear();
        ICY_ERROR(m_args.find_or_insert(std::move(m_key), std::move(val)));
        return {};
    };
    const auto save_cmd = [this]() -> error_type
    {
        m_state = state::find_key;
        ICY_ERROR(to_string(string_view(m_buffer.data(), m_buffer.size()), m_cmd));
        m_buffer.clear();
        return {};
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
    return {};
}

const error_source icy::error_source_xml_parser = register_error_source("xml"_s, xml_error_to_string);
const error_source icy::error_source_osm_parser = register_error_source("osm"_s, osm_error_to_string);
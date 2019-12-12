#pragma once

#include <icy_engine/core/icy_string.hpp>
#include <icy_engine/core/icy_map.hpp>

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
    error_type parse(parser_base& parser, file& file, const size_t buffer = default_parser_capacity) noexcept;
    error_type parse(parser_base& parser, const string_view file_path, const size_t buffer = default_parser_capacity) noexcept;
}

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
        enum class state : uint32_t;
    private:
        state m_state = state(0);
        array<unique_ptr<xml_node>> m_nodes;
        array<char> m_key;
    };
}

namespace icy
{
    class csv_parser : public parser_base
    {
    public: 
        csv_parser(const char delim = '\t', const size_t max_string = default_parser_capacity) noexcept : parser_base(max_string), m_delim(delim)
        {

        }
        error_type operator()(const const_array_view<char> buffer) noexcept override;
        virtual error_type callback(const array_view<string_view> tabs) noexcept = 0;
    private:
        enum class state : uint32_t;
    private:
        const char m_delim;
        state m_state = state(0);
    };
}

namespace icy
{
    enum class osm_error_code : uint32_t
    {
        success,
        index_not_found,
        index_invalid_format,
        index_is_not_positive,
        invalid_tag_parent,
        invalid_tag_name,
        tag_key_not_found,
        tag_val_not_found,
        member_type_not_found,
        member_type_unknown,
    };
    extern const error_source error_source_osm_parser;
    inline error_type make_osm_error(const osm_error_code code) noexcept
    {
        return error_type(unsigned(code), error_source_osm_parser);
    }

    struct osm_type_enum
    {
        enum
        {
            null,
            node,
            way,
            rel,
            _count,
        };
    };
    using osm_type = decltype(osm_type_enum::null);
    struct osm_index
    {
        osm_index() : index(0), type(osm_type::null)
        {

        }
        uint64_t index  : 0x3E;
        uint64_t type   : 0x02;
    };
    struct osm_node : public osm_index
    {        
        map<string, string> tags;
    };
    struct osm_way : public osm_node
    {
        array<osm_index> refs;
    };
    struct osm_rel : public osm_way
    {
        array<string> roles;
    };
    class osm_parser : public xml_parser
    {
    public:
        virtual error_type callback(const osm_node& base) noexcept = 0;
    private:
        error_type callback(const xml_node& node) noexcept override final;
    private:
        osm_rel m_base;
        const xml_node* m_root = nullptr;
    };
}

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
        enum class state : uint32_t;
    private:
        state m_state = state(0);
        string m_cmd;
        string m_key;
        map<string, string> m_args;
    };
}
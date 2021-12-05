#include <icy_engine/core/icy_file.hpp>
#include <icy_engine/core/icy_memory.hpp>
#include <icy_engine/core/icy_string.hpp>
#include <icy_engine/core/icy_array.hpp>
#include <icy_engine/core/icy_console.hpp>
#include <icy_engine/core/icy_thread.hpp>

using namespace icy;

#if _DEBUG
#pragma comment(lib, "icy_engine_cored")
#else
#pragma comment(lib, "icy_engine_core")
#endif
//const auto max_region = 100;
const auto max_xml_length = 1500;
const auto max_str_length = 500;

shared_ptr<console_system> con;

struct house_type1
{
    friend int compare(const house_type1& lhs, const house_type1& rhs) noexcept
    {
        return icy::compare(lhs.key, rhs.key);
    }
    rel_ops(house_type1);
    uint32_t key;
    string text;
};
struct addr_type2
{
    friend int compare(const addr_type2& lhs, const addr_type2& rhs) noexcept
    {
        return icy::compare(lhs.key, rhs.key);
    }
    rel_ops(addr_type2);
    uint32_t key;
    uint32_t par;
    uint32_t level;
    string text;
};
struct house_type2
{
    friend int compare(const house_type2& lhs, const house_type2& rhs) noexcept
    {
        return icy::compare(lhs.key, rhs.key);
    }
    rel_ops(house_type2);
    uint32_t key;
    uint32_t par;
    char text[28];
};

error_type console_write(const string_view msg)
{
    return con->write(msg);   
    
}

error_type parse_houses(const size_t region, const string_view path_input, const string_view dir_output)
{
    file input;
    ICY_ERROR(input.open(path_input, file_access::read, file_open::open_existing, file_share::read));

    file output;
    bool first = {};

    string str_output;
    ICY_ERROR(to_string("%1%2.txt"_s, str_output, dir_output, region));
    ICY_ERROR(output.open(str_output, file_access::app, file_open::create_always, file_share::read));
    first = true;

    auto now = clock_type::now();
    auto offset = 0ui64;
    auto done = false;
    array<char> xml;

    const auto max_file = input.info().size;
    auto old_percent = 0ui64;
    auto new_percent = 0ui64;

    const auto log = [](const uint64_t pos)
    {
        string msg;
        ICY_ERROR(to_string("%1%2"_s, msg, "\r\nInvalid <XML> at offset: ", pos));
        ICY_ERROR(console_write(msg));
        return error_type{};
    };

    while (true)
    {
        char buffer[0x4000];
        auto size = _countof(buffer);
        ICY_ERROR(input.read(buffer, size));

        if (!size)
            break;

        auto k = 0u;
        while (k < size)
        {
            for (; k < size; ++k)
            {
                if (xml.empty())
                {
                    if (buffer[k] == '<')
                        ICY_ERROR(xml.push_back(buffer[k]));
                }
                else
                {
                    if (buffer[k] == '>')
                    {
                        if (xml.back() == '/')
                        {
                            ICY_ERROR(xml.push_back('\0'));
                            if (strstr(xml.data(), "House") == xml.data() + 1)
                            {
                                done = true;
                                break;
                            }
                        }
                        xml.clear();
                    }
                    else if (xml.size() < max_xml_length)
                    {
                        xml.push_back(buffer[k]);
                    }
                    else
                    {
                        ICY_ERROR(log(offset + k));
                        xml.clear();
                    }
                }
            }
            if (done)
            {
                const auto find = [&xml](const char* tag)
                {
                    auto beg = strstr(xml.data(), tag);
                    if (beg)
                        beg += strlen(tag);

                    for (auto ptr = beg; ptr && *ptr; ++ptr)
                    {
                        if (*ptr == '\"')
                        {
                            string_view tmp;
                            to_string(const_array_view<char>(beg, ptr), tmp);
                            return tmp;
                        }
                    }
                    return string_view();
                };

                const auto guid = find("HOUSEGUID=\"");
                const auto name = find("HOUSENUM=\"");
                const auto region = find("REGIONCODE=\"");
                const auto aoguid = find("AOGUID=\"");
                const auto struct_stat = find("STRSTATUS=\"");
                const auto struct_name = find("STRUCNUM=\"");
                const auto build_name = find("BUILDNUM=\"");

                if (!guid.empty() && !region.empty() && !name.empty() && !aoguid.empty())
                {
                    auto stat = 0u;
                    auto id_region = 0u;
                    to_value(struct_stat, stat);
                    to_value(region, id_region);
                    if (id_region > 0 && id_region < max_region)
                    {
                        string full_name;
                        ICY_ERROR(full_name.append(name));
                        if (!build_name.empty())
                        {
                            ICY_ERROR(full_name.append("("_s));
                            ICY_ERROR(full_name.append(build_name));
                            ICY_ERROR(full_name.append(")"_s));
                        }
                        if (stat == 2)
                        {
                            ICY_ERROR(full_name.append(u8" литера "_s));
                            ICY_ERROR(full_name.append(struct_name));
                        }
                        else if (stat == 1)
                        {
                            ICY_ERROR(full_name.append(u8" стр "_s));
                            ICY_ERROR(full_name.append(struct_name));
                        }

                        string msg;
                        ICY_ERROR(to_string("%1%2\t%3\t%4"_s, msg, first[id_region] ? ""_s : "\r\n"_s,
                            hash(guid), hash(aoguid), string_view(full_name)));
                        ICY_ERROR(output[id_region].append(msg.bytes().data(), msg.bytes().size()));
                        first[id_region] = false;
                    }
                }
                else if (!name.empty())
                {
                    ICY_ERROR(log(offset + k));
                }
                xml.clear();
                done = false;
            }
        }
        offset += k;

        new_percent = offset * 10000ui64 / max_file;
        if (new_percent > old_percent)
        {
            string msg;
            ICY_ERROR(to_string("\r\nProgress: %1.%2%%..."_s, msg, new_percent / 100, new_percent % 100));
            ICY_ERROR(console_write(msg));
            old_percent = new_percent;
        }
    }
    const auto time = clock_type::now() - now;
    string msg;
    ICY_ERROR(to_string("\r\nParse done after [%1] ms."_s, msg, std::chrono::duration_cast<std::chrono::milliseconds>(time).count()));
    ICY_ERROR(console_write(msg));
    return {};
}
error_type parse_addrobj(const size_t region, const string_view path_addrobj, const string_view dir_output)
{
    file input;
    ICY_ERROR(input.open(path_addrobj, file_access::read, file_open::open_existing, file_share::read));

    file output;
    bool first = true;

    string str_output;
    ICY_ERROR(to_string("%1%2.txt"_s, str_output, dir_output, region));
    ICY_ERROR(output.open(str_output, file_access::app, file_open::create_always, file_share::read));

    auto now = clock_type::now();
    auto offset = 0ui64;
    auto done = false;
    array<char> xml;

    const auto max_file = input.info().size;
    auto old_percent = 0ui64;
    auto new_percent = 0ui64;

    const auto log = [](const uint64_t pos)
    {
        string msg;
        ICY_ERROR(to_string("%1%2"_s, msg, "\r\nInvalid <XML> at offset: ", pos));
        ICY_ERROR(console_write(msg));
        return error_type{};
    };


    while (true)
    {
        char buffer[0x4000];
        auto size = _countof(buffer);
        ICY_ERROR(input.read(buffer, size));

        if (!size)
            break;

        auto k = 0u;
        while (k < size)
        {
            for (; k < size; ++k)
            {
                if (xml.empty())
                {
                    if (buffer[k] == '<')
                        ICY_ERROR(xml.push_back(buffer[k]));
                }
                else
                {
                    if (buffer[k] == '>')
                    {
                        if (xml.back() == '/')
                        {
                            ICY_ERROR(xml.push_back('\0'));
                            if (strstr(xml.data(), "OBJECT") == xml.data() + 1)
                            {
                                done = true;
                                break;
                            }
                        }
                        xml.clear();
                    }
                    else if (xml.size() < max_xml_length)
                    {
                        xml.push_back(buffer[k]);
                    }
                    else
                    {
                        ICY_ERROR(log(offset + k));
                        xml.clear();
                    }
                }
            }
            if (done)
            {
                const auto find = [&xml](const char* tag)
                {
                    auto beg = strstr(xml.data(), tag);
                    if (beg)
                        beg += strlen(tag);

                    for (auto ptr = beg; ptr && *ptr; ++ptr)
                    {
                        if (*ptr == '\"')
                        {
                            string_view tmp;
                            to_string(const_array_view<char>(beg, ptr), tmp);
                            return tmp;
                        }
                    }
                    return string_view();
                };

                const auto guid = find("AOGUID=\"");
                const auto full_name = find("FORMALNAME=\"");
                const auto shrt_name = find("SHORTNAME=\"");
                const auto region = find("REGIONCODE=\"");
                const auto parent = find("PARENTGUID=\"");
                const auto level = find("AOLEVEL=\"");

                if (!guid.empty() && !region.empty() && !level.empty() && !full_name.empty() && !shrt_name.empty())
                {
                    auto id_region = 0u;
                    auto level_value = 0u;
                    const auto level_street = 7;
                    to_value(region, id_region);
                    to_value(level, level_value);

                    if (id_region > 0 && id_region < max_region && level_value >= 1 && level_value <= level_street)
                    {
                        string msg;
                        ICY_ERROR(to_string("%1%2\t%3\t%4\t%5%6%7\t%8\t%9"_s, msg, first[id_region] ? ""_s : "\r\n"_s, 
                            hash(guid), hash(parent), level_value, full_name, level_value == level_street ? " "_s : ""_s, 
                            level_value == level_street ? shrt_name : ""_s, guid, parent));
                        first[id_region] = false;
                        ICY_ERROR(output[id_region].append(msg.bytes().data(), msg.bytes().size()));
                    }
                }
                else
                {
                    ICY_ERROR(log(offset + k));
                }
                xml.clear();
                done = false;
            }
        }
        offset += k;

        new_percent = offset * 1000ui64 / max_file;
        if (new_percent > old_percent)
        {
            string msg;
            ICY_ERROR(to_string("\r\nProgress: %1.%2%%..."_s, msg, new_percent / 10, new_percent % 10));
            ICY_ERROR(console_write(msg));
            old_percent = new_percent;
        }
    }
    const auto time = clock_type::now() - now;
    string msg;
    ICY_ERROR(to_string("\r\nParse done after [%1] ms."_s, msg, std::chrono::duration_cast<std::chrono::milliseconds>(time).count()));
    ICY_ERROR(console_write(msg));
    return {};
}
error_type remove_duplicate_houses(const string_view path_house, const string_view dir_output, const uint32_t n)
{

    auto now = clock_type::now();

    {
        string file_name_house;
        string file_name_output;
        ICY_ERROR(to_string("%1%2.txt"_s, file_name_house, path_house, n));
        ICY_ERROR(to_string("%1%2.txt"_s, file_name_output, dir_output, n));

        file input_house;
        file output;
        ICY_ERROR(input_house.open(file_name_house, file_access::read, file_open::open_existing, file_share::read));
        ICY_ERROR(output.open(file_name_output, file_access::app, file_open::create_always, file_share::read));

        array<house_type1> houses;
    
        auto done = false;
        array<char> str;

        const auto max_file = input_house.info().size;
        auto old_percent = 0ui64;
        auto new_percent = 0ui64;
        auto offset = 0_z;

        while (true)
        {
            char buffer[0x4000];
            auto size = _countof(buffer);
            ICY_ERROR(input_house.read(buffer, size));

            if (!size)
                break;

            auto k = 0u;
            while (k < size)
            {
                for (; k < size; ++k)
                {
                    if (str.empty())
                    {
                        if (buffer[k] == '\r' || buffer[k] == '\n')
                            continue;

                        str.push_back(buffer[k]);
                    }
                    else
                    {
                        if (buffer[k] == '\n' || buffer[k] == '\r')
                        {
                            done = true;
                            break;
                        }
                        else if (str.size() < max_str_length)
                        {
                            str.push_back(buffer[k]);
                        }
                    }
                }
                if (k == size && offset + k == max_file)
                    done = true;

                if (done)
                {
                    string str_tabs;
                    array<string_view> tabs;
                    ICY_ERROR(to_string(str, str_tabs));
                    ICY_ERROR(split(str_tabs, tabs));

                    if (tabs.size() < 3)
                        continue;

                    house_type1 new_house = {};
                    new_house.key = hash(tabs[0]);
                    new_house.text = std::move(str_tabs);
                    ICY_ERROR(houses.push_back(std::move(new_house)));
                    str.clear();
                    done = false;
                }
            }
            offset += k;

            new_percent = offset * 100ui64 / max_file;
            if (new_percent > old_percent)
            {
                string msg;
                ICY_ERROR(to_string("\r\nRegion %1: Parse house: %2%%..."_s, msg, n, new_percent));
                ICY_ERROR(console_write(msg));
                old_percent = new_percent;
            }
        }
        {
            string msg;
            ICY_ERROR(to_string("\r\nRegion %1: Found %2 total houses. Sorting..."_s, msg, n, houses.size()));
            ICY_ERROR(console_write(msg));
        }
        std::sort(houses.begin(), houses.end());
        auto jt = std::unique(houses.begin(), houses.end());
        houses.pop_back(std::distance(jt, houses.end()));
        {
            string msg;
            ICY_ERROR(to_string("\r\nRegion %1: Found %2 unique houses."_s, msg, n, houses.size()));
            ICY_ERROR(console_write(msg));
        }

        offset = 0_z;
        old_percent = 0_z;
        new_percent = 0_z;

        for (auto&& house : houses)
        {
            if (offset)
                ICY_ERROR(output.append("\r\n", 2));
            
            ICY_ERROR(output.append(house.text.bytes().data(), house.text.bytes().size()));
            ++offset;
            new_percent = offset * 10ui64 / houses.size();
            if (new_percent > old_percent)
            {
                string msg;
                ICY_ERROR(to_string("\r\nRegion %1: Output house: %2%%..."_s, msg, n, new_percent * 100 / 10));
                ICY_ERROR(console_write(msg));
                old_percent = new_percent;
            }
        }

    }

    const auto time = clock_type::now() - now;
    string msg;
    ICY_ERROR(to_string("\r\nParse done after [%1] ms."_s, msg, std::chrono::duration_cast<std::chrono::milliseconds>(time).count()));
    ICY_ERROR(console_write(msg));

    return {};
}


error_type make_list(const string_view path_house, const string_view path_addr, const string_view dir_output, const uint32_t n)
{

    auto now = clock_type::now();

    {
        string file_name_house;
        string file_name_addr;
        string file_name_output;
        ICY_ERROR(to_string("%1%2.txt"_s, file_name_house, path_house, n));
        ICY_ERROR(to_string("%1%2.txt"_s, file_name_addr, path_addr, n));
        ICY_ERROR(to_string("%1%2.txt"_s, file_name_output, dir_output, n));

        file input_house;
        file input_addr;
        file output;
        ICY_ERROR(input_house.open(file_name_house, file_access::read, file_open::open_existing, file_share::read));
        ICY_ERROR(input_addr.open(file_name_addr, file_access::read, file_open::open_existing, file_share::read));
        ICY_ERROR(output.open(file_name_output, file_access::app, file_open::create_always, file_share::read));

        array<addr_type2> addr;
        array<house_type2> houses;
        {
            auto done = false;
            array<char> str;

            const auto max_file = input_addr.info().size;
            auto old_percent = 0ui64;
            auto new_percent = 0ui64;
            auto offset = 0ui64;

            while (true)
            {
                char buffer[0x4000];
                auto size = _countof(buffer);
                ICY_ERROR(input_addr.read(buffer, size));

                if (!size)
                    break;

                auto k = 0u;
                while (k < size)
                {
                    for (; k < size; ++k)
                    {
                        if (str.empty())
                        {
                            if (buffer[k] == '\r' || buffer[k] == '\n')
                                continue;

                            str.push_back(buffer[k]);
                        }
                        else
                        {
                            if (buffer[k] == '\n' || buffer[k] == '\r')
                            {
                                done = true;
                                break;
                            }
                            else if (str.size() < max_str_length)
                            {
                                str.push_back(buffer[k]);
                            }
                        }
                    }
                    if (k == size && offset + k == max_file)
                        done = true;

                    if (done)
                    {
                        string str_tabs;
                        array<string_view> tabs;
                        ICY_ERROR(to_string(str, str_tabs));
                        ICY_ERROR(split(str_tabs, tabs, '\t'));

                        if (tabs.size() < 4)
                            continue;

                        addr_type2 new_addr = {};
                        to_value(tabs[0], new_addr.key);
                        to_value(tabs[1], new_addr.par);
                        if (to_value(tabs[2], new_addr.level) == error_type{})
                        {
                            ICY_ERROR(to_string(tabs[3], new_addr.text));
                            ICY_ERROR(addr.push_back(std::move(new_addr)));
                        }
                        str.clear();
                        done = false;
                    }
                }
                offset += k;

                new_percent = offset * 100ui64 / max_file;
                if (new_percent > old_percent)
                {
                    string msg;
                    ICY_ERROR(to_string("\r\nRegion %1: Parse address: %2%%..."_s, msg, n, new_percent));
                    ICY_ERROR(console_write(msg));
                    old_percent = new_percent;
                }
            }
        }
        {
            string msg;
            ICY_ERROR(to_string("\r\nRegion %1: Found %2 total objects. Sorting..."_s, msg, n, addr.size()));
            ICY_ERROR(console_write(msg));
        }
        std::sort(addr.begin(), addr.end());
        auto it = std::unique(addr.begin(), addr.end());
        addr.pop_back(std::distance(it, addr.end()));
        {
            string msg;
            ICY_ERROR(to_string("\r\nRegion %1: Found %2 unique objects."_s, msg, n, addr.size()));
            ICY_ERROR(console_write(msg));
        }

        {
            auto done = false;
            array<char> str;

            const auto max_file = input_house.info().size;
            auto old_percent = 0ui64;
            auto new_percent = 0ui64;
            auto offset = 0ui64;

            while (true)
            {
                char buffer[0x4000];
                auto size = _countof(buffer);
                ICY_ERROR(input_house.read(buffer, size));

                if (!size)
                    break;

                auto k = 0u;
                while (k < size)
                {
                    for (; k < size; ++k)
                    {
                        if (str.empty())
                        {
                            if (buffer[k] == '\r' || buffer[k] == '\n')
                                continue;

                            str.push_back(buffer[k]);
                        }
                        else
                        {
                            if (buffer[k] == '\n' || buffer[k] == '\r')
                            {
                                done = true;
                                break;
                            }
                            else if (str.size() < max_str_length)
                            {
                                str.push_back(buffer[k]);
                            }
                        }
                    }
                    if (k == size && offset + k == max_file)
                        done = true;

                    if (done)
                    {
                        string str_tabs;
                        array<string_view> tabs;
                        ICY_ERROR(to_string(str, str_tabs));
                        ICY_ERROR(split(str_tabs, tabs, '\t'));

                        if (tabs.size() < 3)
                            continue;

                        house_type2 new_house = {};
                        to_value(tabs[0], new_house.key);
                        to_value(tabs[1], new_house.par);
                        while (true)
                        {
                            if (tabs[2].bytes().size() < _countof(new_house.text))
                                break;

                            auto end = tabs[2].end();
                            --end;
                            tabs[2] = string_view(tabs[2].begin(), end);
                        }

                        strncpy_s(new_house.text, tabs[2].bytes().data(), tabs[2].bytes().size());
                        ICY_ERROR(houses.push_back(std::move(new_house)));
                        str.clear();
                        done = false;
                    }
                }
                offset += k;

                new_percent = offset * 100ui64 / max_file;
                if (new_percent > old_percent)
                {
                    string msg;
                    ICY_ERROR(to_string("\r\nRegion %1: Parse house: %2%%..."_s, msg, n, new_percent));
                    ICY_ERROR(console_write(msg));
                    old_percent = new_percent;
                }
            }
        }
        {
            string msg;
            ICY_ERROR(to_string("\r\nRegion %1: Found %2 total houses. Sorting..."_s, msg, n, houses.size()));
            ICY_ERROR(console_write(msg));
        }
        std::sort(houses.begin(), houses.end());
        auto jt = std::unique(houses.begin(), houses.end());
        houses.pop_back(std::distance(jt, houses.end()));
        {
            string msg;
            ICY_ERROR(to_string("\r\nRegion %1: Found %2 unique houses."_s, msg, n, houses.size()));
            ICY_ERROR(console_write(msg));
        }

        auto offset = 0_z;
        auto old_percent = 0_z;
        auto new_percent = 0_z;
        array<string> output_str;
        output_str.reserve(houses.size());

        for (auto&& house : houses)
        {
            addr_type2 new_addr = {};
            new_addr.key = house.par;
            auto street = binary_search(addr.begin(), addr.end(), new_addr);
            if (street == addr.end())
                continue;
            if (street->level != 7)
                continue;

            new_addr.key = street->par;
            auto town = binary_search(addr.begin(), addr.end(), new_addr);

            string town_str;
            town_str.appendf(town->text);
            if (town != addr.end() && town->level > 4)
            {
                auto open = false;
                for (; town != addr.end() && town->level > 2;)
                {                    
                    new_addr.key = town->par;
                    auto bigger_town = binary_search(addr.begin(), addr.end(), new_addr);
                    if (bigger_town == addr.end())
                        break;

                    if (bigger_town->level <= 2)
                        break;

                    town = bigger_town;
                    town_str.append(" ("_s);
                    open = true;
                    town_str.append(town->text);
                    break;
                }
                if (open)
                    town_str.append(")"_s);
            }


            if (town != addr.end())
            {
                string_view house_text;
                to_string(const_array_view<char>(house.text), house_text);

                string out_str;
                ICY_ERROR(to_string("%1\t%2\t%3\t%4\t%5\t%6"_s, out_str, 
                    string_view(town_str), string_view(street->text), house_text, street->par, street->key, house.key));
                output_str.push_back(std::move(out_str));
            }   
            new_percent = offset * 100ui64 / output_str.size();
            if (new_percent > old_percent)
            {
                string msg;
                ICY_ERROR(to_string("\r\nRegion %1: Making records (town/street/house): %2%%..."_s, msg, n, new_percent));
                ICY_ERROR(console_write(msg));
                old_percent = new_percent;
            }
        }
        {
            string msg;
            ICY_ERROR(to_string("\r\nRegion %1: Sorting %2 records alphabetically..."_s, msg, n, output_str.size()));
            ICY_ERROR(console_write(msg));
        }
        std::sort(output_str.begin(), output_str.end());
        
        old_percent = 0;
        offset = 0_z;
        for (auto&& str : output_str)
        {
            if (offset)
                ICY_ERROR(output.append("\r\n", 2));
            
            ICY_ERROR(output.append(str.bytes().data(), str.bytes().size()));
            ++offset;
            new_percent = offset * 10ui64 / output_str.size();
            if (new_percent > old_percent)
            {
                string msg;
                ICY_ERROR(to_string("\r\nRegion %1: Output record: %2%%..."_s, msg, n, new_percent * 100 / 10));
                ICY_ERROR(console_write(msg));
                old_percent = new_percent;
            }
        }

    }

    const auto time = clock_type::now() - now;
    string msg;
    ICY_ERROR(to_string("\r\nParse done after [%1] ms."_s, msg, std::chrono::duration_cast<std::chrono::milliseconds>(time).count()));
    ICY_ERROR(console_write(msg));

    return {};
}

int main()
{
    heap gheap;
    gheap.initialize(heap_init::global(4_gb));
    shared_ptr<console_system> con;
    create_console_system(con);
    con->thread().launch();
    
    parse_addrobj("");

    //const auto error = parse_houses("C:/VS2015/dat/fias/as_house.xml"_s, "C:/VS2015/dat/fias/test_house/"_s);
    //return 0;
    //const auto error = parse_steads("C:/VS2015/dat/fias/as_stead.xml"_s, "C:/VS2015/dat/fias/test_stead/"_s);
    //const auto error = parse_addrobj("C:/VS2015/dat/fias/as_addrobj.xml"_s, "C:/VS2015/dat/fias/test_addrobj/"_s);
    //con = 0;
    return 0;
    for (auto k = 77; k < 77 + 1; ++k)
    {
        const auto error = make_list(
            "C:/VS2015/dat/fias/test_house/"_s,
            "C:/VS2015/dat/fias/test_addrobj/"_s,
            "C:/VS2015/dat/fias/test_result/"_s, k);
        //const auto error = remove_duplicate_houses("D:/Downloads/fias_xml/test_house/"_s, "D:/Downloads/fias_xml/test_house2/"_s, k);
        if (error)
        {
            string err_str;
            to_string(error, err_str);
            string str;
            to_string("Region %1: %2"_s, str, k, string_view(err_str));
            //win32_message(str);
        }
    }
    con = 0;
    return 0;
}
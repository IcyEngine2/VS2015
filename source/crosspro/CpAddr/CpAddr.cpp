#include <icy_engine/core/icy_core.hpp>
#include <icy_engine/core/icy_file.hpp>
#include <icy_engine/parser/icy_parser_csv.hpp>

using namespace icy;

#if _DEBUG
#pragma comment(lib, "icy_engine_cored")
#else
#pragma comment(lib, "icy_engine_core")
#endif

class addr_comparer_parser : public csv_parser
{
public:
    error_type append(const string_view osm_addr, const string_view fias_addr) noexcept
    {
        string val;
        string key;
        ICY_ERROR(to_string(osm_addr, key));
        ICY_ERROR(to_string(fias_addr, val));
        return m_data.insert(std::move(key), std::move(val));
    }
    string_view find(const string_view osm_addr) const noexcept
    {
        const auto ptr = m_data.try_find(osm_addr);
        return ptr ? *ptr : ""_s;
    }
    error_type save(const string_view path) noexcept
    {
        string tname;
        ICY_ERROR(file::tmpname(tname));

        file tfile;
        ICY_ERROR(tfile.open(tname, file_access::app, file_open::create_always, file_share::none));

        auto first = true;
        for (auto&& pair : m_data)
        {
            string str;
            if (!first)
            {
                ICY_ERROR(str.append("\r\n"_s));
            }
            first = false;
            ICY_ERROR(str.append(pair.key));
            ICY_ERROR(str.append("\t"));
            ICY_ERROR(str.append(pair.value));
            ICY_ERROR(tfile.append(str.bytes().data(), str.bytes().size()));
        }
        tfile.close();
        ICY_ERROR(file::replace(tname, path));
        return {};
    }
private:
    error_type callback(const const_array_view<string_view> tabs) noexcept override
    {
        if (tabs.size() < 2)
            return {};
        return append(tabs[0], tabs[1]);
    }
private:
    dictionary<string> m_data;
};

error_type jaro_winkler_distance(const string_view utf8lhs, const string_view utf8rhs, double& distance) noexcept;

struct pair_type
{
    bool operator<(const pair_type& rhs) const noexcept
    {
        if (distance > rhs.distance)
            return true;
        else if (distance == rhs.distance)
            return name < rhs.name;
        else
            return false;
    }
    string_view name;
    double distance;
};
struct osm_addr
{
    uint64_t index = 0;
    string street;
    string house;
};
struct osm_coord
{
    double lat = 0;
    double lng = 0;
};
struct osm_bounds
{
    osm_coord lower;
    osm_coord upper;
};
struct fias_house
{
    uint64_t index = 0;
    osm_coord coord;
};
struct fias_street
{
    error_type find_or_insert(const string_view house_name, dictionary<fias_house>::iterator& house)
    {
        house = houses.find(house_name);
        if (house == houses.end())
        {
            const auto fix_name = [](const string_view input, string& output)
            {
                for (auto it = input.begin(); it != input.end(); ++it)
                {
                    char32_t chr = 0;
                    it.to_char(chr);
                    if (chr >= '0' && chr <= '9')
                    {
                        ICY_ERROR(output.append(string_view(it, it + 1)));
                    }
                    else if (chr == ' ' || chr == '-' || chr == '/')
                    {
                        ;
                    }
                    else
                    {
                        string lower;
                        ICY_ERROR(to_lower(string_view(it, it + 1), lower));
                        ICY_ERROR(output.append(lower));
                    }
                }
                return error_type();
            };
            string fixed_name;
            ICY_ERROR(fix_name(house_name, fixed_name));
            for (auto it = houses.begin(); it != houses.end(); ++it)
            {
                string compare_name;
                ICY_ERROR(fix_name(it->key, compare_name));
                if (compare_name == fixed_name)
                {
                    house = it;
                    break;
                }
            }
        }
        if (house == houses.end())
        {
            auto house_number = 0u;
            house_name.to_value(house_number);

            array<pair_type> vec;
            auto multi_var = 0u;
            for (auto&& pair : houses)
            {
                auto compare_number = 0u;
                pair.key.to_value(compare_number);
                auto distance = 1.0 / (fabs(double(house_number) - double(compare_number)) + 1);
                ICY_ERROR(vec.push_back({ pair.key, distance }));
                if (house_number == compare_number)
                    multi_var++;
            }
            std::sort(vec.begin(), vec.end());

            string msg;
            ICY_ERROR(msg.appendf("\r\nHouse \"%1\" not found."_s, string_view(house_name)));

            static auto always_answer = false;

            if (multi_var > 1 && !always_answer)
            {
                ICY_ERROR(msg.append(" Variants:"_s));
                for (auto k = vec.size(); k--; )
                {
                    ICY_ERROR(msg.appendf("\r\n[%1] | %2 | %3", k + 1, vec[k].distance, string_view(vec[k].name)));
                }
                ICY_ERROR(msg.appendf("\r\nFix \"%1\" using ID [1 ... %2], skip (0) or always select '1' (-1): "_s,
                    house_name, vec.size()));
            }
            ICY_ERROR(console_write(msg));
            if (vec.front().distance != 1)
                return {};

            auto answer = 0;
            if (multi_var > 1 && !always_answer)
            {
                while (true)
                {
                    auto exit = false;
                    string str;
                    ICY_ERROR(icy::console_read_line(str, exit));
                    if (exit)
                        return {};
                    if (str.to_value(answer) || answer > int(vec.size()))
                    {
                        ICY_ERROR(console_write("\r\nError: invalid house!\r\nTry again: "_s));
                    }
                    else
                    {
                        break;
                    }
                }
            }
            else
                answer = 1;

            if (answer >= 1 && answer <= vec.size())
                house = houses.find(vec[answer - 1].name);
            else if (answer == -1)
                always_answer = true;
        }
        return {};
    }
    uint64_t index = 0;
    dictionary<fias_house> houses;
};
struct fias_city
{
    error_type find( const string_view street_name, const addr_comparer_parser& compare, 
        dictionary<fias_street>::iterator& street)
    {
        street = streets.find(street_name);
        if (street == streets.end())
        {
            const auto alt_street = compare.find(street_name);
            if (!alt_street.empty())
                street = streets.find(alt_street);
        }
        if (street == streets.end())
        {
            array<string_view> street_words_vec;
            ICY_ERROR(split(street_name, street_words_vec));
            if (!street_words_vec.empty())
                street_words_vec.pop_back();

            array<string> street_words;
            const auto fix_words = [](const const_array_view<string_view> input, array<string>& output)
            {
                for (auto&& word : input)
                {
                    string str;
                    ICY_ERROR(to_lower(word, str));
                    ICY_ERROR(str.replace(u8"ё"_s, u8"е"_s));
                    ICY_ERROR(output.push_back(std::move(str)));
                }
                return error_type();
            };
            ICY_ERROR(fix_words(street_words_vec, street_words));

            for (auto it = streets.begin(); it != streets.end(); ++it)
            {
                array<string_view> compare_words_vec;
                ICY_ERROR(split(it->key, compare_words_vec));
                if (compare_words_vec.empty())
                    continue;
                compare_words_vec.pop_back();

                array<string> compare_words;
                ICY_ERROR(fix_words(compare_words_vec, compare_words));

                auto equal_count = 0u;
                for (auto&& compare_word : compare_words)
                {
                    for (auto&& street_word : street_words)
                    {
                        if (street_word == compare_word)
                        {
                            ++equal_count;
                            break;
                        }
                    }
                }
                if (equal_count == compare_words.size() ||
                    equal_count == street_words.size())
                {
                    street = it;
                    break;
                }
            }
        }
        return {};
    }
    error_type insert(const string_view street_name, addr_comparer_parser& compare,
        dictionary<fias_street>::iterator& street, array<string_view>& skip) noexcept
    {
        if (street == streets.end())
        {
            array<pair_type> vec;
            for (auto&& pair : streets)
            {
                auto distance = 0.0;
                ICY_ERROR(jaro_winkler_distance(pair.key, street_name, distance));
                ICY_ERROR(vec.push_back({ pair.key, distance }));
            }
            std::sort(vec.begin(), vec.end());
            string msg;
            ICY_ERROR(msg.appendf("\r\nStreet \"%1\" not found. Variants:"_s, string_view(street_name)));
            for (auto k = vec.size(); k--; )
            {
                ICY_ERROR(msg.appendf("\r\n[%1] | %2 | %3", k + 1, vec[k].distance, string_view(vec[k].name)));
            }
            ICY_ERROR(msg.appendf("\r\nFix \"%1\" using ID [1 ... %2] or cancel (type 0): "_s,
                string_view(street_name), vec.size()));
            ICY_ERROR(console_write(msg));

            auto answer = 0u;
            while (true)
            {
                auto exit = false;
                string str;
                ICY_ERROR(icy::console_read_line(str, exit));
                if (exit)
                    return {};
                if (str.to_value(answer) || answer > vec.size())
                {
                    ICY_ERROR(console_write("\r\nError: invalid street!\r\nTry again: "_s));
                }
                else
                    break;
            }

            if (answer >= 1 && answer <= vec.size())
            {
                ICY_ERROR(compare.append(street_name, vec[answer - 1].name));
                street = streets.find(vec[answer - 1].name);
            }
            else
            {
                ICY_ERROR(skip.push_back(street_name));
            }
        }
        return {};
    }

    uint64_t index = 0;
    dictionary<fias_street> streets;

};
struct coords_addr
{
    string city;
    string street;
    string house;
    osm_coord value;
};


class osm_addr_parser : public csv_parser
{
public:
    const_array_view<osm_addr> nodes() const noexcept
    {
        return m_nodes;
    }
    const_array_view<osm_addr> ways() const noexcept
    {
        return m_ways;
    }
    const_array_view<osm_addr> rels() const noexcept
    {
        return m_rels;
    }
private:
    error_type callback(const const_array_view<string_view> tabs) noexcept override
    {
        if (tabs.size() < 4)
            return {};

        osm_addr addr;
        ICY_ERROR(tabs[1].to_value(addr.index));
        ICY_ERROR(to_string(tabs[2], addr.street));
        ICY_ERROR(to_string(tabs[3], addr.house));
        if (tabs[0] == "n"_s)
        {
            ICY_ERROR(m_nodes.push_back(std::move(addr)));
        }
        else if (tabs[0] == "w"_s)
        {
            ICY_ERROR(m_ways.push_back(std::move(addr)));
        }
        else if (tabs[0] == "r"_s)
        {
            ICY_ERROR(m_rels.push_back(std::move(addr)));
        }
        return {};
    }
private:
    array<osm_addr> m_nodes;
    array<osm_addr> m_ways;
    array<osm_addr> m_rels;
};
class osm_node_parser : public csv_parser
{
public:
    const osm_coord* find(const uint64_t index) const noexcept
    {
        return m_data.try_find(index);
    }
private:
    error_type callback(const const_array_view<string_view> tabs) noexcept override
    {
        if (tabs.size() < 3)
            return {};

        uint64_t index = 0;
        osm_coord coord;
        ICY_ERROR(tabs[0].to_value(index));
        ICY_ERROR(tabs[1].to_value(coord.lat));
        ICY_ERROR(tabs[2].to_value(coord.lng));
        ICY_ERROR(m_data.insert(index, coord));
        return {};
    }
private:
    map<uint64_t, osm_coord> m_data;
};
class osm_way_parser : public csv_parser
{
public:
    const_array_view<uint64_t> find(const uint64_t index) const noexcept
    {
        const auto ptr = m_data.try_find(index);
        return ptr ? *ptr : const_array_view<uint64_t>{};
    }
private:
    error_type callback(const const_array_view<string_view> tabs) noexcept override
    {
        if (tabs.size() < 2)
            return {};

        uint64_t index = 0;
        ICY_ERROR(tabs[0].to_value(index));
        array<uint64_t> nodes;
        for (auto k = 1_z; k < tabs.size(); ++k)
        {
            uint64_t node = 0;
            ICY_ERROR(tabs[k].to_value(node));
            ICY_ERROR(nodes.push_back(node));
        }
        ICY_ERROR(m_data.insert(index, std::move(nodes)));
        return {};
    }
private:
    map<uint64_t, array<uint64_t>> m_data;
};
class osm_rel_parser : public csv_parser
{
public:
    struct tuple_type
    {
        array<uint64_t> nodes;
        array<uint64_t> ways;
        array<uint64_t> rels;
    };
    const tuple_type* find(const uint64_t index) const noexcept
    {
        return m_data.try_find(index);
    }
private:
    error_type callback(const const_array_view<string_view> tabs) noexcept override
    {
        if (tabs.size() < 2)
            return {};

        uint64_t index = 0;
        ICY_ERROR(tabs[0].to_value(index));
        tuple_type tuple;
        for (auto k = 1_z; k < tabs.size(); ++k)
        {
            if (tabs[k].bytes().size() < 3)
                continue;

            uint64_t ref = 0;
            const auto str_ref = string_view(tabs[k].bytes().data() + 2, tabs[k].bytes().size() - 2);
            ICY_ERROR(str_ref.to_value(ref));
            if (tabs[k].bytes()[1] == 'n')
            {
                ICY_ERROR(tuple.nodes.push_back(ref));
            }
            else if (tabs[k].bytes()[1] == 'w')
            {
                ICY_ERROR(tuple.ways.push_back(ref));
            }
            else if (tabs[k].bytes()[1] == 'r')
            {
                ICY_ERROR(tuple.rels.push_back(ref));
            }
        }
        ICY_ERROR(m_data.insert(index, std::move(tuple)));
        return {};
    }
private:
    map<uint64_t, tuple_type> m_data;
};
class osm_bounds_parser : public csv_parser
{
public:
    const osm_bounds* find(const string_view city) const noexcept
    {
        return m_data.try_find(city);
    }
    error_type find(const osm_coord& coord, map<string_view, osm_coord>& cities) const noexcept
    {
        //auto distance = DBL_MAX;
        //auto city_size = DBL_MAX;
        for (auto&& pair : m_data)
        {
            const auto& lower = pair.value.lower;
            const auto& upper = pair.value.upper;
            if (coord.lng >= lower.lng && coord.lng <= upper.lng &&
                coord.lat >= lower.lat && coord.lat <= upper.lat)
                ;
            else
                continue;
            osm_coord center;
            center.lng = (lower.lng + upper.lng) / 2;
            center.lat = (lower.lat + upper.lat) / 2;
            //const auto dx = coord.lng - ;
            //const auto dy = coord.lat - (lower.lat + upper.lat) / 2;
            //const auto new_distance = dx * dx + dy * dy;
            ICY_ERROR(cities.insert(pair.key, center));
            //const auto new_size = abs(upper.lng - lower.lng) * abs(upper.lat - lower.lat);
            /*if (new_size < city_size)
            {
                output = pair.key;
                distance = new_distance;
                city_size = new_size;
            }*/
        }
        return {};
    }
    const_array_view<string> keys() const noexcept
    {
        return m_data.keys();
    }
private:
    error_type callback(const const_array_view<string_view> tabs) noexcept override
    {
        if (tabs.size() < 5)
            return {};

        string city;
        ICY_ERROR(to_string(tabs[0], city));
        
        osm_bounds bounds;
        ICY_ERROR(tabs[1].to_value(bounds.lower.lng));
        ICY_ERROR(tabs[2].to_value(bounds.lower.lat));
        ICY_ERROR(tabs[3].to_value(bounds.upper.lng));
        ICY_ERROR(tabs[4].to_value(bounds.upper.lat));
        
        if (city.empty() || bounds.lower.lng >= bounds.upper.lng || bounds.lower.lat >= bounds.upper.lat)
            return make_stdlib_error(std::errc::invalid_argument);

        ICY_ERROR(m_data.insert(std::move(city), bounds));
        return {};
    }
private:
    dictionary<osm_bounds> m_data;
};
class fias_addr_parser : public csv_parser
{
public:
    fias_city* find(const string_view name) noexcept
    {
        const auto it = m_data.find(name);        
        if (it == m_data.end())
            return nullptr;
        else
            return &it->value;
    }
    error_type save(const string_view path) const noexcept
    {
        string tname;
        ICY_ERROR(file::tmpname(tname));

        file tfile;
        ICY_ERROR(tfile.open(tname, file_access::app, file_open::create_always, file_share::none));
        auto first = true;
        for (auto&& city : m_data)
        {
            for (auto&& street : city.value.streets)
            {
                for (auto&& house : street.value.houses)
                {
                    string str;
                    if (!first)
                        ICY_ERROR(str.append("\r\n"));
                    first = false;
                    ICY_ERROR(str.appendf("%1\t%2\t%3\t%4\t%5\t%6"_s,
                        string_view(city.key), string_view(street.key), string_view(house.key),
                        city.value.index, street.value.index, house.value.index));
                    if (house.value.coord.lng || house.value.coord.lat)
                        ICY_ERROR(str.appendf("\t%1\t%2"_s, house.value.coord.lng, house.value.coord.lat));
                    ICY_ERROR(tfile.append(str.bytes().data(), str.bytes().size()));
                }
            }
        }
        tfile.close();
        ICY_ERROR(file::replace(tname, path));
        return {};
    }
private:
    error_type callback(const const_array_view<string_view> tabs) noexcept override
    {
        if (tabs.size() < 6)
            return {};

        fias_city new_city;
        fias_street new_street;
        fias_house new_house;
        ICY_ERROR(tabs[3].to_value(new_city.index));
        ICY_ERROR(tabs[4].to_value(new_street.index));
        ICY_ERROR(tabs[5].to_value(new_house.index));
        if (tabs.size() >= 8)
        {
            ICY_ERROR(tabs[6].to_value(new_house.coord.lng));
            ICY_ERROR(tabs[7].to_value(new_house.coord.lat));
        }
        auto city = m_data.end();
        ICY_ERROR(m_data.find_or_insert(tabs[0], std::move(new_city), &city));
        auto street = city->value.streets.end();
        ICY_ERROR(city->value.streets.find_or_insert(tabs[1], std::move(new_street), &street));
        auto house = street->value.houses.end();
        ICY_ERROR(street->value.houses.find_or_insert(tabs[2], std::move(new_house), &house));
        return {};
    }
private:
    dictionary<fias_city> m_data;
};
class coords_parser : public csv_parser
{
public:
    const_array_view<coords_addr> data() const noexcept
    {
        return m_data;
    }
private:
    error_type callback(const const_array_view<string_view> tabs) noexcept override
    {
        if (tabs.size() < 5)
            return {};
        
        coords_addr new_addr;
        ICY_ERROR(to_string(tabs[0], new_addr.city));
        ICY_ERROR(to_string(tabs[1], new_addr.street));
        ICY_ERROR(to_string(tabs[2], new_addr.house));
        ICY_ERROR(tabs[3].to_value(new_addr.value.lng));
        ICY_ERROR(tabs[4].to_value(new_addr.value.lat));
        return m_data.push_back(std::move(new_addr));
    }
private:
    array<coords_addr> m_data;
};
class point_parser : public csv_parser
{
public:
    struct data_type
    {
        array<string> vals;
        string city;
        string street;
        string house;
    };
    array_view<data_type> data() noexcept
    {
        return array_view<data_type>{ m_data.data() + 1, m_data.size() - 1 };
    }
    error_type save(const string_view path) const noexcept
    {
        file output;
        ICY_ERROR(output.open(path, file_access::app, file_open::create_always, file_share::none));

        auto first = true;
        for (auto&& data : m_data)
        {
            string str;
            if (!first)
                ICY_ERROR(str.append("\r\n"_s));
            first = false;

            for (auto k = 0_z; k < data.vals.size(); ++k)
            {
                if (k)
                    ICY_ERROR(str.append("\t"_s));
                ICY_ERROR(str.append(data.vals[k]));
            }
            if (!data.city.empty())
            {
                ICY_ERROR(str.appendf("\t%1\t%2\t%3"_s, string_view(data.city),
                    string_view(data.street), string_view(data.house)));
            }
            ICY_ERROR(output.append(str.bytes().data(), str.bytes().size()));
        }
        return {};
    }
private:
    error_type callback(const const_array_view<string_view> tabs) noexcept override
    {
        if (tabs.empty())
            return {};
        
        data_type new_data;
        for (auto&& tab : tabs)
        {
            string str;
            ICY_ERROR(to_string(tab, str));
            ICY_ERROR(new_data.vals.push_back(std::move(str)));
        }
        return m_data.push_back(std::move(new_data));
    }
private:
    array<data_type> m_data;
};

error_type jaro_winkler_distance(const string_view utf8lhs, const string_view utf8rhs, double& distance) noexcept
{
    // Exit early if either are empty
    if (utf8lhs.empty() || utf8rhs.empty())
    {
        distance = 0;
        return {};
    }

    // Exit early if they're an exact match.
    if (utf8lhs == utf8rhs)
    {
        distance = 1;
        return {};
    }

    array<char32_t> lhs;
    array<char32_t> rhs;
    for (auto&& chr : utf8lhs) ICY_ERROR(lhs.push_back(chr));
    for (auto&& chr : utf8rhs) ICY_ERROR(rhs.push_back(chr));

    const auto lhs_size = static_cast<int>(lhs.size());
    const auto rhs_size = static_cast<int>(rhs.size());
    const auto range = std::max(lhs_size, rhs_size) / 2 - 1;

    array<bool> lhs_matches;
    array<bool> rhs_matches;
    ICY_ERROR(lhs_matches.resize(lhs.size()));
    ICY_ERROR(rhs_matches.resize(rhs.size()));

    auto num_match = 0.0;
    auto num_trans = 0.0;

    for (auto i = 0; i < lhs_size; ++i)
    {
        const auto lower = std::max(i - range, 0);
        const auto upper = std::min(i + range, rhs_size);
        for (auto j = lower; j < upper; ++j)
        {
            if (!lhs_matches[i] && !rhs_matches[j] && lhs[i] == rhs[j])
            {
                num_match += 1;
                lhs_matches[i] = true;
                rhs_matches[j] = true;
                break;
            }
        }
    }

    // Exit early if no matches were found
    if (num_match == 0)
    {
        distance = 0;
        return {};
    }

    for (auto i = 0, j = 0; i < lhs_size; ++i)
    {
        if (!lhs_matches[i])
            continue;

        for (; j < rhs_size; ++j)
        {
            if (!rhs_matches[j])
                continue;
        }
        if (j >= rhs_size)
            break;

        num_trans += lhs[i] != rhs[j];
    }
    num_trans /= 2;

    distance = 0;
    distance += num_match / lhs_size;
    distance += num_match / rhs_size;
    distance += (num_match - num_trans) / num_match;
    distance /= 3;
    return {};
}

osm_coord way_coord(const osm_node_parser& node, const osm_way_parser& way, const uint64_t index)
{
    const auto vec = way.find(index);
    osm_coord coord = {};
    auto count = 0u;
    for (auto&& index : vec)
    {
        const auto node_coord = node.find(index);
        if (!node_coord)
            continue;
        coord.lat += node_coord->lat;
        coord.lng += node_coord->lng;
        ++count;
    }
    if (count)
    {
        coord.lat /= count;
        coord.lng /= count;        
    }
    return coord;
}

osm_coord rel_coord(const osm_node_parser& node, const osm_way_parser& way, const osm_rel_parser& rel, const uint64_t index)
{
    const auto tuple = rel.find(index);
    if (!tuple)
        return {};

    osm_coord coord;
    auto count = 0u;
    for (auto&& node_index : tuple->nodes)
    {
        if (const auto next_coord = node.find(node_index))
        {
            coord.lat += next_coord->lat;
            coord.lng += next_coord->lng;
            ++count;
        }
    }
    for (auto&& way_index : tuple->ways)
    {
        const auto next_coord = way_coord(node, way, way_index);
        if (next_coord.lng || next_coord.lat)
        {
            coord.lat += next_coord.lat;
            coord.lng += next_coord.lng;
            ++count;
        }
    }
    for (auto&& rel_index : tuple->rels)
    {
        const auto next_coord = rel_coord(node, way, rel, rel_index);
        if (next_coord.lng || next_coord.lat)
        {
            coord.lat += next_coord.lat;
            coord.lng += next_coord.lng;
            ++count;
        }
    }
    if (count)
    {
        coord.lat /= count;
        coord.lng /= count;
    }
    return coord;
}

const auto root_dir = "C:/VS2015/dat"_s;
const auto region = 91u;
const auto buffer_size = 64_kb;

error_type main2(heap& heap)
{
    string dir;
    string bounds_path;
    string coords_path;
    string compare_path;
    string fias_path;
    string node_path;
    string osm_path;
    string rel_path;
    string way_path;
    ICY_ERROR(to_string("%1/%2", dir, root_dir, region));
    ICY_ERROR(to_string("%1/bounds.txt"_s, bounds_path, string_view(dir)));
    ICY_ERROR(to_string("%1/coords.txt"_s, coords_path, string_view(dir)));
    ICY_ERROR(to_string("%1/compare.txt"_s, compare_path, root_dir));
    ICY_ERROR(to_string("%1/fias.txt"_s, fias_path, string_view(dir)));
    ICY_ERROR(to_string("%1/node.txt"_s, node_path, string_view(dir)));
    ICY_ERROR(to_string("%1/osm.txt"_s, osm_path, string_view(dir)));
    ICY_ERROR(to_string("%1/rel.txt"_s, rel_path, string_view(dir)));
    ICY_ERROR(to_string("%1/way.txt"_s, way_path, string_view(dir)));

    osm_bounds_parser bounds;
    ICY_ERROR(parse(bounds, bounds_path, buffer_size));

    addr_comparer_parser compare;
    ICY_ERROR(parse(compare, compare_path, buffer_size));

    fias_addr_parser fias;
    ICY_ERROR(parse(fias, fias_path, buffer_size));

    osm_node_parser node;
    ICY_ERROR(parse(node, node_path, buffer_size));

    osm_addr_parser osm;
    ICY_ERROR(parse(osm, osm_path, buffer_size));

    osm_rel_parser rel;
    ICY_ERROR(parse(rel, rel_path, buffer_size));

    osm_way_parser way;
    ICY_ERROR(parse(way, way_path, buffer_size));

    file coords;
    ICY_ERROR(coords.open(coords_path, file_access::app, file_open::create_always, file_share::none));
    auto first = true;

    map<string_view, string_view> street_type;
    ICY_ERROR(street_type.insert(u8"Аллея"_s, u8"ал"_s));
    ICY_ERROR(street_type.insert(u8"Бульвар"_s, u8"б-р"_s));
    ICY_ERROR(street_type.insert(u8"Взвоз"_s, u8"взв"_s));
    ICY_ERROR(street_type.insert(u8"Въезд"_s, u8"взд"_s));
    ICY_ERROR(street_type.insert(u8"Дорога"_s, u8"дор"_s));
    ICY_ERROR(street_type.insert(u8"Заезд"_s, u8"ззд"_s));
    ICY_ERROR(street_type.insert(u8"Километр"_s, u8"км"_s));
    ICY_ERROR(street_type.insert(u8"Кольцо"_s, u8"к-цо"_s));
    ICY_ERROR(street_type.insert(u8"Коса"_s, u8"коса"_s));
    ICY_ERROR(street_type.insert(u8"Линия"_s, u8"лн"_s));
    ICY_ERROR(street_type.insert(u8"Магистраль"_s, u8"мгстр"_s));
    ICY_ERROR(street_type.insert(u8"Набережная"_s, u8"наб"_s));
    ICY_ERROR(street_type.insert(u8"Переезд"_s, u8"пер-д"_s));
    ICY_ERROR(street_type.insert(u8"Переулок"_s, u8"пер"_s));
    ICY_ERROR(street_type.insert(u8"Площадка"_s, u8"пл-ка"_s));
    ICY_ERROR(street_type.insert(u8"Площадь"_s, u8"пл"_s));
    ICY_ERROR(street_type.insert(u8"Проезд"_s, u8"пр-д"_s));
    ICY_ERROR(street_type.insert(u8"Просек"_s, u8"пр-к"_s));
    ICY_ERROR(street_type.insert(u8"Просека"_s, u8"	пр-ка"_s));
    ICY_ERROR(street_type.insert(u8"Проселок"_s, u8"пр-лок"_s));
    ICY_ERROR(street_type.insert(u8"Проспект"_s, u8"пр-кт"_s));
    ICY_ERROR(street_type.insert(u8"Проулок"_s, u8"проул"_s));
    ICY_ERROR(street_type.insert(u8"Разъезд"_s, u8"рзд"_s));
    ICY_ERROR(street_type.insert(u8"Ряд"_s, u8"ряд"_s));
    ICY_ERROR(street_type.insert(u8"Ряды"_s, u8"ряд"_s));
    ICY_ERROR(street_type.insert(u8"Ряд(ы)"_s, u8"ряд"_s));
    ICY_ERROR(street_type.insert(u8"Сквер"_s, u8"с-р"_s));
    ICY_ERROR(street_type.insert(u8"Спуск"_s, u8"с-к"_s));
    ICY_ERROR(street_type.insert(u8"Съезд"_s, u8"сзд"_s));
    ICY_ERROR(street_type.insert(u8"Тракт"_s, u8"тракт"_s));
    ICY_ERROR(street_type.insert(u8"Тупик"_s, u8"туп"_s));
    ICY_ERROR(street_type.insert(u8"Улица"_s, u8"ул"_s));
    ICY_ERROR(street_type.insert(u8"Шоссе"_s, u8"ш"_s));

    ICY_ERROR(street_type.insert(u8"аллея"_s, u8"ал"_s));
    ICY_ERROR(street_type.insert(u8"бульвар"_s, u8"б-р"_s));
    ICY_ERROR(street_type.insert(u8"взвоз"_s, u8"взв"_s));
    ICY_ERROR(street_type.insert(u8"въезд"_s, u8"взд"_s));
    ICY_ERROR(street_type.insert(u8"дорога"_s, u8"дор"_s));
    ICY_ERROR(street_type.insert(u8"заезд"_s, u8"ззд"_s));
    ICY_ERROR(street_type.insert(u8"километр"_s, u8"км"_s));
    ICY_ERROR(street_type.insert(u8"кольцо"_s, u8"к-цо"_s));
    ICY_ERROR(street_type.insert(u8"коса"_s, u8"коса"_s));
    ICY_ERROR(street_type.insert(u8"линия"_s, u8"лн"_s));
    ICY_ERROR(street_type.insert(u8"магистраль"_s, u8"мгстр"_s));
    ICY_ERROR(street_type.insert(u8"набережная"_s, u8"наб"_s));
    ICY_ERROR(street_type.insert(u8"переезд"_s, u8"пер-д"_s));
    ICY_ERROR(street_type.insert(u8"переулок"_s, u8"пер"_s));
    ICY_ERROR(street_type.insert(u8"площадка"_s, u8"пл-ка"_s));
    ICY_ERROR(street_type.insert(u8"площадь"_s, u8"пл"_s));
    ICY_ERROR(street_type.insert(u8"проезд"_s, u8"пр-д"_s));
    ICY_ERROR(street_type.insert(u8"просек"_s, u8"пр-к"_s));
    ICY_ERROR(street_type.insert(u8"просека"_s, u8"	пр-ка"_s));
    ICY_ERROR(street_type.insert(u8"проселок"_s, u8"пр-лок"_s));
    ICY_ERROR(street_type.insert(u8"проспект"_s, u8"пр-кт"_s));
    ICY_ERROR(street_type.insert(u8"проулок"_s, u8"проул"_s));
    ICY_ERROR(street_type.insert(u8"разъезд"_s, u8"рзд"_s));
    ICY_ERROR(street_type.insert(u8"ряд"_s, u8"ряд"_s));
    ICY_ERROR(street_type.insert(u8"ряды"_s, u8"ряд"_s));
    ICY_ERROR(street_type.insert(u8"ряд(ы)"_s, u8"ряд"_s));
    ICY_ERROR(street_type.insert(u8"сквер"_s, u8"с-р"_s));
    ICY_ERROR(street_type.insert(u8"спуск"_s, u8"с-к"_s));
    ICY_ERROR(street_type.insert(u8"съезд"_s, u8"сзд"_s));
    ICY_ERROR(street_type.insert(u8"тракт"_s, u8"тракт"_s));
    ICY_ERROR(street_type.insert(u8"тупик"_s, u8"туп"_s));
    ICY_ERROR(street_type.insert(u8"улица"_s, u8"ул"_s));
    ICY_ERROR(street_type.insert(u8"шоссе"_s, u8"ш"_s));

    dictionary<string_view> street_to_city;

    const auto func = [&](const string_view street,
        const string_view house, const osm_coord& coord, bool& exit)
    {
        map<string_view, osm_coord> city_names;
        ICY_ERROR(bounds.find(coord, city_names));
        if (!city_names.empty())
        {
            array<string_view> words;
            ICY_ERROR(split(street, words));
            string_view suffix;
            
            string new_street;

            for (auto&& word : words)
            {
                if (const auto ptr = street_type.try_find(word))
                {
                    suffix = *ptr;
                }
                else
                {
                    if (!new_street.empty())
                        ICY_ERROR(new_street.append(" "_s));
                    ICY_ERROR(new_street.append(word));
                }
            }
            if (!suffix.empty())
            {
                ICY_ERROR(new_street.append(" "_s));
                ICY_ERROR(new_street.append(suffix));
            }

            string str;
            if (!first)
                str.append("\r\n"_s);
            first = false;

            string_view city_name;
            if (city_names.size() > 1)
            {
                for (auto it = city_names.begin(); it != city_names.end(); )
                {
                    if (const auto city_ptr = fias.find(it->key))
                    {
                        auto find_street = city_ptr->streets.end();
                        
                        ICY_ERROR(city_ptr->find(street, compare, find_street));
                        if (find_street == city_ptr->streets.end())
                        {
                            ICY_ERROR(city_ptr->find(new_street, compare, find_street));
                        }
                        if (find_street != city_ptr->streets.end())
                        {
                            if (find_street->key == street || find_street->key == new_street)
                            {
                                city_name = it->key;
                                break;
                            }
                            ++it;
                            continue;
                        }
                    }
                    it = city_names.erase(it);
                }
                static auto always_one = false;

                if (city_names.size() > 1 && city_name.empty())
                {
                    if (const auto ptr = street_to_city.try_find(new_street))
                        city_name = *ptr;
                    else if (always_one)
                        city_name = city_names.front().key;
                }

                if (city_names.size() > 1 && city_name.empty())
                {
                    string msg;
                    ICY_ERROR(to_string("\r\n%1 cities can contain street \"%2\"."_s,
                        msg, city_names.size(), string_view(new_street)));

                    struct pair_type
                    {
                        bool operator<(const pair_type& rhs) const noexcept
                        {
                            return distance < rhs.distance;
                        }
                        string_view name;
                        double distance;
                    };
                    const auto calc_dst = [](const osm_coord& lhs, const osm_coord& rhs)
                    {
                        const auto CpGeoRadius = 6370997.0;
                        const auto M_PI = 3.1415926;

                        const auto lat1 = lhs.lat / 180 * M_PI;
                        const auto lat2 = rhs.lat / 180 * M_PI;

                        const auto dLat = (rhs.lat - lhs.lat) / 180 * M_PI;
                        const auto dLng = (rhs.lng - lhs.lng) / 180 * M_PI;

                        const auto a = cos(lat1) * cos(lat2) *
                            sin(dLng / 2) * sin(dLng / 2) +
                            sin(dLat / 2) * sin(dLat / 2);

                        const auto c = 2 * atan2(sqrt(a), sqrt(1 - a));
                        return CpGeoRadius * c;
                    };

                    array<pair_type> vec;
                    for (auto&& pair : city_names)
                        ICY_ERROR(vec.push_back({ pair.key, calc_dst(coord, pair.value) }));

                    std::sort(vec.begin(), vec.end());
                    for (auto k = vec.size(); k--;)
                    {
                        ICY_ERROR(msg.appendf("\r\n[%1]. %2 (km) - \"%3\" (%4)"_s,
                            k + 1, size_t(vec[k].distance / 1000), vec[k].name, 
                            fias.find(vec[k].name)->streets.size()));
                        for (auto&& sname : fias.find(vec[k].name)->streets)
                            ICY_ERROR(msg.appendf("\r\n  %1", string_view(sname.key)));
                        
                    }
                    ICY_ERROR(msg.appendf("\r\nFix \"%1\" using ID [1 ... %2], skip (0), always '1' (-1): "_s,
                        string_view(new_street), vec.size()));
                    ICY_ERROR(console_write(msg));

                    auto answer = 0;
                    while (true)
                    {
                        string str;
                        ICY_ERROR(console_read_line(str, exit));
                        if (exit)
                            return error_type();
                        
                        if (str.to_value(answer) || answer > int(vec.size()))
                        {
                            ICY_ERROR(console_write("\r\nError: invalid city!\r\nTry again: "_s));
                        }         
                        else if (answer == -1)
                        {
                            always_one = true;
                            answer = 1;
                            break;
                        }
                        else
                        {
                            break;
                        }
                    }
                    if (answer == 0)
                        return error_type();
                    city_name = city_names.keys()[answer - 1];

                    ICY_ERROR(street_to_city.insert(new_street, city_name));
                }
                else if (city_names.size() == 1)
                {
                    city_name = city_names.front().key;
                }
                else if (city_name.empty())
                {
                    string msg;
                    ICY_ERROR(to_string("\r\nError: address info: no city has \"%1\".", msg, string_view(new_street)));
                    ICY_ERROR(console_write(msg));
                }
            }
            else if (!city_names.empty())
                city_name = city_names.front().key;

            if (!city_name.empty())
            {
                ICY_ERROR(str.appendf("%1\t%2\t%3\t%4\t%5"_s, city_name, string_view(new_street), house, coord.lng, coord.lat));
                ICY_ERROR(coords.append(str.bytes().data(), str.bytes().size()));
            }
        }
        return error_type();
    };

    for (auto&& addr : osm.nodes())
    {
        const auto coord = node.find(addr.index);
        if (!coord)
            continue;
        auto exit = false;
        ICY_ERROR(func(addr.street, addr.house, *coord, exit));
        if (exit)
            return {};
    }
    for (auto&& addr : osm.ways())
    {
        const auto coord = way_coord(node, way, addr.index);
        if (coord.lat || coord.lng)
        {
            auto exit = false;
            ICY_ERROR(func(addr.street, addr.house, coord, exit));
            if (exit)
                return {};
        }
    }
    for (auto&& addr : osm.rels())
    {
        const auto coord = rel_coord(node, way, rel, addr.index);
        if (coord.lat || coord.lng)
        {
            auto exit = false;
            ICY_ERROR(func(addr.street, addr.house, coord, exit));
            if (exit)
                return {};
        }
    }


    return {};
}
error_type main3(heap& heap)
{
    string dir;
    string compare_path;
    string coords_path;
    string fias_path;
    string output_path;
    ICY_ERROR(to_string("%1/%2", dir, root_dir, region));
    ICY_ERROR(to_string("%1/compare.txt"_s, compare_path, root_dir));
    ICY_ERROR(to_string("%1/coords.txt"_s, coords_path, string_view(dir)));
    ICY_ERROR(to_string("%1/fias.txt"_s, fias_path, string_view(dir)));

    ICY_ERROR(to_string("%1/fias.txt"_s, fias_path, string_view(dir)));

    addr_comparer_parser compare;
    ICY_ERROR(parse(compare, compare_path, buffer_size));

    coords_parser coords;
    ICY_ERROR(parse(coords, coords_path, buffer_size));

    fias_addr_parser fias;
    ICY_ERROR(parse(fias, fias_path, buffer_size));

    ICY_SCOPE_EXIT{ compare.save(compare_path); };
    ICY_SCOPE_EXIT{ fias.save(fias_path); };

    array<string_view> skip;

    static auto overwrite_always = false;


    auto progress = 0u;
    for (auto&& coord : coords.data())
    {
        {
            string msg;
            ICY_ERROR(to_string("\r\nProgress: [%1/%2]..."_s, msg, progress++, coords.data().size()));
            ICY_ERROR(console_write(msg));
        }
        const auto city = fias.find(coord.city);
        if (!city)
            continue;

        if (std::find(skip.begin(), skip.end(), coord.street) != skip.end())
            continue;

        auto street = city->streets.end();
        ICY_ERROR(city->find(coord.street, compare, street));
        ICY_ERROR(city->insert(coord.street, compare, street, skip));

        if (street != city->streets.end())
        {
            auto house = street->value.houses.end();
            ICY_ERROR(street->value.find_or_insert(coord.house, house));
            if (house != street->value.houses.end())
            {
                auto overwrite = true;
                if (!overwrite_always && (house->value.coord.lng || house->value.coord.lng))
                {
                    if (house->value.coord.lng != coord.value.lng || house->value.coord.lat != coord.value.lat)
                    {
                        string msg;
                        ICY_ERROR(to_string("\r\nHouse \"%1\" already has coordinates (Lon: %2, Lat: %3). Select: "_s, msg,
                            string_view(house->key), house->value.coord.lng, house->value.coord.lat));
                        ICY_ERROR(msg.appendf("\r\n1. Overwrite (this). New value: (Lon: %1, Lat: %2)."_s, coord.value.lng, coord.value.lat));
                        ICY_ERROR(msg.append("\r\n2. Overwrite (this and all other)."_s));
                        ICY_ERROR(msg.append("\r\n3. Skip.\r\nSelect: "_s));
                        ICY_ERROR(console_write(msg));
                        while (true)
                        {
                            auto exit = false;
                            string str;
                            ICY_ERROR(console_read_line(str, exit));
                            if (exit)
                                return {};
                            auto choice = 0u;
                            str.to_value(choice);
                            
                            if (choice == 3)
                            {
                                overwrite = false;
                                break;
                            }
                            else if (choice == 2)
                            {
                                overwrite_always = true;
                                break;
                            }
                            else if (choice == 1)
                            {
                                break;
                            }
                            else
                            {
                                ICY_ERROR(console_write("\r\nError: invalid choice.\r\nTry again: "_s));
                            }
                        }
                    }

                }
                if (overwrite)
                    house->value.coord = coord.value;
            }
        }
        

    }
    return {};
}

error_type main4(heap& heap)
{
    string compare_path;
    string fias_path[2];
    string output_path;
    string input_path;
    ICY_ERROR(to_string("%1/fias_91.txt"_s, fias_path[0], root_dir));
    ICY_ERROR(to_string("%1/fias_92.txt"_s, fias_path[1], root_dir));
    ICY_ERROR(to_string("%1/points2.csv"_s, output_path, root_dir));
    ICY_ERROR(to_string("%1/points.csv"_s, input_path, root_dir));
    ICY_ERROR(to_string("%1/compare.txt"_s, compare_path, root_dir));

    point_parser input;
    ICY_ERROR(parse(input, input_path, buffer_size));
    
    fias_addr_parser fias[_countof(fias_path)];
    for (auto k = 0_z; k < _countof(fias); ++k)
        ICY_ERROR(parse(fias[k], fias_path[k], buffer_size));

    addr_comparer_parser compare;
    ICY_ERROR(parse(compare, compare_path, buffer_size));

    for (auto&& data : input.data())
    {
        if (data.vals.size() < 6)
            continue;
        array<string_view> words;
        ICY_ERROR(split(data.vals[5], words, ','));
        if (words.size() < 3)
            continue;
        
        array<string_view> city_words;
        ICY_ERROR(split(words[0], city_words));
        if (city_words.empty())
            continue;

        const auto city_name = city_words.back();
        fias_city* city = nullptr;
        for (auto&& addr : fias)
        {
            city = addr.find(city_name);
            if (city)
                break;
        }
        if (!city)
            continue;
        
        auto street = city->streets.end();
        ICY_ERROR(city->find(words[1], compare, street));
        if (street == city->streets.end())
            continue;

        const auto house_delim = words[2].find("."_s);
        string_view house_name;
        if (house_delim == words[2].end())
            house_name = words[2];
        else
            house_name = string_view(house_delim + 2, words[2].end());
        
        auto house = street->value.houses.end();
        ICY_ERROR(street->value.find_or_insert(house_name, house));
        if (house == street->value.houses.end())
            continue;

        ICY_ERROR(to_string(city_name, data.city));
        ICY_ERROR(to_string(street->key, data.street));
        ICY_ERROR(to_string(house->key, data.house));
    }
    ICY_ERROR(input.save(output_path));
    

    return {};
}
int main()
{
    heap gheap;
    if (gheap.initialize(heap_init::global(1_gb)))
        return -1;

    if (const auto error = main4(gheap))
        return error.code;

    return 0;
}
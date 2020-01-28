#include <icy_engine/core/icy_core.hpp>
#include <icy_engine/core/icy_file.hpp>
#include <icy_engine/core/icy_json.hpp>
#include <icy_engine/utility/icy_curl.hpp>
#include <icy_engine/parser/icy_parser_csv.hpp>

#pragma comment(lib, "icy_engine_cored")

using namespace icy;

class unique_csv_parser : public icy::csv_parser
{
public:
    const_array_view<string> data() const noexcept
    {
        return m_data;
    }
private:
    error_type callback(const array_view<string_view> tabs) noexcept override
    {
        string str;
        if (tabs.size() >= 1)
            ICY_ERROR(to_string(tabs[0], str));
        if (!str.empty())
            ICY_ERROR(m_data.push_back(std::move(str)));
        return {};
    }
private:
    array<string> m_data;
};

error_type main2(heap& heap)
{
    ICY_ERROR(curl_initialize());
    ICY_SCOPE_EXIT{ curl_shutdown(); };

    unique_csv_parser csv;
    ICY_ERROR(parse(csv, "C:/VS2015/dat/unique_91.txt"_s, 4_kb));

    //const string_view bbox[4] = { "33.198679"_s, "44.207248"_s, "33.897496"_s, "44.959445"_s };
    const string_view bbox[4] = { "32.299905", "44.20657", "36.752468", "46.229215" };
    const string_view api_key = "c16a8df9-219d-42a6-95ce-fa50a6e2a5b2"_s;

    file output;
    ICY_ERROR(output.open("C:/VS2015/dat/bounds_91.txt"_s, 
        file_access::app, file_open::create_always, file_share::read | file_share::write));

    curl_socket curl;
    for (auto&& str : csv.data())
    {
        string request;
        ICY_ERROR(curl_encode(str, request));

        string url;
        ICY_ERROR(to_string("https://geocode-maps.yandex.ru/1.x?geocode=%1&apikey=%2&bbox=%3,%4~%5,%6&rspn=1&format=json"_s,
            url, string_view(request), api_key, bbox[0], bbox[1], bbox[2], bbox[3]));

        ICY_ERROR(curl.initialize(url));
        ICY_ERROR(curl.exec());

        json json;
        if (json::create(string_view(static_cast<const char*>(curl.data()), curl.size()), json))
            continue;
        if (json.type() != json_type::object)
            continue;

        const auto json_response = json.find("response"_s);
        if (!json_response || json_response->type() != json_type::object)
            continue;

        const auto json_collection = json_response->find("GeoObjectCollection"_s);
        if (!json_collection || json_collection->type() != json_type::object)
            continue;

        const auto json_feature = json_collection->find("featureMember"_s);
        if (!json_feature || json_feature->type() != json_type::array)
            continue;

        for (auto&& json_object : json_feature->vals())
        {
            if (json_object.type() != json_type::object)
                continue;

            const auto json_geo = json_object.find("GeoObject"_s);
            if (!json_geo || json_geo->type() != json_type::object)
                continue;

            const auto json_bound = json_geo->find("boundedBy"_s);
            if (!json_bound || json_bound->type() != json_type::object)
                continue;

            const auto json_envelope = json_bound->find("Envelope"_s);
            if (!json_envelope || json_envelope->type() != json_type::object)
                continue;

            const auto json_lower = json_envelope->find("lowerCorner"_s);
            const auto json_upper = json_envelope->find("upperCorner"_s);
            if (!json_lower || !json_upper
                || json_lower->type() != json_type::string
                || json_upper->type() != json_type::string)
                continue;

            const auto delim_lower = json_lower->get().find(" "_s);
            const auto delim_upper = json_upper->get().find(" "_s);
            if (delim_lower == json_lower->get().end() ||
                delim_upper == json_upper->get().end())
                continue;

            const auto lower_lng = string_view(json_lower->get().begin(), delim_lower);
            const auto lower_lat = string_view(delim_lower + 1, json_lower->get().end());
            const auto upper_lng = string_view(json_upper->get().begin(), delim_upper);
            const auto upper_lat = string_view(delim_upper + 1, json_upper->get().end());
            
            string text;
            ICY_ERROR(to_string("%1\t%2\t%3\t%4\t%5\r\n"_s, text, string_view(str),
                lower_lng, lower_lat, upper_lng, upper_lat));
            ICY_ERROR(output.append(text.bytes().data(), text.bytes().size()));
            break;
        }
    }
    return {};
}

int main()
{
    heap gheap;
    if (gheap.initialize(heap_init::global(64_mb)))
        return -1;

    if (const auto error = main2(gheap))
        return error.code;

    return 0;
}
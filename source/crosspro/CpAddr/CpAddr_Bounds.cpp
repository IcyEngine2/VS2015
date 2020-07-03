#include <icy_engine/core/icy_core.hpp>
#include <icy_engine/core/icy_file.hpp>
#include <icy_engine/core/icy_json.hpp>
#include <icy_engine/utility/icy_curl.hpp>
#include <icy_engine/parser/icy_parser_csv.hpp>
#include <icy_engine/core/icy_console.hpp>
#include <icy_engine/core/icy_thread.hpp>
#if _DEBUG
#pragma comment(lib, "icy_engine_cored")
#else
#pragma comment(lib, "icy_engine_core")
#endif

using namespace icy;

shared_ptr<console_system> con;
class unique_csv_parser : public icy::csv_parser
{
public:
    const_array_view<string> data() const noexcept
    {
        return m_data;
    }
private:
    error_type callback(const const_array_view<string_view> tabs) noexcept override
    {
        string str;
        if (tabs.size() >= 3)
        {
            str.append(tabs[0]);
            str.append(", "_s);
            str.append(tabs[1]);
            str.append(", "_s);
            str.append(tabs[2]);
        }
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
    ICY_ERROR(parse(csv, "C:/VS2015/dat/fias/zel1.txt"_s, 4_kb));

    //const string_view bbox[4] = { "33.198679"_s, "44.207248"_s, "33.897496"_s, "44.959445"_s };
    //const string_view bbox[4] = { "32.299905", "44.20657", "36.752468", "46.229215" };
    const string_view bbox[4] = { "10", "30", "170", "70" };
    const string_view api_key = "c16a8df9-219d-42a6-95ce-fa50a6e2a5b2"_s;

    file output;
    ICY_ERROR(output.open("C:/VS2015/dat/fias/zel2.txt"_s, 
        file_access::app, file_open::create_always, file_share::read | file_share::write));

    curl_socket curl;
    for (auto&& str : csv.data())
    {
        con->write("\r\nQuery: ");
        con->write(str);
        con->write("... "_s);

        auto ms = 1000 + (rand() % 1000);
        sleep(std::chrono::microseconds(ms));

        string request;
        ICY_ERROR(curl_encode(str, request));

        string url;
        ICY_ERROR(to_string("https://geocode-maps.yandex.ru/1.x?geocode=%1&apikey=%2&bbox=%3,%4~%5,%6&rspn=1&format=json"_s,
            url, string_view(request), api_key, bbox[0], bbox[1], bbox[2], bbox[3]));

        ICY_ERROR(curl.initialize(url));
        ICY_ERROR(curl.exec());

        json json;
        if (to_value(string_view(static_cast<const char*>(curl.data()), curl.size()), json))
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

        string text;

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
            

            const auto json_point = json_geo->find("Point"_s);
            if (!json_point || json_point->type() != json_type::object)
                continue;

            const auto json_pos  = json_point->find("pos"_s);
            if (!json_pos || json_pos->type() != json_type::string)
                continue;

            const auto delim_pos = json_pos->get().find(" "_s);
            if (delim_pos == json_pos->get().end())
                continue;

            const auto mid_lng = string_view(json_pos->get().begin(), delim_pos);
            const auto mid_lat = string_view(delim_pos + 1, json_pos->get().end());
            ICY_ERROR(to_string("%1\t%2\t%3\t%4\t%5\t%6\t%7\r\n"_s, text, string_view(str),
                lower_lng, lower_lat, upper_lng, upper_lat, mid_lng, mid_lat));
            break;
        }
        ICY_ERROR(output.append(text.bytes().data(), text.bytes().size()));
        if (!text.empty())
            con->write("Ok!"_s);        
    }
    return {};
}

int main()
{
    heap gheap;
    if (gheap.initialize(heap_init::global(64_mb)))
        return -1;

    create_console(con);
    shared_ptr<thread> con_thread;
    create_event_thread(con_thread, con);
    con_thread->launch();

    if (const auto error = main2(gheap))
    {
        string str;
        to_string(error, str);
        con->write(str);
    }
    else
    {
        con->write("Done!"_s);
    }
    con_thread->wait();
    con = 0;
    return 0;
}
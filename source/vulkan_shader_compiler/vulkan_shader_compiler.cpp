#include <icy_engine/core/icy_file.hpp>
#include <icy_engine/core/icy_memory.hpp>
#include <icy_engine/core/icy_array.hpp>
#include <icy_engine/core/icy_string.hpp>
#include <icy_engine/core/icy_map.hpp>
#include <icy_engine/parser/icy_parser_csv.hpp>

using namespace icy;

#if _DEBUG
#pragma comment(lib, "icy_engine_cored")
#else
#pragma comment(lib, "icy_engine_core")
#endif

error_type main_ex()
{
    //array<string> args;
    //ICY_ERROR(win32_parse_cargs(args));
    
    string path;
    ICY_ERROR(process_directory(path));

    array<string> files;
    ICY_ERROR(enum_files(path, files));

    struct cache_parser : csv_parser
    {
        error_type callback(const const_array_view<string_view> tabs) noexcept
        {
            if (tabs.size() < 2)
                return error_type();

            string fname;
            ICY_ERROR(copy(tabs[0], fname));

            uint64_t hash = 0;
            to_value(tabs[1], hash);
            auto it = data.find(fname);
            if (it == data.end())
                ICY_ERROR(data.insert(std::move(fname), hash, &it));
            it->value = hash;
            return error_type();
        }
        map<string, uint64_t> data;
    };

    string cache_path;
    ICY_ERROR(cache_path.appendf("%1_vsc_cache.txt"_s, string_view(path)));
    cache_parser cache;
    parse(cache, cache_path, default_parser_capacity);
        

    for (auto&& file : files)
    {
        file_name fname;
        ICY_ERROR(fname.initialize(file));
        if (fname.extension == ".vert"_s ||
            fname.extension == ".frag"_s)
        {
            fname.extension = string_view(fname.extension.begin() + 1, fname.extension.end());

            string input_fname;
            ICY_ERROR(input_fname.appendf("%1%2.%3"_s, string_view(path), fname.name, fname.extension));

            const auto read_file = [](const string_view file_name, array<uint8_t>& bytes)
            {
                icy::file input_file;
                ICY_ERROR(input_file.open(file_name, file_access::read, file_open::open_existing, file_share::read));

                size_t size = input_file.info().size;
                ICY_ERROR(bytes.resize(size));
                ICY_ERROR(input_file.read(bytes.data(), size));
                return error_type();
            };

            array<uint8_t> text_data;
            ICY_ERROR(read_file(input_fname, text_data));
           
            const auto h = hash64(text_data.data(), text_data.size());
            const auto it = cache.data.find(input_fname);
            if (it != cache.data.end())
            {
                if (it->value == h)
                    continue;
            }

            string spv_fname;
            ICY_ERROR(spv_fname.appendf("%1%2_%3.spv"_s, string_view(path), fname.name, fname.extension));
            
            string arg;
            ICY_ERROR(arg.appendf("glslc.exe \"%1\" -o \"%2\""_s, string_view(input_fname), string_view(spv_fname)));
            if (system(arg.bytes().data()) == 0)
            {
                if (it == cache.data.end())
                {
                    ICY_ERROR(cache.data.insert(std::move(input_fname), h));
                }
                else
                {
                    it->value = h;
                }

                array<uint8_t> spv_data;
                ICY_ERROR(read_file(spv_fname, spv_data));

                string output_fname;
                ICY_ERROR(output_fname.appendf("%1%2_%3.h"_s, string_view(path), fname.name, fname.extension));

                icy::file output_file;
                ICY_ERROR(output_file.open(output_fname, file_access::app, file_open::create_always, file_share::read));

                string msg;
                ICY_ERROR(msg.appendf("#pragma once\r\nstatic const unsigned char g_%1_%2[]={"_s, fname.name, fname.extension));
                for (auto k = 0_z; k < spv_data.size(); ++k)
                {
                    string hex;
                    ICY_ERROR(to_string_unsigned(spv_data[k], 0x10, hex));
                    if (k)
                    {
                        ICY_ERROR(msg.append(","_s));
                    }
                    ICY_ERROR(msg.append(hex));
                }
                ICY_ERROR(msg.append("};"_s));
                ICY_ERROR(output_file.append(msg.ubytes().data(), msg.ubytes().size()));
            }
        }
    }

    file cache_file;
    ICY_ERROR(cache_file.open(cache_path, file_access::app, file_open::create_always, file_share::read));

    for (auto&& data : cache.data)
    {
        string msg;
        ICY_ERROR(msg.appendf("%1\t%2\r\n"_s, string_view(data.key), data.value));
        ICY_ERROR(cache_file.append(msg.bytes().data(), msg.bytes().size()));
    }

    return error_type();
}

int main()
{
    heap gheap;
    if (const auto error = gheap.initialize(heap_init::global(64_mb)))
        return error.code;

    if (const auto error = main_ex())
        return error.code;

    return 0;
}

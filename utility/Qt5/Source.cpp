#include <icy_engine/core/icy_file.hpp>
#include <icy_engine/core/icy_memory.hpp>
#include <icy_engine/core/icy_string.hpp>
#include <icy_engine/core/icy_array.hpp>
#if _DEBUG
#pragma comment(lib, "icy_engine_cored.lib")
#else
#pragma comment(lib, "icy_engine_core.lib")
#endif

using namespace icy;

error_type func(const string_view path)
{

    array<string> files;
    ICY_ERROR(enum_files_rec(path, files));

    string msg;
    ICY_ERROR(to_string("\r\nFound: [%1] files by path \"%2\""_s, msg, files.size(), path));
    ICY_ERROR(console_write(msg));

    auto index = 0_z;
    auto success = 0_z;
    auto sum = 0_z;
    for (auto&& shortname : files)
    {
        ++index;

        string filename;
        ICY_ERROR(to_string("%1%2"_s, filename, path, string_view(shortname)));
        file_info info;
        ICY_ERROR(info.initialize(filename));

        if (info.type == file_type::dir)
            continue;
        
        string srcname;
        {
            file input;
            ICY_ERROR(input.open(filename, file_access::read, file_open::open_existing, file_share::read));
            array<string> lines;
            ICY_ERROR(input.text(1, lines));
            if (lines.empty())
                continue;

            const auto& line = lines[0];
            if (line.find("#include \""_s) != line.begin() ||
                line.find("../src/"_s) == line.end())
                continue;

            const auto beg = line.find("\""_s) + 1;
            const auto end = line.rfind("\""_s);
            if (beg >= end)
                continue;
            
            ICY_ERROR(to_string("%1%2"_s, srcname, file_name(filename).dir, string_view(beg, end)));
        }

        file src;
        ICY_ERROR(src.open(srcname, file_access::read, file_open::open_existing, file_share::read));
        array<char> bytes;
        ICY_ERROR(bytes.resize(src.info().size));
        auto size = bytes.size();
        ICY_ERROR(src.read(bytes.data(), size));

        file output;
        ICY_ERROR(output.open(filename, file_access::app, file_open::create_always, file_share::none));
        ICY_ERROR(output.append(bytes.data(), size));

        ICY_ERROR(to_string("\r\n[%1]: \"%2\" (%3)"_s, msg, index, string_view(filename), bytes.size()));
        ICY_ERROR(console_write(msg));
        ++success;
        sum += bytes.size();
    }
    ICY_ERROR(to_string("\r\nDone: %1 files processed (%2 kb written)"_s, msg, success, sum / 1024));
    ICY_ERROR(console_write(msg));
    return {};
}

int main()
{
    heap gheap;
    if (const auto error = gheap.initialize(heap_init::global(1_gb)))
        return error.code;
    
    array<string> args;
    if (const auto error = win32_parse_cargs(args))
    {
        console_write("Error parsing cmd args"_s);
        return error.code;
    }
    if (args.size() != 2)
    {
        console_write("Usage: cmd PATH/TO/QT5/INCLUDES"_s);
        return uint32_t(std::errc::invalid_argument);
    }
    if (const auto error = func(args[1]))
    {
        string msg;
        to_string("\r\nError: %1"_s, msg, error);
        console_write(msg);
        return error.code;
    }
    return 0;
}
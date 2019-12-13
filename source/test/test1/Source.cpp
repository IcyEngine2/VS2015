#include <icy_engine/core/icy_file.hpp>
#include <icy_engine/core/icy_memory.hpp>
#include <icy_engine/core/icy_string.hpp>
#include <icy_engine/core/icy_array.hpp>
#include <icy_engine/image/icy_image.hpp>
#include <icy_qtgui/icy_qtgui.hpp>
#if _DEBUG
#pragma comment(lib, "icy_engine_cored.lib")
#pragma comment(lib, "icy_engine_imaged.lib")
#else
#pragma comment(lib, "icy_engine_core.lib")
#pragma comment(lib, "icy_engine_image.lib")
#endif

using namespace icy;

error_type func(heap& heap)
{
    //shared_ptr<gui_queue> queue;
   // ICY_ERROR(gui_queue::create(queue, heap));
    
    image image;
    image.load(heap, "D:/test.jpg"_s, image_type::jpg);

    matrix<color> colors = matrix<color>(image.size().y, image.size().x);
    image.view({}, colors);
    image::save(heap, colors, "D:/test2.png"_s, image_type::png);

    return {};
}
int main()
{   
    heap gheap;
    if (const auto error = gheap.initialize(heap_init::global(1_gb)))
        return error.code;
    
    if (const auto error = func(gheap))
    {
        string msg;
        to_string("Error: %1", msg, error);
        win32_message(msg, "Error"_s);
        return error.code;
    }
    return 0;
}
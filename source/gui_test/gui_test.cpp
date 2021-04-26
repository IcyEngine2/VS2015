#include <icy_gui/icy_gui.hpp>
#include <icy_engine/core/icy_thread.hpp>
#include <icy_engine/core/icy_file.hpp>
#include <icy_engine/graphics/icy_adapter.hpp>
#include <icy_engine/graphics/icy_display.hpp>
#include <icy_engine/graphics/icy_window.hpp>
#if _DEBUG
#pragma comment(lib, "icy_engine_cored")
#pragma comment(lib, "icy_engine_graphicsd")
#pragma comment(lib, "icy_engine_imaged")
#pragma comment(lib, "icy_guid")
#else
#pragma comment(lib, "icy_engine_core")
#pragma comment(lib, "icy_engine_graphics")
#pragma comment(lib, "icy_engine_image")
#pragma comment(lib, "icy_gui")
#endif

using namespace icy;

class gui_default_bind : public gui_data_bind
{
public:
    virtual int compare(const gui_data_read_model& model, const gui_node lhs, const gui_node rhs) const noexcept override
    {
        //string name_lhs;
        //string name_rhs;
        //to_string(model.query(lhs, gui_node_prop::data), name_lhs);
        //to_string(model.query(rhs, gui_node_prop::data), name_rhs);
        //return icy::compare(name_lhs, name_rhs);
        return gui_data_bind::compare(model, lhs, rhs);
    }
    virtual error_type notify(const gui_data_read_model& read, const gui_node node, const gui_node_prop prop, const gui_variant& value, bool& decline) const noexcept override
    {
        return error_type();
    }
};

error_type append_nodes(gui_data_write_model& model, gui_node parent,
    const string_view prefix, const size_t level, size_t& index)
{
    if (level >= 3)
        return error_type();
    for (auto k = 0u; k < 7u; ++k)
    {
        gui_node node;
        ICY_ERROR(model.insert(parent, k, 0, node));
        string str;
        ICY_ERROR(copy(prefix, str));
        ICY_ERROR(str.appendf(" %1. (%2, %3) "_s, prefix, k, 0));
        ICY_ERROR(model.modify(node, gui_node_prop::data, str));
        ICY_ERROR(model.modify(node, gui_node_prop::row_header, index++));
        if (level == 0)
        {
            ICY_ERROR(model.modify(node, gui_node_prop::col_header, "0"_s));
        }
        for (auto n = 0u; n < 3; ++n)
        {
            
            gui_node col_node;
            ICY_ERROR(model.insert(parent, k, 1 + n, col_node));
            string col_name;
            ICY_ERROR(col_name.appendf(" %1.(%2, %3) "_s, prefix, k, 1 + n));
            ICY_ERROR(model.modify(col_node, gui_node_prop::data, col_name));
            if (level == 0)
            {
                ICY_ERROR(model.modify(col_node, gui_node_prop::col_header, n + 1));
            }
        }

        ICY_ERROR(append_nodes(model, node, str, level + 1, index));
    }
    return error_type();
}

error_type main_func()
{
    ICY_ERROR(event_system::initialize());

    array<adapter> gpu;
    ICY_ERROR(adapter::enumerate(adapter_flags::d3d11 | adapter_flags::hardware | adapter_flags::debug, gpu));
    if (gpu.empty())
        return error_type();

    const auto adapter = gpu[0];

    shared_ptr<window_system> window_system;
    ICY_ERROR(create_window_system(window_system));
    ICY_ERROR(window_system->thread().launch());
    ICY_ERROR(window_system->thread().rename("Window Thread"_s));
    
    shared_ptr<window> window;
    ICY_ERROR(window_system->create(window));
    ICY_ERROR(window->show(true));
    ICY_ERROR(window->restyle(window_style::windowed));

    shared_ptr<display_system> display_system;
    ICY_ERROR(create_display_system(display_system, window, adapter));
    ICY_ERROR(display_system->thread().launch());
    ICY_ERROR(display_system->thread().rename("Display Thread"_s));
    ICY_ERROR(display_system->frame(display_frame_vsync));
     
    shared_ptr<gui_system> gui_system;
    ICY_ERROR(create_gui_system(gui_system, adapter));
    ICY_ERROR(gui_system->thread().launch());
    ICY_ERROR(gui_system->thread().rename("GUI Thread"_s));
    
    array<char> bytes;
    {
        icy::file f1;
        f1.open("gui_example.json"_s, file_access::read, file_open::open_existing, file_share::read);
        auto size = f1.info().size;

        bytes.resize(size);
        f1.read(bytes.data(), size);
    }
    shared_ptr<gui_window> gui_window;
    ICY_ERROR(gui_system->create_window(gui_window, window, string_view(bytes.data(), bytes.size())));
    
    shared_ptr<gui_data_write_model> model;
    ICY_ERROR(gui_system->create_model(model));

    gui_node root0, root1;
    ICY_ERROR(model->insert(gui_node(), 0, 0, root0));
    ICY_ERROR(model->insert(gui_node(), 1, 0, root1));

    size_t index = 0;
    ICY_ERROR(append_nodes(*model, root1, ""_s, 0, index));
    

    array<gui_widget> list;
    gui_window->find("class"_s, "edit_model"_s, list);
    for (auto&& x : list)
        gui_system->create_bind(make_unique<gui_default_bind>(), *model, *gui_window, root0, x);

    gui_window->find("class"_s, "tree_model"_s, list);
    for (auto&& x : list)
        gui_system->create_bind(make_unique<gui_default_bind>(), *model, *gui_window, root1, x);

    
    shared_ptr<event_queue> loop;
    ICY_ERROR(create_event_system(loop, 0
        | event_type::gui_render
        | event_type::gui_update
        | event_type::display_update 
        | event_type::window_resize 
        | event_type::global_timer));

    auto query = 0u;
    ICY_ERROR(gui_window->render(gui_widget(), query));

    //timer timer_gui;
    timer timer_dsp;
    //ICY_ERROR(timer_gui.initialize(SIZE_MAX, std::chrono::milliseconds(16)));
    ICY_ERROR(timer_dsp.initialize(SIZE_MAX, std::chrono::milliseconds(10)));
    ICY_ERROR(gui_window->render(gui_widget(), query));

    shared_ptr<texture> overlay;

    while (*loop)
    {
        event event;
        ICY_ERROR(loop->pop(event));
        if (!event)
            continue;

        //ICY_ERROR(gui->process(event));
        if (event->type == event_type::gui_render)
        {
            const auto& event_data = event->data<gui_event>();
            if (event_data.texture)
                overlay = event_data.texture;
        }
        else if (event->type == event_type::gui_update)
        {
            ICY_ERROR(gui_window->render(gui_widget(), query));
        }
        else if (event->type == event_type::window_resize)
        {
            const auto& event_data = event->data<window_message>();
            ICY_ERROR(display_system->resize(event_data.size));
            ICY_ERROR(gui_window->resize(event_data.size));
            ICY_ERROR(gui_window->render(gui_widget(), query));
        }
        else if (event->type == event_type::display_update)
        {
            const auto event_data = event->data<display_message>();
            const auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(event_data.frame);
            string new_name;
            ICY_ERROR(to_string("%1ms [%2]"_s, new_name, ms.count(), event_data.index));
            ICY_ERROR(window->rename(new_name));
            //ICY_ERROR(model->modify(root0, gui_node_prop::data, new_name));
        }
        else if (event->type == event_type::global_timer)
        {
            auto event_data = event->data<timer::pair>();
            if (event_data.timer == &timer_dsp)
            {
                if (overlay)
                    ICY_ERROR(display_system->repaint(*overlay, window_size(0, 0), overlay->size()));
            }
        }
    }
    
    return error_type();
}


int main()
{
    icy::heap gheap;
    gheap.initialize(heap_init::global(2_gb));
    return main_func().code;
}
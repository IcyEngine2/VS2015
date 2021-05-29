#include <icy_engine/core/icy_core.hpp>
#include <icy_engine/core/icy_thread.hpp>
#include <icy_engine/core/icy_file.hpp>
#include <icy_engine/graphics/icy_window.hpp>
#include <icy_gui/icy_gui.hpp>
#include <icy_engine/network/icy_network.hpp>
#include "mbox_server.hpp"
#if _DEBUG
#pragma comment(lib, "icy_engine_graphicsd")
#pragma comment(lib, "icy_engine_networkd")
#pragma comment(lib, "icy_guid")
#else
#pragma comment(lib, "icy_engine_graphics")
#pragma comment(lib, "icy_engine_network")
#pragma comment(lib, "icy_gui")
#endif

using namespace icy;

error_type main_ex(heap& heap)
{
    ICY_ERROR(event_system::initialize());
    ICY_ERROR(blob_initialize());
    ICY_SCOPE_EXIT{ blob_shutdown(); };

    array<char> bytes;
    {
        file file;
        ICY_ERROR(file.open("mbox_server_gui.json"_s, file_access::read, file_open::open_existing, file_share::none));
        size_t size = 0;
        ICY_ERROR(bytes.resize(size = file.info().size));
        ICY_ERROR(file.read(bytes.data(), size));
    }

    shared_ptr<window_system> window_system;
    ICY_ERROR(create_window_system(window_system));
    ICY_ERROR(window_system->thread().launch());
    ICY_ERROR(window_system->thread().rename("Window Thread"_s))

    shared_ptr<window> window;
    ICY_ERROR(window_system->create(window));
    ICY_ERROR(window->restyle(window_style::windowed));

  /*  shared_ptr<display_system> display_system;
    ICY_ERROR(create_display_system(display_system, window, adapter));
    ICY_ERROR(display_system->thread().launch());
    ICY_ERROR(display_system->thread().rename("Display Thread"_s));
    ICY_ERROR(display_system->frame(display_frame_vsync));*/

    shared_ptr<gui_system> gui_system;
    ICY_ERROR(create_gui_system(gui_system));
    ICY_ERROR(gui_system->thread().launch());
    ICY_ERROR(gui_system->thread().rename("GUI Thread"_s));

    shared_ptr<gui_window> gui_window;
    ICY_ERROR(gui_system->create_window(gui_window, window, string_view(bytes.data(), bytes.size())));

    array<gui_widget> widget_tabs;
    array<gui_widget> widget_tree;
    array<gui_widget> widget_info;

    ICY_ERROR(gui_window->find("class"_s, "mbox_tabs_model"_s, widget_tabs));
    ICY_ERROR(gui_window->find("class"_s, "mbox_tree_model"_s, widget_tree));
    ICY_ERROR(gui_window->find("class"_s, "mbox_info_model"_s, widget_info));

    shared_ptr<event_queue> loop;
    ICY_ERROR(create_event_system(loop, 0
        | event_type::gui_any
        | event_type::network_any
        | event_type::window_resize
        | event_type::global_timer
    ));

    ICY_ERROR(window->show(true));

    auto query = 0u;
    ICY_ERROR(gui_window->render(gui_widget(), query));

    shared_ptr<gui_data_write_model> tabs_model;
    shared_ptr<gui_data_write_model> tree_model;
    shared_ptr<gui_data_write_model> info_model;

    ICY_ERROR(gui_system->create_model(tabs_model));
    ICY_ERROR(gui_system->create_model(tree_model));
    ICY_ERROR(gui_system->create_model(info_model));

    
    for (auto&& widget : widget_tabs)
    {
        unique_ptr<gui_data_bind> bind;
        ICY_ERROR(make_unique(gui_data_bind(), bind));
        ICY_ERROR(gui_system->create_bind(std::move(bind), *tabs_model, *gui_window, gui_node(), widget));
    }
    for (auto&& widget : widget_tree)
    {
        unique_ptr<gui_data_bind> bind;
        ICY_ERROR(make_unique(gui_data_bind(), bind));
        ICY_ERROR(gui_system->create_bind(std::move(bind), *tree_model, *gui_window, gui_node(), widget));
    }
    for (auto&& widget : widget_info)
    {
        unique_ptr<gui_data_bind> bind;
        ICY_ERROR(make_unique(gui_data_bind(), bind));
        ICY_ERROR(gui_system->create_bind(std::move(bind), *info_model, *gui_window, gui_node(), widget));
    }

    shared_ptr<gui_data_write_model> menu_model;
    ICY_ERROR(gui_system->create_model(menu_model));

    gui_node root_node;
    ICY_ERROR(tree_model->insert(gui_node(), 0, 0, root_node));

    auto dev_index = 0u;
    auto pc_index = 0u;
    mbox_server_database mbox;
    ICY_ERROR(mbox.load(string_view()));
    ICY_ERROR(mbox.create_base(0u, mbox_type::device_folder, "Devices"_s, dev_index));
    ICY_ERROR(mbox.create_base(dev_index, mbox_type::device, "This PC"_s, pc_index));
    ICY_ERROR(mbox.tree_view(0u, *tree_model, root_node));


    array<network_address> addr_list;
    ICY_ERROR(network_address::query(addr_list, MBOX_MULTICAST_ADDR, MBOX_MULTICAST_PORT));
    if (addr_list.empty())
        return error_type();

    network_server_config config;
    config.capacity = 40;
    config.port = addr_list[0].port();
    shared_ptr<network_system_http_server> http_server;
    ICY_ERROR(create_network_http_server(http_server, config));
    
    shared_ptr<event_thread> http_thread;
    ICY_ERROR(make_shared(http_thread, event_thread()));
    http_thread->system = http_server.get();
    ICY_SCOPE_EXIT{ http_thread->wait(); };
    ICY_ERROR(http_thread->launch());
    ICY_ERROR(http_thread->rename("Http Thread"_s));

    while (*loop)
    {
        event event;
        ICY_ERROR(loop->pop(event));
        if (!event)
            continue;

        if (event->type == event_type::gui_render)
        {
            auto& event_data = event->data<gui_event>();
            ICY_ERROR(window->repaint(event_data.render));
        }
        else if (event->type == event_type::gui_update)
        {
            ICY_ERROR(gui_window->render(gui_widget(), query));
        }
        else if (event->type == event_type::gui_select)
        {
            const auto& event_data = event->data<gui_event>();
            if (event_data.model == menu_model)
            {
                auto value = 0;
                if (event_data.data.get(value))
                {
                    if (value == 1)
                    {
                        network_udp_socket udp;
                        ICY_ERROR(udp.initialize(0));
                        const uint64_t version = (uint64_t(MBOX_VERSION_MAJOR) << 0x20) | MBOX_VERSION_MINOR;
                        ICY_ERROR(udp.send(addr_list[0], { reinterpret_cast<const uint8_t*>(&version), sizeof(version) }));
                    }
                }
            }

        }
        else if (event->type == event_type::gui_context)
        {
            const auto& event_data = event->data<gui_event>();
            auto index = 0u;
            if (event_data.data.get(index))
            {
                menu_model->destroy(gui_node());
                gui_node node1, node2;
                menu_model->insert(gui_node(), 0, 0, node1);
                menu_model->insert(gui_node(), 0, 0, node2);
                menu_model->modify(node1, gui_node_prop::data, "Send Ping"_s);
                menu_model->modify(node2, gui_node_prop::data, "456"_s);
                menu_model->modify(node1, gui_node_prop::user, 1);
                menu_model->modify(node2, gui_node_prop::user, 2);
                gui_node node21;
                menu_model->insert(node2, 0, 0, node21);
                menu_model->modify(node21, gui_node_prop::data, "678"_s);
                menu_model->modify(node21, gui_node_prop::user,  3);

                ICY_ERROR(gui_window->show_menu(*menu_model, gui_node()));

            }

            //gui_window->child;
            //event_data.node = 0;
            
            //gui_widget menu;
            //ICY_ERROR(gui_window->insert(widget, 0, gui_widget_type::menu_popup, gui_widget_layout::vbox, menu));
            //ICY_ERROR(widget_menu.push_back(std::move(menu)));

        }
        else if (event->type == event_type::window_resize)
        {
            const auto& event_data = event->data<window_message>();
            //ICY_ERROR(display_system->resize(event_data.size));
            ICY_ERROR(gui_window->resize(event_data.size));
            ICY_ERROR(gui_window->render(gui_widget(), query));
        }
        else if (
            event->type == event_type::network_connect ||
            event->type == event_type::network_recv)
        {
            const auto& event_data = event->data<network_event>();
            http_response response;
            response.type = http_content_type::application_json;
            ICY_ERROR(http_server->reply(event_data.conn, response));
        }
    }

    return error_type();
}

int main()
{
    heap gheap;
    if (gheap.initialize(heap_init::global(1_gb)))
        return ENOMEM;
    if (const auto error = main_ex(gheap))
        return error.code;
    return 0;
}

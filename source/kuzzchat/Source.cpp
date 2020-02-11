#define ICY_QTGUI_STATIC 1
#include <icy_qtgui/icy_xqtgui.hpp>
#include <icy_engine/core/icy_thread.hpp>
#include <icy_engine/core/icy_json.hpp>
#include <icy_engine/network/icy_network_http.hpp>
#pragma comment(lib, "icy_engine_cored")
#pragma comment(lib, "icy_engine_networkd")
#pragma comment(lib, "icy_qtguid")

using namespace icy;

class application;

struct network_thread : thread
{
    application* app = nullptr;
    error_type run() noexcept override;
};
struct logic_thread : thread
{
    application* app = nullptr;
    error_type run() noexcept override;
};

enum class connect_type
{
    none,
    create,
};
class connect_info
{
public:
    connect_type type = connect_type::none;
    string addr;
    string login;
    uint32_t password = 0;
};

class application
{
    friend network_thread;
    friend logic_thread;
public:
    error_type init() noexcept;
    error_type exec() noexcept;
private:
    error_type run_network() noexcept;
    error_type run_logic() noexcept;
private:
    shared_ptr<gui_queue> m_gui;
    network_thread m_thread_network;
    logic_thread m_thread_logic;
    xgui_widget m_window;
    xgui_widget m_menu;
    xgui_submenu m_menu_main;
    xgui_action m_menu_create;
    xgui_action m_menu_exit;
    xgui_widget m_hsplitter;
    xgui_widget m_vsplitter;
    xgui_widget m_rhs;
    xgui_widget m_view_chat;
    xgui_widget m_input_box;
    xgui_widget m_input_edit;
    xgui_widget m_input_send;
    xgui_widget m_view_users;
};


error_type connect(connect_info& conn, bool& ok)
{
    xgui_widget window;
    xgui_widget label_addr;
    xgui_widget label_login;
    xgui_widget label_pass;
    xgui_widget edit_addr;
    xgui_widget edit_login;
    xgui_widget edit_pass;
    xgui_widget buttons;
    xgui_widget button_ok;
    xgui_widget button_cancel;

    string addr;
    string login;
    auto pass = 0u;
    ICY_ERROR(to_string(conn.addr, addr));
    ICY_ERROR(to_string(conn.login, login));
    pass = conn.password;

    ICY_ERROR(window.initialize(gui_widget_type::dialog, {}, gui_widget_flag::layout_grid));
    ICY_ERROR(label_addr.initialize(gui_widget_type::label, window));
    ICY_ERROR(label_login.initialize(gui_widget_type::label, window));
    ICY_ERROR(label_pass.initialize(gui_widget_type::label, window));
    ICY_ERROR(edit_addr.initialize(gui_widget_type::line_edit, window));
    ICY_ERROR(edit_login.initialize(gui_widget_type::line_edit, window));
    ICY_ERROR(edit_pass.initialize(gui_widget_type::line_edit, window));
    ICY_ERROR(buttons.initialize(gui_widget_type::none, window, gui_widget_flag::layout_hbox));
    ICY_ERROR(button_ok.initialize(gui_widget_type::text_button, buttons));
    ICY_ERROR(button_cancel.initialize(gui_widget_type::text_button, buttons));

    ICY_ERROR(label_addr.text("IP Address"_s));
    ICY_ERROR(label_login.text("Username"_s));
    ICY_ERROR(label_pass.text("Password"_s));
    ICY_ERROR(edit_addr.text(conn.addr));
    ICY_ERROR(edit_login.text(conn.login));
    ICY_ERROR(button_ok.text("Ok"_s));
    ICY_ERROR(button_cancel.text("Cancel"_s));
    
    ICY_ERROR(label_addr.insert(gui_insert(0, 0)));
    ICY_ERROR(label_login.insert(gui_insert(0, 1)));
    ICY_ERROR(label_pass.insert(gui_insert(0, 2)));
    ICY_ERROR(edit_addr.insert(gui_insert(1, 0)));
    ICY_ERROR(edit_login.insert(gui_insert(1, 1)));
    ICY_ERROR(edit_pass.insert(gui_insert(1, 2)));
    ICY_ERROR(buttons.insert(gui_insert(0, 3, 2, 1)));

    ICY_ERROR(window.show(true));

    shared_ptr<event_loop> loop;
    ICY_ERROR(event_loop::create(loop, event_type::gui_update | event_type::window_close));
    
    while (true)
    {
        event event;
        ICY_ERROR(loop->loop(event));
        if (event->type == event_type::window_close ||
            event->type == event_type::global_quit)
            break;

        if (event->type == event_type::gui_update)
        {
            auto& event_data = event->data<gui_event>();
            if (event_data.widget == button_ok)
            {
                ok = true;
                conn.addr = std::move(addr);
                conn.login = std::move(login);
                conn.password = pass;
                break;
            }
            else if (event_data.widget == button_cancel)
            {
                break;
            }
            else if (event_data.widget == edit_addr)
            {
                ICY_ERROR(to_string(event_data.data.as_string(), addr));
            }
            else if (event_data.widget == edit_login)
            {
                ICY_ERROR(to_string(event_data.data.as_string(), login));
            }
            else if (event_data.widget == edit_pass)
            {
                pass = hash(event_data.data.as_string());
            }
        }

    }
    return {};
}

error_type network_thread::run() noexcept
{
    return app->run_network();
}
error_type logic_thread::run() noexcept
{
    return app->run_logic();
}

error_type application::init() noexcept
{
    ICY_ERROR(create_gui(m_gui));
    global_gui = m_gui.get();

    ICY_ERROR(m_window.initialize(gui_widget_type::window, {}));
    ICY_ERROR(m_menu.initialize(gui_widget_type::menubar, m_window));
    ICY_ERROR(m_menu_main.initialize("Main"_s));
    ICY_ERROR(m_menu_create.initialize("Create"_s));
    ICY_ERROR(m_menu_exit.initialize("Exit"_s));
    ICY_ERROR(m_hsplitter.initialize(gui_widget_type::hsplitter, m_window));
    ICY_ERROR(m_vsplitter.initialize(gui_widget_type::vsplitter, m_hsplitter));
    ICY_ERROR(m_view_chat.initialize(gui_widget_type::text_edit, m_vsplitter));
    ICY_ERROR(m_input_box.initialize(gui_widget_type::none, m_vsplitter, gui_widget_flag::layout_hbox));
    ICY_ERROR(m_input_edit.initialize(gui_widget_type::text_edit, m_input_box));
    ICY_ERROR(m_input_send.initialize(gui_widget_type::text_button, m_input_box));
    ICY_ERROR(m_view_users.initialize(gui_widget_type::list_view, m_hsplitter));

    ICY_ERROR(m_menu.insert(m_menu_main.action));
    ICY_ERROR(m_menu_main.widget.insert(m_menu_create));
    ICY_ERROR(m_menu_main.widget.insert(m_menu_exit));

    m_thread_logic.app = this;
    m_thread_network.app = this;

    return {};
}
error_type application::exec() noexcept
{
    ICY_SCOPE_EXIT{ event::post(nullptr, event_type::global_quit); };
    ICY_SCOPE_EXIT{ m_thread_network.wait(); };
    ICY_SCOPE_EXIT{ m_thread_logic.wait(); };

    ICY_ERROR(m_thread_network.launch());
    ICY_ERROR(m_thread_logic.launch());

    ICY_ERROR(m_window.show(true));
    ICY_ERROR(m_gui->loop());
    return {};
}
error_type application::run_network() noexcept
{
    ICY_SCOPE_EXIT{ event::post(nullptr, event_type::global_quit); };
    network_system_tcp tcp;
    ICY_ERROR(tcp.initialize());
    ICY_ERROR(tcp.launch(0, network_address_type::ip_v4, 0));

    network_tcp_connection conn(network_connection_type::http);
    
    shared_ptr<event_loop> loop;
    ICY_ERROR(event_loop::create(loop, event_type::user));

    conn.timeout(max_timeout);

    while (true)
    {
        event event;
        ICY_ERROR(loop->loop(event));
        if (event->type == event_type::global_quit)
            break;

        auto data = event->data<connect_info*>();
        if (data->type == connect_type::create)
        {
            const auto delim = data->addr.find(":"_s);
            const auto ip = string_view(data->addr.begin(), delim);
            const auto port = string_view(delim + 1, data->addr.end());

            json json = json_type::object;
            ICY_ERROR(json.insert("user"_s, data->login));
            ICY_ERROR(json.insert("pass"_s, data->password));
            ICY_ERROR(json.insert("type"_s, 1));

            string json_str;
            ICY_ERROR(to_string(json, json_str));
                        
            http_request request;
            request.type = http_request_type::post;
            request.body = std::move(json_str);
            request.content = http_content_type::application_json;

            string request_str;
            ICY_ERROR(to_string(request, request_str));

            array<network_address> addr;
            ICY_ERROR(tcp.address(ip, port, network_default_timeout, network_socket_type::TCP, addr));
            if (addr.empty())
                continue;

            ICY_ERROR(tcp.connect(conn, addr[0], request_str.ubytes()));
            
            network_tcp_reply reply;
            auto code = 0u;
            ICY_ERROR(tcp.loop(reply, code));
            
            ICY_ERROR(tcp.recv(conn, 4_kb));
            ICY_ERROR(tcp.loop(reply, code));

            http_response response;
            const_array_view<uint8_t> body;
            //response.initialize(string_view(reinterpret_cast<const char*>(reply.bytes.data()), 
             //   reply.bytes.size()), body));

            continue;
        }
    }
    return {};
}
error_type application::run_logic() noexcept
{
    ICY_SCOPE_EXIT{ event::post(nullptr, event_type::global_quit); };

    shared_ptr<event_loop> loop;
    ICY_ERROR(event_loop::create(loop, event_type::gui_any | event_type::window_close));

    connect_info info;

    while (true)
    {
        event event;
        ICY_ERROR(loop->loop(event));
        if (event->type == event_type::global_quit)
            break;

        auto& event_data = event->data<gui_event>();

        if (event->type == event_type::window_close)
        {
            if (event_data.widget == m_window)
                break;
        }
        else if (event->type == event_type::gui_action)
        {
            gui_action action;
            action.index = event_data.data.as_uinteger();
            if (action == m_menu_exit)
            {
                break;
            }
            else if (action == m_menu_create)
            {
                auto ok = false;
                ICY_ERROR(connect(info, ok));
                if (ok)
                {
                    info.type = connect_type::create;
                    ICY_ERROR(event::post(nullptr, event_type::user, &info));
                }
            }
            
        }
    }
    return {};
}

error_type main2(heap& heap)
{
    application app;
    ICY_ERROR(app.init());
    ICY_ERROR(app.exec());
    return {};
}

gui_queue* icy::global_gui = nullptr;

int main()
{
    using namespace icy;
    
    heap gheap;
    gheap.initialize(heap_init::global(64_mb));
    main2(gheap);
    return 0;
}
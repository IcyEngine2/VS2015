#include <icy_engine/core/icy_string.hpp>
#include <icy_engine/core/icy_array.hpp>
#include <icy_engine/core/icy_thread.hpp>
#include <icy_engine/graphics/icy_window.hpp>
#include <icy_engine/graphics/icy_graphics.hpp>
#include <icy_engine/network/icy_network.hpp>
#include "../icy_mbox.hpp"

#pragma comment(lib, "icy_engine_cored")
#pragma comment(lib, "icy_engine_graphicsd")
#pragma comment(lib, "icy_engine_networkd")

using namespace icy;

uint32_t show_error(const error_type error, const string_view text) noexcept;

class display_thread : public thread
{
public:
    error_type init(HWND__& handle) noexcept;
    error_type run() noexcept override;
private:
    array<adapter> m_adapters;
    shared_ptr<display> m_display;
};
class network_thread : public thread
{
public:
    error_type run() noexcept override;
};
class mbox_application
{
public:
    error_type init() noexcept;
    error_type exec() noexcept;
private:
    shared_ptr<window> m_window;
    display_thread m_display;
    network_thread m_network;
};

uint32_t show_error(const error_type error, const string_view text) noexcept
{
    string msg;
    to_string("Error: %4 - %1 code [%2]: %3", msg, error.source, long(error.code), error, text);
    win32_message(msg, "Error"_s);
    return error.code;
}

int main()
{
    const auto gheap_capacity = 256_mb;

    heap gheap;
    if (const auto error = gheap.initialize(heap_init::global(gheap_capacity)))
        return error.code;

    mbox_application app;
    if (const auto error = app.init())
        return show_error(error, "init application"_s);

    if (const auto error = app.exec())
        return show_error(error, "exec application"_s);

    return 0;
}


error_type mbox_application::init() noexcept
{
    ICY_ERROR(window::create(m_window, window_flags::quit_on_close));
    ICY_ERROR(m_window->rename("Icy MBox Test v1.0"_s));
    ICY_ERROR(m_display.init(*m_window->handle()));
    return {};
}
error_type mbox_application::exec() noexcept
{
    ICY_SCOPE_EXIT{ event::post(nullptr, event_type::global_quit); };
    ICY_ERROR(m_window->restyle(window_style::windowed));
    ICY_ERROR(m_display.launch());
    ICY_ERROR(m_network.launch());
    ICY_ERROR(m_window->loop(std::chrono::milliseconds(0)));
    ICY_ERROR(m_display.wait());
    ICY_ERROR(m_network.wait());
    return {};
}
error_type display_thread::init(HWND__& handle) noexcept
{
    ICY_ERROR(adapter::enumerate(adapter_flag::d3d11, m_adapters));
    if (m_adapters.empty())
        return make_unexpected_error();
    ICY_ERROR(display::create(m_display, m_adapters[0]));
    ICY_ERROR(m_display->bind(&handle));
    return {};
}
error_type display_thread::run() noexcept
{
    ICY_SCOPE_EXIT{ event::post(nullptr, event_type::global_quit); };
    ICY_ERROR(m_display->loop());
    return {};
}
error_type network_thread::run() noexcept
{
   /* ICY_SCOPE_EXIT{ event::post(nullptr, event_type::global_quit); };
    network_system_udp network;
    ICY_ERROR(network.initialize());
    array<network_address> address;
    ICY_ERROR(network.address(mbox::multicast_addr, mbox::multicast_port,
        max_timeout, network_socket_type::UDP, address));
    ICY_ERROR(network.launch(1844, network_address_type::ip_v4, 0));
    ICY_ERROR(network.multicast(address[0], true));
    ICY_ERROR(network.recv(128));
    icy::network_udp_request request;
    auto exit = false;
    while (!exit)
    {
        ICY_ERROR(network.loop(request, exit));
        if (request.type == network_request_type::recv)
            continue;
    }*/
    return {};
}
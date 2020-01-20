#include "../detail/icy_dxgi.hpp"
#include "../detail/icy_win32.hpp"
#include "../icy_mbox_network.hpp"
#include <icy_engine/core/icy_thread.hpp>
#include <icy_engine/core/icy_queue.hpp>
#include <icy_engine/core/icy_file.hpp>
#include <icy_engine/core/icy_event.hpp>
#include <icy_engine/image/icy_image.hpp>
#include <icy_engine/network/icy_network.hpp>
#include <icy_engine/utility/icy_minhook.hpp>
#include <icy_engine/core/icy_input.hpp>
#if _DEBUG
#pragma comment(lib, "icy_engine_cored")
#pragma comment(lib, "icy_engine_imaged")
#pragma comment(lib, "icy_engine_networkd")
#else
#pragma comment(lib, "icy_engine_core")
#pragma comment(lib, "icy_engine_image")
#pragma comment(lib, "icy_engine_network")
#endif
#pragma comment(lib, "dxgi")

using namespace icy;

extern "C" unsigned long GetCurrentProcessId();
extern "C" __declspec(dllimport) void __stdcall FreeLibraryAndExitThread(HINSTANCE__ * hLibModule, unsigned long dwExitCode);


#define ICY_MBOX_LOG(STR, ERROR)    \
    if (const auto error = ERROR)   \
        return log(STR, ERROR);     \
    else ICY_ERROR(log(STR));

static const auto network_code_update = 1u;
class network_udp_thread : public thread
{
public:
    error_type run() noexcept override;
};
class network_tcp_thread : public thread
{
public:
    error_type run() noexcept override;
public:
    network_address address;
};

class mbox_main_thread : public icy::thread
{
public:
    error_type run() noexcept override;
};
class mbox_hook : public window_hook
{
public:
    error_type callback(const icy::input_message& msg, bool& cancel) noexcept override;
};
class mbox_application
{
    friend mbox_hook;
    friend mbox_main_thread;
    friend network_udp_thread;
    friend network_tcp_thread;
    enum class pause_type : uint32_t
    {
        begin,
        end,
        toggle,
    };
public:
    error_type initialize() noexcept;
    error_type launch(IDXGISwapChain& chain) noexcept;
    error_type log(const string_view msg, const error_type code) noexcept;
    error_type exec() noexcept;
private:
    error_type dxgi_callback(IDXGISwapChain* chain, unsigned sync, unsigned flags) noexcept;    
    error_type window_callback(const input_message& input, bool& cancel) noexcept;
private:
    heap m_heap;
    string m_log;
    IDXGISwapChain* m_chain = nullptr;   
    mbox_hook m_window_hook;
    HWND__* m_hwnd;
    hook<dxgi_present> m_dxgi_hook;
    mbox::info m_info;
    mbox_main_thread m_main;
    network_system_udp m_udp_network;
    network_system_tcp m_tcp_network;
    network_udp_thread m_udp_thread;
    //detail::spin_lock<> m_lock;
    mpsc_queue<array<rectangle>> m_recv_image;
    //mpsc_queue<array<input_message>> m_recv_input;
    mpsc_queue<array<matrix<color>>> m_send_image;
    mpsc_queue<array<input_message>> m_send_input;
    bool m_paused = false;
    array<std::pair<key_message, pause_type>> m_pause;
};
static mbox_application& instance()
{
    static mbox_application global;
    return global;
}
static std::atomic<bool> g_init = false;
error_type mbox_main_thread::run() noexcept
{
    return instance().exec();
}

extern "C" HINSTANCE__ * __stdcall GetModuleHandleA(const char* lpModuleName);

int main()
{
    if (source == dll_main::thread_attach || source == dll_main::process_attach)
    {
        auto expected = false;
        if (g_init.compare_exchange_strong(expected, true))
        {
            instance().initialize();
        }
    }
   /* else if (source == dll_main::process_detach)
    {
        return 1;
    }*/
    return 1;
}

error_type mbox_application::launch(IDXGISwapChain& chain) noexcept
{
    if (m_chain)
        return {};
    m_chain = &chain;
    return m_main.launch();
}
error_type mbox_application::initialize() noexcept
{
    this->~mbox_application();
    new (this) mbox_application;
    
    ICY_ERROR(m_heap.initialize(heap_init::global(64_mb)));
    
    ICY_ERROR(m_pause.push_back({ key_message(key::slash, key_event::press), pause_type::begin }));
    ICY_ERROR(m_pause.push_back({ key_message(key::esc, key_event::press), pause_type::end }));
    ICY_ERROR(m_pause.push_back({ key_message(key::enter, key_event::press), pause_type::toggle }));
    ICY_ERROR(m_pause.push_back({ key_message(key::p, key_event::press, key_mod(key_mod::lctrl | key_mod::rctrl)), pause_type::toggle }));

    string libname;
    ICY_ERROR(icy::process_name(win32_instance(), libname));
    string dirname;
    ICY_ERROR(to_string("%1mbox_logs/"_s, dirname, file_name(libname).directory));
    make_directory(dirname);
    ICY_ERROR(to_string("%1%2%3%4"_s, m_log, string_view(dirname), "mbox_log_"_s, uint32_t(GetCurrentProcessId()), ".txt"_s));
    ICY_ERROR(log("Startup"_s, error_type()));

    const auto module = GetModuleHandleA("dxgi");
    const auto offset = 17152u;
    const auto dxgi_callback = [](IDXGISwapChain* chain, unsigned sync, unsigned flags)
    {
        instance().launch(*chain);
        instance().dxgi_callback(chain, sync, flags);
        return 0l;
    };
    if (const auto error = m_dxgi_hook.initialize(dxgi_present(reinterpret_cast<uint8_t*>(module) + offset), dxgi_callback))
        return log("Minhook initialize (DXGI SwapChain)", error);
    
    return {};
}

error_type mbox_application::exec() noexcept
{
    auto module = win32_instance();
    ICY_SCOPE_EXIT
    {
        m_udp_network.stop(network_code_exit);
        m_dxgi_hook.shutdown();
        m_window_hook.shutdown();
        g_init.store(false);
    };
    window_size wsize;
    string pname_long;
    string pname_short;
    string cname;
    string wname;
    ICY_ERROR(computer_name(cname));
    ICY_ERROR(process_name(nullptr, pname_long));
    ICY_ERROR(to_string(file_name(pname_long).name, pname_short));
    ICY_ERROR(dxgi_window(*m_chain, wname, wsize, m_hwnd));
    ICY_ERROR(m_window_hook.initialize(m_hwnd));
    m_info.key.pid = GetCurrentProcessId();
    m_info.key.hash = hash(cname);

    copy(cname, instance().m_info.computer_name);
    copy(wname, instance().m_info.window_name);
    copy(pname_short, instance().m_info.process_name);

    ICY_ERROR(instance().m_udp_network.initialize());
    array<network_address> address;
    ICY_ERROR(instance().m_udp_network.address(mbox::multicast_addr, mbox::multicast_port, max_timeout, network_socket_type::UDP, address));
    if (address.empty())
        return make_stdlib_error(std::errc::bad_address);

    auto port = 0ui16;
    ICY_ERROR(mbox::multicast_port.to_value(port));
    ICY_ERROR(m_udp_network.launch(port, network_address_type::ip_v4, 0));
    ICY_ERROR(m_udp_network.multicast(address[0], true));
    ICY_ERROR(m_udp_thread.launch());
    ICY_ERROR(m_udp_thread.wait());
    return {};
}
error_type mbox_application::dxgi_callback(IDXGISwapChain* chain, unsigned sync, unsigned flags) noexcept
{
    ICY_SCOPE_EXIT{ m_dxgi_hook(chain, sync, flags); };
    
    array<rectangle> rects;
    {
        array<rectangle> tmp_rects;
        while (m_recv_image.pop(tmp_rects))
        {
            ICY_ERROR(rects.append(tmp_rects));
        }
    }

    if (!rects.empty())
    {
        array<matrix<color>> colors;
        ICY_ERROR(dxgi_copy(*chain, rects, colors));
        ICY_ERROR(m_send_image.push(std::move(colors)));
        ICY_ERROR(m_tcp_network.stop(network_code_update));
    }

    return {};
}
error_type mbox_application::window_callback(const input_message& input, bool& cancel) noexcept
{
    cancel = false;
    auto new_paused = m_paused;
    if (input.type == input_type::key)
    {
        for (auto&& pause_input : m_pause)
        {
            if (pause_input.first.key == input.key.key && pause_input.first.event == input.key.event)
            {
                if (key_mod_and(pause_input.first.mod, input.key.mod))
                {
                    switch (pause_input.second)
                    {
                    case pause_type::begin:
                        new_paused = true;
                        break;
                    case pause_type::end:
                        new_paused = false;
                        break;
                    case pause_type::toggle:
                        new_paused = !new_paused;
                        break;
                    }
                }
            }
        }
    }
    auto changed = m_paused != new_paused;
    m_paused = new_paused;
    auto send = false;

    switch (input.type)
    {
    case input_type::text:
        if (!new_paused && !changed)
            cancel = true;
        break;

    case input_type::active:
        if (!new_paused || changed)
            send = true;
        break;

    case input_type::mouse:
        if (!new_paused)
            send = true;
        break;

    case input_type::key:
        if (!new_paused && !changed)
            cancel = send = true;
        break;
    }

    if (send)
    {
        array<input_message> vec;
        ICY_ERROR(vec.push_back(input));
        ICY_ERROR(m_send_input.push(std::move(vec)));
        ICY_ERROR(m_tcp_network.stop(network_code_update));
    }
    if (changed)
    {
        const auto defname = string_view(m_info.window_name,
            strnlen(m_info.window_name, _countof(m_info.window_name)));

        string str;

        if (m_paused)
            ICY_ERROR(str.append("[PAUSED] "_s));

        ICY_ERROR(str.append(defname));
        ICY_ERROR(m_window_hook.rename(str));
    }
    return {};
}
error_type mbox_application::log(const string_view msg, const error_type code) noexcept
{
    static auto first = true;
    string time_str;
    ICY_ERROR(to_string(icy::clock_type::now(), time_str));

    string str;
    if (first)
    {
        ICY_ERROR(to_string("[%1]: %2"_s, str, string_view(time_str), msg));
    }
    else if (code)
    {
        ICY_ERROR(to_string("\r\n[%1]: %2 error (source: %3), (code: %4) \"%5\""_s,
            str, string_view(time_str), msg, code.source, code.code, code));
    }
    else
    {
        ICY_ERROR(to_string("\r\n[%1]: %2"_s, str, string_view(time_str), msg));
    }
    first = false;
    file log;
    ICY_ERROR(log.open(m_log, file_access::app, file_open::open_always, file_share::read | file_share::write));
    ICY_ERROR(log.append(str.bytes().data(), str.bytes().size()));
    return {};
}
error_type network_udp_thread::run() noexcept
{
    auto& network = instance().m_udp_network;
    auto code = 0u;
    network_tcp_thread tcp;
    ICY_SCOPE_EXIT{ instance().m_tcp_network.stop(network_code_exit); tcp.wait(); };

    while (true)
    {
        ICY_ERROR(network.recv(sizeof(mbox::ping)));
        network_udp_request request;
        ICY_ERROR(network.loop(request, code));
        if (code == network_code_exit)
            break;
        if (request.type == network_request_type::recv)
        {
            if (request.bytes.size() != sizeof(mbox::ping))
                continue;

            const auto func = [&request, &network, &tcp]
            {
                const auto ping = reinterpret_cast<const mbox::ping*>(request.bytes.data());
                if (ping->version != mbox::protocol_version)
                    return make_stdlib_error(std::errc::protocol_error);

                const auto str = string_view(ping->address, strnlen(ping->address, sizeof(ping->address)));
                const auto delim = str.find(":"_s);
                if (delim == str.end())
                    return make_stdlib_error(std::errc::bad_address);

                const auto host = string_view(str.begin(), delim);
                const auto port = string_view(delim + 1, str.end());
                array<network_address> addr;
                ICY_ERROR(network.address(host, port, network_default_timeout, network_socket_type::TCP, addr));
                if (addr.empty())
                    return make_stdlib_error(std::errc::bad_address);

                tcp.address = std::move(addr[0]);
                return error_type();
            };
            ICY_ERROR(instance().log("Resolve TCP server address"_s, func()));
            instance().m_tcp_network.stop(network_code_exit);
            if (tcp.state() == thread_state::run)
            {
                ICY_ERROR(instance().log("Shutdown TCP thread"_s, tcp.wait()));
            }
            ICY_ERROR(instance().log("Launch TCP thread"_s, tcp.launch()));
        }
    }
    return {};
}
error_type mbox_hook::callback(const input_message& input, bool& cancel) noexcept
{
    if (const auto error = instance().window_callback(input, cancel))
        instance().log("Window Callback"_s, error);
    return {};
}

error_type network_tcp_thread::run() noexcept
{
    auto& network = instance().m_tcp_network;
    const auto log = [](const string_view str, const error_type error = error_type())
    {
        return instance().log(str, error);
    };
    ICY_MBOX_LOG("TCP Network initialize"_s, network.initialize());

    auto now = clock_type::now();
    auto retry = 1u;
   // const auto retry_regen = std::chrono::seconds(5);
    for (auto k = 0u; k < retry; ++k)
    {
        //if (clock_type::now() - now > retry_regen)
        //    retry += 1;
       // now = clock_type::now();

        ICY_MBOX_LOG("TCP Network launch"_s, network.launch(0, network_address_type::ip_v4, 0));

        network_tcp_connection conn(network_connection_type::none);
        ICY_MBOX_LOG("Begin connect"_s, network.connect(conn, address,
            { reinterpret_cast<const uint8_t*>(&instance().m_info), sizeof(instance().m_info) }));

        while (true)
        {
            network_tcp_reply reply;
            auto code = 0u;
            ICY_MBOX_LOG("Await connect"_s, network.loop(reply, code));
            if (code == network_code_exit)
                return {};
            if (reply.error)
                return log("Connect fail"_s, reply.error);
            else if (reply.type == network_request_type::connect)
                break;
        }
        conn.timeout(max_timeout);
        ICY_ERROR(log("Connect OK"));

        if (const auto error = network.recv(conn, sizeof(mbox::command)))
            return log("Recv command"_s, error);

        auto sending = false;
        array<uint8_t> send_buffer;

        mbox::command last_cmd;

        while (true)
        {
            network_tcp_reply reply;
            auto code = 0u;

            if (const auto error = network.loop(reply, code))
                return log("Network loop"_s, error);

            if (code == network_code_exit)
                return log("Network loop cancel"_s, error_type());

            if (code == network_code_update)
            {
                array<input_message> all_input;
                array<input_message> vec_input;

                while (instance().m_send_input.pop(vec_input))
                    ICY_ERROR(all_input.append(vec_input));

                if (!all_input.empty())
                {
                    mbox::command cmd;
                    cmd.type = mbox::command_type::input;
                    cmd.size = uint32_t(all_input.size() * sizeof(input_message));
                    if (const auto error = send_buffer.append(const_array_view<uint8_t>{ reinterpret_cast<const uint8_t*>(&cmd), sizeof(cmd) }))
                        return log("Append send buffer"_s, error);
                }
                for (auto&& msg : all_input)
                {
                    if (const auto error = send_buffer.append(const_array_view<uint8_t>{ reinterpret_cast<const uint8_t*>(&msg), sizeof(msg) }))
                        return log("Append send buffer"_s, error);
                }
                array<matrix<color>> vec_image;
                while (instance().m_send_image.pop(vec_image))
                {
                    for (auto&& msg : vec_image)
                    {
                        array<uint8_t> img_buffer;
                        if (const auto error = image::save(detail::global_heap, msg, image_type::png, img_buffer))
                            return log("Save image to png"_s, error);

                        mbox::command cmd;
                        cmd.type = mbox::command_type::image;
                        cmd.size = uint32_t(img_buffer.size());
                        if (const auto error = send_buffer.append(const_array_view<uint8_t>{ reinterpret_cast<const uint8_t*>(&cmd), sizeof(cmd) }))
                            return log("Append send buffer"_s, error);
                        if (const auto error = send_buffer.append(img_buffer))
                            return log("Append send buffer"_s, error);
                    }
                }
            }
            else if (reply.type == network_request_type::recv)
            {
                if (reply.error)
                    return log("Process recv command"_s, reply.error);

                if (last_cmd.type == mbox::command_type::none)
                {
                    if (reply.bytes.size() != sizeof(mbox::command))
                        return log("Recv bad command size"_s, make_stdlib_error(std::errc::message_size));
                    const auto cmd = reinterpret_cast<const mbox::command*>(reply.bytes.data());
                    if (cmd->version != mbox::protocol_version)
                        return log("Recv bad protocol", make_stdlib_error(std::errc::protocol_error));
                    if (cmd->type == mbox::command_type::exit)
                    {
                        return instance().m_udp_network.stop(network_code_exit);
                    }
                    else if (cmd->size && (false
                        || cmd->type == mbox::command_type::input
                        || cmd->type == mbox::command_type::image
                        || cmd->type == mbox::command_type::profile))
                    {
                        last_cmd.type = cmd->type;
                        last_cmd.size = cmd->size;
                        if (const auto error = network.recv(conn, cmd->size))
                            return log("Try recv buffer"_s);
                    }
                    else
                        return log("Recv bad command type", make_stdlib_error(std::errc::illegal_byte_sequence));
                }
                else if (reply.bytes.size() == last_cmd.size)
                {
                    if (last_cmd.type == mbox::command_type::input)
                    {
                        const auto ptr = reinterpret_cast<const input_message*>(reply.bytes.data());
                        const auto len = reply.bytes.size() / sizeof(*ptr);
                        const auto vec = const_array_view<input_message>(ptr, len);                      
                        if (const auto error = instance().m_window_hook.send(vec))
                            return log("Append recv input buffer"_s, error);
                     
                    }
                    else if (last_cmd.type == mbox::command_type::image)
                    {
                        const auto ptr = reinterpret_cast<const rectangle*>(reply.bytes.data());
                        array<rectangle> vec;
                        ICY_ERROR(vec.assign(ptr, ptr + reply.bytes.size() / sizeof(*ptr)));
                        if (const auto error = instance().m_recv_image.push(std::move(vec)))
                            return log("Append recv image buffer"_s, error);
                    }
                    else if (last_cmd.type == mbox::command_type::profile)
                    {
                        const auto ptr = reinterpret_cast<const mbox::info*>(reply.bytes.data());
                        if (reply.bytes.size() == sizeof(*ptr))
                        {
                            instance().m_info.profile = ptr->profile;
                            auto name = string_view(ptr->window_name, strnlen(ptr->window_name, _countof(ptr->window_name)));
                            memcpy(instance().m_info.window_name, name.bytes().data(), name.bytes().size());
                            if (const auto error = instance().m_window_hook.rename(name))
                                return log("Set profile"_s, error);
                        }
                    }
                    last_cmd = {};
                    if (const auto error = network.recv(conn, sizeof(mbox::command)))
                        return log("Try recv buffer"_s);
                }
                else
                {
                    return log("Recv partial command (size)", make_stdlib_error(std::errc::illegal_byte_sequence));
                }
            }
            else if (reply.type == network_request_type::send)
            {
                if (reply.error)
                    return log("Process send command"_s, reply.error);
                sending = false;
            }
            else if (reply.type == network_request_type::disconnect || reply.type == network_request_type::shutdown)
            {
                break;
                // return log("Disconnected"_s, reply.error);
            }

            if (!sending && !send_buffer.empty())
            {
                if (const auto error = network.send(conn, send_buffer))
                    return log("Try send buffer"_s, error);
                send_buffer.clear();
            }
        }
    }
    return {};
}
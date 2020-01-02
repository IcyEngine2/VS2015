#include "../detail/icy_dxgi.hpp"
#include "../icy_mbox.hpp"
#include <icy_engine/core/icy_thread.hpp>
#include <icy_engine/core/icy_queue.hpp>
#include <icy_engine/core/icy_file.hpp>
#include <icy_engine/core/icy_event.hpp>
#include <icy_engine/image/icy_image.hpp>
#include <icy_engine/network/icy_network.hpp>
#include <icy_engine/utility/icy_minhook.hpp>
#if _DEBUG
#pragma comment(lib, "icy_engine_cored")
#pragma comment(lib, "icy_engine_imaged")
#pragma comment(lib, "icy_engine_networkd")
#else
#pragma comment(lib, "icy_engine_core")
#endif
#pragma comment(lib, "dxgi")

using namespace icy;

extern "C" unsigned long GetCurrentProcessId();

static const auto network_code_dxgi = 1u;
class network_udp_thread : public thread
{
public:
    error_type run() noexcept override;
};
class network_tcp_thread : public thread
{
public:
    error_type run() noexcept override;
};
class main_thread : public thread
{
    friend network_udp_thread;
    friend network_tcp_thread;
    main_thread(error_type* error) noexcept
    {
        if (error)
            *error = initialize();
    }
    ~main_thread() noexcept
    {
        m_func.shutdown();
        while (auto ptr = m_dxgi_recv.pop())
            m_heap.realloc(ptr, 0);
        while (auto ptr = m_dxgi_send.pop())
            m_heap.realloc(ptr, 0);
    }
public:
    static main_thread& instance(error_type* error = nullptr) noexcept
    {
        static main_thread global(error);
        return global;
    }
    error_type launch(IDXGISwapChain& chain) noexcept;
    error_type run() noexcept override;
private:
    error_type initialize() noexcept;
    error_type callback(IDXGISwapChain* chain, unsigned sync, unsigned flags) noexcept;
private:
    IDXGISwapChain* m_chain = nullptr;
    heap m_heap;
    hook<dxgi_present> m_func;
    mbox::command_info m_info;
    network_system_tcp m_tcp_network;
    network_system_udp m_udp_network;
    network_udp_thread m_udp_thread;
    network_tcp_thread m_tcp_thread;
    detail::intrusive_mpsc_queue m_dxgi_recv;
    detail::intrusive_mpsc_queue m_dxgi_send;
    std::atomic<bool> m_pause = false;    
};

extern "C" HINSTANCE__ * __stdcall GetModuleHandleA(const char* lpModuleName);

int main()
{    
    if (source == dll_main::process_attach)
    {
        error_type error;
        main_thread::instance(&error);
    }
    else if (source == dll_main::process_detach)
    {
        main_thread::instance().wait();
    }
    return 1;
}

error_type main_thread::launch(IDXGISwapChain& chain) noexcept
{
    m_chain = &chain;
    return thread::launch();
}
error_type main_thread::initialize() noexcept
{
    ICY_ERROR(m_heap.initialize(heap_init::global(64_mb)));
    const auto module = GetModuleHandleA("dxgi");
    const auto offset = 17152u;
    const auto callback = [](IDXGISwapChain* chain, unsigned sync, unsigned flags)
    {
        if (main_thread::instance().state() == thread_state::none)
            main_thread::instance().launch(*chain);
        
        main_thread::instance().callback(chain, sync, flags);
        return 0l;
    };
    ICY_ERROR(m_func.initialize(dxgi_present(reinterpret_cast<uint8_t*>(module) + offset), callback));
    return {};
}
error_type main_thread::run() noexcept
{
    ICY_SCOPE_EXIT{ m_tcp_network.stop(network_code_exit); m_udp_network.stop(network_code_exit); };
    window_size wsize;
    string pname_long;
    string pname_short;
    string cname;
    string wname;
    ICY_ERROR(computer_name(cname));
    ICY_ERROR(process_name(pname_long));
    ICY_ERROR(to_string(file_name(pname_long).name, pname_short));
    ICY_ERROR(dxgi_window(*m_chain, wname, wsize));
    m_info.process_id = GetCurrentProcessId();
    copy(cname, m_info.computer_name);
    copy(wname, m_info.window_name);
    copy(pname_short, m_info.process_name);

    ICY_ERROR(m_udp_network.initialize());
    ICY_ERROR(m_tcp_network.initialize());

    array<network_address> address;
    ICY_ERROR(m_udp_network.address(mbox::multicast_addr, mbox::multicast_port, max_timeout, network_socket_type::UDP, address));
    if (address.empty())
        return make_stdlib_error(std::errc::bad_address);

    auto port = 0ui16;
    ICY_ERROR(mbox::multicast_port.to_value(port));
    ICY_ERROR(m_udp_network.launch(port, network_address_type::ip_v4, 0));
    ICY_ERROR(m_udp_network.multicast(address[0], true));
    ICY_ERROR(m_tcp_network.launch(0, network_address_type::ip_v4, 5));

    array<network_address> addr;
    ICY_ERROR(m_tcp_network.address({}, {}, max_timeout, network_socket_type::TCP, addr));
    if (addr.empty())
        return make_stdlib_error(std::errc::address_not_available);

    string addr_str;
    ICY_ERROR(to_string(addr[0], addr_str));
    copy(addr_str, m_info.address);

    ICY_ERROR(m_udp_thread.launch());
    ICY_ERROR(m_tcp_thread.launch());
    ICY_ERROR(m_tcp_thread.wait());
    ICY_ERROR(m_udp_thread.wait());
    return {};
}
error_type main_thread::callback(IDXGISwapChain* chain, unsigned sync, unsigned flags) noexcept
{
    ICY_SCOPE_EXIT{ if (! m_pause.load(std::memory_order_acquire)) m_func(chain, sync, flags); };
    
    array<rectangle> rects;
    array<guid> guids;
    if (auto ptr = static_cast<mbox::command*>(m_dxgi_recv.pop()))
    {
        ICY_ERROR(rects.push_back(ptr->rectangle));
        ICY_ERROR(guids.push_back(ptr->uid));
    }

    array<matrix<color>> colors;
    ICY_ERROR(dxgi_copy(*chain, rects, colors));
    
    if (colors.size() != guids.size() ||
        colors.size() != rects.size())
        return make_unexpected_error();
    
    for (auto k = 0u; k < guids.size(); ++k)
    {
        array<uint8_t> buffer;
        ICY_ERROR(image::save(m_heap, colors[k], image_type::png, buffer));

        const auto size = sizeof(mbox::command) + buffer.size();
        auto cmd = static_cast<mbox::command*>(m_heap.realloc(nullptr, size));
        if (!cmd)
            return make_stdlib_error(std::errc::not_enough_memory);
        ICY_SCOPE_EXIT{ m_heap.realloc(cmd, 0); };

        cmd->type = mbox::command_type::screen;
        cmd->arg = mbox::command_arg_type::image;
        cmd->uid = guids[k];
        cmd->binary.size = buffer.size();
        memcpy(cmd->binary.bytes, buffer.data(), buffer.size());
        m_dxgi_send.push(cmd);
        ICY_ERROR(m_tcp_network.stop(1));
    }

    return {};
}
error_type network_udp_thread::run() noexcept
{
    auto& network = main_thread::instance().m_udp_network;
    auto code = 0u;
    while (true)
    {
        ICY_ERROR(network.recv(sizeof(mbox::command)));
        network_udp_request request;
        ICY_ERROR(network.loop(request, code));
        if (code == network_code_exit)
            break;
        if (request.type == network_request_type::recv)
        {
            if (request.bytes.size() < sizeof(mbox::command))
                continue;

            auto cmd = reinterpret_cast<mbox::command*>(request.bytes.data());

            if (cmd->version != mbox::protocol_version)
                continue;

            if (cmd->type == mbox::command_type::info)
            {
                cmd->arg = mbox::command_arg_type::info;
                cmd->info = main_thread::instance().m_info;
                ICY_ERROR(network.send(request.address, request.bytes));
            }
        }
    }
    return {};
}
error_type network_tcp_thread::run() noexcept
{
    auto& network = main_thread::instance().m_tcp_network;
    network_tcp_connection conn(network_connection_type::none);
    conn.timeout(max_timeout);
        
    const auto func = [&network, &conn](bool& exit)
    {
        while (true)
        {
            network_tcp_reply reply;
            auto code = 0u;
            ICY_ERROR(network.recv(conn, sizeof(mbox::command)));
            ICY_ERROR(network.loop(reply, code));
            if (code == network_code_exit)
            {
                exit = true;
                break;
            }
            while (auto ptr = static_cast<mbox::command*>(main_thread::instance().m_dxgi_send.pop()))
            {
                ICY_ERROR(network.send(conn, { reinterpret_cast<const uint8_t*>(ptr), sizeof(mbox::command) + ptr->binary.size }));
            }
            if (reply.type == network_request_type::recv)
            {
                if (reply.bytes.size() < sizeof(mbox::command))
                    break;

                const auto cmd = reinterpret_cast<mbox::command*>(reply.bytes.data());
                if (cmd->version != mbox::protocol_version)
                    continue;

                if (cmd->type == mbox::command_type::exit)
                {
                    exit = true;
                    break;
                }
                else if (cmd->type == mbox::command_type::screen)
                {
                    if (cmd->arg != mbox::command_arg_type::rectangle)
                        continue;
                    main_thread::instance().m_dxgi_recv.push(cmd);
                }
                ICY_ERROR(network.recv(conn, sizeof(mbox::command)));
            }
            else if (reply.type == network_request_type::disconnect || reply.type == network_request_type::shutdown)
            {
                break;
            }
        }
        return error_type();

    };

    while (true)
    {
        ICY_ERROR(network.accept(conn));
        auto code = 0u;
        network_tcp_reply reply;
        ICY_ERROR(network.loop(reply, code));
        if (code == network_code_exit)
            break;

        if (reply.conn != &conn || reply.error || reply.type != network_request_type::accept)
            continue;

        auto exit = false;
        func(exit);
        if (exit)
            break;
    }
    return {};
}
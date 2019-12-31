#include "../detail/icy_dxgi.hpp"
#include "../icy_mbox.hpp"
#include <icy_engine/core/icy_thread.hpp>
#include <icy_engine/core/icy_queue.hpp>
#include <icy_engine/core/icy_file.hpp>
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

class main_thread : public thread
{
    main_thread() noexcept = default;
    ~main_thread() noexcept
    {
        m_func.shutdown();
        while (auto ptr = m_dxgi_requests.pop())
            m_heap.realloc(ptr, 0);
    }
public:
    static main_thread& instance() noexcept
    {
        static main_thread global;
        return global;
    }
    error_type launch(IDXGISwapChain& chain) noexcept;
    error_type initialize() noexcept;
    error_type run() noexcept override;
    void cancel() noexcept override
    {
        m_network.stop();
    }
private:
    error_type callback(IDXGISwapChain* chain, unsigned sync, unsigned flags) noexcept;
private:
    IDXGISwapChain* m_chain = nullptr;
    heap m_heap;
    hook<dxgi_present> m_func;
    network_system_udp m_network;
    network_address m_address;
    detail::intrusive_mpsc_queue m_dxgi_requests;
    std::atomic<bool> m_pause = false;    
};

extern "C" HINSTANCE__ * __stdcall GetModuleHandleA(const char* lpModuleName);
extern "C" int __stdcall GetModuleHandleExA(unsigned long, const char* func, HINSTANCE__** module);

int main()
{
    static auto counter = 0u;
    
    if (source == dll_main::process_attach)
    {
        ++counter;
        if (counter == 1)
        {
            main_thread::instance().initialize();
        }
    }
    else if (source == dll_main::process_detach)
    {
        --counter;
        if (counter == 0)
        {
            main_thread::instance().wait();
        }
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
    window_size wsize;
    string pname_long;
    string pname_short;
    string cname;
    string wname;
    ICY_ERROR(computer_name(cname));
    ICY_ERROR(process_name(pname_long));
    ICY_ERROR(to_string(file_name(pname_long).name, pname_short));
    ICY_ERROR(dxgi_window(*m_chain, wname, wsize));

    ICY_ERROR(m_network.initialize());
    array<network_address> address;
    ICY_ERROR(m_network.address(mbox::multicast_addr, mbox::multicast_port, 
        max_timeout, network_socket_type::UDP, address));
    if (address.empty())
        return make_stdlib_error(std::errc::bad_address);

    auto port = 0ui16;
    ICY_ERROR(mbox::multicast_port.to_value(port));
    ICY_ERROR(m_network.launch(port, network_address_type::ip_v4, 0));
    ICY_ERROR(m_network.multicast(address[0], true));

    auto exit = false;
    while (!exit)
    {
        ICY_ERROR(m_network.recv(sizeof(mbox::command)));
        network_udp_request request;
        ICY_ERROR(m_network.loop(request, exit));
        if (request.type == network_request_type::recv)
        {
            if (request.bytes.size() < sizeof(mbox::command))
                continue;

            auto cmd = reinterpret_cast<mbox::command*>(request.bytes.data());
            
            if (cmd->version != mbox::protocol_version)
                continue;

            if (cmd->type == mbox::command_type::exit)
            {
                break;
            }
            else if (cmd->type == mbox::command_type::info)
            {
                cmd->arg = mbox::command_arg_type::info;
                cmd->info.process_id = GetCurrentProcessId();
                copy(cname, cmd->info.computer_name);
                copy(wname, cmd->info.window_name);
                copy(pname_short, cmd->info.process_name);
                ICY_ERROR(m_network.send(request.address, request.bytes));
            }
            else if (cmd->type == mbox::command_type::screen)
            {
                if (cmd->arg != mbox::command_arg_type::rectangle)
                    continue;
                m_dxgi_requests.push(cmd);
            }
        }
    }
    //GetModuleHandleExA();
    //FreeLibraryAndExitThread(0);
    
    return {};
}
error_type main_thread::callback(IDXGISwapChain* chain, unsigned sync, unsigned flags) noexcept
{
    ICY_SCOPE_EXIT{ if (!m_pause.load(std::memory_order_acquire)) m_func(chain, sync, flags); };
    
    array<rectangle> rects;
    array<guid> guids;
    if (auto ptr = static_cast<mbox::command*>(m_dxgi_requests.pop()))
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
        ICY_ERROR(m_network.send(m_address, { reinterpret_cast<const uint8_t*>(cmd), size }));
    }

    return {};
}
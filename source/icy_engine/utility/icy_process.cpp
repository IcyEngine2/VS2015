#include <icy_engine/utility/icy_process.hpp>
#include <icy_engine/core/icy_map.hpp>
#include <icy_engine/core/icy_queue.hpp>
#include <Windows.h>
#include <tlhelp32.h>

using namespace icy;

ICY_STATIC_NAMESPACE_BEG
enum class internal_event_type
{
    none,
    launch,
    inject,
    send,
};
struct internal_event
{
    internal_event_type type = internal_event_type::none;
    uint32_t index = 0;
    string str;
};
enum class buffer_type
{
    none,
    connect,
    recv,
    send,
};
struct buffer_data
{
    buffer_data(const buffer_type type) noexcept : type(type)
    {
        memset(&ovl, 0, sizeof(ovl));
    }
    buffer_data(buffer_data&&) noexcept = default;
    ICY_DEFAULT_MOVE_ASSIGN(buffer_data);
    error_type initialize() noexcept
    {
        ICY_ERROR(sync.initialize());
        ovl.hEvent = sync.handle();
        return error_type();
    }
    OVERLAPPED ovl;
    buffer_type type = buffer_type::none;
    array<uint8_t> data;
    size_t offset = 0;
    sync_handle sync;
};
class process_data
{
public:
    process_data(const uint32_t index = 0) noexcept : m_index(index)
    {

    }
    process_data(process_data&& rhs) noexcept : m_index(rhs.m_index),
        m_proc_id(rhs.m_proc_id), m_proc_handle(rhs.m_proc_handle),
        m_thrd_id(rhs.m_thrd_id), m_thrd_handle(rhs.m_thrd_handle),
        m_conn(std::move(rhs.m_conn)),
        m_recv(std::move(rhs.m_recv)),
        m_send(std::move(rhs.m_send)),
        m_queue(std::move(rhs.m_queue))
    {
        rhs.m_proc_handle = nullptr;
        rhs.m_thrd_handle = nullptr;
    }
    ICY_DEFAULT_MOVE_ASSIGN(process_data);
    ~process_data() noexcept
    {
        if (m_pipe) CloseHandle(m_pipe);
        if (m_thrd_handle) CloseHandle(m_thrd_handle);
        if (m_proc_handle) CloseHandle(m_proc_handle);
    }
    error_type launch(const string_view path) noexcept;
    error_type inject(const string_view lib) noexcept;
    error_type send(event_system& system, const string_view request) noexcept;
    error_type process(event_system& system, const buffer_type type) noexcept;
    void* proc_handle() const noexcept
    {
        return m_proc_handle;
    }
    void* conn_handle() const noexcept
    {
        return m_conn ? m_conn->sync.handle() : nullptr;
    }
    void* send_handle() const noexcept
    {
        return m_send && !m_send->data.empty() ? m_send->sync.handle() : nullptr;
    }
    void* recv_handle() const noexcept
    {
        return m_recv && !m_recv->data.empty() ? m_recv->sync.handle() : nullptr;
    }
private:
    uint32_t m_index = 0;
    uint32_t m_proc_id = 0;
    uint32_t m_thrd_id = 0;
    void* m_proc_handle = nullptr;
    void* m_thrd_handle = nullptr;
    void* m_pipe = nullptr;
    unique_ptr<buffer_data> m_conn;
    unique_ptr<buffer_data> m_recv;
    unique_ptr<buffer_data> m_send;
    mpsc_queue<buffer_data> m_queue;
};
class process_system_data : public process_system
{
public:
    ~process_system_data() noexcept;
    error_type initialize() noexcept;
private:
    error_type signal(const event_data* event) noexcept override
    {
        return m_sync.wake();
    }
    error_type exec() noexcept override;
    const icy::thread& thread() const noexcept override
    {
        return *m_thread;
    }
    error_type launch(const string_view path, process_handle& index) noexcept override;
    error_type inject(const process_handle index, const string_view lib) noexcept override;
    error_type send(const process_handle index, const string_view request) noexcept override;
private:
    sync_handle m_sync;
    shared_ptr<event_thread> m_thread;  
    map<uint32_t, process_data> m_data;
    std::atomic<uint32_t> m_index = 0;
};
ICY_STATIC_NAMESPACE_END

error_type process_data::launch(const string_view path) noexcept
{
    array<wchar_t> wpath;
    ICY_ERROR(to_utf16(path, wpath));
    array<wchar_t> args;
    ICY_ERROR(args.reserve(wpath.size() + 2));
    ICY_ERROR(args.push_back(L'\"'));
    ICY_ERROR(args.append(wpath));
    ICY_ERROR(args.push_back(L'\"'));

    STARTUPINFOW start = { sizeof(start) };
    PROCESS_INFORMATION pinfo = {};
    if (!CreateProcessW(nullptr, args.data(), nullptr, nullptr, FALSE, 0, nullptr, nullptr, &start, &pinfo))
        return last_system_error();

    m_proc_id = pinfo.dwProcessId;
    m_thrd_id = pinfo.dwThreadId;
    m_proc_handle = pinfo.hProcess;
    m_thrd_handle = pinfo.hThread;
    return error_type();
}
error_type process_data::inject(const string_view lib) noexcept
{
    array<wchar_t> wpath;
    ICY_ERROR(to_utf16(lib, wpath));
    //auto offset = 0_z;
    const auto size = wpath.size() * sizeof(wchar_t);

    const auto memory = VirtualAllocEx(m_proc_handle, nullptr, size, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    if (!memory)
        return last_system_error();
    ICY_SCOPE_EXIT{ VirtualFreeEx(m_proc_handle, memory, 0, MEM_RELEASE); };

    auto count = 0_z;
    if (!WriteProcessMemory(m_proc_handle, memory, wpath.data(), size, &count))
        return last_system_error();

    const auto thread = CreateRemoteThread(m_proc_handle, nullptr, 0, LPTHREAD_START_ROUTINE(LoadLibraryW), memory, 0, nullptr);
    if (!thread)
        return last_system_error();
    ICY_SCOPE_EXIT{ CloseHandle(thread); };

    const auto wait = WaitForSingleObject(thread, INFINITE);
    if (wait != WAIT_OBJECT_0)
        return last_system_error();

    return error_type();
}
error_type process_data::send(event_system& system, const string_view request) noexcept
{
    if (!m_pipe)
    {
        wchar_t name[64];
        swprintf(name, _countof(name), L"\\\\.\\pipe\\icy_pipe_%u", m_proc_id);

        m_pipe = CreateNamedPipeW(name, PIPE_ACCESS_DUPLEX | FILE_FLAG_OVERLAPPED,
            PIPE_TYPE_MESSAGE | PIPE_READMODE_MESSAGE,
            PIPE_UNLIMITED_INSTANCES, 0, 0, 0, nullptr);
        if (m_pipe == INVALID_HANDLE_VALUE)
        {
            m_pipe = nullptr;
            return last_system_error();
        }
    }
    if (!m_conn)
    {
        ICY_ERROR(make_unique(buffer_data(buffer_type::connect), m_conn));
        ICY_ERROR(m_conn->initialize());
        if (!ConnectNamedPipe(m_pipe, &m_conn->ovl))
        {
            const auto code = GetLastError();
            switch (code)
            {
            case ERROR_PIPE_CONNECTED:
                ICY_ERROR(m_conn->sync.wake());
                break;

            case ERROR_IO_PENDING:
                break;
            default:
                return last_system_error();
            }
        }
        else
        {
            ICY_ERROR(m_conn->sync.wake());
        }
    }

    buffer_data new_buffer(buffer_type::send);
    ICY_ERROR(new_buffer.data.assign(request.ubytes()));
    ICY_ERROR(m_queue.push(std::move(new_buffer)));
    
    if (m_send && m_send->data.empty())
    {
        ICY_ERROR(process(system, buffer_type::send));
    }

    return error_type();
}
error_type process_data::process(event_system& system, const buffer_type type) noexcept
{
    const auto on_send = [&]
    {
        buffer_data buffer(buffer_type::send);
        while (m_queue.pop(buffer))
        {
            m_send->data = std::move(buffer.data);
            auto bytes = 0ul;
            if (!WriteFile(m_pipe, m_send->data.data(), DWORD(m_send->data.size()), &bytes, &m_send->ovl))
            {
                const auto code = GetLastError();
                if (code != ERROR_IO_PENDING)
                    return last_system_error();
                break;
            }
        }
        return error_type();
    };
    const auto on_recv = [&]()
    {
        auto bytes = 0ul;
        while (ReadFile(m_pipe, m_recv->data.data(), DWORD(m_recv->data.size()), &bytes, &m_recv->ovl))
        {
            process_event event;
            event.type = process_event_type::recv;
            event.handle.index = m_index;
            const auto ptr = reinterpret_cast<const char*>(m_recv->data.data());
            ICY_ERROR(copy(string_view(ptr, bytes), event.recv));
            ICY_ERROR(event::post(&system, event_type_process, std::move(event)));
        }
        const auto code = GetLastError();
        if (code != ERROR_IO_PENDING)
            return last_system_error();
        else
            return error_type();
    };

    if (type == buffer_type::connect)
    {
        ICY_ERROR(make_unique(buffer_data(buffer_type::recv), m_recv));
        ICY_ERROR(m_recv->initialize());
        ICY_ERROR(m_recv->data.resize(0x4000));
        ICY_ERROR(on_recv());

        ICY_ERROR(make_unique(buffer_data(buffer_type::send), m_send));
        ICY_ERROR(m_send->initialize());
        ICY_ERROR(on_send());
    }
    else if (type == buffer_type::send)
    {
        ICY_ERROR(on_send());
    }
    else if (type == buffer_type::recv)
    {
        ICY_ERROR(on_recv());
    }
    return error_type();
}

process_system_data::~process_system_data() noexcept
{
    if (m_thread)
        m_thread->wait();
    filter(0);
}
error_type process_system_data::initialize() noexcept
{
    ICY_ERROR(m_sync.initialize());
    ICY_ERROR(make_shared(m_thread));
    m_thread->system = this;
    filter(event_type::system_internal);
    return error_type();
}
error_type process_system_data::exec() noexcept
{
    while (*this)
    {
        while (auto event = pop())
        {
            if (event->type != event_type::system_internal)
                continue;

            const auto& event_data = event->data<internal_event>();
            switch (event_data.type)
            {
            case internal_event_type::launch:
            {
                process_event post_event;
                post_event.type = process_event_type::launch;
                post_event.handle.index = event_data.index;

                auto it = m_data.find(event_data.index);
                if (it == m_data.end())
                {
                    process_data pdata(event_data.index);
                    if (post_event.error = pdata.launch(event_data.str))
                    {

                    }
                    else
                    {
                        ICY_ERROR(m_data.insert(event_data.index, std::move(pdata)));
                    }                    
                }
                else
                {
                    post_event.error = make_stdlib_error(std::errc::invalid_argument);
                }
                ICY_ERROR(event::post(this, event_type_process, std::move(post_event)));
                break;
            }
            case internal_event_type::inject:
            {
                process_event post_event;
                post_event.type = process_event_type::inject;
                post_event.handle.index = event_data.index;

                auto it = m_data.find(event_data.index);
                if (it != m_data.end())
                {
                    post_event.error = it->value.inject(event_data.str);
                }
                else
                {
                    post_event.error = make_stdlib_error(std::errc::invalid_argument);
                }
                ICY_ERROR(event::post(this, event_type_process, std::move(post_event)));
                break;
            }
            case internal_event_type::send:
            {
                process_event post_event;
                post_event.type = process_event_type::send;
                post_event.handle.index = event_data.index;

                auto it = m_data.find(event_data.index);
                if (it != m_data.end())
                {
                    post_event.error = it->value.send(*this, event_data.str);
                }
                else
                {
                    post_event.error = make_stdlib_error(std::errc::invalid_argument);
                }
                ICY_ERROR(event::post(this, event_type_process, std::move(post_event)));
                break;
            }
            default:
                break;
            }

        }
        void* handles[MAXIMUM_WAIT_OBJECTS] = {};
        auto count = 0u;
        handles[count++] = m_sync.handle();

        for (auto&& pair : m_data)
        {
            if (count >= MAXIMUM_WAIT_OBJECTS)
                break;
            handles[count++] = pair.value.proc_handle();
        }
        for (auto&& pair : m_data)
        {
            if (count >= MAXIMUM_WAIT_OBJECTS)
                break;
            if (auto handle = pair.value.conn_handle())
                handles[count++] = handle;
        }
        for (auto&& pair : m_data)
        {
            if (count >= MAXIMUM_WAIT_OBJECTS)
                break;
            if (auto handle = pair.value.recv_handle())
                handles[count++] = handle;
        }
        for (auto&& pair : m_data)
        {
            if (count >= MAXIMUM_WAIT_OBJECTS)
                break;
            if (auto handle = pair.value.send_handle())
                handles[count++] = handle;
        }

        auto wait = WaitForMultipleObjectsEx(count, handles, FALSE, ms_timeout(max_timeout), TRUE);
        if (wait >= WAIT_OBJECT_0 && wait < WAIT_OBJECT_0 + MAXIMUM_WAIT_OBJECTS)
        {
            const auto handle = handles[wait - WAIT_OBJECT_0];
            if (handle == m_sync.handle())
                continue;

            for (auto it = m_data.begin(); it != m_data.end(); ++it)
            {
                process_event post_event;
                post_event.type = process_event_type::close;
                post_event.handle.index = it->key;

                if (it->value.proc_handle() == handle)
                {

                }
                else if (it->value.conn_handle() == handle)
                {
                    post_event.error = it->value.process(*this, buffer_type::connect);
                    if (!post_event.error)
                        continue;
                }
                else if (it->value.send_handle() == handle)
                {
                    post_event.error = it->value.process(*this, buffer_type::send);
                    if (!post_event.error)
                        continue;
                }
                else if (it->value.recv_handle() == handle)
                {
                    post_event.error = it->value.process(*this, buffer_type::recv);
                    if (!post_event.error)
                        continue;
                }
                else
                {
                    continue;
                }

                ICY_ERROR(event::post(this, event_type_process, std::move(post_event)));
                m_data.erase(it);
                break;
            }
        }
        else
        {
            return last_system_error();
        }
    }
    return error_type();
}
error_type process_system_data::launch(const string_view path, process_handle& proc) noexcept
{
    const auto index = m_index.fetch_add(1, std::memory_order_acq_rel) + 1;
    internal_event msg;
    msg.type = internal_event_type::launch;
    msg.index = proc.index = index;
    ICY_ERROR(copy(path, msg.str));
    ICY_ERROR(event_system::post(nullptr, event_type::system_internal, std::move(msg)));
    return error_type();
}
error_type process_system_data::inject(const process_handle proc, const string_view lib) noexcept
{
    internal_event msg;
    msg.type = internal_event_type::inject;
    msg.index = proc.index;
    ICY_ERROR(copy(lib, msg.str));
    ICY_ERROR(event_system::post(nullptr, event_type::system_internal, std::move(msg)));
    return error_type();
}
error_type process_system_data::send(const process_handle proc, const string_view request) noexcept
{
    internal_event msg;
    msg.type = internal_event_type::send;
    msg.index = proc.index;
    ICY_ERROR(copy(request, msg.str));
    ICY_ERROR(event_system::post(nullptr, event_type::system_internal, std::move(msg)));
    return error_type();
}

error_type icy::create_process_system(shared_ptr<process_system>& system) noexcept
{
    shared_ptr<process_system_data> new_system;
    ICY_ERROR(make_shared(new_system));
    ICY_ERROR(new_system->initialize());
    system = std::move(new_system);
    return error_type();
}

/*error_type icy::process_path(const uint32_t index, string& str) noexcept
{
    auto handle = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, index);
    if (!handle)
        return last_system_error();

    ICY_SCOPE_EXIT{ CloseHandle(handle); };
    
    wchar_t wpath[4096];
    DWORD count = _countof(wpath);
    if (!QueryFullProcessImageNameW(handle, 0, wpath, &count))
        return last_system_error();

    return to_string(const_array_view<wchar_t>(wpath, count), str);
}
error_type icy::process_launch(const string_view path, uint32_t& process, uint32_t& thread) noexcept
{
    array<wchar_t> wpath;
    ICY_ERROR(to_utf16(path, wpath));
    array<wchar_t> args;
    ICY_ERROR(args.reserve(wpath.size() + 2));
    ICY_ERROR(args.push_back(L'\"'));
    ICY_ERROR(args.append(wpath));
    ICY_ERROR(args.push_back(L'\"'));
        
    STARTUPINFOW start = { sizeof(start) };
    PROCESS_INFORMATION pinfo = {};
    if (!CreateProcessW(nullptr, args.data(), nullptr, nullptr, FALSE, 0, nullptr, nullptr, &start, &pinfo))
        return last_system_error();
    
    process = pinfo.dwProcessId;
    thread = pinfo.dwThreadId;
    CloseHandle(pinfo.hThread);
    CloseHandle(pinfo.hProcess);
    return error_type();
}
error_type icy::process_threads(const uint32_t index, array<uint32_t>& threads) noexcept
{
    auto snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, index);
    if (snapshot && snapshot != INVALID_HANDLE_VALUE)
        return last_system_error();
    ICY_SCOPE_EXIT{ CloseHandle(snapshot); };

    THREADENTRY32 thread = { sizeof(thread) };
    if (Thread32First(snapshot, &thread))
    {
        do
        {
            if (thread.dwSize >= FIELD_OFFSET(THREADENTRY32, th32OwnerProcessID) + sizeof(thread.th32OwnerProcessID))
                ICY_ERROR(threads.push_back(thread.th32ThreadID));
            thread.dwSize = sizeof(thread);
        } while (Thread32Next(snapshot, &thread));
    }
    return error_type();
}
*/
/*
    class process
    {
        friend remote_library;
    public:
        static error_type enumerate(array<std::pair<string, uint32_t>>& proc) noexcept;
        ~process() noexcept
        {
            wait(nullptr, {});
        }
        error_type wait(uint32_t* const code, const duration_type timeout = max_timeout) noexcept;
        error_type open(const uint32_t index, const bool read = false) noexcept;
        error_type launch(const string_view path) noexcept;
        error_type inject(const string_view library) noexcept;
        uint32_t index() const noexcept
        {
            return m_index;
        }
        explicit operator bool() const noexcept
        {
            return m_handle && m_index;
        }
    private:
        void* m_handle = nullptr;
        uint32_t m_index = 0;
    };
    class remote_library
    {
    public:
        error_type open(const process& process, const string_view path) noexcept;
        error_type close() noexcept;
        error_type call(const char* const name, const string_view args, error_type& error) noexcept;
    private:
        void* m_process = nullptr;
        void* m_handle = nullptr;
        array<wchar_t> m_wpath;
    };
error_type process::inject(const string_view library) noexcept
{
    array<wchar_t> wpath;
    ICY_ERROR(to_utf16(library, wpath));
    auto offset = 0_z;
    const auto size = wpath.size() * sizeof(wchar_t);

    const auto memory = VirtualAllocEx(m_handle, nullptr, size, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    if (!memory)
        return last_system_error();
    ICY_SCOPE_EXIT{ VirtualFreeEx(m_handle, memory, 0, MEM_RELEASE); };

    auto count = 0_z;
    if (!WriteProcessMemory(m_handle, memory, wpath.data(), size, &count))
        return last_system_error();

    const auto thread = CreateRemoteThread(m_handle, nullptr, 0, LPTHREAD_START_ROUTINE(LoadLibraryW), memory, 0, nullptr);
    if (!thread)
        return last_system_error();
    ICY_SCOPE_EXIT{ CloseHandle(thread); };

    const auto wait = WaitForSingleObject(thread, INFINITE);
    if (wait != WAIT_OBJECT_0)
        return last_system_error();

    return {};
}

error_type remote_library::open(const process& proc, const string_view library) noexcept
{
    if (m_handle || library.empty() || !proc.m_handle)
        return make_stdlib_error(std::errc::invalid_argument);

    auto is_64 = 0;
    if (!IsWow64Process(proc.m_handle, &is_64))
        return last_system_error();
    if (is_64)
        return make_stdlib_error(std::errc::invalid_argument);

    string name;
    ICY_ERROR(to_lower(icy::file_name(library).name, name));

    const auto find = [&name, &proc](void*& addr)
    {
        const auto snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE, proc.m_index);
        if (snapshot == INVALID_HANDLE_VALUE)
            return last_system_error();
        
        ICY_SCOPE_EXIT{ CloseHandle(snapshot); };

        const auto end = make_system_error(make_system_error_code(ERROR_NO_MORE_FILES));

        MODULEENTRY32W module = { sizeof(module) };
        if (!Module32FirstW(snapshot, &module))
        {
            const auto error = last_system_error();
            if (error == end)
                return error_type();
            return error;
        }
        while (true)
        {
            string mixed_case_str;
            string lower_case_str;
            ICY_ERROR(to_string(module.szModule, mixed_case_str));
            ICY_ERROR(to_lower(file_name(mixed_case_str).name, lower_case_str));
            if (name == lower_case_str)
            {
                addr = module.modBaseAddr;
                break;
            }
            if (!Module32NextW(snapshot, &module))
            {
                const auto error = last_system_error();
                if (error == end)
                    break;
                return error;
            }
        }
        return error_type();
    };
    ICY_ERROR(find(m_handle));
    ICY_ERROR(to_utf16(library, m_wpath));
    
    if (m_handle)
    {
        m_process = proc.m_handle;
        return {};
    }

    auto offset = 0_z;
    const auto size = m_wpath.size() * sizeof(wchar_t);

    const auto memory = VirtualAllocEx(proc.m_handle, nullptr, size, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    if (!memory)
        return last_system_error();
    ICY_SCOPE_EXIT{ VirtualFreeEx(proc.m_handle, memory, 0, MEM_RELEASE); };

    auto count = 0_z;
    if (!WriteProcessMemory(proc.m_handle, memory, m_wpath.data(), size, &count))
        return last_system_error();
    
    const auto handle = CreateRemoteThread(proc.m_handle, nullptr, 0,
        LPTHREAD_START_ROUTINE(LoadLibraryW), memory, 0, nullptr);
    if (!handle)
        return last_system_error();
    ICY_SCOPE_EXIT{ CloseHandle(handle); };

    const auto wait = WaitForSingleObject(handle, INFINITE);
    if (wait != WAIT_OBJECT_0)
        return last_system_error();

    ICY_ERROR(find(m_handle));
    if (!m_handle)
        return make_unexpected_error();
    
    m_process = proc.m_handle;
    return {};
}
error_type remote_library::close() noexcept
{
    if (!m_handle)
        return make_stdlib_error(std::errc::invalid_argument);

    const auto thread = CreateRemoteThread(m_process, nullptr, 0, 
        reinterpret_cast<LPTHREAD_START_ROUTINE>(&FreeLibraryAndExitThread), m_handle, 0, nullptr);
    if (!thread)
        return last_system_error();
    ICY_SCOPE_EXIT{ CloseHandle(thread); };
    return {};
}
error_type remote_library::call(const char* const func, const string_view args, error_type& error) noexcept
{
    if (!m_handle || !func)
        return make_stdlib_error(std::errc::invalid_argument);
    
    auto offset = 0_z;
    auto local = LoadLibraryW(m_wpath.data());
    if (!local)
        return last_system_error();

    ICY_SCOPE_EXIT{ FreeLibrary(local); };
    auto proc = GetProcAddress(local, func);
    if (!proc)
        return make_stdlib_error(std::errc::function_not_supported);

    offset = detail::distance(local, proc);

    const auto size = sizeof(inject_args) + args.bytes().size();
    array<char> buffer;
    ICY_ERROR(buffer.resize(size));
    auto inject = reinterpret_cast<inject_args*>(buffer.data());
    inject->len = args.bytes().size();
    memcpy(inject->str, args.bytes().data(), args.bytes().size());

    const auto memory = (VirtualAllocEx(m_process, nullptr, size, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE));
    if (!memory)
        return last_system_error();
    ICY_SCOPE_EXIT{ VirtualFreeEx(m_process, memory, 0, MEM_RELEASE); };

    auto count = 0_z;
    if (!WriteProcessMemory(m_process, memory, buffer.data(), size, &count))
        return last_system_error();
    if (count != size)
        return make_unexpected_error();

    const auto thread = CreateRemoteThread(m_process, nullptr, 0,
        LPTHREAD_START_ROUTINE(reinterpret_cast<uint8_t*>(m_handle) + offset), memory, 0, nullptr);
    if (!thread)
        return last_system_error();
    ICY_SCOPE_EXIT{ CloseHandle(thread); };

    const auto wait = WaitForSingleObject(thread, INFINITE);
    if (wait != WAIT_OBJECT_0)
        return last_system_error();

    auto code = 0ul;
    if (!GetExitCodeThread(thread, &code))
        return last_system_error();

    inject_args read_args;
    if (!ReadProcessMemory(m_process, memory, &read_args, sizeof(read_args), &count))
        return last_system_error();

    if (count != sizeof(read_args))
        return make_unexpected_error();

    error = read_args.error;
    return {};
}
* 
error_type process::enumerate(array<std::pair<string, uint32_t>>& proc) noexcept
{
    auto lib = "wtsapi32"_lib;
    ICY_ERROR(lib.initialize());
    auto ptr = static_cast<WTS_PROCESS_INFOW*>(nullptr);
    auto len = 0ul;

    const auto func = ICY_FIND_FUNC(lib, WTSEnumerateProcessesW);
    const auto free = ICY_FIND_FUNC(lib, WTSFreeMemory);
    if (!func || !free)
        return make_stdlib_error(std::errc::not_supported);

    if (!func(WTS_CURRENT_SERVER_HANDLE, 0, 1, &ptr, &len))
        return last_system_error();
    ICY_SCOPE_EXIT{ free(ptr); };

    for (auto k = 0u; k < len; ++k)
    {
        if (!ptr[k].ProcessId)
            continue;

        string name;
        ICY_ERROR(to_string(const_array_view<wchar_t>(ptr[k].pProcessName, wcslen(ptr[k].pProcessName)), name));
        ICY_ERROR(proc.push_back(std::make_pair(std::move(name), uint32_t(ptr[k].ProcessId))));
    }
    return {};
}
error_type process::wait(uint32_t* const code, const duration_type timeout ) noexcept
{
    if (!m_handle)
        return make_stdlib_error(std::errc::invalid_argument);

    ICY_SCOPE_EXIT{ CloseHandle(m_handle); m_handle = nullptr; };
    const auto value = WaitForSingleObject(m_handle, ms_timeout(timeout));
    if (value == WAIT_OBJECT_0)
    {
        if (code)
        {
            auto dwcode = 0ul;
            if (!GetExitCodeProcess(m_handle, &dwcode))
                return last_system_error();
            *code = dwcode;
        }
        return {};
    }
    else if (value == WAIT_TIMEOUT)
    {
        return make_stdlib_error(std::errc::timed_out);
    }
    else
    {
        return last_system_error();
    }
}
*/

const event_type icy::event_type_process = icy::next_event_user();

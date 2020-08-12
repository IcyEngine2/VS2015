#include <icy_engine/utility/icy_process.hpp>
#include <icy_engine/core/icy_array.hpp>
#include <Windows.h>
#include <tlhelp32.h>

using namespace icy;

error_type icy::process_path(const uint32_t index, string& str) noexcept
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
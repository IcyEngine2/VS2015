#include <icy_engine/core/icy_thread.hpp>
#include <icy_engine/core/icy_memory.hpp>
#include <icy_engine/core/icy_event.hpp>
#include <icy_engine/core/icy_string_view.hpp>
#include <windows.h>
#include <winternl.h> // for Unicode_string
#include <cstdio>
#include <process.h>
#include <thread>
#include <thr/threads.h>
using namespace icy;

#define LDRP_IMAGE_DLL                          0x00000004
#define LDRP_DONT_CALL_FOR_THREADS              0x00040000
#define LDRP_PROCESS_ATTACH_CALLED              0x00080000

using namespace icy;

ICY_STATIC_NAMESPACE_BEG
icy::detail::rw_spin_lock g_lock;
icy::thread_system* g_list = nullptr;
struct LDR_DATA_TABLE_ENTRY 
{
    LIST_ENTRY InLoadOrderModuleList;
    LIST_ENTRY InMemoryOrderModuleList;
    LIST_ENTRY InInitializationOrderModuleList;
    PVOID BaseAddress;
    PVOID EntryPoint;
    ULONG SizeOfImage;
    UNICODE_STRING FullDllName;
    UNICODE_STRING BaseDllName;
    ULONG Flags;
    SHORT LoadCount;
    SHORT TlsIndex;
    union
    {
        LIST_ENTRY HashLinks;
        PVOID SectionPointer;
    };
    ULONG Checksum;
    union
    {
        ULONG TimeDataStamp;
        PVOID LoadedImports;
    };
    PVOID EntryPointActivationContext;
    PVOID PatchInformation;
};
typedef NTSTATUS(NTAPI* pfnLdrFindEntryForAddress)(HMODULE hMod, LDR_DATA_TABLE_ENTRY** ppLdrData);
BOOL APIENTRY dll_callback(HMODULE, uint32_t reason, PVOID pDynamic) noexcept
{
    (void)pDynamic;
    if (reason == DLL_THREAD_ATTACH || reason == DLL_THREAD_DETACH)
        thread_system::notify(GetCurrentThreadId(), reason == DLL_THREAD_ATTACH);
    return TRUE;
}
void dll_insert(LIST_ENTRY* pOurListEntry, LIST_ENTRY* pK32ListEntry) noexcept
{
    // dll detach are called in reverse list order
    // so after Kernel32 is before it in the list
    // our forward link wants to point to whatever is after
    // k32ListEntry and our back link wants to point to pK32ListEn
    LIST_ENTRY* pEntryToInsertAfter = pK32ListEntry->Flink;
    pOurListEntry->Flink = pEntryToInsertAfter;
    pOurListEntry->Blink = pEntryToInsertAfter->Blink;
    pEntryToInsertAfter->Blink = pOurListEntry;
    pOurListEntry->Blink->Flink = pOurListEntry;
}
bool dll_init() noexcept
{
    const auto func = reinterpret_cast<pfnLdrFindEntryForAddress>(GetProcAddress(GetModuleHandle(L"ntdll.dll"), "LdrFindEntryForAddress"));
    if (func)
    {
        LDR_DATA_TABLE_ENTRY* pDLLEntry = nullptr;
        LDR_DATA_TABLE_ENTRY* pK32Entry = nullptr;
        func(GetModuleHandle(nullptr), &pDLLEntry);
        func(GetModuleHandle(L"kernel32.dll"), &pK32Entry);
        if (pDLLEntry)
        {
            pDLLEntry->EntryPoint = (PVOID)&dll_callback;
            pDLLEntry->Flags |= LDRP_PROCESS_ATTACH_CALLED | LDRP_IMAGE_DLL;
            pDLLEntry->Flags &= ~(LDRP_DONT_CALL_FOR_THREADS);
            pDLLEntry->BaseAddress = (PVOID)(((ULONG_PTR)pDLLEntry->BaseAddress) + 2);
            if (pK32Entry)
            {
                dll_insert(&pDLLEntry->InInitializationOrderModuleList, &pK32Entry->InInitializationOrderModuleList);
                return true;
            }
        }
    }
    return false;
};
ICY_STATIC_NAMESPACE_END

void icy::thread_system::notify(const uint32_t index, const bool attach) noexcept
{
    ICY_LOCK_GUARD_WRITE(g_lock);
    for (auto ptr = g_list; ptr; ptr = ptr->m_prev)
        (*ptr)(index, attach);
}
icy::thread_system::thread_system() noexcept
{
    ICY_LOCK_GUARD_WRITE(g_lock);
    m_prev = g_list;
    static const bool g_init = dll_init();
    g_list = this;
}
icy::thread_system::~thread_system() noexcept
{
    ICY_LOCK_GUARD_WRITE(g_lock);
    thread_system* prev = nullptr;
    thread_system* next = g_list;
    while (next)
    {
        if (next == this)
        {
            if (prev)
                prev->m_prev = next->m_prev;
            else
                g_list = next->m_prev;
            break;
        }
        else
        {
            prev = next;
            next = next->m_prev;
        }
    }
}


static thread_local thread* g_this_thread = nullptr;
struct thread::data_type 
{
    error_type wait(const duration_type timeout) noexcept
    {
        if (handle)
        {
            {
                ICY_LOCK_GUARD(lock);
                state = thread_state::done;
            }
            cvar.wake();
            WaitForSingleObject(handle, ms_timeout(timeout));
            CloseHandle(handle);
            handle = nullptr;
        }
        return error;
    }
    ~data_type() noexcept
    {
        wait(std::chrono::seconds(1));
    }
    mutex lock;
    cvar cvar;
    thread_state state = thread_state::none;
    uint32_t index = 0u;
    uint32_t source = 0u;
    HANDLE handle = nullptr;
    error_type error;
};

const thread* icy::this_thread() noexcept
{
    return g_this_thread;
}
void icy::sleep(const clock_type::duration duration) noexcept
{
    const auto ms = ms_timeout(duration);
    if (!ms)
    {
        thrd_yield();
    }
    else
    {
        xtime tm;
        tm.sec = time_t(std::chrono::duration_cast<std::chrono::seconds>(duration).count());
        tm.nsec = long(std::chrono::duration_cast<std::chrono::nanoseconds>(duration - std::chrono::seconds(tm.sec)).count());
        thrd_sleep(&tm);
    }
    //SleepEx(ms, TRUE);
}

uint32_t thread::this_index() noexcept
{
    return GetCurrentThreadId();
}
unsigned thread::index() const noexcept
{
    return m_data ? m_data->index : 0;
}
thread_state thread::state() const noexcept
{
    if (m_data)
    {
        ICY_LOCK_GUARD(m_data->lock);
        return m_data->state;
    }
    return thread_state::none;
}
error_type thread::error() const noexcept
{
    if (m_data)
    {
        ICY_LOCK_GUARD(m_data->lock);
        return m_data->error;
    }
    return error_type();
}
void thread::exit(const error_type error) noexcept
{
    if (m_data)
    {
        ICY_LOCK_GUARD(m_data->lock);
        m_data->error = error;
        m_data->state = thread_state::done;
    }
}
error_type thread::wait() noexcept
{
    error_type error;
    if (m_data)
    {
        cancel();
        error = m_data->wait(max_timeout);
        allocator_type::destroy(m_data);
        allocator_type::deallocate(m_data);
    }
    m_data = nullptr;
    return error;
}
error_type thread::launch() noexcept
{
    ICY_ERROR(wait());

    m_data = allocator_type::allocate<data_type>(1);
    if (!m_data)
        return make_stdlib_error(std::errc::not_enough_memory);
    
    auto success = false;
    ICY_SCOPE_EXIT{ if (!success) wait(); };
    
    allocator_type::construct(m_data);
    ICY_ERROR(m_data->lock.initialize());
    m_data->source = this_index();

    const auto proc = [](void* ptr)
    {
        auto thr = make_shared_from_this(static_cast<thread*>(ptr));
        g_this_thread = thr.get();
        {
            ICY_LOCK_GUARD(thr->m_data->lock);
            thr->m_data->state = thread_state::run;
        }
        thr->m_data->cvar.wake();
        const auto error = thr->run();
        {
            ICY_LOCK_GUARD(thr->m_data->lock);
            thr->m_data->state = thread_state::done;
            thr->m_data->error = error;
        }
        thr->m_data->cvar.wake();
        return 0u;
    };
    m_data->handle = reinterpret_cast<HANDLE>(_beginthreadex(nullptr, 0, proc, this, 0, &m_data->index));
    if (!m_data->handle)
        return last_system_error();
    
    while (true)
    {
        ICY_ERROR(m_data->cvar.wait(m_data->lock, std::chrono::milliseconds(200)));
        ICY_LOCK_GUARD(m_data->lock);
        ICY_ERROR(m_data->error);
        if (m_data->state != thread_state::none)
            break;
    }
    success = true;
    return error_type();
}
error_type thread::rename(const string_view name) noexcept
{
    wchar_t wide[256] = {};
    auto size = _countof(wide);
    ICY_ERROR(name.to_utf16(wide, &size));
    const auto hr = SetThreadDescription(m_data->handle, wide);
    if (FAILED(hr))
        return make_system_error(hr);
    return error_type();
}

thread::~thread() noexcept
{
    if (m_data)
    {
        allocator_type::destroy(m_data);
        allocator_type::deallocate(m_data);
    }
    m_data = nullptr;
}

size_t thread::cores() noexcept
{
    return std::thread::hardware_concurrency();
}
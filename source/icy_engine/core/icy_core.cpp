#include <icy_engine/core/icy_string.hpp>
#include <icy_engine/core/icy_array.hpp>
#include <icy_engine/core/icy_key.hpp>
#include <icy_engine/core/icy_color.hpp>
#include <icy_engine/core/icy_event.hpp>
#include <Windows.h>
#include <objbase.h>
#include <winternl.h>

using namespace icy;

static error_type system_error_to_string(const unsigned code, const string_view locale, string& str) noexcept
{
    auto wlocale_len = 0_z;
    ICY_ERROR(locale.to_utf16(nullptr, &wlocale_len));
    array<wchar_t> wlocale;
    ICY_ERROR(wlocale.resize(wlocale_len));
    ICY_ERROR(locale.to_utf16(wlocale.data(), &wlocale_len));

    const auto lang = LocaleNameToLCID(wlocale.data(), 0);
    const auto flags = FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS;
    const auto cpUTF8 = CP_UTF8;

    wchar_t* buffer = nullptr;
    auto length = FormatMessageW(flags, nullptr, code, lang, reinterpret_cast<LPWSTR>(&buffer), 0, nullptr);
    if (!length)
        return last_system_error();
    ICY_SCOPE_EXIT{ LocalFree(buffer); };

    while (length && (buffer[length - 1] == '\r' || buffer[length - 1] == '\n'))
        --length;
    ICY_ERROR(icy::to_string(const_array_view<wchar_t>(buffer, length), str));
    return error_type();
}
static error_type stdlib_error_to_string(const unsigned code, const string_view, string& str) noexcept
{
    switch (code)
    {
    case EPERM: return icy::to_string("Operation not permitted"_s, str);
    case ENOENT: return icy::to_string("No such file or directory"_s, str);
    case ESRCH: return icy::to_string("No such process"_s, str);
    case EINTR: return icy::to_string("Interrupted system call"_s, str);
    case EIO: return icy::to_string("I/O error"_s, str);
    case ENXIO: return icy::to_string("No such device or address"_s, str);
    case E2BIG: return icy::to_string("Argument list too long"_s, str);
    case ENOEXEC: return icy::to_string("Exec format error"_s, str);
    case EBADF: return icy::to_string("Bad file number"_s, str);
    case ECHILD: return icy::to_string("No child processes"_s, str);
    case EAGAIN: return icy::to_string("Try again"_s, str);
    case ENOMEM: return icy::to_string("Out of memory"_s, str);
    case EACCES: return icy::to_string("Permission denied"_s, str);
    case EFAULT: return icy::to_string("Bad address"_s, str);
        //case ENOTBLK:	"Block device required"
    case EBUSY: return icy::to_string("Device or resource busy"_s, str);
    case EEXIST: return icy::to_string("File exists"_s, str);
    case EXDEV: return icy::to_string("Cross-device link"_s, str);
    case ENODEV: return icy::to_string("No such device"_s, str);
    case ENOTDIR: return icy::to_string("Not a directory"_s, str);
    case EISDIR: return icy::to_string("Is a directory"_s, str);
    case EINVAL: return icy::to_string("Invalid argument"_s, str);
    case ENFILE: return icy::to_string("File table overflow"_s, str);
    case EMFILE: return icy::to_string("Too many open files"_s, str);
    case ENOTTY: return icy::to_string("Not a typewriter"_s, str);
        //	26
    case EFBIG: return icy::to_string("File too large"_s, str);
    case ENOSPC: return icy::to_string("No space left on device"_s, str);
    case ESPIPE: return icy::to_string("Illegal seek"_s, str);
    case EROFS: return icy::to_string("Read-only file system"_s, str);
    case EMLINK: return icy::to_string("Too many links"_s, str);
    case EPIPE: return icy::to_string("Broken pipe"_s, str);
    case EDOM: return icy::to_string("Math argument out of domain of func"_s, str);
    case ERANGE: return icy::to_string("Math result not representable"_s, str);
        //	35
    case EDEADLK: return icy::to_string("Resource deadlock_would_occur"_s, str);
        //	37
    case ENAMETOOLONG: return icy::to_string("File name too long"_s, str);
    case ENOLCK: return icy::to_string("No locks available"_s, str);
    case ENOSYS: return icy::to_string("Function not implemented"_s, str);
    case ENOTEMPTY: return icy::to_string("Directory not empty"_s, str);
    case EILSEQ: return icy::to_string("Illegal byte sequence"_s, str);
        //	43-79
    case STRUNCATE: return icy::to_string("A string copy or concatenation resulted in a truncated string"_s, str);
        //	81-99
        //	...
        //	...
        //	...
        //	...
    case EADDRINUSE: return icy::to_string("Address already in use"_s, str);
    case EADDRNOTAVAIL: return icy::to_string("Can't assign requested address"_s, str);
    case EAFNOSUPPORT: return icy::to_string("Address family not supported by protocol family"_s, str);
    case EALREADY: return icy::to_string("Operation already in progress"_s, str);
    case EBADMSG: return icy::to_string("Not a data message"_s, str);
    case ECANCELED: return icy::to_string("Operation Canceled"_s, str);
    case ECONNABORTED: return icy::to_string("Software caused connection abort"_s, str);
    case ECONNREFUSED: return icy::to_string("Connection refused"_s, str);
    case ECONNRESET: return icy::to_string("Connection reset by peer"_s, str);
    case EDESTADDRREQ: return icy::to_string("Destination address required"_s, str);
    case EHOSTUNREACH: return icy::to_string("No route to host"_s, str);
    case EIDRM: return icy::to_string("Identifier removed"_s, str);
    case EINPROGRESS: return icy::to_string("Operation now in progress"_s, str);
    case EISCONN: return icy::to_string("Socket is already connected"_s, str);
    case ELOOP: return icy::to_string("Too many levels of symbolic links"_s, str);
    case EMSGSIZE: return icy::to_string("Message too long"_s, str);
    case ENETDOWN: return icy::to_string("Network is down"_s, str);
    case ENETRESET: return icy::to_string("Network dropped connection on reset"_s, str);
    case ENETUNREACH: return icy::to_string("Network is unreachable"_s, str);
    case ENOBUFS: return icy::to_string("No buffer space available"_s, str);
    case ENODATA: return icy::to_string("No data available"_s, str);
    case ENOLINK: return icy::to_string("Link has been severed"_s, str);
    case ENOMSG: return icy::to_string("No message of desired type"_s, str);
    case ENOPROTOOPT: return icy::to_string("Protocol not available"_s, str);
    case ENOSR: return icy::to_string("Out of streams resources"_s, str);
    case ENOSTR: return icy::to_string("Device not a stream"_s, str);
    case ENOTCONN: return icy::to_string("Socket is not connected"_s, str);
    case ENOTRECOVERABLE: return icy::to_string("State not recoverable"_s, str);
    case ENOTSOCK: return icy::to_string("Socket operation on non-socket"_s, str);
    case ENOTSUP: return icy::to_string("Not supported"_s, str);
    case EOPNOTSUPP: return icy::to_string("Operation not supported"_s, str);
        //case EOTHER:
    case EOVERFLOW: return icy::to_string("Value too large for defined data type"_s, str);
    case EOWNERDEAD: return icy::to_string("Owner died"_s, str);
    case EPROTO: return icy::to_string("Protocol error"_s, str);
    case EPROTONOSUPPORT: icy::to_string("Protocol not supported"_s, str);
    case EPROTOTYPE: return icy::to_string("Protocol wrong type for socket"_s, str);
    case ETIME: return icy::to_string("Timer expired"_s, str);
    case ETIMEDOUT: return icy::to_string("Operation timed out"_s, str);
    case ETXTBSY: return icy::to_string("Text file busy"_s, str);
    case EWOULDBLOCK: return icy::to_string("Operation would block"_s, str);
    }
    return make_stdlib_error(std::errc::invalid_argument);
}

const error_source icy::error_source_system = register_error_source("system"_s, system_error_to_string);
const error_source icy::error_source_stdlib = register_error_source("stdlib"_s, stdlib_error_to_string);
error_type icy::last_stdlib_error() noexcept
{
    return make_stdlib_error(std::errc(errno));
}
error_type icy::make_unexpected_error() noexcept
{
    return error_type(uint32_t(E_UNEXPECTED), error_source_system);
}

namespace icy
{
    struct error_pair
    {
        char name[16] = {};
        uint64_t hash = 0;
        error_type(*func)(unsigned, string_view, string&) = nullptr;
    };
    constexpr auto error_source_capacity = 0x20;
    std::atomic<size_t>& error_source_count() noexcept
    {
        static std::atomic<size_t> global = 0;
        return global;
    }
    error_pair* error_source_array() noexcept
    {
        static error_pair global[error_source_capacity];
        return global;
    }
}
error_source icy::register_error_source(const string_view name, error_type(*func)(const unsigned code, const string_view locale, string& str))
{
    auto count = error_source_count().load(std::memory_order_acquire);
    while (true)
    {
        if (count >= error_source_capacity)
            return error_source{};

        if (error_source_count().compare_exchange_weak(count, count + 1, std::memory_order_acq_rel))
            break;
    }

    auto& pair = error_source_array()[count];
    pair.func = func;
    pair.hash = hash64(name);
    icy::strcopy(name, array_view<char>(pair.name));

    error_source source;
    source.hash = pair.hash;
    return source;
}

error_type icy::to_string(const error_source source, string& str) noexcept
{
    for (auto k = 0_z; k < error_source_count().load(std::memory_order_acquire); ++k)
    {
        if (error_source_array()[k].hash != source.hash)
            continue;

        return to_string(string_view(error_source_array()[k].name, strlen(error_source_array()[k].name)), str);
    }
    return make_stdlib_error(std::errc::invalid_argument);
}
error_type icy::to_string(const error_type error, string& str) noexcept
{
    return to_string(error, str, {});
}
error_type icy::to_string(const error_type error, string& str, const string_view locale) noexcept
{
    for (auto k = 0_z; k < error_source_count().load(std::memory_order_acquire); ++k)
    {
        if (error_source_array()[k].hash != error.source.hash)
            continue;

        return error_source_array()[k].func(error.code, locale, str);
    }
    return make_stdlib_error(std::errc::invalid_argument);
}

error_type library::initialize() noexcept
{
    HINSTANCE__* new_module = nullptr;
    new_module = LoadLibraryA(m_name);
    if (!new_module)
        return last_system_error();
    shutdown();
    m_module = new_module;
    return error_type();
}
void library::shutdown() noexcept
{
    if (m_module)
        FreeLibrary(m_module);
    m_module = nullptr;
}

mutex::~mutex() noexcept
{
    if (*this)
		DeleteCriticalSection(reinterpret_cast<CRITICAL_SECTION*>(this));
}
mutex::operator bool() const noexcept
{
    char buffer[sizeof(m_buffer)] = {};
    return memcmp(buffer, m_buffer, sizeof(buffer)) != 0;
}
error_type mutex::initialize() noexcept
{
	if (!InitializeCriticalSectionAndSpinCount(reinterpret_cast<CRITICAL_SECTION*>(this), 1 | RTL_CRITICAL_SECTION_FLAG_NO_DEBUG_INFO))
		return last_system_error();
	return error_type();
}
bool mutex::try_lock() noexcept
{
	return !!TryEnterCriticalSection(reinterpret_cast<CRITICAL_SECTION*>(this));
}
void mutex::lock() noexcept
{
	EnterCriticalSection(reinterpret_cast<CRITICAL_SECTION*>(this));
}
void mutex::unlock() noexcept
{
	LeaveCriticalSection(reinterpret_cast<CRITICAL_SECTION*>(this));
}
void cvar::wake() noexcept
{
    WakeConditionVariable(reinterpret_cast<CONDITION_VARIABLE*>(this));
}
error_type cvar::wait(mutex& mutex, const duration_type timeout) noexcept
{
    if (timeout.count() < 0)
        return make_stdlib_error(std::errc::invalid_argument);

    mutex.lock();
    const auto sleep = SleepConditionVariableCS(
        reinterpret_cast<CONDITION_VARIABLE*>(this), 
        reinterpret_cast<CRITICAL_SECTION*>(&mutex), 
        ms_timeout(timeout));
    mutex.unlock();
    if (!sleep)
    {
        if (const auto error = last_system_error())
        {
            if (error == make_system_error(make_system_error_code(ERROR_TIMEOUT)))
                return make_stdlib_error(std::errc::timed_out);
            else
                return error;
        }
    }
    return error_type();
}

EXTERN_C IMAGE_DOS_HEADER __ImageBase;
HINSTANCE icy::win32_instance() noexcept
{
    return reinterpret_cast<HINSTANCE>(&__ImageBase);
}

void timer::cancel() noexcept
{
    if (m_data)
    {
        const auto ptr = static_cast<PTP_TIMER>(m_data);
        SetThreadpoolTimer(ptr, nullptr, 0, 0);
        WaitForThreadpoolTimerCallbacks(ptr, TRUE);
        CloseThreadpoolTimer(ptr);
        m_data = nullptr;
    }
}
error_type timer::initialize(const size_t count, const duration_type timeout) noexcept
{
    if (count == 0 || timeout.count() < 0)
        return make_stdlib_error(std::errc::invalid_argument);

    const auto callback = [](PTP_CALLBACK_INSTANCE, void* this_timer, PTP_TIMER)
    {
        if (const auto ptr = static_cast<timer*>(this_timer))
        {
            const auto count = ptr->m_count.fetch_sub(1, std::memory_order_acq_rel);
            event::post(nullptr, event_type::global_timer, pair{ ptr, count });
            if (count > 1)
            {
                union
                {
                    LARGE_INTEGER integer;
                    FILETIME time;
                };
                integer.QuadPart = -std::chrono::duration_cast<std::chrono::nanoseconds>(ptr->m_timeout).count() / 100;
                SetThreadpoolTimer(static_cast<PTP_TIMER>(ptr->m_data), &time, 0, 0);
            }
        }
    };

    auto new_timer = CreateThreadpoolTimer(callback, this, nullptr);
    if (!new_timer)
        return last_system_error();

    cancel();
    
    m_count = count;
    m_timeout = timeout;
    m_data = new_timer;

    union
    {
        LARGE_INTEGER integer;
        FILETIME time;
    };
    integer.QuadPart = -std::chrono::duration_cast<std::chrono::nanoseconds>(timeout).count() / 100;
    SetThreadpoolTimer(new_timer, &time, 0, 0);

    return error_type();
}

error_type icy::message_box(const string_view text, const string_view header, bool* const yesno) noexcept
{
    array<wchar_t> whdr;
    array<wchar_t> wtxt;
    ICY_ERROR(to_utf16(header, whdr));
    ICY_ERROR(to_utf16(text, wtxt));

    library user32("user32");    
    ICY_ERROR(user32.initialize());
    const auto func = ICY_FIND_FUNC(user32, MessageBoxW);
    if (!func)
        return make_stdlib_error(std::errc::invalid_argument);

    const auto flags_ptr = reinterpret_cast<uint32_t*>(NtCurrentTeb()->ProcessEnvironmentBlock->ProcessParameters->Reserved1 + 8);
    const auto flags_old = *flags_ptr;
    *flags_ptr |= 0x00080000;
    ICY_SCOPE_EXIT{ *flags_ptr = flags_old; };

    const auto code = func(nullptr, wtxt.data(), whdr.data(), yesno ? MB_YESNO : MB_OK);
    if (yesno)
        *yesno = code == IDYES;
    return error_type();

/*    static decltype(&GetWindowLongPtrW) win32_get_window_long_ptr = nullptr;
    win32_get_window_long_ptr = ICY_FIND_FUNC(user32, GetWindowLongPtrW);
    if (!win32_get_window_long_ptr) return make_stdlib_error(std::errc::function_not_supported);

#define LOAD_USER32(X,Y) data.X = ICY_FIND_FUNC(user32, Y); if (!data.X) return make_stdlib_error(std::errc::function_not_supported)
    
    struct func_data
    {
        decltype(&DefWindowProcW) win32_def_window_proc = nullptr;
        decltype(&RegisterClassExW) win32_register_class = nullptr;
        decltype(&LoadCursorW) win32_load_cursor = nullptr;
        decltype(&UnregisterClassW) win32_unregister_class = nullptr;
        decltype(&CreateWindowExW) win32_create_window = nullptr;
        decltype(&DestroyWindow) win32_destroy_window = nullptr;
        decltype(&SetWindowTextW) win32_set_window_text = nullptr;
        decltype(&SetWindowLongPtrW) win32_set_window_long_ptr = nullptr;
        decltype(&PostQuitMessage) win32_post_quit_message = nullptr;
        decltype(&MsgWaitForMultipleObjectsEx) win32_msg_wait = nullptr;
        decltype(&PeekMessageW) win32_peek_message = nullptr;
        decltype(&TranslateMessage) win32_translate_message = nullptr;
        decltype(&DispatchMessageW) win32_dispatch_message = nullptr;
        decltype(&SetWindowLongW) win32_set_window_long = nullptr;
        bool done = false;
    } data;
    LOAD_USER32(win32_def_window_proc, DefWindowProcW);
    LOAD_USER32(win32_register_class, RegisterClassExW);
    LOAD_USER32(win32_load_cursor, LoadCursorW);
    LOAD_USER32(win32_unregister_class, UnregisterClassW);
    LOAD_USER32(win32_create_window, CreateWindowExW);
    LOAD_USER32(win32_destroy_window, DestroyWindow);
    LOAD_USER32(win32_set_window_text, SetWindowTextW);
    LOAD_USER32(win32_set_window_long_ptr, SetWindowLongPtrW);
    LOAD_USER32(win32_post_quit_message, PostQuitMessage);
    LOAD_USER32(win32_msg_wait, MsgWaitForMultipleObjectsEx);
    LOAD_USER32(win32_peek_message, PeekMessageW);
    LOAD_USER32(win32_translate_message, TranslateMessage);
    LOAD_USER32(win32_dispatch_message, DispatchMessageW);
    LOAD_USER32(win32_set_window_long, SetWindowLongW);

    wchar_t cname[64];
    swprintf_s(cname, L"Icy Message Box %u.%u", GetCurrentProcessId(), GetCurrentThreadId());

    const auto proc = [](HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) -> LRESULT
    {
        auto ptr = reinterpret_cast<func_data*>(win32_get_window_long_ptr(hwnd, GWLP_USERDATA));
        if (msg == WM_CLOSE)
        {
            ptr->done = true;
            return 0;
        }
        return ptr->win32_def_window_proc(hwnd, msg, wparam, lparam);
    };

    WNDCLASSEXW cls = { sizeof(WNDCLASSEXW) };
    cls.lpszClassName = cname;
    cls.style = CS_HREDRAW | CS_VREDRAW | CS_DBLCLKS;
    cls.lpfnWndProc = data.win32_def_window_proc;
    cls.hInstance = win32_instance();
    cls.hCursor = data.win32_load_cursor(nullptr, IDC_ARROW);
    if (data.win32_register_class(&cls) == 0)
        return last_system_error();

    ICY_SCOPE_EXIT{ data.win32_unregister_class(cls.lpszClassName, cls.hInstance); };

    array<wchar_t> whdr;
    ICY_ERROR(to_utf16(header, whdr));

    //auto hwnd = data.win32_create_window(WS_EX_DLGMODALFRAME, cname, whdr.data(), WS_CAPTION | WS_VISIBLE,
    //    CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT,nullptr, nullptr, win32_instance(), nullptr);

    auto hwnd = data.win32_create_window(WS_EX_DLGMODALFRAME, cname, whdr.data(), WS_CAPTION,
        CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, nullptr, nullptr, nullptr, nullptr);

    if (!hwnd)
        return last_system_error();
    ICY_SCOPE_EXIT{ data.win32_destroy_window(hwnd); };

    data.win32_set_window_long_ptr(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(&data));
    data.win32_set_window_long_ptr(hwnd, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(static_cast<WNDPROC>(proc)));

    data.win32_set_window_long(hwnd, GWL_STYLE, WS_VISIBLE | WS_CAPTION);

    while (true)
    {
        const auto index = data.win32_msg_wait(0, nullptr, INFINITE, QS_ALLINPUT, 0);

        if (index == WAIT_OBJECT_0)
        {
            MSG win32_msg;
            while (data.win32_peek_message(&win32_msg, hwnd, 0, 0, PM_REMOVE))
            {
                data.win32_translate_message(&win32_msg);
                data.win32_dispatch_message(&win32_msg);
            }
            if (data.done)
                break;
        }
        else if (index == WAIT_ABANDONED)
        {
            return error_type();
        }
        else if (index == WAIT_TIMEOUT)
        {
            return make_stdlib_error(std::errc::timed_out);
        }
        else //if (index == WAIT_FAILED)
        {
            return last_system_error();
        }
    }
    return error_type();*/
}
error_type icy::win32_parse_cargs(array<string>& args) noexcept
{
    args.clear();
    for (auto k = 0; k < __argc; ++k)
    {
        string str;
        if (__wargv)
        {
            ICY_ERROR(to_string(const_array_view<wchar_t>(__wargv[k], wcslen(__wargv[k])), str));
        }
        else if (__argv)
        {
            ICY_ERROR(to_string(__argv[k], str));
        }

        ICY_ERROR(args.push_back(std::move(str)));
    }
    return error_type();
}
guid icy::guid::create() noexcept
{
    guid output;
    auto lib = "ole32.dll"_lib;
    lib.initialize();
    if (const auto func = ICY_FIND_FUNC(lib, CoCreateGuid))
        func(reinterpret_cast<GUID*>(&output));
    return output;
}

error_type icy::computer_name(string& str) noexcept
{
    const auto format = COMPUTER_NAME_FORMAT::ComputerNameDnsFullyQualified;
    auto size = 0ul;
    if (!GetComputerNameExW(format, nullptr, &size))
    {
        const auto error = last_system_error();
        if (error == make_system_error(make_system_error_code(ERROR_MORE_DATA)))
            ;
        else
            return error;
    }
    array<wchar_t> wname;
    ICY_ERROR(wname.resize(size));
    if (!GetComputerNameExW(format, wname.data(), &size))
        return last_system_error();

    ICY_ERROR(to_string(wname, str));
    return{};
}
error_type icy::process_name(HINSTANCE__* module, string& str) noexcept
{
    wchar_t buffer[4096];
    auto length = GetModuleFileNameW(module, buffer, _countof(buffer));
    return to_string(const_array_view<wchar_t>(buffer, length), str);
}
uint32_t icy::process_index() noexcept
{
    return GetCurrentProcessId();
}
error_type icy::win32_debug_print(const string_view str) noexcept
{
    array<wchar_t> wstr;
    ICY_ERROR(to_utf16(str, wstr));
    OutputDebugStringW(wstr.data());
    return error_type();
}

/*
    library ole32("ole32");
        ICY_ERROR(ole32.initialize());
        const auto com_create = ICY_FIND_FUNC(ole32, CoCreateInstance);
        if (!com_create)
            return make_stdlib_error(std::errc::function_not_supported);

        IGlobalOptions* options = nullptr;
        auto hr = com_create(CLSID_GlobalOptions, NULL, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&options));
        if (FAILED(hr))
            return make_system_error(hr);
        ICY_SCOPE_EXIT{ options->Release(); };

        auto value = 0_z;
        hr = options->Query(COMGLB_EXCEPTION_HANDLING, &value);
        if (FAILED(hr)) return make_system_error(hr);

        hr = options->Set(COMGLB_EXCEPTION_HANDLING, COMGLB_EXCEPTION_DONOT_HANDLE);
        if (FAILED(hr)) return make_system_error(hr);
        ICY_SCOPE_EXIT{ options->Set(COMGLB_EXCEPTION_HANDLING, value); };
* 
static const auto PROCESS_CALLBACK_FILTER_ENABLED = 0x1;
    typedef BOOL(WINAPI* GETPROCESSUSERMODEEXCEPTIONPOLICY)(__out LPDWORD lpFlags);
    typedef BOOL(WINAPI* SETPROCESSUSERMODEEXCEPTIONPOLICY)(__in DWORD dwFlags);

       library k32("kernel32");
        ICY_ERROR(k32.initialize());

        auto get_except = reinterpret_cast<GETPROCESSUSERMODEEXCEPTIONPOLICY>(k32.find("GetProcessUserModeExceptionPolicy"));
        auto set_except = reinterpret_cast<SETPROCESSUSERMODEEXCEPTIONPOLICY>(k32.find("SetProcessUserModeExceptionPolicy"));
        auto flags = 0ul;
        if (get_except && set_except)
        {
            get_except(&flags);
            set_except(flags & ~PROCESS_CALLBACK_FILTER_ENABLED);
            set_except(flags);
        }
*/
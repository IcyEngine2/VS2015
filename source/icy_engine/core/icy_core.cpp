#include <icy_engine/core/icy_string.hpp>
#include <icy_engine/core/icy_array.hpp>
#include <icy_engine/core/icy_key.hpp>
#include <icy_engine/core/icy_color.hpp>
#include <icy_engine/core/icy_event.hpp>
#include <Windows.h>
#include <objbase.h>

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
    return {};
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
    copy(name, pair.name);

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

        return to_string(string_view(error_source_array()[k].name), str);
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

ICY_STATIC_NAMESPACE_BEG
struct console_write_args
{
    array<wchar_t> text;
    color color;
};
struct console_event
{
    bool exit = false;
    key key = key::none;
    string line;
};
mutex g_console_mutex;
ICY_STATIC_NAMESPACE_END

static error_type console_init() noexcept
{
    if (g_console_mutex)
        return {};

    if (!AllocConsole())
    {
        const auto error = last_system_error();
        if (error == make_system_error(make_system_error_code(ERROR_ACCESS_DENIED)))
            ;   //  already has console
        else
            ICY_ERROR(error);
    }
    
    const auto func = [](DWORD)
    {
        event::post(nullptr, event_type::global_quit);
        console_exit();        
        Sleep(INFINITE);
        return 0;
    };
    if (!SetConsoleCtrlHandler(func, TRUE))
        return last_system_error();

    ICY_ERROR(g_console_mutex.initialize());
    return {};
}
static error_type console_loop(const event_type type, console_write_args&& args, console_event& output) noexcept
{
    ICY_ERROR(console_init());
    shared_ptr<event_loop> loop;
    ICY_ERROR(event_loop::create(loop, type));
    if (type == event_type::console_write)
    {
        ICY_ERROR(event::post(nullptr, type, std::move(args)));
    }
    else
    {
        ICY_ERROR(event::post(nullptr, type));
    }
    event event;
    ICY_ERROR(loop->loop(event));

    if (event->type == event_type::global_quit)
    {
        output.exit = true;
    }
    else if (event->type == event_type::console_write)
    {
        auto& write = event->data<console_write_args>();
        const auto r = (write.color.r) ? FOREGROUND_RED : 0;
        const auto g = (write.color.g) ? FOREGROUND_GREEN : 0;
        const auto b = (write.color.b) ? FOREGROUND_BLUE : 0;
        const auto a = (write.color.a) ? FOREGROUND_INTENSITY : 0;
        const auto attr = static_cast<WORD>(r | g | b | a);

        CONSOLE_SCREEN_BUFFER_INFO info = { sizeof(info) };
        if (!GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE), &info))
            return last_system_error();

        if (info.wAttributes != attr)
        {
            if (!SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE), attr))
                return last_system_error();
        }

        auto count = 0ul;
        if (!WriteConsoleW(GetStdHandle(STD_OUTPUT_HANDLE), write.text.data(), uint32_t(write.text.size()) - 1, &count, nullptr))
            return last_system_error();

        if (info.wAttributes != attr)
        {
            if (!SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE), info.wAttributes))
                return last_system_error();
        }
    }
    else if (event->type == event_type::console_read_key)
    {
        auto done = false;
        while (!done)
        {
            const auto wait = WaitForSingleObject(GetStdHandle(STD_INPUT_HANDLE), ms_timeout(max_timeout));
            if (wait != WAIT_OBJECT_0)
                return last_system_error();

            INPUT_RECORD buffer[256];
            auto count = 0ul;
            if (!ReadConsoleInputW(GetStdHandle(STD_INPUT_HANDLE), buffer, _countof(buffer), &count))
                return last_system_error();

            for (auto k = 0u; k < count; ++k)
            {
                if (buffer[k].EventType == 0)
                    return {};

                if (buffer[k].EventType != KEY_EVENT)
                    continue;

                const auto win32_msg = buffer[k].Event.KeyEvent;
                output.key = key(win32_msg.wVirtualKeyCode);
                //  detail::scan_vk_to_key(win32_msg.wVirtualKeyCode, win32_msg.wVirtualScanCode, false);
                //ICY_ERROR(event::post(nullptr, event_type::console_read_key, std::move(read_key)));
                done = true;
                break;
            }
        }
    }
    else if (event->type == event_type::console_read_line)
    {
        wchar_t buffer[4096] = {};
        auto count = 0ul;
        CONSOLE_READCONSOLE_CONTROL control = { sizeof(control) };
        control.dwCtrlWakeupMask = 1 << uint32_t(key::enter);
        if (!ReadConsoleW(GetStdHandle(STD_INPUT_HANDLE), buffer, _countof(buffer), &count, &control))
            return last_system_error();

        if (count < 2)
            output.exit = true;

        while (count && (buffer[count - 1] == '\r' || buffer[count - 1] == '\n'))
            --count;

        ICY_ERROR(to_string({ buffer, count }, output.line));
    }
    else
    {
        return make_stdlib_error(std::errc::invalid_argument);
    }
    return {};
}
icy::error_type icy::console_exit() noexcept
{
    INPUT_RECORD buffer[1] = {};
    buffer[0].EventType = KEY_EVENT;
    buffer[0].Event.KeyEvent = { TRUE, 1, VK_RETURN, 0, { L'\r' } };
    auto count = 0ul;
    if (!WriteConsoleInputW(GetStdHandle(STD_INPUT_HANDLE), buffer, _countof(buffer), &count))
        return last_system_error();
    return {};
}
error_type icy::console_write(const string_view str) noexcept
{
    return console_write(str, color(colors::white, 0x00));
}
error_type icy::console_write(const string_view str, const color foreground) noexcept
{
    console_write_args args;
    ICY_ERROR(to_utf16(str, args.text));
    args.color = foreground;
    console_event output;
    ICY_ERROR(console_loop(event_type::console_write, std::move(args), output));
    return {};
}
error_type icy::console_read_line(string& str, bool& exit) noexcept
{
    console_event output;
    ICY_ERROR(console_loop(event_type::console_read_line, {}, output));
    str = std::move(output.line);
    exit = output.exit;
    return {};
}
error_type icy::console_read_key(key& key) noexcept
{
    console_event output;
    ICY_ERROR(console_loop(event_type::console_read_key, {}, output));
    key = output.key;
    return {};
}

error_type library::initialize() noexcept
{
    HINSTANCE__* new_module = nullptr;
    new_module = LoadLibraryA(m_name);
    if (!new_module)
        return last_system_error();
    shutdown();
    m_module = new_module;
    return {};
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
	return {};
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
        ICY_ERROR(last_system_error());
    return {};
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
            event::post(nullptr, event_type::global_timer, pair{ ptr, ptr->m_count });
            --ptr->m_count;
            if (ptr->m_count != 0)
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

    return {};
}

error_type icy::win32_message(const string_view text, const string_view header) noexcept
{
    library lib("user32.dll");
    ICY_ERROR(lib.initialize());
    if (const auto func = ICY_FIND_FUNC(lib, MessageBoxW))
    {
        array<wchar_t> wtxt;
        array<wchar_t> whdr;
        ICY_ERROR(to_utf16(text, wtxt));
        ICY_ERROR(to_utf16(header, whdr));
        func(nullptr, wtxt.data(), whdr.data(), MB_OK);
    }
    else
    {
        return make_stdlib_error(std::errc::function_not_supported);
    }
    return {};
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
    return {};
}
error_type icy::create_guid(guid& guid) noexcept
{
    auto lib = "ole32.dll"_lib;
    ICY_ERROR(lib.initialize());
    if (const auto func = ICY_FIND_FUNC(lib, CoCreateGuid))
    {
        if (const auto hr = func(reinterpret_cast<GUID*>(&guid)))
            return make_system_error(hr);
    }
    else
    {
        return make_stdlib_error(std::errc::function_not_supported);
    }
    return {};
}
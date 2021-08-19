#include <icy_engine/core/icy_console.hpp>
#include <icy_engine/core/icy_array.hpp>
#include <icy_engine/core/icy_string.hpp>
#include <icy_engine/core/icy_thread.hpp>
#include <Windows.h>

using namespace icy;

error_type icy::create_console_system(shared_ptr<console_system>& system) noexcept
{
    auto new_console = false;
    if (!AllocConsole())
    {
        const auto error = last_system_error();
        if (error == make_system_error(make_system_error_code(ERROR_ACCESS_DENIED)))
            ;   //  already has console
        else
            ICY_ERROR(error);
    }
    else
    {
        new_console = true;
    }

    ICY_SCOPE_EXIT{ if (new_console) FreeConsole(); };
    const auto func = [](DWORD)
    {
        icy::post_quit_event();
        Sleep(INFINITE);
        return 0;
    };
    if (!SetConsoleCtrlHandler(func, TRUE))
        return last_system_error();

    shared_ptr<console_system> new_system;
    ICY_ERROR(make_shared(new_system, console_system::tag()));
    ICY_ERROR(new_system->m_lib.initialize());
    ICY_ERROR(new_system->m_sync.initialize()); 

    shared_ptr<event_thread> thread;
    ICY_ERROR(make_shared(thread));
    thread->system = new_system.get();
    new_system->m_thread = std::move(thread);

    new_system->filter(event_type::system_internal);
    system = std::move(new_system);
    new_console = false;
    return error_type();
}
console_system::~console_system() noexcept
{
    filter(0);
}

error_type console_system::show(const bool value) noexcept
{
    const auto win32_show_window = ICY_FIND_FUNC(m_lib, ShowWindowAsync);
    if (!win32_show_window)
        return make_stdlib_error(std::errc::function_not_supported);

    if (!win32_show_window(GetConsoleWindow(), value ? SW_SHOW : SW_HIDE))
        return last_system_error();
    
    return error_type();
}
error_type console_system::exec() noexcept
{  
    uint32_t wait_key = 0;
    while (*this)
    {
        while (auto event = event_system::pop())
        {
            const auto& event_data = event->data<console_event>();
            if (event_data._type == event_type::console_write)
            {
                const auto r = (event_data.color.r) ? FOREGROUND_RED : 0;
                const auto g = (event_data.color.g) ? FOREGROUND_GREEN : 0;
                const auto b = (event_data.color.b) ? FOREGROUND_BLUE : 0;
                const auto a = (event_data.color.a) ? FOREGROUND_INTENSITY : 0;
                const auto attr = static_cast<WORD>(r | g | b | a);

                CONSOLE_SCREEN_BUFFER_INFO info = { sizeof(info) };
                if (!GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE), &info))
                    return last_system_error();

                if (info.wAttributes != attr)
                {
                    if (!SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE), attr))
                        return last_system_error();
                }

                array<wchar_t> wide;
                ICY_ERROR(to_utf16(event_data.text, wide));

                auto count = 0ul;
                if (!WriteConsoleW(GetStdHandle(STD_OUTPUT_HANDLE), wide.data(),
                    uint32_t(wide.size()) - 1, &count, nullptr))
                    return last_system_error();

                if (info.wAttributes != attr)
                {
                    if (!SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE), info.wAttributes))
                        return last_system_error();
                }
            }
            else if (event_data._type == event_type::console_read_key)
            {
                ++wait_key;
            }
            else if (event_data._type == event_type::console_read_line)
            {
                wchar_t buffer[4096] = {};
                auto count = 0ul;
                CONSOLE_READCONSOLE_CONTROL control = { sizeof(control) };
                control.dwCtrlWakeupMask = 1 << uint32_t(key::enter);
                if (!ReadConsoleW(GetStdHandle(STD_INPUT_HANDLE), buffer, _countof(buffer), &count, &control))
                    return last_system_error();

                if (count < 2)
                    break;

                while (count && (buffer[count - 1] == '\r' || buffer[count - 1] == '\n'))
                    --count;

                console_event new_event;
                ICY_ERROR(to_string({ buffer, count }, new_event.text));
                ICY_ERROR(event::post(this, event_type::console_read_line, std::move(new_event)));
            }
        }
        HANDLE handles[2] = {};
        handles[0] = m_sync.handle();
        handles[1] = wait_key ? GetStdHandle(STD_INPUT_HANDLE) : nullptr;
        auto wait = WaitForMultipleObjectsEx(wait_key ? 2 : 1, handles, FALSE, ms_timeout(max_timeout), TRUE);
        if (wait == WAIT_OBJECT_0)
        {
            ;//
        }
        else if (wait == WAIT_OBJECT_0 + 1)
        {
            INPUT_RECORD buffer[256];
            auto count = 0ul;
            if (!ReadConsoleInputW(GetStdHandle(STD_INPUT_HANDLE), buffer, _countof(buffer), &count))
                return last_system_error();

            for (auto k = 0u; k < count; ++k)
            {
                if (buffer[k].EventType == 0)
                    return error_type();
                if (buffer[k].EventType != KEY_EVENT)
                    continue;
                if (!buffer[k].Event.KeyEvent.bKeyDown)
                    continue;

                const auto win32_msg = buffer[k].Event.KeyEvent;
                console_event new_event;
                new_event.key = key(win32_msg.wVirtualKeyCode);
                ICY_ERROR(event::post(this, event_type::console_read_key, std::move(new_event)));
                --wait_key;
                break;
            }
        }
    }
    return error_type();
}
error_type console_system::signal(const event_data* event) noexcept
{
    if (!event)
    {
        INPUT_RECORD buffer[1] = {};
        buffer[0].EventType = KEY_EVENT;
        buffer[0].Event.KeyEvent = { TRUE, 1, VK_RETURN, 0, { L'\r' } };
        auto count = 0ul;
        if (!WriteConsoleInputW(GetStdHandle(STD_INPUT_HANDLE), buffer, _countof(buffer), &count))
            return last_system_error();
    }
    ICY_ERROR(m_sync.wake());
    return error_type();
}
#include <cassert>
#include <thread>
#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include "../../libs/scintilla/include/Scintilla.h"
#include "../../libs/scintilla/include/SciLexer.h"
#include "../../libs/scintilla/include/ILexer.h"
#include "../../libs/scintilla/src/LexerModule.h"
#include "../../libs/scintilla/src/Catalogue.h"
#include "../../libs/scintilla/src/LexAccessor.h"
#include <icy_engine/utility/icy_text_edit.hpp>
#include <icy_engine/core/icy_array.hpp>
#include <icy_engine/core/icy_thread.hpp>
#pragma comment(lib, "scintillad")
#pragma comment(lib, "user32")
#pragma comment(lib, "gdi32")
#pragma comment(lib, "ole32")
#pragma comment(lib, "Imm32")
#pragma comment(lib, "OleAut32")
#pragma comment(lib, "Msimg32")

using namespace icy;

ICY_STATIC_NAMESPACE_BEG
class text_edit_thread_data;
class text_edit_system_data;
enum class internal_message_type : uint32_t
{
    none,
    quit,
    create,
    destroy,
    show,
    text,
    lexer,
    save,
    undo,
    redo,
};
struct internal_message
{
    internal_message_type type = internal_message_type::none;
    uint32_t index = 0;
    bool show = false;
    string text;
    text_edit_lexer lexer;
};
class text_edit_window_data
{
public:
    text_edit_window_data(const text_edit_system_data* const system = nullptr, const int index = 0) noexcept :
        m_system(const_cast<text_edit_system_data*>(system)), m_index(index)
    {

    }
    text_edit_window_data(text_edit_window_data&& rhs) noexcept : m_system(rhs.m_system), m_index(rhs.m_index), m_hwnd(rhs.m_hwnd), m_edit(rhs.m_edit), m_func(rhs.m_func), m_data(rhs.m_data)
    {
        rhs.m_hwnd = nullptr;
        rhs.m_edit = nullptr;
    }
    ICY_DEFAULT_MOVE_ASSIGN(text_edit_window_data);
    ~text_edit_window_data() noexcept;
    error_type initialize(const wchar_t* const clsname) noexcept;
    HWND hwnd() const noexcept
    {
        return m_hwnd;
    }
    error_type text(const string& str) noexcept;
    error_type lexer(const text_edit_lexer& style) noexcept;
    error_type save() noexcept;
    error_type undo() noexcept;
    error_type redo() noexcept;
private:
    error_type notify(const SCNotification& data) noexcept;
private:
    text_edit_system_data* m_system = nullptr;
    int m_index = 0;
    HWND m_hwnd = nullptr;
    HWND m_edit = nullptr;
    SciFnDirect m_func = nullptr;
    sptr_t m_data = 0;
    bool m_ignore = false;
};
class text_edit_thread_data : public thread
{   
public:
    error_type run() noexcept override;
public:
    text_edit_system_data* system = nullptr;
};
class text_edit_system_data : public text_edit_system
{
public:
    text_edit_system_data() noexcept = default;
    ~text_edit_system_data() noexcept;
    error_type initialize() noexcept;
    error_type exec() noexcept override;
private:
    error_type signal(const event_data& event) noexcept override
    {
        ICY_LOCK_GUARD(m_lock);
        if (m_hwnd && !PostMessageW(m_hwnd, WM_USER, 0, 0))
            return last_system_error();
        return error_type();
    }
    event pop() noexcept
    {
        return event_system::pop();
    }
public:
    error_type create(text_edit_window& window) const noexcept;
    const icy::thread& thread() const noexcept override
    {
        return *m_thread;
    }
    void destroy(const uint32_t window) noexcept;
    error_type show(const text_edit_window window, const bool value) noexcept;
    error_type text(const text_edit_window window, const string_view value) noexcept;
    error_type lexer(const text_edit_window window, text_edit_lexer&& value) noexcept;
    error_type save(const text_edit_window window) noexcept;
    error_type undo(const text_edit_window window) noexcept;
    error_type redo(const text_edit_window window) noexcept;
    HWND hwnd(const uint32_t window) noexcept;
    HWND wait(const uint32_t window) noexcept;
    void exit(const error_type error) noexcept
    {
        m_thread->exit(m_error = error);
        m_thread->cancel();
    }
private:
    mutable mutex m_lock;
    wchar_t m_cname[64] = {};
    bool m_lib = false;
    mutable map<uint32_t, text_edit_window_data> m_data;
    mutable uint32_t m_next = 1;
    HWND m_hwnd = nullptr;
    shared_ptr<text_edit_thread_data> m_thread;
    error_type m_error;
};
ICY_STATIC_NAMESPACE_END

class text_edit_window::data_type
{
public:
    std::atomic<uint32_t> ref = 1;
    uint32_t index = 0;
    weak_ptr<text_edit_system_data> system;
};

text_edit_window_data::~text_edit_window_data() noexcept
{
    SetWindowLongPtrW(m_hwnd, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(DefWindowProcW));
    if (m_edit) DestroyWindow(m_edit);
    if (m_hwnd) DestroyWindow(m_hwnd);
}
error_type text_edit_window_data::initialize(const wchar_t* const clsname) noexcept
{
    m_hwnd = CreateWindowExW(0, clsname, nullptr, WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, nullptr, nullptr, win32_instance(), nullptr);
    if (!m_hwnd)
        return last_system_error();

    const auto proc = [](HWND hwnd, UINT32 msg, WPARAM wparam, LPARAM lparam) -> LRESULT
    {
        const auto self = reinterpret_cast<text_edit_window_data*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
        if (self)
        {
            if (msg == WM_NOTIFY)
            {
                const auto hdr = reinterpret_cast<SCNotification*>(lparam);
                if (hdr && hdr->nmhdr.hwndFrom == self->m_edit)
                {
                    if (const auto error = self->notify(*hdr))
                        self->m_system->exit(error);
                }
            }
            else if (msg == WM_SIZE)
            {
                RECT rect;
                if (!GetClientRect(hwnd, &rect) || !SetWindowPos(self->m_edit, nullptr, 0, 0, rect.right - rect.left, rect.bottom - rect.top, 0))
                    self->m_system->exit(last_system_error());
            }
            else if (msg == WM_CLOSE)
            {
                text_edit_event new_event;
                new_event.window = self->m_index;
                new_event.type = text_edit_event::type::close;
                if (const auto error = event::post(self->m_system, text_edit_event_type, std::move(new_event)))
                    self->m_system->exit(error);
                return 0;
            }
        }
        return DefWindowProcW(hwnd, msg, wparam, lparam);
    };

    SetWindowLongPtrW(m_hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(this));
    SetWindowLongPtrW(m_hwnd, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(static_cast<WNDPROC>(proc)));

    m_edit = CreateWindowExW(0, L"Scintilla", nullptr, WS_VISIBLE | WS_TABSTOP | WS_CLIPCHILDREN | WS_CHILD,
        CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, m_hwnd, nullptr, win32_instance(), nullptr);
    if (!m_edit)
        return last_system_error();

    m_func = reinterpret_cast<SciFnDirect>(SendMessageW(m_edit, SCI_GETDIRECTFUNCTION, 0, 0));
    m_data = sptr_t(SendMessageW(m_edit, SCI_GETDIRECTPOINTER, 0, 0));
    m_func(m_data, SCI_SETMARGINWIDTHN, 0, 16);
        
    SendMessageW(m_hwnd, WM_SIZE, 0, 0);

    return error_type();
}
error_type text_edit_window_data::text(const string& str) noexcept 
{
    const auto ignore = m_ignore;
    m_ignore = true;
    ICY_SCOPE_EXIT{ m_ignore = ignore; };
    m_func(m_data, SCI_SETTEXT, 0, reinterpret_cast<uptr_t>(str.bytes().data()));
    m_func(m_data, SCI_EMPTYUNDOBUFFER, 0, 0);
    return error_type();
}
error_type text_edit_window_data::lexer(const text_edit_lexer& value) noexcept
{
    const auto ignore = m_ignore;
    m_ignore = true;
    ICY_SCOPE_EXIT{ m_ignore = ignore; };

    m_func(m_data, SCI_SETLEXERLANGUAGE, 0, reinterpret_cast<sptr_t>(value.name.bytes().data()));
    auto index = 0;
    for (auto&& keywords : value.keywords)
        m_func(m_data, SCI_SETKEYWORDS, index++, reinterpret_cast<sptr_t>(keywords.bytes().data()));
   
    for (auto&& pair : value.styles)
    {
        const auto& style = pair.value;

        if (!style.font.empty())
            m_func(m_data, SCI_STYLESETFONT, pair.key, reinterpret_cast<sptr_t>(style.font.bytes().data()));
        
        if (style.foreground != color())
            m_func(m_data, SCI_STYLESETFORE, pair.key, RGB(style.foreground.r, style.foreground.g, style.foreground.b));
        
        if (style.background != color())
            m_func(m_data, SCI_STYLESETBACK, pair.key, RGB(style.background.r, style.background.g, style.background.b));

        if (style.size > 0)
            m_func(m_data, SCI_STYLESETSIZEFRACTIONAL, pair.key, sptr_t(style.size * 100));

        if (style.weight)
            m_func(m_data, SCI_STYLESETWEIGHT, pair.key, style.weight);

        if (style.flags & text_edit_flag::italic)
            m_func(m_data, SCI_STYLESETITALIC, pair.key, 1);

        if (style.flags & text_edit_flag::bold)
            m_func(m_data, SCI_STYLESETBOLD, pair.key, 1);

        if (style.flags & text_edit_flag::underline)
            m_func(m_data, SCI_STYLESETUNDERLINE, pair.key, 1);


        if (style.flags & text_edit_flag::case_camel)
            m_func(m_data, SCI_STYLESETCASE, pair.key, SC_CASE_CAMEL);
        else if (style.flags & text_edit_flag::case_lower)
            m_func(m_data, SCI_STYLESETCASE, pair.key, SC_CASE_LOWER);
        else if (style.flags & text_edit_flag::case_upper)
            m_func(m_data, SCI_STYLESETCASE, pair.key, SC_CASE_UPPER);

        if (style.flags & text_edit_flag::hotspot)
            m_func(m_data, SCI_STYLESETHOTSPOT, pair.key, 1);

        if (style.flags & text_edit_flag::hidden)
            m_func(m_data, SCI_STYLESETVISIBLE, pair.key, 0);
    }
    return error_type();
}
error_type text_edit_window_data::save() noexcept
{
    m_func(m_data, SCI_SETSAVEPOINT, 0, 0);
    return error_type();
}
error_type text_edit_window_data::undo() noexcept
{
    m_func(m_data, SCI_UNDO, 0, 0);
    return error_type();
}
error_type text_edit_window_data::redo() noexcept
{
    m_func(m_data, SCI_REDO, 0, 0);
    return error_type();
}
error_type text_edit_window_data::notify(const SCNotification& data) noexcept
{
    if (m_ignore)
        return error_type();

    if (data.nmhdr.code == SCN_MODIFIED)
    {
        text_edit_event new_event;
        new_event.window = m_index;
        if (data.modificationType & SC_MOD_INSERTTEXT)
        {
            new_event.type = text_edit_event::type::insert;
            ICY_ERROR(new_event.text.append(data.text, data.text + data.length));
        }
        else if (data.modificationType & SC_MOD_DELETETEXT)
        {
            new_event.type = text_edit_event::type::remove;
        }
        else
        {
            return error_type();
        }
        new_event.offset = data.position;
        new_event.length = data.length;
        new_event.can_redo = m_func(m_data, SCI_CANREDO, 0, 0);
        new_event.can_undo = m_func(m_data, SCI_CANUNDO, 0, 0);
        ICY_ERROR(event::post(m_system, text_edit_event_type, std::move(new_event)));        
    }
    return error_type();
}

text_edit_window::text_edit_window(const text_edit_window& rhs) noexcept : data(rhs.data)
{
    if (data)
        data->ref.fetch_add(1, std::memory_order_release);
}
text_edit_window::~text_edit_window() noexcept
{
    if (data && data->ref.fetch_sub(1, std::memory_order_acq_rel) == 1)
    {
        if (auto system = shared_ptr<text_edit_system_data>(data->system))
            system->destroy(data->index);
        allocator_type::destroy(data);
        allocator_type::deallocate(data);
    }
}
HWND__* text_edit_window::hwnd() const noexcept
{
    if (data)
    {
        if (auto system = shared_ptr<text_edit_system_data>(data->system))
            return system->wait(data->index);
    }
    return nullptr;
}
uint32_t text_edit_window::index() const noexcept
{
    return data ? data->index : 0;
}
error_type text_edit_window::show(const bool value) noexcept
{
    if (data)
    {
        if (auto system = shared_ptr<text_edit_system_data>(data->system))
            return system->show(*this, value);
    }
    return make_stdlib_error(std::errc::invalid_argument);
}
error_type text_edit_window::text(const string_view value) noexcept
{
    if (data)
    {
        if (auto system = shared_ptr<text_edit_system_data>(data->system))
            return system->text(*this, value);
    }
    return make_stdlib_error(std::errc::invalid_argument);
}
error_type text_edit_window::lexer(text_edit_lexer&& value) noexcept
{
    if (data)
    {
        if (auto system = shared_ptr<text_edit_system_data>(data->system))
            return system->lexer(*this, std::move(value));
    }
    return make_stdlib_error(std::errc::invalid_argument);
}
error_type text_edit_window::save() noexcept
{
    if (data)
    {
        if (auto system = shared_ptr<text_edit_system_data>(data->system))
            return system->save(*this);
    }
    return make_stdlib_error(std::errc::invalid_argument);
}
error_type text_edit_window::undo() noexcept
{
    if (data)
    {
        if (auto system = shared_ptr<text_edit_system_data>(data->system))
            return system->undo(*this);
    }
    return make_stdlib_error(std::errc::invalid_argument);
}
error_type text_edit_window::redo() noexcept
{
    if (data)
    {
        if (auto system = shared_ptr<text_edit_system_data>(data->system))
            return system->redo(*this);
    }
    return make_stdlib_error(std::errc::invalid_argument);
}

error_type text_edit_thread_data::run() noexcept
{
    return system->exec();
}

error_type text_edit_system_data::exec() noexcept
{
    auto hwnd = CreateWindowExW(0, L"STATIC", nullptr, 0, 0, 0, 0, 0, HWND_MESSAGE, nullptr, win32_instance(), nullptr);
    if (!hwnd)
        return last_system_error();

    ICY_SCOPE_EXIT
    {
        {
            ICY_LOCK_GUARD(m_lock);
            m_hwnd = nullptr;
        }
        if (hwnd)
        {
            SetWindowLongPtrW(hwnd, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(DefWindowProcW));
            DestroyWindow(hwnd);
        };
    };
    {
        ICY_LOCK_GUARD(m_lock);
        m_hwnd = hwnd;
    }
    
    const auto proc = [](HWND hwnd, UINT32 msg, WPARAM wparam, LPARAM lparam) -> LRESULT
    {
        const auto self = reinterpret_cast<text_edit_system_data*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
        if (self)
        {
            if (msg == WM_USER)
            {
                while (auto event = self->pop())
                {
                    if (event->type == event_type::global_quit)
                    {
                        self->exit(error_type());
                    }
                    else if (event->type == event_type::system_internal)
                    {
                        const auto& msg_data = event->data<internal_message>();
                        ICY_LOCK_GUARD(self->m_lock);
                        auto it = self->m_data.find(msg_data.index);
                        if (it == self->m_data.end())
                        {
                            self->exit(make_stdlib_error(std::errc::invalid_argument));
                            continue;
                        }
                        auto& wnd = it->value;
                        if (msg_data.type == internal_message_type::create)
                        {
                            if (!wnd.hwnd())
                            {
                                if (const auto error = wnd.initialize(self->m_cname))
                                {
                                    self->m_data.erase(it);
                                    self->exit(error);
                                }
                            }
                        }
                        else if (msg_data.type == internal_message_type::destroy)
                        {
                            self->m_data.erase(it);
                        }
                        else if (msg_data.type == internal_message_type::show)
                        {
                            const auto window_hwnd = wnd.hwnd();
                            auto style = GetWindowLongW(window_hwnd, GWL_STYLE);
                            style &= ~WS_VISIBLE;
                            if (msg_data.show)
                                style |= WS_VISIBLE;
                            SetWindowLongW(window_hwnd, GWL_STYLE, style);
                        }
                        else if (msg_data.type == internal_message_type::text)
                        {
                            if (const auto error = wnd.text(msg_data.text))
                                self->exit(error);
                        }
                        else if (msg_data.type == internal_message_type::lexer)
                        {
                            if (const auto error = wnd.lexer(msg_data.lexer))
                                self->exit(error);
                        }
                        else if (msg_data.type == internal_message_type::save)
                        {
                            if (const auto error = wnd.save())
                                self->exit(error);
                        }
                        else if (msg_data.type == internal_message_type::undo)
                        {
                            if (const auto error = wnd.undo())
                                self->exit(error);
                        }
                        else if (msg_data.type == internal_message_type::redo)
                        {
                            if (const auto error = wnd.redo())
                                self->exit(error);
                        }
                    }
                }
            }
            else if (msg == WM_USER + 1)
            {
                ICY_LOCK_GUARD(self->m_lock);
                const auto it = self->m_data.find(wparam);
                if (it == self->m_data.end())
                {
                    self->exit(make_stdlib_error(std::errc::invalid_argument));
                    return 0;
                }

                auto& wnd = it->value;
                if (!wnd.hwnd())
                {
                    if (const auto error = wnd.initialize(self->m_cname))
                    {
                        self->m_data.erase(it);
                        self->exit(error);
                        return 0;
                    }
                }
                return reinterpret_cast<LRESULT>(wnd.hwnd());
            }
        }
        return DefWindowProcW(hwnd, msg, wparam, lparam);
    };
    SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(this));
    SetWindowLongPtrW(hwnd, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(static_cast<WNDPROC>(proc)));

    const auto timeout = max_timeout;
    while (true)
    {
        const auto wait = MsgWaitForMultipleObjectsEx(0, nullptr, ms_timeout(timeout), QS_ALLINPUT, MWMO_ALERTABLE);
        if (wait == WAIT_OBJECT_0)
        {
            MSG msg = {};
            while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE))
            {
                TranslateMessage(&msg);
                DispatchMessageW(&msg);
            }
            ICY_ERROR(m_error);
        }
        else if (wait == WAIT_TIMEOUT)
        {
            return make_stdlib_error(std::errc::timed_out);
        }
        else if (wait == WAIT_IO_COMPLETION)
        {
            return m_error;
        }
        else
        {
            return last_system_error();
        }
    }
    return error_type();
}
text_edit_system_data::~text_edit_system_data() noexcept
{
    if (m_thread)
        m_thread->wait();
    if (m_lib)
        Scintilla_ReleaseResources();
    if (*m_cname)
        UnregisterClassW(m_cname, win32_instance());
    filter(0);
}
error_type text_edit_system_data::initialize() noexcept
{
    ICY_ERROR(m_lock.initialize());

    swprintf_s(m_cname, L"Icy Text Edit Window %u.%u", GetCurrentProcessId(), GetCurrentThreadId());

    WNDCLASSEXW cls = { sizeof(WNDCLASSEXW) };
    cls.lpszClassName = m_cname;
    cls.lpfnWndProc = DefWindowProcW;
    cls.hInstance = win32_instance();
    cls.hCursor = LoadCursorW(cls.hInstance, IDC_ARROW);
    if (RegisterClassExW(&cls) == 0)
        return last_system_error();
        
    if (!Scintilla_RegisterClasses(cls.hInstance))
        return last_system_error();
    
    if (!Scintilla_LinkLexers())
    {
        const auto error = last_system_error();
        Scintilla_ReleaseResources();
        return error;
    }
    m_lib = true;

    ICY_ERROR(make_shared(m_thread));
    m_thread->system = this;
    return error_type();
}
error_type text_edit_system_data::create(text_edit_window& window) const noexcept
{
    auto new_data = allocator_type::allocate<text_edit_window::data_type>(1);
    if (!new_data)
        return make_stdlib_error(std::errc::not_enough_memory);
    allocator_type::construct(new_data);
    ICY_SCOPE_EXIT{ if (new_data) { allocator_type::destroy(new_data); allocator_type::deallocate(new_data); } };
    new_data->system = make_shared_from_this(this);
    {
        ICY_LOCK_GUARD(m_lock);
        new_data->index = m_next;
        ICY_ERROR(m_data.insert(m_next, text_edit_window_data(this, m_next)));
        m_next++;

        const auto self = const_cast<text_edit_system_data*>(this);

        internal_message msg;
        msg.type = internal_message_type::create;
        msg.index = new_data->index;
        ICY_ERROR(self->post(self, event_type::system_internal, std::move(msg)));
    }
    window = text_edit_window();
    window.data = new_data;
    new_data = nullptr;
    return error_type();
}
void text_edit_system_data::destroy(const uint32_t window) noexcept
{
    internal_message msg;
    msg.type = internal_message_type::destroy;
    msg.index = window;
    event_system::post(this, event_type::system_internal, std::move(msg));
}
error_type text_edit_system_data::show(const text_edit_window window, const bool value) noexcept
{
    if (!window.data)
        return make_stdlib_error(std::errc::invalid_argument);
    internal_message msg;
    msg.type = internal_message_type::show;
    msg.index = window.data->index;
    msg.show = value;
    return event_system::post(this, event_type::system_internal, std::move(msg));
}
error_type text_edit_system_data::text(const text_edit_window window, const string_view str) noexcept
{
    if (!window.data)
        return make_stdlib_error(std::errc::invalid_argument);
    internal_message msg;
    msg.type = internal_message_type::text;
    msg.index = window.data->index;
    ICY_ERROR(copy(str, msg.text));
    return event_system::post(this, event_type::system_internal, std::move(msg));
}
error_type text_edit_system_data::lexer(const text_edit_window window, text_edit_lexer&& value) noexcept
{
    if (!window.data)
        return make_stdlib_error(std::errc::invalid_argument);
    internal_message msg;
    msg.type = internal_message_type::lexer;
    msg.index = window.data->index;
    msg.lexer = std::move(value);
    return event_system::post(this, event_type::system_internal, std::move(msg));
}
error_type text_edit_system_data::save(const text_edit_window window) noexcept
{
    if (!window.data)
        return make_stdlib_error(std::errc::invalid_argument);
    internal_message msg;
    msg.type = internal_message_type::save;
    msg.index = window.data->index;
    return event_system::post(this, event_type::system_internal, std::move(msg));
}
error_type text_edit_system_data::undo(const text_edit_window window) noexcept
{
    if (!window.data)
        return make_stdlib_error(std::errc::invalid_argument);
    internal_message msg;
    msg.type = internal_message_type::undo;
    msg.index = window.data->index;
    return event_system::post(this, event_type::system_internal, std::move(msg));
}
error_type text_edit_system_data::redo(const text_edit_window window) noexcept
{
    if (!window.data)
        return make_stdlib_error(std::errc::invalid_argument);
    internal_message msg;
    msg.type = internal_message_type::redo;
    msg.index = window.data->index;
    return event_system::post(this, event_type::system_internal, std::move(msg));
}
HWND text_edit_system_data::hwnd(const uint32_t window) noexcept
{
    ICY_LOCK_GUARD(m_lock);
    if (const auto ptr = m_data.try_find(window))
        return ptr->hwnd();
    return nullptr;
}
HWND text_edit_system_data::wait(const uint32_t window) noexcept
{
    return reinterpret_cast<HWND>(SendMessageW(m_hwnd, WM_USER + 1, window, 0));
}

error_type icy::create_event_system(shared_ptr<text_edit_system>& system) noexcept 
{
    shared_ptr<text_edit_system_data> new_ptr;
    ICY_ERROR(make_shared(new_ptr));
    ICY_ERROR(new_ptr->initialize());
    system = std::move(new_ptr);
    return error_type();
}

const event_type icy::text_edit_event_type = icy::next_event_user();
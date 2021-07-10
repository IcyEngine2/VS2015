#include <icy_engine/utility/icy_minhook.hpp>
#include "mbox_server.hpp"
#include "mbox_client.hpp"

#include <Windows.h>
#include <windowsx.h>

#pragma comment(lib, "user32")
#if _DEBUG
#pragma comment(lib, "icy_engine_cored")
#pragma comment(lib, "icy_engine_networkd")
#else
#pragma comment(lib, "icy_engine_core")
#pragma comment(lib, "icy_engine_network")
#endif

using namespace icy;

enum class dll_reason
{
    proc_exit,
    proc_load,
    thrd_load,
    thrd_exit,
};

static heap g_heap;
ICY_DECLARE_GLOBAL(input_thread::instance);
static hook<decltype(&GetMessageW)> g_hook_gw;
static hook<decltype(&GetMessageA)> g_hook_ga;
static hook<decltype(&PeekMessageW)> g_hook_pw;
static hook<decltype(&PeekMessageA)> g_hook_pa;
static hook<decltype(&DestroyWindow)> g_hook_destroy;
static std::atomic<HWND> g_hwnd;


static bool pop_queue(MSG* msg, uint32_t min, uint32_t max, const bool remove = true)
{
    static error_type g_launch = input_thread::initialize();

    if (!msg || g_launch)
        return false;

    static MSG g_recv_msg;
    if (g_recv_msg.message == 0)
    {
        input_message input;
        if (!input_thread::instance->recv(input))
            return false;
        
        detail::to_winapi(input, g_recv_msg.message, g_recv_msg.wParam, g_recv_msg.lParam);
    }
    if (g_recv_msg.message == 0)
        return false;
    
    if (min == 0 && max == 0 || g_recv_msg.message >= min && g_recv_msg.message <= max)
    {
        *msg = g_recv_msg;
        if (remove)
            g_recv_msg = {};
        return true;
    }
    return false;
}
static error_type push_queue(const MSG* msg) noexcept
{
    if (!msg || !input_thread::is_ready())
        return error_type();

    switch (msg->message)
    {
    case WM_KEYDOWN:
    case WM_KEYUP:
    case WM_SYSKEYDOWN:
    case WM_SYSKEYUP:
    {
        uint8_t keyboard[0x100];
        if (!GetKeyboardState(keyboard))
            return last_system_error();

        input_message key_input;
        auto count = detail::key_from_winapi(key_input, msg->wParam, msg->lParam, keyboard);
        for (auto k = 0u; k < count; ++k)
        {
            ICY_ERROR(input_thread::instance->send(std::move(key_input)));
        }
        break;
    }
    case WM_MOUSEWHEEL:
    case WM_LBUTTONDOWN:
    case WM_RBUTTONDOWN:
    case WM_MBUTTONDOWN:
    case WM_XBUTTONDOWN:
    case WM_LBUTTONUP:
    case WM_RBUTTONUP:
    case WM_MBUTTONUP:
    case WM_XBUTTONUP:
    case WM_LBUTTONDBLCLK:
    case WM_RBUTTONDBLCLK:
    case WM_MBUTTONDBLCLK:
    case WM_XBUTTONDBLCLK:
    {
        uint8_t keyboard[0x100];
        if (!GetKeyboardState(keyboard))
            return last_system_error();

        auto offset_x = 0;
        auto offset_y = 0;
        input_message mouse_input;
        detail::mouse_from_winapi(mouse_input, *msg, keyboard);
        if (msg->message == WM_MOUSEWHEEL)
        {
            auto point = POINT{ GET_X_LPARAM(msg->lParam), GET_Y_LPARAM(msg->lParam) };
            if (ScreenToClient(msg->hwnd, &point))
            {
                mouse_input.point_x = point.x;
                mouse_input.point_y = point.y;
            }
        }
        ICY_ERROR(input_thread::instance->send(std::move(mouse_input)));
        break;
    }
    }
    return error_type();
}
static int __stdcall get_message_w(MSG* lpMsg, HWND__* hWnd, uint32_t wMsgFilterMin, uint32_t wMsgFilterMax)
{
    HWND__* exp = nullptr;
    g_hwnd.compare_exchange_strong(exp, hWnd);
    
    if (pop_queue(lpMsg, wMsgFilterMin, wMsgFilterMax))
        return 1;
    auto value = g_hook_gw(lpMsg, hWnd, wMsgFilterMin, wMsgFilterMax);
    if (value == 1)
    {
        push_queue(lpMsg);
    }
    else
    {
        //g_thread->wait();
    }
    return value;
}
static int __stdcall get_message_a(MSG* lpMsg, HWND__* hWnd, uint32_t wMsgFilterMin, uint32_t wMsgFilterMax)
{
    HWND__* exp = nullptr;
    g_hwnd.compare_exchange_strong(exp, hWnd);

    if (pop_queue(lpMsg, wMsgFilterMin, wMsgFilterMax))
        return 1;
    auto value = g_hook_ga(lpMsg, hWnd, wMsgFilterMin, wMsgFilterMax);
    if (value == 1)
    {
        push_queue(lpMsg);
    }
    else 
    {
        //g_thread->wait();        
    }
    return value;
}
static int __stdcall peek_message_w(MSG* lpMsg, HWND__* hWnd, uint32_t wMsgFilterMin, uint32_t wMsgFilterMax, UINT wRemoveMsg)
{
    HWND__* exp = nullptr;
    g_hwnd.compare_exchange_strong(exp, hWnd);

    if (pop_queue(lpMsg, wMsgFilterMin, wMsgFilterMax, wRemoveMsg != 0))
        return 1;
    auto value = g_hook_pw(lpMsg, hWnd, wMsgFilterMin, wMsgFilterMax, wRemoveMsg);
    if (value == 1 && wRemoveMsg)
    {
        push_queue(lpMsg);
    }
    else if (lpMsg && (lpMsg->message == WM_QUIT))
    {
        //g_thread->wait();
    }
    return value;
}
static int __stdcall peek_message_a(MSG* lpMsg, HWND__* hWnd, uint32_t wMsgFilterMin, uint32_t wMsgFilterMax, UINT wRemoveMsg)
{
    HWND__* exp = nullptr;
    g_hwnd.compare_exchange_strong(exp, hWnd);

    if (pop_queue(lpMsg, wMsgFilterMin, wMsgFilterMax, wRemoveMsg != 0))
        return 1;
    auto value = g_hook_pa(lpMsg, hWnd, wMsgFilterMin, wMsgFilterMax, wRemoveMsg);
    if (value == 1 && wRemoveMsg)
    {
        push_queue(lpMsg);
    }
    else if (lpMsg && (lpMsg->message == WM_QUIT))
    {
        //g_thread->wait();
    }
    return value;
}
static int __stdcall destroy_window(HWND hwnd)
{
    if (hwnd == g_hwnd)
    {
        if (auto instance = input_thread::instance)
        {
            input_thread::instance->exit(error_type());
            input_thread::instance->wait();
        }
    }
    return g_hook_destroy(hwnd);
}

static error_type initialize() noexcept
{
    if (g_heap.info().capacity)
        return error_type();

    ICY_ERROR(g_heap.initialize(heap_init::global(64_mb)));
    ICY_ERROR(g_hook_gw.initialize(&GetMessageW, get_message_w));
    ICY_ERROR(g_hook_ga.initialize(&GetMessageA, get_message_a));
    ICY_ERROR(g_hook_pw.initialize(&PeekMessageA, peek_message_a));
    ICY_ERROR(g_hook_pa.initialize(&PeekMessageW, peek_message_w));
    ICY_ERROR(g_hook_destroy.initialize(&DestroyWindow, destroy_window));
    return error_type();
}

int __stdcall DllMain(HINSTANCE__* handle, unsigned long reason, void* reserved)
{
    if (reason == uint32_t(dll_reason::proc_load))
    {
        if (const auto error = initialize())
            return 0;
    }
    return 1;
}
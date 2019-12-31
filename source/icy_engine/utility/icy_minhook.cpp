#include <icy_engine/utility/icy_minhook.hpp>
#include <icy_engine/core/icy_string.hpp>
#include "../../libs/minhook/MinHook.h"

#if _DEBUG
#pragma comment(lib, "minhookd")
#else
#pragma comment(lib, "minhook")
#endif

using namespace icy;

static detail::spin_lock<> g_hook_lock;
static uint32_t g_hook_count = 0;
static error_type make_minhook_error(const MH_STATUS code) noexcept
{
    return error_type(code, error_source_minhook);
}

void hook_base::shutdown() noexcept
{
    if (!m_new_func)
        return;

    MH_RemoveHook(m_new_func);
    m_new_func = nullptr;
    ICY_LOCK_GUARD(g_hook_lock);
    --g_hook_count;
    if (g_hook_count == 0)
        MH_Uninitialize();
}
error_type hook_base::initialize(void* const old_func, void* const new_func) noexcept
{
    if (!old_func || !new_func)
        return make_stdlib_error(std::errc::invalid_argument);

    shutdown();
    {
        ICY_LOCK_GUARD(g_hook_lock);
        if (g_hook_count == 0)
        {
            if (const auto error = MH_Initialize())
                return make_minhook_error(error);
        }
        ++g_hook_count;
    }
    auto error = MH_OK;
    if (!error) error = MH_CreateHook(old_func, m_new_func = new_func, &m_old_func);
    if (!error) error = MH_EnableHook(old_func);
    if (error)
    {
        shutdown();
        return make_minhook_error(error);
    }
    return {};
}

static error_type minhook_error_to_string(const unsigned code, const string_view locale, string& str) noexcept
{
    switch (MH_STATUS(code))
    {
    case MH_UNKNOWN: return to_string("Unknown"_s, str);
    case MH_ERROR_ALREADY_INITIALIZED: return to_string("MinHook is not initialized yet, or already uninitialized", str);
    case MH_ERROR_NOT_INITIALIZED: return to_string("MinHook is not initialized yet, or already uninitialized", str);
    case MH_ERROR_ALREADY_CREATED: return to_string("The hook for the specified target function is already created", str);
    case MH_ERROR_NOT_CREATED: return to_string("The hook for the specified target function is not created yet", str);
    case MH_ERROR_ENABLED: return to_string("The hook for the specified target function is already enabled", str);
    case MH_ERROR_DISABLED: return to_string("The hook for the specified target function is not enabled yet, or already disabled", str);
    case MH_ERROR_NOT_EXECUTABLE: return to_string("The specified pointer is invalid. It points the address of non-allocated and/or non-executable region", str);
    case MH_ERROR_UNSUPPORTED_FUNCTION: return to_string("The specified target function cannot be hooked", str);
    case MH_ERROR_MEMORY_ALLOC: return to_string("Failed to allocate memory", str);
    case MH_ERROR_MEMORY_PROTECT: return to_string("Failed to change the memory protection", str);
    case MH_ERROR_MODULE_NOT_FOUND:return to_string("The specified module is not loaded", str);
    case MH_ERROR_FUNCTION_NOT_FOUND:return to_string("The specified function is not found", str);
    default:
        return make_stdlib_error(std::errc::invalid_argument);
    }     
}
const error_source icy::error_source_minhook = register_error_source("minhook"_s, minhook_error_to_string);

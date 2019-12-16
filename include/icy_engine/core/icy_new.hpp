//  no '#pragma once', include in any source

#include "icy_memory.hpp"

#ifndef ICY_STATIC_BUFFER_CAPACITY
#error define ICY_STATIC_BUFFER_CAPACITY > 0
#endif

namespace
{
    static char static_buffer[ICY_STATIC_BUFFER_CAPACITY];
    static size_t static_capacity = 0;
    void* static_realloc(const void* const ptr, const size_t size) noexcept
    {
        if (!ptr)
        {
            if (icy::detail::global_heap.realloc)
            {
                return icy::realloc(ptr, size);
            }
            else if (static_capacity + size > _countof(static_buffer))
            {
                ICY_ASSERT(false, "OUT OF STATIC MEMORY");
            }
            else
            {
                const auto memory = static_buffer + static_capacity;
                static_capacity += size;
                return memory;
            }
        }
        if (ptr >= static_buffer && ptr < static_buffer + _countof(static_buffer))
        {
            ;
        }
        else if (ptr)
        {
            if (icy::detail::global_heap.realloc)
                return icy::realloc(ptr, size);
            ICY_ASSERT(false, "INVALID 'REALLOC' POINTER");
        }
        return nullptr;
    }
}

void* operator new (std::size_t count)
{
	const auto ptr = static_realloc(nullptr, count);
	if (!ptr)
		throw std::bad_alloc();
	return ptr;
}
void* operator new[](std::size_t count)
{
	const auto ptr = static_realloc(nullptr, count);
	if (!ptr)
		throw std::bad_alloc();
	return ptr;
}
void* operator new  (std::size_t count, const std::nothrow_t&) noexcept
{
	return static_realloc(nullptr, count);
}
void* operator new[](std::size_t count, const std::nothrow_t&) noexcept
{
	return static_realloc(nullptr, count);
}
void operator delete  (void* ptr) noexcept
{
    static_realloc(ptr, 0);
}
void operator delete[](void* ptr) noexcept
{
    static_realloc(ptr, 0);
}
void operator delete  (void* ptr, const std::nothrow_t&) noexcept
{
    static_realloc(ptr, 0);
}
void operator delete[](void* ptr, const std::nothrow_t&) noexcept
{
    static_realloc(ptr, 0);
}
void operator delete  (void* ptr, std::size_t) noexcept
{
    static_realloc(ptr, 0);
}
void operator delete[](void* ptr, std::size_t) noexcept
{
    static_realloc(ptr, 0);
}
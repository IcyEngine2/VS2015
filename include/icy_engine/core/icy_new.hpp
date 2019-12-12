//  no '#pragma once', include in any sourcerc

#include "icy_memory.hpp"

void* operator new (std::size_t count)
{
	const auto ptr = icy::realloc(nullptr, count);
	if (!ptr)
		throw std::bad_alloc();
	return ptr;
}
void* operator new[](std::size_t count)
{
	const auto ptr = icy::realloc(nullptr, count);
	if (!ptr)
		throw std::bad_alloc();
	return ptr;
}
void* operator new  (std::size_t count, const std::nothrow_t&) noexcept
{
	return icy::realloc(nullptr, count);
}
void* operator new[](std::size_t count, const std::nothrow_t&) noexcept
{
	return icy::realloc(nullptr, count);
}
void operator delete  (void* ptr) noexcept
{
	icy::realloc(ptr, 0);
}
void operator delete[](void* ptr) noexcept
{
	icy::realloc(ptr, 0);
}
void operator delete  (void* ptr, const std::nothrow_t&) noexcept
{
	icy::realloc(ptr, 0);
}
void operator delete[](void* ptr, const std::nothrow_t&) noexcept
{
	icy::realloc(ptr, 0);
}
void operator delete  (void* ptr, std::size_t) noexcept
{
    icy::realloc(ptr, 0);
}
void operator delete[](void* ptr, std::size_t) noexcept
{
    icy::realloc(ptr, 0);
}
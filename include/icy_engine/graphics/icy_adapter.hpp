#pragma once

#include "icy_core.hpp"

namespace icy
{
    template<typename T> class array;
    class string_view;

 	enum class adapter_flag : uint32_t
	{
		none		=	0x00,
		hardware	=	0x01,
		d3d11		=	0x02,
		d3d12		=	0x04,
	};
    inline bool operator&(const adapter_flag lhs, const adapter_flag rhs) noexcept
    {
        return !!(uint32_t(lhs) & uint32_t(rhs));
    }
    inline adapter_flag operator|(const adapter_flag lhs, const adapter_flag rhs) noexcept
    {
        return adapter_flag(uint32_t(lhs) | uint32_t(rhs));
    }

	class adapter
	{
	public:
		static error_type enumerate(const adapter_flag filter, array<adapter>& array) noexcept;
		adapter() noexcept = default;
        adapter(const adapter& rhs) noexcept;
		ICY_DEFAULT_COPY_ASSIGN(adapter);
		~adapter() noexcept;
	public:
		string_view name() const noexcept;
        explicit operator bool() const noexcept
        {
            return !!data;
        }
        void* handle() const noexcept;
		size_t vid_mem = 0;
		size_t sys_mem = 0;
		size_t shr_mem = 0;
		uint64_t luid = 0;
		adapter_flag flags = adapter_flag::none;
	public:
		class data_type;
		data_type* data = nullptr;
	};
}
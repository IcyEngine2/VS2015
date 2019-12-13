#pragma once

#include <icy_engine/core/icy_core.hpp>
#include <Unknwnbase.h>

#define ICY_COM_ERROR(HR) if (const auto hr_ = (HR)) return make_system_error(uint32_t(hr_))

namespace icy
{
	template<typename T> class com_ptr
	{
		template<typename> friend class com_ptr;
		template<typename U>
		com_ptr(const com_ptr<U>& rhs, std::true_type) noexcept : m_ptr{ rhs.m_ptr }
		{
			if (m_ptr) m_ptr->AddRef();
		}
		template<typename U>
		com_ptr(com_ptr<U>&& rhs, std::true_type) noexcept : m_ptr{ rhs.m_ptr }
		{
			rhs.m_ptr = nullptr;
		}
		template<typename U>
		com_ptr(const com_ptr<U>& rhs, std::false_type) noexcept
		{
			if (rhs)
				rhs->QueryInterface(&m_ptr);
		}
		template<typename U>
		com_ptr(com_ptr<U>&& rhs, std::false_type) noexcept
		{
			if (rhs && SUCCEEDED(rhs->QueryInterface(&m_ptr)))
			{
				rhs->Release();
				rhs.m_ptr = nullptr;
			}
		}
	public:
		com_ptr() noexcept : m_ptr{ nullptr }
		{

		}
		explicit com_ptr(T* const ptr) noexcept : m_ptr{ ptr }
		{
			if (ptr) ptr->AddRef();
		}
		template<typename U>
		com_ptr(const com_ptr<U>& rhs) noexcept :
			com_ptr{ rhs, std::bool_constant<std::is_base_of<T, U>::value>{} }
		{

		}
		template<typename U>
		com_ptr(com_ptr<U>&& rhs) noexcept :
			com_ptr{ std::move(rhs), std::bool_constant<std::is_base_of<T, U>::value>{} }
		{

		}
		com_ptr(std::nullptr_t) : com_ptr{}
		{

		}
		com_ptr(const com_ptr& rhs) noexcept : m_ptr{ rhs.m_ptr }
		{
			if (rhs.m_ptr) m_ptr->AddRef();
		}
		com_ptr(com_ptr&& rhs) noexcept : m_ptr{ rhs.m_ptr }
		{
			rhs.m_ptr = nullptr;
		}
		com_ptr& operator=(nullptr_t) noexcept
		{
			release();
			return *(new (this) com_ptr);
		}
		ICY_DEFAULT_COPY_ASSIGN(com_ptr);
		ICY_DEFAULT_MOVE_ASSIGN(com_ptr);
		~com_ptr() noexcept
		{
			release();
		}
		T* operator->() const noexcept
		{
			return m_ptr;
		}
		T** operator&() noexcept
		{
			return &m_ptr;
		}
		T* const* operator&() const noexcept
		{
			return &m_ptr;
		}
		explicit operator bool() const noexcept
		{
			return !!m_ptr;
		}
		operator T* () const noexcept
		{
			return m_ptr;
		}
	private:
		void release() noexcept
		{
			if (m_ptr)
			{
				m_ptr->Release();
				m_ptr = nullptr;
			}
		}
	private:
		T* m_ptr = nullptr;
	};
}
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
#if CINTERFACE
            if (m_ptr)
                m_ptr->lpVtbl->AddRef(m_ptr);
#else
			if (m_ptr) 
                m_ptr->AddRef();
#endif
		}
		template<typename U>
		com_ptr(com_ptr<U>&& rhs, std::true_type) noexcept : m_ptr{ rhs.m_ptr }
		{
			rhs.m_ptr = nullptr;
		}
		template<typename U>
		com_ptr(const com_ptr<U>& rhs, std::false_type) noexcept
		{
#if CINTERFACE
            if (rhs)
                rhs->lpVtbl->QueryInterface(rhs, &m_ptr);
#else
            if (rhs)
                rhs->QueryInterface(&m_ptr);
#endif
		}
		template<typename U>
		com_ptr(com_ptr<U>&& rhs, std::false_type) noexcept
		{
#if CINTERFACE
            if (rhs && SUCCEEDED(rhs->lpVtbl->QueryInterface(rhs, &m_ptr)))
            {
                rhs->lpVtbl->Release(rhs);
                rhs.m_ptr = nullptr;
            }
#else
			if (rhs && SUCCEEDED(rhs->QueryInterface(&m_ptr)))
			{
				rhs->Release();
				rhs.m_ptr = nullptr;
			}
#endif
		}
	public:
		com_ptr() noexcept : m_ptr{ nullptr }
		{

		}
		explicit com_ptr(T* const ptr) noexcept : m_ptr{ ptr }
		{
#if CINTERFACE
            if (m_ptr)
                m_ptr->lpVtbl->AddRef(m_ptr);
#else
            if (m_ptr)
                m_ptr->AddRef();
#endif
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
#if CINTERFACE
            if (rhs.m_ptr)
                m_ptr->lpVtbl->AddRef(m_ptr);
#else
            if (rhs.m_ptr)
                m_ptr->AddRef();
#endif
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
#if CINTERFACE
                m_ptr->lpVtbl->Release(m_ptr);
#else
				m_ptr->Release();
#endif
				m_ptr = nullptr;
			}
		}
	private:
		T* m_ptr = nullptr;
	};
}
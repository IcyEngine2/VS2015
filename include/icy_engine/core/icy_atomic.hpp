#pragma once

#include "icy_core.hpp"
#include <atomic>

#define ICY_LOCK_GUARD(MUTEX) icy::detail::lock_guard<decltype(MUTEX)> ICY_ANONYMOUS_VARIABLE(ICY_LOCK_GUARD_) { MUTEX } 
#define ICY_LOCK_GUARD_READ(MUTEX) icy::detail::lock_guard_read<decltype(MUTEX)> ICY_ANONYMOUS_VARIABLE(ICY_LOCK_GUARD_READ_) { MUTEX } 
#define ICY_LOCK_GUARD_WRITE(MUTEX) icy::detail::lock_guard_write<decltype(MUTEX)> ICY_ANONYMOUS_VARIABLE(ICY_LOCK_GUARD_WRITE_) { MUTEX } 

#pragma warning(push)
#pragma warning(disable:4296)

extern "C" __declspec(dllimport) int __stdcall SwitchToThread();
extern "C" __declspec(dllimport) unsigned long __stdcall GetCurrentThreadId();

namespace icy
{
    template<typename T> class shared_ptr;
    namespace detail
    {
        template<size_t spin = 0> class spin_lock
        {
        public:
            spin_lock() noexcept : m_flag{ ATOMIC_FLAG_INIT }
            {

            }
            spin_lock(const spin_lock&) noexcept : m_flag{ ATOMIC_FLAG_INIT }
            {

            }
            ICY_DEFAULT_COPY_ASSIGN(spin_lock);
            void lock() noexcept
            {
                lock(std::bool_constant<(spin > 0)>{});
            }
            bool try_lock() noexcept
            {
                return !m_flag.test_and_set(std::memory_order_acquire);
            }
            void unlock() noexcept
            {
                m_flag.clear(std::memory_order_release);
            }
        private:
            void lock(std::true_type) noexcept
            {
                for (;;)
                {
                    for (auto k = 0_z; k < spin; ++k)
                    {
                        if (!m_flag.test_and_set(std::memory_order_acquire))
                            return;
                    }
                    SwitchToThread();
                }
            }
            void lock(std::false_type) noexcept
            {
                while (!m_flag.test_and_set(std::memory_order_acquire))
                {

                }
            }
		private:
            std::atomic_flag m_flag;
        };
        template<size_t spin = 0> class rw_spin_lock
        {
        protected:
            using tag = std::bool_constant<(spin > 0)>;
        public:
            rw_spin_lock() noexcept = default;
            rw_spin_lock(const rw_spin_lock& rhs) noexcept : m_counter{ rhs.m_counter.load() }
            {

            }
            ICY_DEFAULT_COPY_ASSIGN(rw_spin_lock);
            void lock_write() noexcept
            {
                lock_write(tag{});
            }
            void lock_read() noexcept
            {
                lock_read(tag{});
            }
            bool try_lock_read() noexcept
            {
                auto value = m_counter.load(std::memory_order_acquire);
                while (value >= 0)
                {
                    if (m_counter.compare_exchange_weak(value, value + 1, std::memory_order_acq_rel))
                    {
                        return true;
                    }
                }
                return false;
            }
            bool try_lock_write() noexcept
            {
                auto value = m_counter.load(std::memory_order_acquire);
                if (value == 0 && m_counter.compare_exchange_strong(value, -1, std::memory_order_acq_rel))
                {
                    return true;
                }
                return false;
            }
            void unlock_read() noexcept
            {
                m_counter.fetch_sub(1, std::memory_order_release);
            }
            void unlock_write() noexcept
            {
                m_counter.fetch_add(1, std::memory_order_release);
            }
        protected:
            void lock_read(std::true_type) noexcept
            {
                for (;;)
                {
                    auto value = m_counter.load(std::memory_order_acquire);
                    for (auto k = 0_z; k < spin; ++k)
                    {
                        if (value < 0)
                        {
                            // spin
                            value = m_counter.load(std::memory_order_acquire);
                        }
                        else if (m_counter.compare_exchange_weak(value, value + 1, std::memory_order_acq_rel))
                        {
                            ; // read lock acquired
                            return;
                        }
                        // else: value changed
                    }
                    SwitchToThread();
                }
            }
            void lock_read(std::false_type) noexcept
            {
                auto value = m_counter.load(std::memory_order_acquire);
                for (;;)
                {
                    if (value < 0)
                    {
                        // spin
                        value = m_counter.load(std::memory_order_acquire);
                    }
                    else if (m_counter.compare_exchange_weak(value, value + 1, std::memory_order_acq_rel))
                    {
                        ; // read lock acquired
                        return;
                    }
                    // else: value changed
                }
            }
            void lock_write(std::true_type) noexcept
            {
                for (;;)
                {
                    auto value = m_counter.load(std::memory_order_acquire);
                    for (auto k = 0_z; k < spin; ++k)
                    {
                        if (value != 0)
                        {
                            // spin
                            value = m_counter.load(std::memory_order_acquire);
                        }
                        else if (m_counter.compare_exchange_weak(value, -1, std::memory_order_acq_rel))
                        {
                            ; // write lock acquired
                            return;
                        }
                        // else: value changed
                    }
                    SwitchToThread();
                }
            }
            void lock_write(std::false_type) noexcept
            {
                auto value = m_counter.load(std::memory_order_acquire);
                for (;;)
                {
                    if (value != 0)
                    {
                        // spin
                        value = m_counter.load(std::memory_order_acquire);
                    }
                    else if (m_counter.compare_exchange_weak(value, -1, std::memory_order_acq_rel))
                    {
                        ; // write lock acquired
                        return;
                    }
                    // else: value changed
                }
            }
        private:
            std::atomic<int32_t> m_counter = 0;
        };
        
        template<size_t spin = 0> class spin_semaphore
        {
            using tag = std::bool_constant<(spin > 0)>;
        public:
            spin_semaphore() noexcept = default;
            spin_semaphore(const spin_semaphore&) = delete;
            spin_semaphore& operator=(const spin_semaphore&) = delete;
			template<typename clock_type>
            bool sync(const size_t threads, const typename clock_type::duration timeout = std::chrono::seconds{ 1 }) noexcept
            {
                ICY_ASSERT(threads, "INVALID SEMAPHORE THREADS COUNT");
                ICY_ASSERT(timeout.count() >= 0, "INVALID SYNC TIMEOUT");
                const auto index = m_counter.fetch_add(1, std::memory_order_acq_rel);
                const auto now = clock::now();
                sync(index, now, timeout, threads, tag{});
                return index == 0;
            }
        private:
			template<typename clock_type>
            void check_time(const typename clock_type::time_point now, const typename clock_type::duration timeout) noexcept
            {
                ICY_ASSERT(timeout.count() == 0 || clock::now() - now < timeout,
                    "SEMAPHORE SYNC TIMEOUT");
            }
			template<typename clock_type>
            void sync(const size_t index, const typename clock_type::time_point now,
                const typename clock_type::duration timeout, const size_t threads, std::true_type) noexcept
            {
                while (index)
                {
                    for (auto k = 0_z; k < spin; ++k)
                    {
                        if (m_counter.load(std::memory_order_acquire) == 0)
                            return;
                    }
                    check_time(now, timeout);
                    SwitchToThread();
                }
                while (true)
                {
                    for (auto k = 0_z; k < spin; ++k)
                    {
                        if (m_counter.load(std::memory_order_acquire) == threads)
                        {
                            m_counter.store(0, std::memory_order_release);
                            return;
                        }
                    }
                    check_time(now, timeout);
                    SwitchToThread();
                }
            }
			template<typename clock_type>
            void sync(const size_t index, const typename clock_type::time_point now,
                const typename clock_type::duration timeout, const size_t threads, std::false_type) noexcept
            {
                while (index)
                {
                    if (m_counter.load(std::memory_order_acquire) == 0)
                        return;
                    check_time(now, timeout);
                }
                while (true)
                {
                    if (m_counter.load(std::memory_order_acquire) == threads)
                    {
                        m_counter.store(0, std::memory_order_release);
                        return;
                    }
                    check_time(now, timeout);
                }
            }
        private:
            std::atomic<size_t> m_counter = 0;
        };
        template<typename mutex_type> class lock_guard
        {
        public:
            lock_guard(mutex_type& mutex) noexcept : m_mutex{ mutex }
            {
                m_mutex.lock();
            }
            ~lock_guard() noexcept
            {
                m_mutex.unlock();
            }
        private:
            mutex_type& m_mutex;
        };
        template<typename mutex_type> class lock_guard_read
        {
        public:
            lock_guard_read(mutex_type& mutex) noexcept : m_mutex{ mutex }
            {
                m_mutex.lock_read();
            }
			lock_guard_read& operator=(const lock_guard_read&) = delete;
            ~lock_guard_read() noexcept
            {
                m_mutex.unlock_read();
            }
        private:
            mutex_type& m_mutex;
        };
        template<typename mutex_type> class lock_guard_write
        {
        public:
            lock_guard_write(mutex_type& mutex) noexcept : m_mutex{ mutex }
            {
                m_mutex.lock_write();
            }
			lock_guard_write& operator=(const lock_guard_write&) = delete;
            ~lock_guard_write() noexcept
            {
                m_mutex.unlock_write();
            }
        private:
            mutex_type& m_mutex;
        };
        class intrusive_mpsc_queue
        {
        public:
			intrusive_mpsc_queue() noexcept = default;
			intrusive_mpsc_queue(const intrusive_mpsc_queue&) = delete;
			intrusive_mpsc_queue& operator=(const intrusive_mpsc_queue&) = delete;
            intrusive_mpsc_queue(intrusive_mpsc_queue&& rhs) noexcept : m_head(rhs.m_head.load()), m_tail(rhs.m_tail)
            {
                rhs.m_head.store(0);
                rhs.m_tail = nullptr;
            }
            ICY_DEFAULT_MOVE_ASSIGN(intrusive_mpsc_queue);
            void push(void* ptr) noexcept
            {
                auto head = m_head.load(std::memory_order_acquire);
                while (!m_head.compare_exchange_weak(head, new (ptr) node{ head },
                    std::memory_order_acq_rel))
                {
                    // loop until cas
                }
                // now m_head looks at new node (and that new node stores previous head)
            }
            void* pop() noexcept
            {
                if (m_tail)
                {
                    const auto result = static_cast<void*>(m_tail);
                    m_tail = m_tail->next;
                    return result;
                }
                else
                {
                    auto head = m_head.load(std::memory_order_acquire);
                    if (head)
                    {
                        while (!m_head.compare_exchange_weak(head, nullptr,
                            std::memory_order_acq_rel))
                        {
                            // loop until cas
                        }
                        // m_head is null, we can reverse the pointers in nodes (prev <-> next)

                        auto prev = head->prev;
                        head->next = nullptr;
                        while (prev)
                        {
                            auto const tmp = prev->prev;
                            prev->next = head;
                            head = prev;
                            prev = tmp;
                        }
                        // now head->prev == nullptr
                        m_tail = head->next;
                        return head;
                    }
                    else
                    {
                        return nullptr;
                    }
                }
            }
        private:
            union node
            {
                node* next;
                node* prev;
            };
        private:
            std::atomic<node*> m_head = nullptr;
            node* m_tail = nullptr;
        };
    }
}

#pragma warning(pop)
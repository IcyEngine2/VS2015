#pragma once

#include "icy_core.hpp"

namespace icy
{
    class thread_system
    {
    public:
        static void notify(const unsigned index, bool attach) noexcept;
        thread_system() noexcept = default;
        thread_system(const thread_system& rhs) noexcept = delete;
        ~thread_system() noexcept;
    protected:
        virtual void operator()(const unsigned index, const bool attach) noexcept = 0;
        error_type initialize() noexcept;
    private:
        thread_system* m_prev = nullptr;
    };
    enum class thread_state : uint32_t
    {
        none,
        run,
        done,
    };
    class thread
    {
    public:
        thread() noexcept = default;
        thread(thread&& rhs) noexcept : m_data(rhs.m_data)
        {
            rhs.m_data = nullptr;
        }
        virtual ~thread() noexcept;
        static uint32_t this_index() noexcept;
        static size_t cores() noexcept;
    public:
        unsigned index() const noexcept;
        thread_state state() const noexcept;
        error_type error() const noexcept;
        void* handle() const noexcept;
        void exit(const error_type error) noexcept;
        void quit() noexcept
        {
            exit({});
        }
        error_type launch() noexcept;
        error_type wait() noexcept;
        error_type rename(const string_view name) noexcept;
        virtual void cancel() noexcept;
    protected:
        virtual error_type run() = 0;
    private:
        struct data_type;
        data_type* m_data = nullptr;
    };
    const thread* this_thread() noexcept;
    void sleep(const clock_type::duration duration) noexcept;

    class thread_local_data
    {
    public:
        thread_local_data() noexcept = default;
        thread_local_data(const thread_local_data&) = delete;
        ~thread_local_data() noexcept
        {
            shutdown();
        }
        void shutdown() noexcept;
        error_type initialize() noexcept;
        error_type assign(void* ptr) noexcept;
        void* query() const noexcept;
        explicit operator bool() const noexcept
        {
            return m_index != 0;
        }
    private:
        uint32_t m_index = 0;
    };
}
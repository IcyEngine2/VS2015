#pragma once

#include "icy_smart_pointer.hpp"

#if _DEBUG
#define ICY_EVENT_CHECK_TYPE 1
#endif

namespace icy
{
    namespace event_type_enum
    {
        enum : uint64_t
        {            
            bitcnt_global   =   0x04,
            bitcnt_fileio   =   0x02,
            bitcnt_network  =   0x05,
            bitcnt_console  =   0x03,
            bitcnt_window   =   0x07,
            bitcnt_gui      =   0x04,
            bitcnt_render   =   0x02,

            bitcnt_user     =   0x20,
        };
        enum : uint64_t
        {
            offset_global   =   0x00,
            offset_fileio   =   offset_global + bitcnt_global,
            offset_network  =   offset_fileio + bitcnt_fileio,
            offset_console  =   offset_network + bitcnt_network,
            offset_window   =   offset_console + bitcnt_console,
            offset_gui      =   offset_window + bitcnt_window,
            offset_render   =   offset_gui + bitcnt_gui,

            offset_user     =   0x20,
        };
        enum : uint64_t
        {
            mask_global     =   ((1ui64 << bitcnt_global)   - 1)    <<  offset_global,
            mask_fileio     =   ((1ui64 << bitcnt_fileio)   - 1)    <<  offset_fileio,
            mask_network    =   ((1ui64 << bitcnt_network)  - 1)    <<  offset_network,
            mask_console    =   ((1ui64 << bitcnt_console)  - 1)    <<  offset_console,
            mask_window     =   ((1ui64 << bitcnt_window)   - 1)    <<  offset_window,
            mask_gui        =   ((1ui64 << bitcnt_gui)      - 1)    <<  offset_gui,
            mask_render     =   ((1ui64 << bitcnt_render)   - 1)    <<  offset_render,
            mask_user       =   ((1ui64 << bitcnt_user)     - 1)    <<  offset_user,
        };
        enum : uint64_t
        {
            none                    =   0,

            global_quit             =   1ui64   <<  (offset_global + 0x00),
            global_timer            =   1ui64   <<  (offset_global + 0x01),
            global_task             =   1ui64   <<  (offset_global + 0x02),
            global_timeout          =   1ui64   <<  (offset_global + 0x03),
            global_any              =   mask_global,

            file_read               =   1ui64   <<  (offset_fileio + 0x00),
            file_write              =   1ui64   <<  (offset_fileio + 0x01),
            file_any                =   mask_fileio,

            network_connect         =   1ui64   <<  (offset_network + 0x00),
            network_disconnect      =   1ui64   <<  (offset_network + 0x01),
            //network_shutdown        =   1ui64   <<  (offset_network + 0x02),
            network_send            =   1ui64   <<  (offset_network + 0x03),
            network_recv            =   1ui64   <<  (offset_network + 0x04),
            //network_timeout         =   1ui64   <<  (offset_network + 0x04),
            network_any             =   mask_network,
            
            console_write           =   1ui64   <<  (offset_console + 0x00),
            console_read_key        =   1ui64   <<  (offset_console + 0x01),
            console_read_line       =   1ui64   <<  (offset_console + 0x02),
            console_any             =   mask_console,
            
            window_close            =   1ui64   <<  (offset_window + 0x00),
            window_resize           =   1ui64   <<  (offset_window + 0x02),
            window_input            =   1ui64   <<  (offset_window + 0x03),
            window_internal         =   1ui64   <<  (offset_window + 0x04),
            window_active           =   1ui64   <<  (offset_window + 0x05),
            window_minimized        =   1ui64   <<  (offset_window + 0x06),
            window_repaint          =   1ui64   <<  (offset_window + 0x07),
            window_any              =   mask_window,

            gui_action              =   1ui64   <<  (offset_gui + 0x00),    //  action index
            gui_update              =   1ui64   <<  (offset_gui + 0x01),    //  widget index + variant
            gui_context             =   1ui64   <<  (offset_gui + 0x02),    //  widget(view) + node
            gui_select              =   1ui64   <<  (offset_gui + 0x03),    //  widget(view) + node
            gui_any                 =   mask_gui,

            render_update           =   1ui64   <<  (offset_render + 0x00),
            render_frame            =   1ui64   <<  (offset_render + 0x01),
            render_any              =   mask_render,

            user                    =   1ui64   <<  (offset_user + 0x00),
            user_any                =   mask_user,
        };
        static_assert(offset_user >= offset_render + bitcnt_render, "INVALID USER MESSAGE OFFSET");
    }
    
    using event_type = decltype(event_type_enum::none);
    
    class event;
    class event_data;

    class event_system
    {
        friend event_system;
        friend event_data;
        struct event_ptr
        {
            void* _unused;
            event_data* value;
        };
    public:
        virtual ~event_system() noexcept = 0
        {

        }
        virtual error_type exec() noexcept = 0;
        error_type post(event_system* const source, const event_type type) noexcept;
        template<typename T> error_type post(event_system* const source, const event_type type, T&& arg) noexcept
        {
            event_data* new_event = nullptr;
            ICY_ERROR(event_data::create(type, source, sizeof(T), new_event));
            event_data::initialize(*new_event, std::is_trivially_destructible<T> {}, std::move(arg));
            const auto error = post(*new_event);
            new_event->release();
            return error;
        }
    protected:
        void filter(const uint64_t mask) noexcept;
        event pop() noexcept;        
    private:
        virtual error_type signal(const event_data& event) noexcept = 0;
        error_type post(event_data& new_event) noexcept;
    private:
        detail::intrusive_mpsc_queue m_queue;
        event_system* m_prev = nullptr;
        uint64_t m_mask = 0;
        static detail::rw_spin_lock g_lock;
        static event_system* g_list;
    };

    class event_queue final : public event_system
    {
        struct tag {};
    public:
        friend error_type create_event_system(shared_ptr<event_queue>& queue, const uint64_t mask) noexcept;
    public:
        event_queue(tag) noexcept { }
        ~event_queue() noexcept
        {
            filter(0);
        }
        error_type pop(event& event, const duration_type timeout = max_timeout) noexcept;
        error_type exec() noexcept override
        {
            return make_stdlib_error(std::errc::function_not_supported);
        }
        error_type signal(const event_data& event) noexcept override
        {
            m_cvar.wake();
            return error_type();
        }
    private:
        mutex m_mutex;
        cvar m_cvar;
    };
    //error_type create_event_system(shared_ptr<event_queue>& queue, const uint64_t mask) noexcept;

    class event_data
    {      
        friend event;
        friend event_system;
        using destructor_type = void(*)(void*);
    public:
        template<typename T> const T& data() const noexcept
        {
#if ICY_EVENT_CHECK_TYPE
            ICY_ASSERT(m_type == get_type<T>(), "INVALID EVENT TYPE");
#endif
            return *reinterpret_cast<const T*>(reinterpret_cast<const char*>(this) + sizeof(*this));
        }
        template<typename T> T& data() noexcept
        {
#if ICY_EVENT_CHECK_TYPE
            ICY_ASSERT(m_type == get_type<T>(), "INVALID EVENT TYPE");
#endif
            return *reinterpret_cast<T*>(reinterpret_cast<char*>(this) + sizeof(*this));
        }
    public:
        const clock_type::time_point time;
        const event_type type;
        const weak_ptr<event_system> source;
    private:
#if ICY_EVENT_CHECK_TYPE
        static uint32_t next_type() noexcept
        {
            static uint32_t counter = 0;
            return ++counter;
        }
        template<typename T>
        static uint32_t get_type() noexcept
        {
            static auto index = next_type();
            return index;
        }
#endif
        event_data(const event_type type, weak_ptr<event_system>&& source) noexcept : time(clock_type::now()), type(type), source(std::move(source))
        {

        }
        static error_type post(event_data& new_event) noexcept;
        static error_type create(const event_type type, event_system* const source, const size_t type_size, event_data*& new_event) noexcept;
        template<typename T> static void initialize(event_data& new_event, std::false_type, T&& arg) noexcept
        {
            static_assert(std::is_rvalue_reference<decltype(arg)>::value, "ONLY RVALUE REFERENCE AS ARG");
            union
            {
                event_data* as_event;
                char* as_ptr;
                T* as_arg;
            };
            as_event = &new_event;
            as_ptr += sizeof(event_data);
            new_event.m_destructor = [](void* ptr) { static_cast<T*>(ptr)->~T(); };
#if ICY_EVENT_CHECK_TYPE
            new_event.m_type = get_type<T>();
#endif
            new (as_arg) T{ std::move(arg) };
        }
        template<typename T> static void initialize(event_data& new_event, std::true_type, T&& arg) noexcept
        {
            union
            {
                event_data* as_event;
                char* as_ptr;
                T* as_arg;
            };
            as_event = &new_event;
            as_ptr += sizeof(event_data);
#if ICY_EVENT_CHECK_TYPE
            new_event.m_type = get_type<T>();
#endif
            new (as_arg) T{ std::move(arg) };
        }
        void release() noexcept;
    private:
        std::atomic<uint32_t> m_ref = 1;
        destructor_type m_destructor = nullptr;
#if ICY_EVENT_CHECK_TYPE
        uint32_t m_type = 0;
#endif
    };
    class event
    {
        friend event_system;
    public:
        event() noexcept = default;
        event(const event& rhs) noexcept : m_ptr(rhs.m_ptr)
        {
            if (m_ptr)
                m_ptr->m_ref.fetch_add(1, std::memory_order_release);
        }
        event(event&& rhs) noexcept : m_ptr(rhs.m_ptr)
        {
            rhs.m_ptr = nullptr;
        }
        ICY_DEFAULT_COPY_ASSIGN(event);
        ICY_DEFAULT_MOVE_ASSIGN(event);
        ~event() noexcept
        {
            if (m_ptr)
                m_ptr->release();
        }
        event_data* operator->() const noexcept
        {
            return m_ptr;
        }
        explicit operator bool() const noexcept
        {
            return !!m_ptr;
        }
        static error_type post(event_system* const source, const event_type type) noexcept;
        template<typename T> static error_type post(event_system* const source, const event_type type, T&& arg) noexcept
        {
            event_data* new_event = nullptr;
            ICY_ERROR(event_data::create(type, source, sizeof(T), new_event));
            event_data::initialize(*new_event, std::is_trivially_destructible<T>{}, std::move(arg));
            const auto error = event_data::post(*new_event);
            new_event->release();
            return error;
        }
    private:
        event_data* m_ptr = nullptr;
    };

    class string_view;
    string_view to_string(const event_type type) noexcept;

    class timer
    {
    public:
        struct pair { timer* timer; size_t count; };
        timer() noexcept = default;
        timer(const timer&) = delete;
        ~timer() noexcept
        {
            cancel();
        }
        void cancel() noexcept;
        error_type initialize(const size_t count, const duration_type timeout) noexcept;        
    private:
        void* m_data = nullptr;
        size_t m_count = 0;
        duration_type m_timeout = {};
    };

    inline constexpr event_type event_user(const uint64_t index) noexcept 
    {
        return event_type(uint64_t(event_type::user) << index); 
    }
    event_type next_event_user() noexcept;
}
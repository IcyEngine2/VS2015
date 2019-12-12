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
            bitcnt_glo  =   0x04,
            bitcnt_fio  =   0x02,
            bitcnt_net  =   0x00,// 0x07
            bitcnt_con  =   0x03,
            bitcnt_win  =   0x06,
            bitcnt_dsp  =   0x02,
            bitcnt_gui  =   0x02,

            bitcnt_usr  =   0x20,
        };
        enum : uint64_t
        {
            offset_glo  =   0x00,
            offset_fio  =   offset_glo + bitcnt_glo,
            offset_net  =   offset_fio + bitcnt_fio,
            offset_con  =   offset_net + bitcnt_net,
            offset_win  =   offset_con + bitcnt_con,
            offset_dsp  =   offset_win + bitcnt_win,
            offset_gui  =   offset_dsp + bitcnt_dsp,

            offset_usr  =   0x20,
        };
        enum : uint64_t
        {
            mask_glo    =   ((1ui64 << bitcnt_glo) - 1) <<  offset_glo,
            mask_fio    =   ((1ui64 << bitcnt_fio) - 1) <<  offset_fio,
            mask_net    =   ((1ui64 << bitcnt_net) - 1) <<  offset_net,
            mask_con    =   ((1ui64 << bitcnt_con) - 1) <<  offset_con,
            mask_win    =   ((1ui64 << bitcnt_win) - 1) <<  offset_win,
            mask_dsp    =   ((1ui64 << bitcnt_dsp) - 1) <<  offset_dsp,
            mask_gui    =   ((1ui64 << bitcnt_gui) - 1) <<  offset_gui,
            mask_usr    =   ((1ui64 << bitcnt_usr) - 1) <<  offset_usr,
        };
        enum : uint64_t
        {
            none                    =   0,

            global_quit             =   1ui64   <<  (offset_glo + 0x00),
            global_timer            =   1ui64   <<  (offset_glo + 0x01),
            global_task             =   1ui64   <<  (offset_glo + 0x02),
            global_timeout          =   1ui64   <<  (offset_glo + 0x03),
            global_any              =   mask_glo,

            file_read               =   1ui64   <<  (offset_fio + 0x00),
            file_write              =   1ui64   <<  (offset_fio + 0x01),
            file_any                =   mask_fio,

            /*network_connect         =   1ui64   <<  (offset_net + 0x00),
            network_disconnect      =   1ui64   <<  (offset_net + 0x01),
            network_shutdown        =   1ui64   <<  (offset_net + 0x02),
            network_accept          =   1ui64   <<  (offset_net + 0x03),
            network_send            =   1ui64   <<  (offset_net + 0x04),
            network_recv            =   1ui64   <<  (offset_net + 0x05),
            network_timeout         =   1ui64   <<  (offset_net + 0x06),
            network_any             =   mask_net,*/
            
            console_write           =   1ui64   <<  (offset_con + 0x00),
            console_read_key        =   1ui64   <<  (offset_con + 0x01),
            console_read_line       =   1ui64   <<  (offset_con + 0x02),
            console_any             =   mask_con,
            
            window_close            =   1ui64   <<  (offset_win + 0x00),
            window_resize           =   1ui64   <<  (offset_win + 0x02),
            window_input            =   1ui64   <<  (offset_win + 0x03),
#if ICY_WINDOW_INTERNAL
            window_rename           =   1ui64   <<  (offset_win + 0x04),
#endif
            window_active           =   1ui64   <<  (offset_win + 0x05),
            window_minimized        =   1ui64   <<  (offset_win + 0x06),
            window_any              =   mask_win,

            display_refresh         =   1ui64   <<  (offset_dsp + 0x00),
            display_any             =   mask_dsp,

            gui_action              =   1ui64   <<  (offset_gui + 0x00),    //  action index
            gui_update              =   1ui64   <<  (offset_gui + 0x01),    //  widget index + variant
            gui_any                 =   mask_gui,

            user                    =   1ui64   <<  (offset_usr + 0x00),
            user_any                =   mask_usr,
        };
        static_assert(offset_usr >= offset_gui + bitcnt_gui, "INVALID USER MESSAGE OFFSET");
    }
    using event_type = decltype(event_type_enum::none);
    class event;
    class event_data;

    class event_queue
    {
        friend event_data;
        struct event_ptr
        {
            void* _unused;
            event_data* value;
        };
    public:
        virtual ~event_queue() noexcept
        {
            filter(0);
        }
    protected:
        void filter(const uint64_t mask) noexcept;
        event pop() noexcept;
        error_type post(event_queue* const source, const event_type type) noexcept;
        template<typename T> error_type post(event_queue* const source, const event_type type, T&& arg) noexcept
        {
            event_data* new_event = nullptr;
            ICY_ERROR(event_data::create(type, source, sizeof(T), new_event));
            event_data::initialize(*new_event, std::is_trivially_destructible<T> {}, std::move(arg));
            const auto error = post(*new_event);
            new_event->release();
            return error;
        }
    private:
        error_type post(event_data& new_event) noexcept;
        virtual error_type signal(const event_data& event) noexcept = 0;
    private:
        detail::intrusive_mpsc_queue m_queue;
        event_queue* m_prev = nullptr;
        uint64_t m_mask = 0;
        static detail::rw_spin_lock<> g_lock;
        static event_queue* g_list;
    };
    class event_loop : public event_queue
    {
    public:
        static error_type create(shared_ptr<event_loop>& loop, const uint64_t mask) noexcept;
        virtual error_type loop(event& event, const duration_type timeout = max_timeout) noexcept = 0;
    };
    class event_data
    {      
        friend event;
        friend event_queue;
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
        const weak_ptr<event_queue> source;
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
        event_data(const event_type type, weak_ptr<event_queue>&& source) noexcept : time(clock_type::now()), type(type), source(std::move(source))
        {

        }
        static error_type post(event_data& new_event) noexcept;
        static error_type create(const event_type type, event_queue* const source, const size_t type_size, event_data*& new_event) noexcept;
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
        friend event_queue;
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
        static error_type post(event_queue* const source, const event_type type) noexcept;
        template<typename T> static error_type post(event_queue* const source, const event_type type, T&& arg) noexcept
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
}
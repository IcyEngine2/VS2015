#include <icy_engine/core/icy_memory.hpp>
#include <icy_engine/core/icy_thread.hpp>
#include <Windows.h>
#include <DbgHelp.h>

#pragma warning(disable:4324) // conditional expression is constant

using namespace icy;

static size_t   align_size(const size_t size, size_t& size_class) noexcept;
static void     memset64(void* dest, const uint64_t value, const uintptr_t size) noexcept;
static void*    system_realloc(const void* const ptr, const size_t capacity, const uint32_t flags) noexcept;
static size_t   system_memsize(const void* const ptr) noexcept;

static_assert(ATOMIC_LLONG_LOCK_FREE, "INVALID PLATFORM");

ICY_STATIC_NAMESPACE_BEG
static constexpr size_t size_classes[79] =
{
    16, 24, 32, 48, 64, 96, 128, 192,
    256, 384, 512, 768, 1_kb, 1536, 2_kb, 3_kb,
    4_kb, 6_kb, 8_kb, 12_kb, 16_kb, 24_kb, 32_kb, 48_kb,
    64_kb,  96_kb, 128_kb, 192_kb, 256_kb, 384_kb, 512_kb, 768_kb,
    1_mb, 1536_kb, 2_mb, 3_mb, 4_mb, 6_mb, 8_mb, 12_mb,
    16_mb,  24_mb, 32_mb, 48_mb, 64_mb, 96_mb, 128_mb, 192_mb,
    256_mb, 384_mb, 512_mb, 768_mb, 1_gb, 1536_mb, 2_gb, 3_gb,
    4_gb, 6_gb, 8_gb, 12_gb, 16_gb, 24_gb, 32_gb, 48_gb,
    64_gb, 96_gb, 128_gb, 192_gb, 256_gb, 384_gb, 512_gb, 768_gb,
    1_tb, 1536_gb, 2_tb, 3_tb, 4_tb, 6_tb, 8_tb // user space ends
};
constexpr auto cache_line = SYSTEM_CACHE_ALIGNMENT_SIZE;
constexpr auto page_size = cache_line * 64;
constexpr auto min_allocation = 16;
constexpr auto max_allocation = 48_kb;
constexpr auto size_of_bits = 8;
constexpr auto node_data_size_0 = (min_allocation * 24) << (4 * 0);
constexpr auto node_data_size_1 = (min_allocation * 24) << (4 * 1);
constexpr auto node_data_size_2 = (min_allocation * 24) << (4 * 2);
constexpr auto unit_data_size = page_size / size_of_bits * node_data_size_0;
constexpr auto node_count_0 = unit_data_size / node_data_size_0;
constexpr auto node_count_1 = unit_data_size / node_data_size_1;
constexpr auto node_count_2 = unit_data_size / node_data_size_2;
constexpr auto num_size_types = 3;
constexpr auto num_size_classes = num_size_types * 8;
constexpr auto bit_free = 0xFEEEFEEEFEEEFEEEui64;
constexpr auto bit_alloc = 0xBAADF00DBAADF00Dui64;
constexpr auto system_flag_commit = MEM_COMMIT;
constexpr auto system_flag_reserve = MEM_RESERVE;
auto static_buffer_size = 0_z;
char static_buffer[0x100];
struct heap_node_list;
struct heap_node_unit;
struct heap_node_tloc;
struct debug_trace_unit
{
    debug_trace_unit* prev = nullptr;
    uint32_t hash = 0;
    uint32_t size = 0;
    void* user_data = nullptr;
    void* func_data[1];
};
struct __declspec(align(SYSTEM_CACHE_ALIGNMENT_SIZE)) heap_init_data : public heap_init
{
    heap_init_data(const heap_init init) : heap_init(init)
    {

    }
    const uint32_t index = GetCurrentThreadId();
    uint32_t tls = TLS_OUT_OF_INDEXES;
    uint8_t* bytes = nullptr;
    uint8_t* sizes = nullptr;
    heap_node_unit* units = nullptr;
    heap_node_tloc* tlocs = nullptr;
};
struct __declspec(align(SYSTEM_CACHE_ALIGNMENT_SIZE)) heap_unit_data
{
    std::atomic<size_t> count = 0;
    size_t capacity = 0;
};
struct __declspec(align(SYSTEM_CACHE_ALIGNMENT_SIZE)) heap_tloc_data
{
    detail::rw_spin_lock lock;
    size_t count = 0;
    size_t capacity = 0;
    detail::intrusive_mpsc_queue queue;
};
struct __declspec(align(SYSTEM_CACHE_ALIGNMENT_SIZE)) heap_stat_data
{
    std::atomic<uint64_t> bytes = 0;
};
struct __declspec(align(SYSTEM_CACHE_ALIGNMENT_SIZE)) heap_trac_data 
{
    icy::detail::rw_spin_lock lock;
    debug_trace_unit* units = nullptr;
};
class heap_base
{
    friend heap_node_list;
    friend heap_node_unit;
    friend heap_node_tloc;
public:
    heap_base(const heap_init init) noexcept : m_init(init)
    {

    }
    ~heap_base() noexcept;
    error_type initialize(void* const base) noexcept;
    void* realloc(const void* const ptr, const size_t size) noexcept;
    size_t memsize(const void* const ptr) const noexcept;
private:
    void* raw_alloc(const size_t size) noexcept;
    void* alloc(const size_t size) noexcept;
    void raw_free(const void* const ptr) noexcept;
    void free(const void* const ptr) noexcept;
protected:
    heap_init_data m_init;
    heap_unit_data m_units;
    heap_tloc_data m_tlocs;
    heap_stat_data m_stats;
    heap_trac_data m_trace;
};
class heap_multi : public heap_base, public thread_system
{
public:
    using heap_base::heap_base;
    void operator()(const uint32_t, const bool attach) noexcept override
    {
        if (!attach)
        {
            if (const auto value = TlsGetValue(m_init.tls))
                m_tlocs.queue.push(value);
        }
    }
};
struct heap_capacity
{
    heap_capacity(const heap_init init) noexcept;
    operator size_t() const noexcept
    {
        return base + data + unit + size + tloc;
    }
    size_t base = 0;    //  sizeof(class)
    size_t data = 0;    //  aligned capacity
    size_t unit = 0;    //  meta data
    size_t size = 0;    //  size classes
    size_t tloc = 0;    //  thread local
};
struct heap_node_list
{
    friend heap_node_unit;
    enum : uint32_t
    {
        invalid_value = INT32_MAX,
    };
public:
    heap_node_unit* pop(heap_base& heap) noexcept;
private:
    std::atomic<uint32_t> m_head = invalid_value;
    uint32_t m_tail = invalid_value;
};
struct heap_node_unit
{
    friend heap_node_list;
    enum : uint64_t
    {
        bits_mask = 0x0000'0000'FFFF'FFFF,
        flag_mask = 0x8000'0000'0000'0000,
        info_mask = 0x7FFF'FFFF'0000'0000,
    };
public:
    size_t allocate(const uint64_t thread_idx, const size_t size_class) noexcept;
    void free(heap_base& heap, const size_t cell_idx, const size_t size_class) noexcept;
    uint32_t get_info() const noexcept
    {
        return (m_bits.load(std::memory_order_acquire) & info_mask) >> 32;
    }
    void set_info(const uint32_t value) noexcept
    {
        m_bits.fetch_and(~info_mask, std::memory_order_release);
        m_bits.fetch_or(uint64_t(value) << 32, std::memory_order_release);
    }
private:
    std::atomic<uint64_t> m_bits = 0;
};
struct heap_node_tloc
{
    heap_node_tloc() : units{}, sizes{}
    {

    }
    void* _unused;
    struct
    {
        heap_node_unit* ptr;
        size_t size;
    } units[num_size_types];
    struct
    {
        heap_node_unit* current;
        heap_node_list list;
    } sizes[num_size_classes];
};
ICY_STATIC_NAMESPACE_END

void heap::shutdown() noexcept
{
    if (m_ptr)
    {
        if (m_init.multithread)
        {
            static_cast<heap_multi*>(m_ptr)->~heap_multi();
        }
        else
        {
            static_cast<heap_base*>(m_ptr)->~heap_base();
        }
        system_realloc(m_ptr, 0, 0);
        m_ptr = nullptr;
    }
}
error_type heap::initialize(const heap_init init) noexcept
{
    shutdown();

    if (init.global_heap && (!init.multithread || detail::global_heap.user))
        return make_stdlib_error(std::errc::invalid_argument);

    const heap_capacity capacity(init);
    auto mem = system_realloc(nullptr, capacity, system_flag_reserve);
    if (!mem)
        return last_system_error();
    ICY_SCOPE_EXIT{ if (mem) system_realloc(mem, 0, 0); };
    
    if (!system_realloc(mem, capacity.base, system_flag_commit))
        return last_system_error();

    if (init.multithread)
    {
        const auto heap = new (mem) heap_multi(init);
        if (const auto error = heap->initialize(mem))
        {
            heap->~heap_multi();
            return error;
        }
    }
    else
    {
        const auto heap = new (mem) heap_base(init);
        if (const auto error = heap->initialize(mem))
        {
            heap->~heap_base();
            return error;
        }
    }
    std::swap(mem, m_ptr);
    m_init = init;
    return {};
}
void* heap::realloc(const void* const old_ptr, const size_t new_size) noexcept
{
    if (!m_ptr)
        return nullptr;
    if (m_init.multithread)
        return static_cast<heap_multi*>(m_ptr)->realloc(old_ptr, new_size);
    else
        return static_cast<heap_base*>(m_ptr)->realloc(old_ptr, new_size);
}
size_t heap::memsize(const void* const ptr) noexcept
{
    if (!m_ptr)
        return 0;
    if (m_init.multithread)
        return static_cast<heap_multi*>(m_ptr)->memsize(ptr);
    else
        return static_cast<heap_base*>(m_ptr)->memsize(ptr);
}

static size_t align_size(const size_t size, size_t& size_class) noexcept
{
    if (size > min_allocation)
    {
        // 4 == log2(min_allocation)
        enum { min_size_class = detail::constexpr_log2(min_allocation) };

        const auto m = detail::log2(size - 1);
        const auto lower = 1_z << m;
        const auto upper = lower * 2;
        const bool pow32 = 2 * size > 3 * lower;

        size_class = 2 * (m - min_size_class) + 1 + pow32;
        const auto result = size_classes[size_class];
        return result;
    }
    else
    {
        size_class = 0;
        return min_allocation;
    }
}
static void memset64(void* dest, const uint64_t value, const uintptr_t size) noexcept
{
    uintptr_t i = 0;
    for (; i < (size & (~7)); i += 8)
    {
        memcpy(((char*)dest) + i, &value, 8);
    }
    for (; i < size; i++)
    {
        ((char*)dest)[i] = ((char*)&value)[i & 7];
    }
}
static void* system_realloc(const void* const ptr, const size_t capacity, const uint32_t flags) noexcept
{
    if (capacity == 0)
    {
        VirtualFree(const_cast<void*>(ptr), 0, MEM_RELEASE);
        return nullptr;
    }
    return VirtualAlloc(const_cast<void*>(ptr), capacity, flags, PAGE_READWRITE);
}
static size_t system_memsize(const void* const ptr) noexcept
{
    MEMORY_BASIC_INFORMATION basic = {};
    VirtualQuery(ptr, &basic, sizeof(basic));
    return basic.RegionSize;
}
static void* static_realloc(const void* const ptr, const size_t capacity, void*) noexcept
{
    if (capacity)
    {
        if (static_buffer_size + capacity > sizeof(static_buffer))
        {
            ICY_ASSERT(false, "NOT ENOUGH STATIC MEMORY");
            ::exit(int(std::errc::not_enough_memory));
        }
        else
        {
            const auto new_ptr = static_buffer + static_buffer_size;
            static_buffer_size += capacity;
            return new_ptr;
        }
    }
    return nullptr;
}
static size_t static_memsize(const void* const ptr, void* = nullptr) noexcept
{
    return 0;
}

heap_base::~heap_base() noexcept
{
    if (m_init.global_heap && detail::global_heap.user == this)
        detail::global_heap = {};

    if (m_init.tls != TLS_OUT_OF_INDEXES)
        TlsFree(m_init.tls);
}
error_type heap_base::initialize(void* base) noexcept
{
    const auto capacity = heap_capacity(m_init);
    if (m_init.multithread)
    {
        m_init.tls = TlsAlloc();
        if (m_init.tls == TLS_OUT_OF_INDEXES)
            return last_system_error();
    }
    union
    {
        uint8_t* ptr;
        heap_node_tloc* tlocs;
        heap_node_unit* units;
    };
    ptr = static_cast<uint8_t*>(base);
    ptr += capacity.base;

    m_init.tlocs = tlocs;
    ptr += capacity.tloc;
    
    m_init.units = units;
    ptr += capacity.unit;
    
    m_init.bytes = ptr;
    ptr += capacity.data;

    m_init.sizes = ptr;
    ptr += capacity.size;

    m_tlocs.capacity = capacity.tloc / sizeof(heap_tloc_data);
    m_units.capacity = capacity.unit / page_size;
    
    if (!system_realloc(m_init.sizes, capacity.size, system_flag_commit))
        return last_system_error();
    
    if (m_init.global_heap && m_init.multithread)
    {
        detail::global_heap.user = this;
        detail::global_heap.memsize =
            [](const void* const ptr, void* const heap) { return static_cast<heap_base*>(heap)->memsize(ptr); };
        detail::global_heap.realloc =
            [](const void* const ptr, const size_t size, void* const heap) { return static_cast<heap_base*>(heap)->realloc(ptr, size); };
    }
    return {};
}
size_t heap_base::memsize(const void* const ptr) const noexcept
{
    if (!ptr || !m_units.capacity)
        return 0;

    if (ptr >= m_init.bytes && ptr < m_init.bytes + m_units.capacity * unit_data_size)
    {
        const auto offset = detail::distance(m_init.bytes, ptr);
        const auto size_class = size_t(m_init.sizes[offset / node_data_size_0]);
        return size_classes[size_class];
    }
    else
    {
        return system_memsize(ptr);
    }
}
void* heap_base::realloc(const void* const old_ptr, size_t new_size) noexcept
{
    if (!new_size)
    {
        free(old_ptr);
        return nullptr;
    }

    const auto new_ptr = alloc(new_size);
    if (!new_ptr)
        return nullptr;

    memcpy(new_ptr, old_ptr, memsize(old_ptr));
    free(old_ptr);
    return new_ptr;  
}
void* heap_base::raw_alloc(size_t size) noexcept
{
    if (!size)
        return nullptr;

    //if (!m_threads.ptr)
    //    return static_heap::allocate(size);

    if (size <= 48_kb)
    {
        auto size_class = 0_z;
        size = align_size(size, size_class);

        const auto size_type = size_class / 8;
        const auto node_data_size = (min_allocation * 24) << (4 * size_type);

        heap_node_tloc* tloc = nullptr;
        if (m_init.multithread)
        {
            tloc = static_cast<heap_node_tloc*>(::TlsGetValue(m_init.tls));
        }
        else
        {
            ICY_LOCK_GUARD_READ(m_tlocs.lock);
            if (m_tlocs.count)
                tloc = m_init.tlocs;
        }
        if (!tloc)
        {
            auto next = m_tlocs.queue.pop();
            if (!next)
            {
                ICY_LOCK_GUARD_WRITE(m_tlocs.lock);
                if (m_tlocs.count >= m_tlocs.capacity)
                    return nullptr;

                const auto old_size = icy::align_up((m_tlocs.count + 0) * sizeof(heap_node_tloc), page_size);
                const auto new_size = icy::align_up((m_tlocs.count + 1) * sizeof(heap_node_tloc), page_size);
                if (new_size > old_size)
                {
                    auto bytes = reinterpret_cast<uint8_t*>(m_init.tlocs);
                    if (!system_realloc(bytes + old_size, new_size - old_size, system_flag_commit))
                        return nullptr;
                }
                next = m_init.tlocs + m_tlocs.count;
                new (next) heap_node_tloc;
                if (m_init.multithread)
                {
                    if (!TlsSetValue(m_init.tls, next))
                        return nullptr;
                }
                m_tlocs.count += 1;
            }
            tloc = static_cast<heap_node_tloc*>(next);
        }
        auto& node = tloc->sizes[size_class].current;

        // current node -> queue -> new node from unit -> new unit
        while (true)
        {
            auto new_node = false;
            if (!node) node = tloc->sizes[size_class].list.pop(*this);
            if (!node)
            {
                auto& unit = tloc->units[size_type];
                const auto unit_capacity = unit_data_size / node_data_size;

                if (!unit.ptr || unit.size == unit_capacity)
                {
                    // request new unit
                    const size_t count = m_units.count.fetch_add(1, std::memory_order_acq_rel);
                    if (count + 1 >= m_units.capacity)
                        return nullptr;

                    if (!system_realloc(m_init.bytes + count * unit_data_size, unit_data_size, system_flag_commit))
                        return nullptr;
                    if (!system_realloc(m_init.units + count * node_count_0, page_size, system_flag_commit))
                        return nullptr;

                    unit.ptr = m_init.units + node_count_0 * count;
                    unit.size = 0;
                }

                new_node = true;
                node = unit.ptr + unit.size;
                unit.size += 1;
            }
            auto idx = node->allocate(uint64_t(tloc - m_init.tlocs), size_class);
            if (idx != SIZE_MAX)
            {
                const auto aligned_ptr = icy::align_down(node, page_size);
                const auto unit_idx = detail::distance(m_init.units, aligned_ptr) / page_size;
                const auto node_idx = detail::distance(aligned_ptr, node) / sizeof(heap_node_unit);

                auto ptr = m_init.bytes;
                ptr += unit_idx * unit_data_size;
                ptr += node_idx * node_data_size;
                ptr += idx * size;

                m_stats.bytes.fetch_add(size, std::memory_order_release);

                if (new_node)
                {
                    const auto sz_ptr = m_init.sizes + unit_idx * (unit_data_size / node_data_size_0);
                    const auto begin = sz_ptr + ((node_idx + 0) << (4 * size_type));
                    const auto end = sz_ptr + ((node_idx + 1) << (4 * size_type));
                    std::fill(begin, end, char(size_class));
                }
                return ptr;
            }
            else
            {
                node = nullptr;
            }
        }
    }
    if (m_init.disable_sys)
        return nullptr;

    const auto ptr = system_realloc(nullptr, align_up(size, page_size * 16), system_flag_commit | system_flag_reserve);
    if (ptr)
        m_stats.bytes.fetch_add(memsize(ptr), std::memory_order_release);
    return ptr;
}
void heap_base::raw_free(const void* ptr) noexcept
{
    if (!ptr)
        return;

    const auto sz = memsize(ptr);
    size_t size = 0;
    if (ptr >= m_init.bytes && ptr < m_init.bytes + m_units.capacity * unit_data_size)
    {
        const auto offset = detail::distance(m_init.bytes, ptr);
        const auto size_class = size_t(m_init.sizes[offset / node_data_size_0]);
        size = size_classes[size_class];

        const auto size_type = size_class / 8;
        const auto node_data_size = node_data_size_0 << (4 * size_type);

        const auto unit_begin = offset / unit_data_size * unit_data_size;
        const auto node_begin = offset / node_data_size * node_data_size;

        const auto unit_idx = unit_begin / unit_data_size;
        const auto node_idx = (node_begin - unit_begin) / node_data_size;
        const auto cell_idx = (offset - node_begin) / size;

        m_init.units[node_count_0 * unit_idx + node_idx].free(*this, cell_idx, size_class);
    }
    else if (m_init.disable_sys)
    {
        return;
    }
    else
    {
        //size = system_heap::size_of(ptr);
        //system_heap::deallocate(ptr);
    }
    m_stats.bytes.fetch_sub(size, std::memory_order_release);
}
void* heap_base::alloc(size_t size) noexcept
{
    if (size == 0)
        return nullptr;

    auto data_ptr = static_cast<uint8_t*>(raw_alloc(size));
    if (!data_ptr)
        return nullptr;

    if (m_init.debug_trace)
    {
        void* stack[0x20];
        auto hash = 0ul;
        auto trace_len = RtlCaptureStackBackTrace(3, _countof(stack), stack, &hash);
        if (!trace_len)
        {
            raw_free(data_ptr);
            return nullptr;
        }
        auto trace_raw = raw_alloc(sizeof(debug_trace_unit) + (trace_len - 1) * sizeof(void*));
        if (!trace_raw)
        {
            raw_free(data_ptr);
            return nullptr;
        }
        auto trace_ptr = new (trace_raw) debug_trace_unit;
        trace_ptr->hash = hash;
        trace_ptr->size = trace_len;
        trace_ptr->user_data = data_ptr;
        memcpy(trace_ptr->func_data, stack, trace_len * sizeof(void*));
        ICY_LOCK_GUARD_WRITE(m_trace.lock);
        trace_ptr->prev = m_trace.units;
        trace_ptr->size;
        m_trace.units = trace_ptr;
    }

    if (m_init.bit_pattern)
        memset64(data_ptr, bit_alloc, memsize(data_ptr));

    return data_ptr;
}
void heap_base::free(const void* data_ptr) noexcept
{
    if (!data_ptr)
        return;

    const auto ptr = static_cast<const uint8_t*>(data_ptr);
    if (m_init.bit_pattern)
    {
        const auto size = memsize(ptr);
        memset64(const_cast<void*>(data_ptr), bit_free, size);
    }
    if (m_init.debug_trace)
    {
        ICY_LOCK_GUARD_WRITE(m_trace.lock);
        debug_trace_unit* prev = nullptr;
        debug_trace_unit* next = m_trace.units;
        while (next)
        {
            if (next->user_data == data_ptr)
            {
                (prev ? prev->prev : m_trace.units) = next->prev;
                break;
            }
            else
            {
                prev = next;
                next = next->prev;
            }
        }
        raw_free(next);
    }
    return raw_free(ptr);
}

heap_capacity::heap_capacity(const heap_init init) noexcept
{
    base = align_up(init.multithread ? sizeof(heap) : sizeof(heap), page_size);
    data = round_up(init.capacity, unit_data_size);
    const auto units_count = data / unit_data_size;
    unit = units_count * page_size;
    size = align_up(data / node_data_size_0, page_size);
    tloc = align_up(units_count * sizeof(heap_node_tloc), page_size);
}
size_t heap_node_unit::allocate(const uint64_t thread_idx, const size_t size_class) noexcept
{
    const auto capacity = uint32_t("YQMIGEDC"[size_class % 8] - 'A');
    auto old_bits = m_bits.load(std::memory_order_acquire);

    while (true)
    {
        const auto bits = (unsigned long)(old_bits & bits_mask);
        unsigned long idx;
        if (_BitScanForward(&idx, bits))
        {
            if (idx == 0)
            {
                _BitScanForward(&idx, ~bits);
                if (idx < capacity); // (most probable case, do nothing)
                else
                {
                    if (m_bits.compare_exchange_strong(old_bits,
                        uint64_t(bits) | (thread_idx << 32) | flag_mask,
                        std::memory_order_acq_rel)) return SIZE_MAX;
                    else continue; // keep looping
                }
            }
            else idx = 0;
        }
        else idx = 0;

        m_bits.fetch_or(1ui64 << idx, std::memory_order_release);
        return idx;
    }
}
void heap_node_unit::free(heap_base& heap, const size_t cell_idx, const size_t size_class) noexcept
{
    // array{ 16, 12, 8, 4, 3, 2, 1 }
    const auto threshold = uint64_t("QMIGEDCB"[size_class % 8] - 'A');
    const auto mask = ~(1ui64 << cell_idx);

    auto success = false;
    auto bits = m_bits.fetch_and(mask, std::memory_order_acq_rel) & mask;
    while (true)
    {
        const auto cnt =
#if _WIN64
            __popcnt64(bits & bits_mask);
#else
            __popcnt(uint32_t(((bits & bits_mask) >> 0)& UINT32_MAX)) +
            __popcnt(uint32_t(((bits & bits_mask) >> 32)& UINT32_MAX));
#endif
        if ((bits & flag_mask) && (cnt <= threshold))
        {
            if (m_bits.compare_exchange_weak(bits, bits & ~flag_mask, std::memory_order_acq_rel))
            {
                success = true;
                break;
            }
            // else: somebody else has changed bits -> retry
        }
        else
        {
            success = false;
            break;
        }
    }

    // if success == true --> we need to add those bits to queue
    if (success)
    {
        const auto this_idx = uint32_t(this - heap.m_init.units);
        const auto thread_idx = (bits & info_mask) >> 32;
        auto& queue_head = heap.m_init.tlocs[thread_idx].sizes[size_class].list.m_head;
        auto head = queue_head.load(std::memory_order_acquire);

        do { set_info(head); } while
            (!queue_head.compare_exchange_strong(head, this_idx, std::memory_order_acq_rel));
    }
}
heap_node_unit* heap_node_list::pop(heap_base& heap) noexcept
{
    const auto meta = heap.m_init.units;
    if (m_tail != invalid_value)
    {
        const auto result = meta + m_tail;
        m_tail = result->get_info();
        return result;
    }
    else
    {
        auto head = m_head.load(std::memory_order_acquire);
        if (head != invalid_value)
        {
            while (!m_head.compare_exchange_weak(head, invalid_value,
                std::memory_order_acq_rel, std::memory_order_relaxed))
            {
                ; // loop until CAS
            }

            auto prev = meta[head].get_info();
            meta[head].set_info(invalid_value);

            while (prev != invalid_value)
            {
                const auto tmp = meta[prev].get_info();
                meta[prev].set_info(head);
                head = prev;
                prev = tmp;
            }
            m_tail = meta[head].get_info();
            return meta + head;
        }
        else
        {
            return nullptr;
        }
    }
}
size_t icy::align_size(const size_t size) noexcept { size_t tmp; return ::align_size(size, tmp); }

void detail::rw_spin_lock::lock_write() noexcept
{
    AcquireSRWLockExclusive(reinterpret_cast<SRWLOCK*>(this));
}
void detail::rw_spin_lock::lock_read() noexcept
{
    AcquireSRWLockShared(reinterpret_cast<SRWLOCK*>(this));
}
bool detail::rw_spin_lock::try_lock_read() noexcept
{
    return !!TryAcquireSRWLockShared(reinterpret_cast<SRWLOCK*>(this));
}
bool detail::rw_spin_lock::try_lock_write() noexcept
{
    return !!TryAcquireSRWLockExclusive(reinterpret_cast<SRWLOCK*>(this));
}
void detail::rw_spin_lock::unlock_read() noexcept
{
    ReleaseSRWLockShared(reinterpret_cast<SRWLOCK*>(this));
}
void detail::rw_spin_lock::unlock_write() noexcept
{
    ReleaseSRWLockExclusive(reinterpret_cast<SRWLOCK*>(this));
}

icy::global_heap_type detail::global_heap;
#include <icy_engine/core/icy_blob.hpp>
#include <icy_engine/core/icy_map.hpp>
#include <icy_engine/core/icy_string.hpp>
#include <icy_engine/core/icy_smart_pointer.hpp>

using namespace icy;

ICY_STATIC_NAMESPACE_BEG
struct blob_pair
{
    array<uint8_t> bytes;
    string type;
};
static mutex g_lock;
static uint32_t g_index = 0;
static map<uint32_t, unique_ptr<blob_pair>> g_blobs;
ICY_STATIC_NAMESPACE_END

error_type icy::blob_initialize() noexcept
{
    blob_shutdown();
    if (!g_lock)
    {
        ICY_ERROR(g_lock.initialize());
    }
    return error_type();
}
void icy::blob_shutdown() noexcept
{
    if (g_lock)
    {
        g_lock.lock();
        g_blobs.clear();
        g_index = 0;
        g_lock.unlock();
    }
}

error_type icy::blob_add(const const_array_view<uint8_t> bytes, const string_view type, blob& object) noexcept
{
    ICY_LOCK_GUARD(g_lock);
    unique_ptr<blob_pair> new_ptr;
    ICY_ERROR(make_unique(blob_pair(), new_ptr));
    ICY_ERROR(new_ptr->bytes.assign(bytes));
    ICY_ERROR(new_ptr->type.append(type));
    ICY_ERROR(g_blobs.insert(object.index = g_index + 1, std::move(new_ptr)));
    g_index += 1;
    return error_type();
}
const_array_view<uint8_t> icy::blob_data(const blob object) noexcept
{
    ICY_LOCK_GUARD(g_lock);
    if (auto ptr = g_blobs.try_find(object.index))
    {
        return ptr->get()->bytes;
    }
    return const_array_view<uint8_t>();
}
string_view icy::blob_type(const blob object) noexcept
{
    ICY_LOCK_GUARD(g_lock);
    if (auto ptr = g_blobs.try_find(object.index))
    {
        return ptr->get()->type;
    }
    return string_view();
}
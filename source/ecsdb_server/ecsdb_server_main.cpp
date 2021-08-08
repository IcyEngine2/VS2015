#include <icy_engine/core/icy_core.hpp>
#include <icy_engine/core/icy_memory.hpp>
#include <icy_engine/core/icy_event.hpp>
#include <icy_engine/core/icy_console.hpp>
#include <icy_engine/network/icy_network.hpp>
#include <ecsdb/ecsdb_core.hpp>
#include <ecsdb/ecsdb_file_system.hpp>
#if _DEBUG
#pragma comment(lib, "icy_engine_cored")
#else
#pragma comment(lib, "icy_engine_core")
#endif
using namespace icy;

error_type main_ex(heap& heap)
{
    ICY_ERROR(event_system::initialize());

    shared_ptr<console_system> con;
    ICY_ERROR(create_console_system(con));
    ICY_ERROR(con->thread().launch());

    shared_ptr<event_queue> loop;
    ICY_ERROR(create_event_system(loop, 0
        | event_type::user_any
    ));

    ecsdb_file_system ecsdb;
    ICY_ERROR(ecsdb.load("ecsdb_1.dat"_s, 1_gb));

    const auto user = guid::create();
    ICY_ERROR(ecsdb.create_user(user, "user1"_s));

    ecsdb_transaction txn;
    txn.user = user;

    ecsdb_action action;
    action.action = ecsdb_action_type::create_folder;
    const auto index_folder = action.index = guid::create();
    ICY_ERROR(copy("Folder1"_s, action.name));
    action.folder = ecsdb_root;
    ICY_ERROR(txn.actions.push_back(std::move(action)));


    ICY_ERROR(ecsdb.exec(txn));
    

    while (*loop)
    {
        event event;
        ICY_ERROR(loop->pop(event));
        if (!event)
            continue;

    }

    return error_type();

}
int main()
{
    heap gheap;
    if (gheap.initialize(heap_init::global(1_gb)))
        return ENOMEM;
    if (const auto error = main_ex(gheap))
        return error.code;
    return 0;
}
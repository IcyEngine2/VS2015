#pragma once

#include <icy_engine/core/icy_event.hpp>
#include <icy_engine/core/icy_string.hpp>

namespace icy
{
    // error_type process_path(const uint32_t index, string& str) noexcept;
    // error_type process_launch(const string_view path, uint32_t& process, uint32_t& thread) noexcept;
    // error_type process_threads(const uint32_t index, array<uint32_t>& threads) noexcept;

    struct process_handle
    {
        uint32_t index = 0;
    };

    class process_system : public event_system
    {
    public:
        virtual ~process_system() noexcept = 0
        {

        }
        virtual const icy::thread& thread() const noexcept = 0;
        icy::thread& thread() noexcept
        {
            return const_cast<icy::thread&>(static_cast<const process_system*>(this)->thread());
        }
        virtual error_type launch(const string_view path, process_handle& index) = 0;
        virtual error_type inject(const process_handle index, const string_view lib) = 0;
        virtual error_type send(const process_handle index, const string_view request) = 0;
    };

    extern const event_type event_type_process;
    enum class process_event_type : uint32_t
    {
        none,
        close,
        launch,
        inject,
        send,
        recv,
    };
    struct process_event
    {
        process_event_type type = process_event_type::none;
        process_handle handle;
        error_type error;
        string recv;
    };

    error_type create_process_system(shared_ptr<process_system>& system) noexcept;
}
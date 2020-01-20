#pragma once

#include <icy_engine/core/icy_set.hpp>
#include <icy_engine/core/icy_thread.hpp>
#include "../icy_mbox_script2.hpp"
#include "../icy_mbox_network.hpp"

class mbox_server_network;

class mbox_script_thread : public icy::thread
{
    struct mbox_send
    {
        icy::array<icy::input_message> input;
    };
    struct mbox_button_event
    {
        bool press = false;
        mbox::button_type button;
        icy::guid command;
    };
    struct mbox_timer_event
    {
        icy::guid timer;
        icy::guid command;
    };
    struct mbox_character
    {
        icy::guid index;
        mbox::key process = {};
        icy::set<icy::guid> groups;
        icy::array<mbox_button_event> bevents;
        icy::array<mbox_timer_event> tevents;
        icy::map<icy::guid, icy::guid> virt;
    };
public:
    mbox_script_thread(mbox_server_network& network) noexcept : m_network(network)
    {

    }
    icy::error_type run() noexcept override;
    icy::error_type process(mbox_character& character, const icy::const_array_view<mbox::action> actions, icy::map<mbox::key, mbox_send>& send) noexcept;
    icy::error_type process(mbox_character& character, const icy::const_array_view<icy::input_message> data, icy::map<mbox::key, mbox_send>& send) noexcept;
private:
    mbox_server_network& m_network;
    mbox::library m_library;
    icy::map<icy::guid, mbox_character> m_characters;
};
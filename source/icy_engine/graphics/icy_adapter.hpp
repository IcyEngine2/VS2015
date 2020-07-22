#pragma once

#include <icy_engine/utility/icy_com.hpp>
#include <icy_engine/graphics/icy_adapter.hpp>
#include <icy_engine/core/icy_string.hpp>

struct IDXGIAdapter;

class icy::adapter::data_type
{
    friend icy::adapter;
public:
    icy::error_type initialize(const icy::com_ptr<IDXGIAdapter> adapter, const icy::adapter_flags flags) noexcept;
    icy::com_ptr<IDXGIAdapter> adapter() const noexcept
    {
        return m_adapter;
    }
    icy::adapter_flags flags() const noexcept
    {
        return m_flags;
    }
    icy::error_type query_msaa(icy::array<uint32_t>& array) const noexcept;
    std::atomic<uint32_t> ref = 1;
private:
    icy::library m_lib_dxgi = icy::library("dxgi");
    icy::library m_lib_debug = icy::library("dxgidebug");
    icy::com_ptr<IDXGIAdapter> m_adapter;
    icy::adapter_flags m_flags = icy::adapter_flags::none;
    icy::string m_name;
    uint64_t m_shr_mem = 0;
    uint64_t m_vid_mem = 0;
    uint64_t m_sys_mem = 0;
    uint64_t m_luid = 0;
};
#pragma once

#include <icy_engine/core/icy_array.hpp>
#include <icy_engine/core/icy_string.hpp>
#include <icy_engine/core/icy_smart_pointer.hpp>
#include <icy_engine/core/icy_queue.hpp>
#include <icy_engine/graphics/icy_gpu.hpp>
#include <icy_engine/graphics/icy_display.hpp>
#include <icy_engine/graphics/icy_render.hpp>

#define VK_NO_PROTOTYPES 1
#define VK_USE_PLATFORM_WIN32_KHR 1
#include "vulkan/vulkan.h"

namespace icy
{
    class vulkan_gpu_device;
    class vulkan_system
    {
        friend vulkan_gpu_device;
    public:
        vulkan_system(const gpu_flags flags) noexcept : m_flags(flags)
        {

        }
        ~vulkan_system() noexcept;
        error_type initialize() noexcept;
        error_type create_gpu_list(array<shared_ptr<gpu_device>>& array) noexcept;
        VkInstance instance() noexcept
        {
            return m_instance;
        }
        VkAllocationCallbacks& alloc() noexcept
        {
            return m_alloc;
        }
    private:
        const gpu_flags m_flags;
        library m_lib = "vulkan-1"_lib;
        VkAllocationCallbacks m_alloc = {};
        VkInstance m_instance = nullptr;
    };
    class vulkan_gpu_device : public gpu_device
    {
    public:
        vulkan_gpu_device(const shared_ptr<vulkan_system> system) noexcept : m_system(system)
        {

        }
        error_type initialize(const VkPhysicalDeviceProperties& desc, VkPhysicalDevice handle, const gpu_flags flags) noexcept;
        vulkan_system& system() noexcept
        {
            return *m_system;
        }
        VkPhysicalDevice device() noexcept
        {
            return m_handle;
        }
        template<typename T>
        auto call(const char* name) noexcept
        {
            return static_cast<T>(m_system->m_lib.find(name));
        }
        gpu_flags flags() const noexcept override
        {
            return m_flags;
        }
        bool has_extension(const char* name) const noexcept
        {
            for (auto&& ext : m_ext)
            {
                if (strcmp(ext.extensionName, name) == 0)
                    return true;
            }
            return false;
        }
    private:
        string_view name() const noexcept override
        {
            return m_name;
        }
        error_type msaa(render_flags& flags) const noexcept override;
        void* handle() const noexcept override
        {
            return m_handle;
        }
        size_t vid_mem() const noexcept override
        {
            return m_vid_mem;
        }
        size_t sys_mem() const noexcept override
        {
            return m_sys_mem;
        }
        size_t shr_mem() const noexcept override
        {
            return m_shr_mem;
        }
        uint64_t luid() const noexcept override
        {
            return m_luid;
        }
    private:
        shared_ptr<vulkan_system> m_system;
        VkPhysicalDevice m_handle;
        string m_name;
        gpu_flags m_flags = gpu_flags::none;
        size_t m_vid_mem = 0;
        size_t m_sys_mem = 0;
        size_t m_shr_mem = 0;
        uint64_t m_luid = 0;
        array<VkExtensionProperties> m_ext;
    };

    struct vulkan_display_internal_event
    {
        duration_type frame = duration_type(-1);
        window_size size;
    };
    struct vulkan_display_texture
    {

    };
    class vulkan_display_system : public display_system
    {
    public:
        vulkan_display_system(const shared_ptr<gpu_device> adapter, shared_ptr<window> hwnd) : m_adapter(adapter), m_hwnd(hwnd)
        {

        }
        ~vulkan_display_system() noexcept;
        error_type initialize() noexcept;
        error_type do_resize(const window_size size) noexcept;
        error_type do_repaint(const bool vsync) noexcept;
    private:
        error_type exec() noexcept override;
        error_type signal(const event_data* event) noexcept override;
        const icy::thread& thread() const noexcept override
        {
            return *m_thread;
        }
        error_type repaint(const string_view tag, render_gui_frame& frame) noexcept override;
        error_type repaint(const string_view tag, shared_ptr<render_surface> surface) noexcept override;
        error_type resize(const window_size size) noexcept override
        {
            if (size.x == 0 || size.y == 0)
                return make_stdlib_error(std::errc::invalid_argument);

            vulkan_display_internal_event msg;
            msg.size = size;
            return event_system::post(nullptr, event_type::system_internal, msg);
        }
        error_type frame(const duration_type duration) noexcept override
        {
            if (duration.count() < 0)
                return make_stdlib_error(std::errc::invalid_argument);

            vulkan_display_internal_event msg;
            msg.frame = duration;
            return event_system::post(nullptr, event_type::system_internal, msg);
        }
        void clear_chain() noexcept;
    private:
        struct frame_type
        {
            size_t index = 0;
            duration_type delta = display_frame_vsync;
            clock_type::time_point next = {};
            mpsc_queue<std::pair<string, render_gui_frame>> queue2d;
            mpsc_queue<std::pair<string, vulkan_display_texture>> queue3d;
            map<string, render_gui_frame> map2d;
            map<string, vulkan_display_texture> map3d;
        };
    private:
        shared_ptr<vulkan_gpu_device> m_adapter;
        weak_ptr<window> m_hwnd;
        void* m_cpu_timer = nullptr;
        sync_handle m_update;
        bool m_gpu_ready = true;
        bool m_cpu_ready = true;
        shared_ptr<event_thread> m_thread;
        frame_type m_frame;
        VkSurfaceKHR m_surface = nullptr;
        VkSurfaceFormatKHR m_format = {};
        VkPresentModeKHR m_mode = VK_PRESENT_MODE_MAX_ENUM_KHR;
        VkDevice m_device = nullptr;
        VkQueue m_queue = nullptr;
        VkSwapchainKHR m_swap_chain = nullptr;
        array<VkImageView> m_swap_views;
    };

    extern const error_source error_source_vulkan;
}
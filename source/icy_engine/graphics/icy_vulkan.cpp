#include "icy_vulkan.hpp"

using namespace icy;

ICY_STATIC_NAMESPACE_BEG
/*static PFN_vkCreateInstance vulkan_create_instance;
static PFN_vkDestroyInstance vulkan_destroy_instance;

static PFN_vkCreateDevice vulkan_create_device;
static PFN_vkDestroyDevice vulkan_destroy_device;
static PFN_vkGetPhysicalDeviceQueueFamilyProperties vulkan_get_physical_device_queue_family_properties;

static PFN_vkCreateWin32SurfaceKHR vulkan_create_win32_surface_khr;
static PFN_vkDestroySurfaceKHR vulkan_destroy_surface_khr;

static PFN_vkGetDeviceQueue vulkan_get_device_queue;
//static PFN_vkGetPhysicalDeviceSurfaceSupportKHR vulkan_get_physical_device_surface_support_khr;
static PFN_vkCreateSwapchainKHR vulkan_func_create_swapchain_khr;
static PFN_vkDestroySwapchainKHR vulkan_func_destroy_swapchain_khr;*/

static error_type vulkan_error_to_string(const unsigned code, const string_view locale, string& str) noexcept
{
    return error_type();
}
static error_type make_vulkan_error(const VkResult code) noexcept
{
    return error_type(code, error_source_vulkan);
}
ICY_STATIC_NAMESPACE_END

#define CHECK_VK_FUNC_INLINE(var, name) const auto var = PFN_##name(m_lib.find(#name)); if (!var) return make_stdlib_error(std::errc::function_not_supported)
#define CHECK_VK_FUNC(var, name) const auto var = m_adapter->call<PFN_##name>(#name); if (!var) return make_stdlib_error(std::errc::function_not_supported)
#define GET_VK_FUNC(var, name) const auto var = m_adapter->call<PFN_##name>(#name)

error_type vulkan_system::initialize() noexcept
{
    ICY_ERROR(m_lib.initialize());

    m_alloc.pfnAllocation = [](void* pUserData, size_t size, size_t alignment, VkSystemAllocationScope allocationScope)
    {
        return icy::realloc(nullptr, std::max(size, alignment));
    };
    m_alloc.pfnFree = [](void* pUserData, void* pMemory)
    {
        icy::realloc(pMemory, 0);
    };
    m_alloc.pfnReallocation = [](void* pUserData, void* pOriginal, size_t size, size_t alignment, VkSystemAllocationScope allocationScope)
    {
        return icy::realloc(pOriginal, std::max(size, alignment));
    };

    VkApplicationInfo app_info = {};
    app_info.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    app_info.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
    app_info.engineVersion = VK_MAKE_VERSION(1, 0, 0);
    app_info.apiVersion = VK_API_VERSION_1_0;

    VkInstanceCreateInfo create_info = {};
    create_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    create_info.pApplicationInfo = &app_info;

    const char* ext_list[16] = {};
    auto ext_size = 0u;
    ext_list[ext_size++] = VK_KHR_SURFACE_EXTENSION_NAME;
    if (m_flags & gpu_flags::debug)
    {
        ext_list[ext_size++] = VK_EXT_DEBUG_UTILS_EXTENSION_NAME;
    }    
#if _WIN32
    ext_list[ext_size++] = VK_KHR_WIN32_SURFACE_EXTENSION_NAME;
#endif
    
    create_info.enabledExtensionCount = ext_size;
    create_info.ppEnabledExtensionNames = ext_list;

    const char* layer_list[16] = {};
    auto layer_size = 0u;
    if (m_flags & gpu_flags::debug)
        layer_list[layer_size++] = "VK_LAYER_KHRONOS_validation";

    create_info.enabledLayerCount = layer_size;
    create_info.ppEnabledLayerNames = layer_list;

    VkDebugUtilsMessengerCreateInfoEXT debug_info = {};
    debug_info.sType = 
        VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
    debug_info.messageSeverity = 0
        | VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT 
        | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT 
        | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
    debug_info.messageType = 0
        | VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT 
        | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT 
        | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;

    debug_info.pfnUserCallback = [](VkDebugUtilsMessageSeverityFlagBitsEXT sev, 
        VkDebugUtilsMessageTypeFlagsEXT type, const VkDebugUtilsMessengerCallbackDataEXT* data, void* user)
    {
        const auto vk = static_cast<vulkan_system*>(user);
        string msg;
        (to_string(const_array_view<char>(data->pMessage, strlen(data->pMessage)), msg));
        string str;
        (str.appendf("Vulkan: %1\r\n"_s, string_view(msg)));
        (win32_debug_print(str));        
        return VK_FALSE;
    };
    debug_info.pUserData = this;
    if (m_flags & gpu_flags::debug)
        create_info.pNext = &debug_info;
    
    
    CHECK_VK_FUNC_INLINE(vulkan_create_instance, vkCreateInstance);
    if (const auto code = vulkan_create_instance(&create_info, &m_alloc, &m_instance))
        return make_vulkan_error(code);

    return error_type();
}
error_type vulkan_system::create_gpu_list(array<shared_ptr<gpu_device>>& list) noexcept
{
    CHECK_VK_FUNC_INLINE(vulkan_enum, vkEnumeratePhysicalDevices);
    CHECK_VK_FUNC_INLINE(vulkan_props, vkGetPhysicalDeviceProperties);
    
    auto count = 0u;
    if (const auto code = vulkan_enum(m_instance, &count, nullptr))
        return make_vulkan_error(code);

    array<VkPhysicalDevice> devices;
    ICY_ERROR(devices.resize(count));
    if (const auto code = vulkan_enum(m_instance, &count, devices.data()))
        return make_vulkan_error(code);

    for (auto k = 0u; k < count; ++k)
    {
        VkPhysicalDeviceProperties props = {};
        vulkan_props(devices[k], &props);

        const auto is_hardware =
            props.deviceType == VkPhysicalDeviceType::VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU ||
            props.deviceType == VkPhysicalDeviceType::VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU;
        if ((m_flags & gpu_flags::hardware) && !is_hardware)
            continue;
       
        gpu_flags flag = gpu_flags::vulkan;
        if (m_flags & gpu_flags::debug)
            flag = flag | gpu_flags::debug;
        if (is_hardware)
            flag = flag | gpu_flags::hardware;

        shared_ptr<vulkan_gpu_device> new_adapter;
        ICY_ERROR(make_shared(new_adapter, make_shared_from_this(this)));
        ICY_ERROR(new_adapter->initialize(props, devices[k], flag));
        ICY_ERROR(list.push_back(std::move(new_adapter)));
    }
    return error_type();
}
vulkan_system::~vulkan_system() noexcept
{
    const auto vulkan_destroy_instance = PFN_vkDestroyInstance(m_lib.find("vkDestroyInstance"));
    if (vulkan_destroy_instance && m_instance)
        vulkan_destroy_instance(m_instance, &m_alloc);
}

error_type vulkan_gpu_device::msaa(render_flags& flags) const noexcept
{
    return error_type();
}
error_type vulkan_gpu_device::initialize(const VkPhysicalDeviceProperties& desc, VkPhysicalDevice handle, const gpu_flags flags) noexcept
{
    m_handle = handle;
    m_flags = flags;

    ICY_ERROR(to_string(const_array_view<char>(desc.deviceName), m_name));

    //m_shr_mem = desc.SharedSystemMemory;
    //m_vid_mem = desc.DedicatedVideoMemory;
    //m_sys_mem = desc.DedicatedSystemMemory;
    m_luid = (uint64_t(desc.deviceID) << 0x20) | desc.vendorID;

    const auto func = call<PFN_vkEnumerateDeviceExtensionProperties>("vkEnumerateDeviceExtensionProperties");
    if (!func)
        return make_stdlib_error(std::errc::function_not_supported);

    auto count = 0u;
    if (const auto code = func(handle, nullptr, &count, nullptr))
        return make_vulkan_error(code);
    
    ICY_ERROR(m_ext.resize(count));
    if (const auto code = func(handle, nullptr, &count, m_ext.data()))
        return make_vulkan_error(code);
    
    return error_type();
}

vulkan_display_system::~vulkan_display_system() noexcept
{
    if (m_thread) m_thread->wait();
    
    clear_chain();
    
    GET_VK_FUNC(vulkan_destroy_device, vkDestroyDevice);
    if (vulkan_destroy_device)
        vulkan_destroy_device(m_device, &m_adapter->system().alloc());

    GET_VK_FUNC(vulkan_destroy_surface, vkDestroySurfaceKHR);
    if (vulkan_destroy_surface) 
        vulkan_destroy_surface(m_adapter->system().instance(), m_surface, &m_adapter->system().alloc());

    //if (m_cpu_timer) CloseHandle(m_cpu_timer);
    filter(0);
}
void vulkan_display_system::clear_chain() noexcept
{
    GET_VK_FUNC(vulkan_wait_device, vkDeviceWaitIdle);
    if (vulkan_wait_device && m_device)
        vulkan_wait_device(m_device);

    GET_VK_FUNC(vulkan_destroy_view, vkDestroyImageView);
    if (vulkan_destroy_view)
    {
        for (auto k = m_swap_views.size(); k; --k)
            vulkan_destroy_view(m_device, m_swap_views[k - 1], &m_adapter->system().alloc());
    }

    GET_VK_FUNC(vulkan_destroy_swap_chain, vkDestroySwapchainKHR);
    if (vulkan_destroy_swap_chain && m_swap_chain)
        vulkan_destroy_swap_chain(m_device, m_swap_chain, &m_adapter->system().alloc());
}
error_type vulkan_display_system::initialize() noexcept
{
    if (!m_adapter->has_extension(VK_KHR_SWAPCHAIN_EXTENSION_NAME))
        return make_vulkan_error(VkResult::VK_ERROR_EXTENSION_NOT_PRESENT);

    auto window = shared_ptr<icy::window>(m_hwnd);
    if (!window)
        return make_stdlib_error(std::errc::invalid_argument);

    CHECK_VK_FUNC(vulkan_create_win32_surface_khr, vkCreateWin32SurfaceKHR);
    CHECK_VK_FUNC(vulkan_get_qfprops, vkGetPhysicalDeviceQueueFamilyProperties);
    CHECK_VK_FUNC(vulkan_get_surface_support, vkGetPhysicalDeviceSurfaceSupportKHR);
    CHECK_VK_FUNC(vulkan_create_device, vkCreateDevice);
    CHECK_VK_FUNC(vulkan_get_queue, vkGetDeviceQueue);
    
    void* hwnd = nullptr;
    ICY_ERROR(window->win_handle(hwnd));    

    VkWin32SurfaceCreateInfoKHR surface_info = {};
    surface_info.sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR;
    surface_info.hinstance = win32_instance();
    surface_info.hwnd = HWND(hwnd);
    if (const auto code = vulkan_create_win32_surface_khr(m_adapter->system().instance(), &surface_info, &m_adapter->system().alloc(), &m_surface))
        return make_vulkan_error(code);

    auto queue_family_count = 0u;
    vulkan_get_qfprops(m_adapter->device(), &queue_family_count, nullptr);

    VkQueueFamilyProperties queue_family[256] = {};
    queue_family_count = std::min(queue_family_count, uint32_t(_countof(queue_family)));
    vulkan_get_qfprops(m_adapter->device(), &queue_family_count, queue_family);

    auto queue_family_index = UINT32_MAX;
    for (auto k = 0u; k < queue_family_count; ++k)
    {
        if (!(queue_family[k].queueFlags & VK_QUEUE_GRAPHICS_BIT))
            continue;

        auto present = 0u;
        if (const auto code = vulkan_get_surface_support(m_adapter->device(), k, m_surface, &present))
            return make_vulkan_error(code);

        if (!present)
            continue;

        queue_family_index = k;
        break;
    }
    if (queue_family_index == UINT32_MAX)
        return make_vulkan_error(VkResult::VK_ERROR_INITIALIZATION_FAILED);

    CHECK_VK_FUNC(vulkan_get_surface_fmts, vkGetPhysicalDeviceSurfaceFormatsKHR);
    CHECK_VK_FUNC(vulkan_get_surface_mode, vkGetPhysicalDeviceSurfacePresentModesKHR);

    auto format_count = 0u;
    if (const auto code = vulkan_get_surface_fmts(m_adapter->device(), m_surface, &format_count, nullptr))
        return make_vulkan_error(code);

    VkSurfaceFormatKHR formats[0x100] = {};
    format_count = uint32_t(_countof(formats));
    if (const auto code = vulkan_get_surface_fmts(m_adapter->device(), m_surface, &format_count, formats))
        return make_vulkan_error(code);

    VkPresentModeKHR modes[0x100] = {};
    auto mode_count = uint32_t(_countof(modes));
    if (const auto code = vulkan_get_surface_mode(m_adapter->device(), m_surface, &mode_count, modes))
        return make_vulkan_error(code);

    for (auto k = 0u; k < format_count; ++k)
    {
        if (formats[k].format == VkFormat::VK_FORMAT_B8G8R8A8_SRGB &&
            formats[k].colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
        {
            m_format = formats[k];
            break;
        }
    }
    if (!m_format.format)
        return make_vulkan_error(VkResult::VK_ERROR_FORMAT_NOT_SUPPORTED);

    for (auto k = 0u; k < mode_count; ++k)
    {
        if (modes[k] == VK_PRESENT_MODE_MAILBOX_KHR)
        {
            m_mode = modes[k];
            break;
        }
        else if (modes[k] == VK_PRESENT_MODE_FIFO_KHR)
        {
            m_mode = modes[k];
        }
    }
    if (m_mode == VK_PRESENT_MODE_MAX_ENUM_KHR)
        return make_vulkan_error(VkResult::VK_ERROR_INITIALIZATION_FAILED);

    VkPhysicalDeviceFeatures features = {};   

    const float queue_priority = 0.0f;
    VkDeviceQueueCreateInfo queue_info = {};
    queue_info.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    queue_info.queueFamilyIndex = queue_family_index;
    queue_info.queueCount = 1;
    queue_info.pQueuePriorities = &queue_priority;

    const char* ext_list[0x10] = {};
    auto ext_size = 0u;
    ext_list[ext_size++] = VK_KHR_SWAPCHAIN_EXTENSION_NAME;

    if ((m_adapter->flags() & gpu_flags::debug) && m_adapter->has_extension(VK_EXT_DEBUG_MARKER_EXTENSION_NAME))
        ext_list[ext_size++] = VK_EXT_DEBUG_MARKER_EXTENSION_NAME;

    VkDeviceCreateInfo device_info = {};
    device_info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    device_info.pQueueCreateInfos = &queue_info;
    device_info.queueCreateInfoCount = 1;
    device_info.pEnabledFeatures = &features;
    device_info.ppEnabledExtensionNames = ext_list;
    device_info.enabledExtensionCount = ext_size;

    if (const auto code = vulkan_create_device(m_adapter->device(), &device_info, &m_adapter->system().alloc(), &m_device))
        return make_vulkan_error(code);
    vulkan_get_queue(m_device, queue_family_index, 0, &m_queue);
    
    ICY_ERROR(do_resize(window->size()));

    ICY_ERROR(m_update.initialize());

    ICY_ERROR(make_shared(m_thread));
    m_thread->system = this;

    filter(event_type::system_internal | event_type::resource_load);
    return error_type();
}
error_type vulkan_display_system::do_resize(const window_size size) noexcept
{
    clear_chain();
    
    CHECK_VK_FUNC(vulkan_create_swap_chain, vkCreateSwapchainKHR);
    CHECK_VK_FUNC(vulkan_get_surface_caps, vkGetPhysicalDeviceSurfaceCapabilitiesKHR);
    CHECK_VK_FUNC(vulkan_get_images, vkGetSwapchainImagesKHR);
    CHECK_VK_FUNC(vulkan_create_view, vkCreateImageView);
    
    

    VkSurfaceCapabilitiesKHR surface_caps = {};
    vulkan_get_surface_caps(m_adapter->device(), m_surface, &surface_caps);

    auto w = 0u;
    if (surface_caps.currentExtent.width != UINT32_MAX)
        w = surface_caps.currentExtent.width;
    else
        w = std::max(std::min(size.x, surface_caps.maxImageExtent.width), surface_caps.minImageExtent.width);
    
    auto h = 0u;
    if (surface_caps.currentExtent.height != UINT32_MAX)
        h = surface_caps.currentExtent.height;
    else
        h = std::max(std::min(size.y, surface_caps.maxImageExtent.height), surface_caps.minImageExtent.height);

    auto image_count = surface_caps.minImageCount + 1;
    if (surface_caps.maxImageCount > 0)
        image_count = std::min(image_count, surface_caps.maxImageCount);

    VkSwapchainCreateInfoKHR create_info = {};

    create_info.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    if (surface_caps.supportedTransforms & VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR)
        create_info.preTransform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
    else
        create_info.preTransform = surface_caps.currentTransform;


    create_info.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    // Simply select the first composite alpha format available
    const VkCompositeAlphaFlagBitsKHR alpha_flags[] = 
    {
        VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
        VK_COMPOSITE_ALPHA_PRE_MULTIPLIED_BIT_KHR,
        VK_COMPOSITE_ALPHA_POST_MULTIPLIED_BIT_KHR,
        VK_COMPOSITE_ALPHA_INHERIT_BIT_KHR,
    };
    for (const auto flag : alpha_flags) 
    {
        if (surface_caps.supportedCompositeAlpha & flag) 
        {
            create_info.compositeAlpha = flag;
            break;
        }
    }

    create_info.surface = m_surface;
    create_info.minImageCount = image_count;
    create_info.imageFormat = m_format.format;
    create_info.imageColorSpace = m_format.colorSpace;
    create_info.imageExtent = { w, h };
    create_info.imageArrayLayers = 1;
    create_info.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    create_info.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    create_info.imageArrayLayers = 1;
    create_info.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    create_info.queueFamilyIndexCount = 0;
    create_info.presentMode = m_mode;

    if (surface_caps.supportedUsageFlags & VK_IMAGE_USAGE_TRANSFER_SRC_BIT)
        create_info.imageUsage |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
    
    if (surface_caps.supportedUsageFlags & VK_IMAGE_USAGE_TRANSFER_DST_BIT)
        create_info.imageUsage |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;

    if (const auto code = vulkan_create_swap_chain(m_device, &create_info, &m_adapter->system().alloc(), &m_swap_chain))
        return make_vulkan_error(code);

    VkImage image[16] = {};
    auto view_count = uint32_t(_countof(image));
    if (const auto code = vulkan_get_images(m_device, m_swap_chain, &view_count, image))
        return make_vulkan_error(code);

    for (auto k = 0u; k < view_count; ++k)
    {
        VkImageViewCreateInfo view = {};
        view.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        view.format = m_format.format;
        view.components =
        {
            VK_COMPONENT_SWIZZLE_R,
            VK_COMPONENT_SWIZZLE_G,
            VK_COMPONENT_SWIZZLE_B,
            VK_COMPONENT_SWIZZLE_A
        };
        view.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        view.subresourceRange.baseMipLevel = 0;
        view.subresourceRange.levelCount = 1;
        view.subresourceRange.baseArrayLayer = 0;
        view.subresourceRange.layerCount = 1;
        view.viewType = VK_IMAGE_VIEW_TYPE_2D;
        view.image = image[k];

        VkImageView new_view = {};
        if (const auto code = vulkan_create_view(m_device, &view, &m_adapter->system().alloc(), &new_view))
            return make_vulkan_error(code);

        ICY_ERROR(m_swap_views.push_back(new_view));
    }
    return error_type();
}
error_type vulkan_display_system::do_repaint(const bool vsync) noexcept
{
    return error_type();
}
error_type vulkan_display_system::exec() noexcept
{
    const auto reset = [this]
    {
        if (m_frame.delta == display_frame_vsync || m_frame.delta == display_frame_unlim && true)//m_gpu_event)
        {
            m_cpu_ready = true;
        }
        else
        {
            auto now = clock_type::now();
            auto delta = m_frame.next - now;
            //LARGE_INTEGER offset = {};
            if (delta.count() > 0)
            {
                m_frame.next = m_frame.next + m_frame.delta;
            }
            else
            {
                m_frame.next = now + (delta = m_frame.delta);
            }
            //offset.QuadPart = -std::chrono::duration_cast<std::chrono::nanoseconds>(delta).count() / 100;
            //SetWaitableTimer(m_cpu_timer, &offset, 0, nullptr, nullptr, FALSE);
        }
    };
    while (*this)
    {
        window_size size;
        while (auto event = pop())
        {
            if (event->type == event_type::window_resize)
            {
                const auto& event_data = event->data<window_message>();
                const auto is_fullscreen = uint32_t(event_data.state) & uint32_t(window_state::popup | window_state::maximized);
                if (!is_fullscreen)
                {
                    auto window = shared_ptr<icy::window>(m_hwnd);
                    if (window && event_data.window == window->index())
                        size = event_data.size;
                }
            }
            else if (event->type == event_type::system_internal)
            {
                const auto& event_data = event->data<vulkan_display_internal_event>();
                if (event_data.frame.count() >= 0)
                {
                    m_frame.delta = event_data.frame;
                    reset();
                }
                if (event_data.size.x && event_data.size.y)
                {
                    size = event_data.size;
                }
            }
            else if (event->type == event_type::resource_load)
            {
                const auto& event_data = event->data<resource_event>();
                if (event_data.error)
                    continue;
               
            }
        }
        if (size.x && size.y)
        {
            ICY_ERROR(do_resize(size));
        }

        if (m_cpu_ready && m_gpu_ready)
        {
            std::pair<string, render_gui_frame> pair2d;
            if (m_frame.queue2d.pop(pair2d))
            {
                while (m_frame.queue2d.pop(pair2d))
                    ;
                m_cpu_ready = false;
                auto it = m_frame.map2d.find(pair2d.first);
                if (it == m_frame.map2d.end())
                {
                    ICY_ERROR(m_frame.map2d.insert(std::move(pair2d.first), render_gui_frame(), &it));
                }
                it->value = std::move(pair2d.second);
            }
            std::pair<string, vulkan_display_texture> pair3d;
            if (m_frame.queue3d.pop(pair3d))
            {
                while (m_frame.queue3d.pop(pair3d))
                    ;
                m_cpu_ready = false;
                auto it = m_frame.map3d.find(pair3d.first);
                if (it == m_frame.map3d.end())
                {
                    ICY_ERROR(m_frame.map3d.insert(std::move(pair3d.first), vulkan_display_texture(), &it));
                }
                it->value = std::move(pair3d.second);
            }

            if (m_cpu_ready == false)
            {
                m_gpu_ready = true;////m_chain ? false : true;

                display_message msg;
                const auto now = clock_type::now();
                ICY_ERROR(do_repaint(m_frame.delta == display_frame_vsync));
                msg.frame = clock_type::now() - now;
                msg.index = m_frame.index++;
                ICY_ERROR(event::post(this, event_type::display_update, std::move(msg)));
                reset();
            }
        }

        /*HANDLE handles[3] = {};
        auto count = 0u;
        handles[count++] = m_update.handle();
        if (m_cpu_timer) handles[count++] = m_cpu_timer;
        if (m_gpu_event) handles[count++] = m_gpu_event;

        const auto index = WaitForMultipleObjectsEx(count, handles, FALSE, INFINITE, TRUE);*/
        void* handle = nullptr;
        //if (index >= WAIT_OBJECT_0 && index < WAIT_OBJECT_0 + count)
        //    handle = handles[index - WAIT_OBJECT_0];

        if (handle == m_update.handle())
        {
            continue;
        }
        else if (handle && handle == m_cpu_timer)
        {
            m_cpu_ready = true;
        }
        //else if (handle && handle == m_gpu_event)
        //{
        //    m_gpu_ready = true;
        //}
    }
    return error_type();
}
error_type vulkan_display_system::signal(const event_data* event) noexcept
{
    return m_update.wake();
}
error_type vulkan_display_system::repaint(const string_view tag, render_gui_frame& frame) noexcept
{
    string str;
    ICY_ERROR(copy(tag, str));
    ICY_ERROR(m_frame.queue2d.push(std::make_pair(std::move(str), std::move(frame))));
    ICY_ERROR(m_update.wake());
    return error_type();
}
error_type vulkan_display_system::repaint(const string_view tag, shared_ptr<render_surface> surface) noexcept
{
    string str;
    ICY_ERROR(copy(tag, str));
    ICY_ERROR(m_update.wake());
    return error_type();
}
error_type repaint(const string_view tag, shared_ptr<render_surface> surface) noexcept
{
    return error_type();
}

const error_source icy::error_source_vulkan = register_error_source("vulkan"_s, vulkan_error_to_string);
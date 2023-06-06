// Copyright 2023 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

// Include the vulkan platform specific header
#if defined(ANDROID) || defined(__ANDROID__)
#define VK_USE_PLATFORM_ANDROID_KHR
#elif defined(_WIN32)
#define VK_USE_PLATFORM_WIN32_KHR
#elif defined(__APPLE__)
#define VK_USE_PLATFORM_METAL_EXT
#else
#define VK_USE_PLATFORM_WAYLAND_KHR
#define VK_USE_PLATFORM_XLIB_KHR
#endif

#define VULKAN_HPP_NO_CONSTRUCTORS
#include "common/assert.h"
#include "common/logging/log.h"
#include "core/frontend/emu_window.h"
#include "video_core/renderer_vulkan/vk_platform.h"

namespace Vulkan {

vk::DynamicLoader& GetVulkanLoader() {
#ifdef __APPLE__
    try {
        // Attempt to load system Vulkan first, since it may support more capabilities like
        // validation layers.
        static vk::DynamicLoader dl;
        return dl;
    } catch (std::runtime_error&) {
        // Fall back to directly loading bundled MoltenVK library.
        static vk::DynamicLoader dl("libMoltenVK.dylib");
        return dl;
    }
#else
    static vk::DynamicLoader dl;
    return dl;
#endif
}

vk::SurfaceKHR CreateSurface(vk::Instance instance, const Frontend::EmuWindow& emu_window) {
    const auto& window_info = emu_window.GetWindowInfo();
    vk::SurfaceKHR surface{};

#if defined(VK_USE_PLATFORM_WIN32_KHR)
    if (window_info.type == Frontend::WindowSystemType::Windows) {
        const vk::Win32SurfaceCreateInfoKHR win32_ci = {
            .hinstance = nullptr, .hwnd = static_cast<HWND>(window_info.render_surface)};

        if (instance.createWin32SurfaceKHR(&win32_ci, nullptr, &surface) != vk::Result::eSuccess) {
            LOG_CRITICAL(Render_Vulkan, "Failed to initialize Win32 surface");
            UNREACHABLE();
        }
    }
#elif defined(VK_USE_PLATFORM_XLIB_KHR) || defined(VK_USE_PLATFORM_WAYLAND_KHR)
    if (window_info.type == Frontend::WindowSystemType::X11) {
        const vk::XlibSurfaceCreateInfoKHR xlib_ci = {
            .dpy = static_cast<Display*>(window_info.display_connection),
            .window = reinterpret_cast<Window>(window_info.render_surface)};

        if (instance.createXlibSurfaceKHR(&xlib_ci, nullptr, &surface) != vk::Result::eSuccess) {
            LOG_ERROR(Render_Vulkan, "Failed to initialize Xlib surface");
            UNREACHABLE();
        }
    } else if (window_info.type == Frontend::WindowSystemType::Wayland) {
        const vk::WaylandSurfaceCreateInfoKHR wayland_ci = {
            .display = static_cast<wl_display*>(window_info.display_connection),
            .surface = static_cast<wl_surface*>(window_info.render_surface)};

        if (instance.createWaylandSurfaceKHR(&wayland_ci, nullptr, &surface) !=
            vk::Result::eSuccess) {
            LOG_ERROR(Render_Vulkan, "Failed to initialize Wayland surface");
            UNREACHABLE();
        }
    }
#elif defined(VK_USE_PLATFORM_METAL_EXT)
    if (window_info.type == Frontend::WindowSystemType::MacOS) {
        const vk::MetalSurfaceCreateInfoEXT macos_ci = {
            .pLayer = static_cast<const CAMetalLayer*>(window_info.render_surface)};

        if (instance.createMetalSurfaceEXT(&macos_ci, nullptr, &surface) != vk::Result::eSuccess) {
            LOG_CRITICAL(Render_Vulkan, "Failed to initialize MacOS surface");
            UNREACHABLE();
        }
    }
#elif defined(VK_USE_PLATFORM_ANDROID_KHR)
    if (window_info.type == Frontend::WindowSystemType::Android) {
        vk::AndroidSurfaceCreateInfoKHR android_ci = {
            .window = reinterpret_cast<ANativeWindow*>(window_info.render_surface)};

        if (instance.createAndroidSurfaceKHR(&android_ci, nullptr, &surface) !=
            vk::Result::eSuccess) {
            LOG_CRITICAL(Render_Vulkan, "Failed to initialize Android surface");
            UNREACHABLE();
        }
    }
#endif

    if (!surface) {
        LOG_CRITICAL(Render_Vulkan, "Presentation not supported on this platform");
        UNREACHABLE();
    }

    return surface;
}

std::vector<const char*> GetInstanceExtensions(Frontend::WindowSystemType window_type,
                                               bool enable_debug_utils) {
    const auto properties = vk::enumerateInstanceExtensionProperties();
    if (properties.empty()) {
        LOG_ERROR(Render_Vulkan, "Failed to query extension properties");
        return std::vector<const char*>{};
    }

    // Add the windowing system specific extension
    std::vector<const char*> extensions;
    extensions.reserve(6);

#if defined(__APPLE__)
    extensions.push_back(VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME);
#endif

    switch (window_type) {
    case Frontend::WindowSystemType::Headless:
        break;
#if defined(VK_USE_PLATFORM_WIN32_KHR)
    case Frontend::WindowSystemType::Windows:
        extensions.push_back(VK_KHR_WIN32_SURFACE_EXTENSION_NAME);
        break;
#elif defined(VK_USE_PLATFORM_XLIB_KHR) || defined(VK_USE_PLATFORM_WAYLAND_KHR)
    case Frontend::WindowSystemType::X11:
        extensions.push_back(VK_KHR_XLIB_SURFACE_EXTENSION_NAME);
        break;
    case Frontend::WindowSystemType::Wayland:
        extensions.push_back(VK_KHR_WAYLAND_SURFACE_EXTENSION_NAME);
        break;
#elif defined(VK_USE_PLATFORM_METAL_EXT)
    case Frontend::WindowSystemType::MacOS:
        extensions.push_back(VK_EXT_METAL_SURFACE_EXTENSION_NAME);
        break;
#elif defined(VK_USE_PLATFORM_ANDROID_KHR)
    case Frontend::WindowSystemType::Android:
        extensions.push_back(VK_KHR_ANDROID_SURFACE_EXTENSION_NAME);
        break;
#endif
    default:
        LOG_ERROR(Render_Vulkan, "Presentation not supported on this platform");
        break;
    }

    if (window_type != Frontend::WindowSystemType::Headless) {
        extensions.push_back(VK_KHR_SURFACE_EXTENSION_NAME);
    }

    if (enable_debug_utils) {
        extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
        extensions.push_back(VK_EXT_DEBUG_REPORT_EXTENSION_NAME);
    }

    // Sanitize extension list
    std::erase_if(extensions, [&](const char* extension) -> bool {
        const auto it =
            std::find_if(properties.begin(), properties.end(), [extension](const auto& prop) {
                return std::strcmp(extension, prop.extensionName) == 0;
            });

        if (it == properties.end()) {
            LOG_INFO(Render_Vulkan, "Candidate instance extension {} is not available", extension);
            return true;
        }
        return false;
    });

    return extensions;
}

void LoadInstanceFunctions(vk::Instance instance) {
    // Perform instance function loading here, to also load window system functions
    VULKAN_HPP_DEFAULT_DISPATCHER.init(instance);
}

vk::InstanceCreateFlags GetInstanceFlags() {
#if defined(__APPLE__)
    return vk::InstanceCreateFlagBits::eEnumeratePortabilityKHR;
#else
    return static_cast<vk::InstanceCreateFlags>(0);
#endif
}

} // namespace Vulkan

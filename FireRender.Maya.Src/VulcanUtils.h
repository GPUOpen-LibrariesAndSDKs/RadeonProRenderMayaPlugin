/**********************************************************************
Copyright 2020 Advanced Micro Devices, Inc
Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at
    http://www.apache.org/licenses/LICENSE-2.0
Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
********************************************************************/
#pragma once

#define VK_ENABLE_BETA_EXTENSIONS
#include <vulkan/vulkan.h>
#include <vulkan/vulkan.hpp>
#include <cstring>

#ifdef _WIN32
#include <Windows.h>
#elif defined __linux || defined __APPLE__
#include <dlfcn.h>
#endif

#ifdef _WIN32
#define LIBRARY_TYPE HMODULE
#elif defined __linux || defined __APPLE__
#define LIBRARY_TYPE void*
#endif


namespace RprVulkanUtils
{

    bool LoadVulkanLibrary(LIBRARY_TYPE& vulkan_library) 
    {
#if defined _WIN32
        vulkan_library = LoadLibrary(TEXT("vulkan-1.dll"));
#elif defined __linux
        vulkan_library = dlopen("libvulkan.so.1", RTLD_NOW);
#elif defined __APPLE__
        vulkan_library = dlopen("libvulkan.1.dylib", RTLD_NOW);
#endif

        if (vulkan_library == nullptr) 
        {
            return false;
        }
        return true;
    }

    struct VulcanFunctions 
    {
        PFN_vkGetInstanceProcAddr vkGetInstanceProcAddr = nullptr;
        PFN_vkCreateInstance vkCreateInstance = nullptr;
        PFN_vkEnumerateInstanceExtensionProperties vkEnumerateInstanceExtensionProperties = nullptr;
        PFN_vkEnumeratePhysicalDevices vkEnumeratePhysicalDevices = nullptr;
        PFN_vkGetPhysicalDeviceProperties vkGetPhysicalDeviceProperties = nullptr;
        PFN_vkGetPhysicalDeviceMemoryProperties vkGetPhysicalDeviceMemoryProperties = nullptr;
        PFN_vkDestroyInstance vkDestroyInstance = nullptr;
    };

    void UnloadVulkanLibrary(LIBRARY_TYPE& vulkan_library) 
    {
#if defined _WIN32
        FreeLibrary(vulkan_library);
#elif defined __linux || defined __APPLE__
        dlclose(vulkan_library);
#endif
    }

    bool LoadGlobalFunctions(LIBRARY_TYPE const& vulkan_library, VulcanFunctions& vkf) 
    {
#if defined _WIN32
#define LoadFunction GetProcAddress
#elif defined __linux || defined __APPLE__
#define LoadFunction dlsym
#endif

        vkf.vkGetInstanceProcAddr = (PFN_vkGetInstanceProcAddr)LoadFunction(vulkan_library, "vkGetInstanceProcAddr");
        if (vkf.vkGetInstanceProcAddr == nullptr) {
            return false;
        }

        vkf.vkCreateInstance = (PFN_vkCreateInstance)vkf.vkGetInstanceProcAddr(nullptr, "vkCreateInstance");
        if (vkf.vkCreateInstance == nullptr) {
            return false;
        }

        vkf.vkEnumerateInstanceExtensionProperties = (PFN_vkEnumerateInstanceExtensionProperties)vkf.vkGetInstanceProcAddr(nullptr, "vkEnumerateInstanceExtensionProperties");
        if (vkf.vkEnumerateInstanceExtensionProperties == nullptr) {
            return false;
        }

        return true;
    }

    bool LoadInstanceFunctions(LIBRARY_TYPE const& vulkan_library, VkInstance instance, VulcanFunctions& vkf) 
    {
        vkf.vkEnumeratePhysicalDevices = (PFN_vkEnumeratePhysicalDevices)vkf.vkGetInstanceProcAddr(instance, "vkEnumeratePhysicalDevices");
        if (vkf.vkEnumeratePhysicalDevices == nullptr) 
        {
            return false;
        }

        vkf.vkGetPhysicalDeviceProperties = (PFN_vkGetPhysicalDeviceProperties)vkf.vkGetInstanceProcAddr(instance, "vkGetPhysicalDeviceProperties");
        if (vkf.vkGetPhysicalDeviceProperties == nullptr) 
        {
            return false;
        }

        vkf.vkGetPhysicalDeviceMemoryProperties = (PFN_vkGetPhysicalDeviceMemoryProperties)vkf.vkGetInstanceProcAddr(instance, "vkGetPhysicalDeviceMemoryProperties");
        if (vkf.vkGetPhysicalDeviceProperties == nullptr) 
        {
            return false;
        }

        vkf.vkDestroyInstance = (PFN_vkDestroyInstance)vkf.vkGetInstanceProcAddr(instance, "vkDestroyInstance");
        if (vkf.vkDestroyInstance == nullptr) 
        {
            return false;
        }

        return true;
    }

    bool CheckAvailableInstanceExtensions(std::vector<VkExtensionProperties>& available_extensions, VulcanFunctions& vkf) 
    {
        uint32_t extensions_count = 0;
        VkResult result = VK_SUCCESS;

        result = vkf.vkEnumerateInstanceExtensionProperties(nullptr, &extensions_count, nullptr);
        if ((result != VK_SUCCESS) || (extensions_count == 0)) 
        {
            return false;
        }

        available_extensions.resize(extensions_count);
        result = vkf.vkEnumerateInstanceExtensionProperties(nullptr, &extensions_count, available_extensions.data());
        if ((result != VK_SUCCESS) || (extensions_count == 0)) 
        {
            return false;
        }

        return true;
    }

    bool IsExtensionSupported(std::vector<VkExtensionProperties> const& available_extensions, char const* const extension) 
    {
        for (auto& available_extension : available_extensions) 
        {
            if (strstr(available_extension.extensionName, extension)) 
            {
                return true;
            }
        }

        return false;
    }

    bool CreateVulkanInstance(std::vector<char const*> const& desired_extensions, VkInstance& instance, VulcanFunctions& vkf)
    {
        std::vector<VkExtensionProperties> available_extensions;
        if (!CheckAvailableInstanceExtensions(available_extensions, vkf)) 
        {
            return false;
        }

        for (auto& extension : desired_extensions) 
        {
            if (!IsExtensionSupported(available_extensions, extension)) 
            {
                return false;
            }
        }

        VkApplicationInfo application_info = 
        {
            VK_STRUCTURE_TYPE_APPLICATION_INFO,               // VkStructureType           sType
            nullptr,                                          // const void              * pNext
            "Rpr",                                            // const char              * pApplicationName
            VK_MAKE_VERSION(1, 0, 0),                         // uint32_t                  applicationVersion
            "Rpr",                                            // const char              * pEngineName
            VK_MAKE_VERSION(1, 0, 0),                         // uint32_t                  engineVersion
            VK_MAKE_VERSION(1, 0, 0)                          // uint32_t                  apiVersion
        };

        VkInstanceCreateInfo instance_create_info = 
        {
            VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,           // VkStructureType           sType
            nullptr,                                          // const void              * pNext
            0,                                                // VkInstanceCreateFlags     flags
            &application_info,                                // const VkApplicationInfo * pApplicationInfo
            0,                                                // uint32_t                  enabledLayerCount
            nullptr,                                          // const char * const      * ppEnabledLayerNames
            0,                                                // uint32_t                  enabledExtensionCount
            nullptr                                           // const char * const      * ppEnabledExtensionNames
        };

        VkResult result = vkf.vkCreateInstance(&instance_create_info, nullptr, &instance);
        if ((result != VK_SUCCESS) || (instance == VK_NULL_HANDLE)) 
        {
            return false;
        }

        return true;
    }

    bool EnumeratePhysicalDevices(VkInstance instance, std::vector<VkPhysicalDevice>& available_devices, VulcanFunctions& vkf) 
    {
        uint32_t devices_count = 0;
        VkResult result = VK_SUCCESS;

        result = vkf.vkEnumeratePhysicalDevices(instance, &devices_count, nullptr);
        if ((result != VK_SUCCESS) || (devices_count == 0)) 
        {
            return false;
        }

        available_devices.resize(devices_count);
        result = vkf.vkEnumeratePhysicalDevices(instance, &devices_count, available_devices.data());
        if ((result != VK_SUCCESS) || (devices_count == 0)) 
        {
            return false;
        }

        return true;
    }

    void GetPhysicalDeviceProperties(VkPhysicalDevice physical_device, VkPhysicalDeviceProperties& device_properties, VulcanFunctions& vkf) 
    {
        vkf.vkGetPhysicalDeviceProperties(physical_device, &device_properties);
    }

    void GetPhysicalDeviceMemoryProperties(VkPhysicalDevice physical_device, VkPhysicalDeviceMemoryProperties& memory_properties, VulcanFunctions& vkf) 
    {
        vkf.vkGetPhysicalDeviceMemoryProperties(physical_device, &memory_properties);
    }

    void DestroyInstance(VkInstance instance, VulcanFunctions& vkf) 
    {
        if (instance) 
        {
            vkf.vkDestroyInstance(instance, nullptr);
        }
    }

    size_t GetGpuMemory(void)
    {
        LIBRARY_TYPE vulkan_library = nullptr;
        LoadVulkanLibrary(vulkan_library);

        VulcanFunctions vkf;
        if (!LoadGlobalFunctions(vulkan_library, vkf)) 
        {
            UnloadVulkanLibrary(vulkan_library);
            return 0;
        }

        VkInstance instance = nullptr;
        std::vector<char const*> extentions = 
        {
            VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME,
        };

        if (!CreateVulkanInstance(extentions, instance, vkf)) 
        {
            UnloadVulkanLibrary(vulkan_library);
            return 0;
        }

        if (!LoadInstanceFunctions(vulkan_library, instance, vkf)) 
        {
            DestroyInstance(instance, vkf);
            UnloadVulkanLibrary(vulkan_library);
            return 0;
        }

        std::vector<VkPhysicalDevice> physical_devices;
        if (!EnumeratePhysicalDevices(instance, physical_devices, vkf)) 
        {
            DestroyInstance(instance, vkf);
            UnloadVulkanLibrary(vulkan_library);
            return 0;
        }

        std::vector<std::size_t> deviceSizes;
        for (auto& physical_device : physical_devices) 
        {
            VkPhysicalDeviceProperties device_properties;

            GetPhysicalDeviceProperties(physical_device, device_properties, vkf);

            vk::PhysicalDeviceMemoryProperties memory_properties;
            GetPhysicalDeviceMemoryProperties(physical_device, memory_properties, vkf);

            std::uint32_t localHeapIndex = 0;

            for (std::uint32_t i = 0; i < memory_properties.memoryTypeCount; i++)
            {
                using tx = decltype(vk::MemoryPropertyFlagBits::eDeviceLocal);
                using ty = decltype(memory_properties.memoryTypes[i].propertyFlags);

                if (memory_properties.memoryTypes[i].propertyFlags & vk::MemoryPropertyFlagBits::eDeviceLocal)
                {
                    localHeapIndex = memory_properties.memoryTypes[i].heapIndex;
                    break;
                }
            }

            deviceSizes.push_back(memory_properties.memoryHeaps[localHeapIndex].size);
        }

        auto maxSizeIter = std::max_element(deviceSizes.begin(), deviceSizes.end());

        UnloadVulkanLibrary(vulkan_library);

        return *maxSizeIter;
    }
}

auto operator""_MB(const unsigned long long x) -> unsigned long long { return 1024L * 1024L * x; }
auto operator""_GB(const unsigned long long x) -> unsigned long long { return 1024L * 1024L * 1024L * x; }
/*
class VulkanUtils
{
public:
    struct MemoryAllocationInfo
    {
        std::uint32_t meshMemorySizeMb;
        std::uint32_t accelerationMemorySizeMb;
        std::uint32_t stagingMemorySizeMb;
        std::uint32_t scratchMemorySizeMb;
    };

    friend std::ostream& operator<<(std::ostream& os, const MemoryAllocationInfo& g);

    static std::size_t GetGpuMemory()
    {
        auto applicationInfo = vk::ApplicationInfo()
            .setPApplicationName("AMD RenderStudio")
            .setApplicationVersion(VK_MAKE_VERSION(1, 0, 0))
            .setPEngineName("AMD RenderStudio")
            .setEngineVersion(VK_MAKE_VERSION(1, 0, 0))
            .setApiVersion(VK_API_VERSION_1_2);

        auto instanceCreateInfo = vk::InstanceCreateInfo()
            .setPApplicationInfo(&applicationInfo);

        vk::UniqueInstance instance = vk::createInstanceUnique(instanceCreateInfo);
        std::vector<vk::PhysicalDevice> devices = instance->enumeratePhysicalDevices();
        std::vector<std::size_t> deviceSizes;

        std::transform(devices.begin(), devices.end(), std::back_inserter(deviceSizes), [](const vk::PhysicalDevice& device)
            {
                vk::PhysicalDeviceMemoryProperties properties = device.getMemoryProperties();
                std::uint32_t localHeapIndex = 0;

                for (std::uint32_t i = 0; i < properties.memoryTypeCount; i++)
                {
                    using tx = decltype(vk::MemoryPropertyFlagBits::eDeviceLocal);
                    using ty = decltype(properties.memoryTypes[i].propertyFlags);

                    if (properties.memoryTypes[i].propertyFlags & vk::MemoryPropertyFlagBits::eDeviceLocal)
                    {
                        localHeapIndex = properties.memoryTypes[i].heapIndex;
                        break;
                    }
                }

                return properties.memoryHeaps[localHeapIndex].size;
            });

        auto maxSizeIter = std::max_element(deviceSizes.begin(), deviceSizes.end());

        if (maxSizeIter == deviceSizes.end())
        {
            throw std::runtime_error("Can't determine VRAM amount");
        }

        return *maxSizeIter;
    }

    static MemoryAllocationInfo GetHybridMemoryAllocationInfo(std::size_t vertexSizeBytes)
    {
        // Allocate additional 128 Mb for appended primitives
        std::uint32_t meshMemorySize = (vertexSizeBytes + 128_MB) >> 20;
        std::uint32_t accelerationMemorySize = meshMemorySize * 2;
        std::uint32_t stagingMemorySize = 512_MB >> 20;
        std::uint32_t scratchMemorySize = 256_MB >> 20;

        if (VulkanUtils::GetGpuMemory() < 10_GB)
        {
            stagingMemorySize = 32_MB >> 20;
            scratchMemorySize = 16_MB >> 20;
        }

        return MemoryAllocationInfo
        {
            meshMemorySize < 1 ? 1 : meshMemorySize,
            accelerationMemorySize < 1 ? 1 : accelerationMemorySize,
            stagingMemorySize < 1 ? 1 : stagingMemorySize,
            scratchMemorySize < 1 ? 1 : scratchMemorySize
        };
    }
};

std::ostream& operator<<(std::ostream& os, const VulkanUtils::MemoryAllocationInfo& g)
{
    os << "MeshMemorySize: " << g.meshMemorySizeMb << '\n';
    os << "AccelerationMemorySize: " << g.accelerationMemorySizeMb << '\n';
    os << "StagingMemorySize: " << g.stagingMemorySizeMb << '\n';
    os << "ScratchMemorySize: " << g.scratchMemorySizeMb << '\n';
    os << "Total pre-allocated memory: " << g.meshMemorySizeMb + g.accelerationMemorySizeMb + g.stagingMemorySizeMb + g.scratchMemorySizeMb << std::endl;
    return os;
}*/



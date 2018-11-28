#include "RayTracingApplication.h"

RayTracingApplication::RayTracingApplication()
{
}

// ============================================================
// Tutorial 01: Create device with ray_tracing support
// ============================================================
void RayTracingApplication::CreateDevice()
{
    std::vector<VkDeviceQueueCreateInfo> deviceQueueCreateInfos;
    float priority = 0.0f;

    VkDeviceQueueCreateInfo deviceQueueCreateInfo;
    deviceQueueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    deviceQueueCreateInfo.pNext = nullptr;
    deviceQueueCreateInfo.flags = 0;
    deviceQueueCreateInfo.queueFamilyIndex = _queuesInfo.Graphics.QueueFamilyIndex;
    deviceQueueCreateInfo.queueCount = 1;
    deviceQueueCreateInfo.pQueuePriorities = &priority;
    deviceQueueCreateInfos.push_back(deviceQueueCreateInfo);

    if (_queuesInfo.Compute.QueueFamilyIndex != _queuesInfo.Graphics.QueueFamilyIndex)
    {
        deviceQueueCreateInfo.queueFamilyIndex = _queuesInfo.Compute.QueueFamilyIndex;
        deviceQueueCreateInfos.push_back(deviceQueueCreateInfo);
    }
    if (_queuesInfo.Transfer.QueueFamilyIndex != _queuesInfo.Graphics.QueueFamilyIndex && _queuesInfo.Transfer.QueueFamilyIndex != _queuesInfo.Compute.QueueFamilyIndex)
    {
        deviceQueueCreateInfo.queueFamilyIndex = _queuesInfo.Transfer.QueueFamilyIndex;
        deviceQueueCreateInfos.push_back(deviceQueueCreateInfo);
    }

    VkPhysicalDeviceDescriptorIndexingFeaturesEXT descriptorIndexing = { };
    descriptorIndexing.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_INDEXING_FEATURES_EXT;

    VkPhysicalDeviceFeatures2 features2 = { };
    features2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;

    auto found = std::find(_deviceExtensions.begin(), _deviceExtensions.end(), VK_EXT_DESCRIPTOR_INDEXING_EXTENSION_NAME);
    if (found != _deviceExtensions.end())
        features2.pNext = &descriptorIndexing;

    vkGetPhysicalDeviceFeatures2( _physicalDevice, &features2 );

    VkDeviceCreateInfo deviceCreateInfo;
    deviceCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    deviceCreateInfo.pNext = &features2;
    deviceCreateInfo.flags = 0;
    deviceCreateInfo.queueCreateInfoCount = (uint32_t)deviceQueueCreateInfos.size();
    deviceCreateInfo.pQueueCreateInfos = deviceQueueCreateInfos.data();
    deviceCreateInfo.enabledLayerCount = 0;
    deviceCreateInfo.ppEnabledLayerNames = nullptr;
    deviceCreateInfo.enabledExtensionCount = (uint32_t)_deviceExtensions.size();
    deviceCreateInfo.ppEnabledExtensionNames = _deviceExtensions.data();
    deviceCreateInfo.pEnabledFeatures = nullptr;

    VkResult code = vkCreateDevice(_physicalDevice, &deviceCreateInfo, nullptr, &_device);
    if (code == VK_ERROR_EXTENSION_NOT_PRESENT)
    {
        NVVK_CHECK_ERROR(code, L"vkCreateDevice failed due to missing extension.\n\nMake sure VK_NV_RAY_TRACING_EXTENSION is supported by installed driver!\n\n");
    }
    else
    {
        NVVK_CHECK_ERROR(code, L"vkCreateDevice");
    }
}

void RayTracingApplication::InitRayTracing()
{
    NVVK_RESOLVE_DEVICE_FUNCTION_ADDRESS(_device, vkCreateAccelerationStructureNV);
    NVVK_RESOLVE_DEVICE_FUNCTION_ADDRESS(_device, vkDestroyAccelerationStructureNV);
    NVVK_RESOLVE_DEVICE_FUNCTION_ADDRESS(_device, vkGetAccelerationStructureMemoryRequirementsNV);
    NVVK_RESOLVE_DEVICE_FUNCTION_ADDRESS(_device, vkCmdCopyAccelerationStructureNV);
    NVVK_RESOLVE_DEVICE_FUNCTION_ADDRESS(_device, vkBindAccelerationStructureMemoryNV);
    NVVK_RESOLVE_DEVICE_FUNCTION_ADDRESS(_device, vkCmdBuildAccelerationStructureNV);
    NVVK_RESOLVE_DEVICE_FUNCTION_ADDRESS(_device, vkCmdTraceRaysNV);
    NVVK_RESOLVE_DEVICE_FUNCTION_ADDRESS(_device, vkGetRayTracingShaderGroupHandlesNV);
    NVVK_RESOLVE_DEVICE_FUNCTION_ADDRESS(_device, vkCreateRayTracingPipelinesNV);
    NVVK_RESOLVE_DEVICE_FUNCTION_ADDRESS(_device, vkGetAccelerationStructureHandleNV);

    // ============================================================
    // Query values of shaderGroupHandleSize and maxRecursionDepth in current implementation 
    _rayTracingProperties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PROPERTIES_NV;
    _rayTracingProperties.pNext = nullptr;
    _rayTracingProperties.maxRecursionDepth = 0;
    _rayTracingProperties.shaderGroupHandleSize = 0;

    VkPhysicalDeviceProperties2 props;
    props.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
    props.pNext = &_rayTracingProperties;
    props.properties = { };

    vkGetPhysicalDeviceProperties2(_physicalDevice, &props);
}
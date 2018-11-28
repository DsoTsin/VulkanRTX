#include "../Common/Application.h"
#include "vulkan/vulkan.h"

class TutorialApplication : public Application
{
public:
    // ============================================================
    // RAY_TRACING DATA
    // ============================================================

    PFN_vkCreateAccelerationStructureNV vkCreateAccelerationStructureNV = VK_NULL_HANDLE;
    PFN_vkDestroyAccelerationStructureNV vkDestroyAccelerationStructureNV = VK_NULL_HANDLE;
    PFN_vkGetAccelerationStructureMemoryRequirementsNV vkGetAccelerationStructureMemoryRequirementsNV = VK_NULL_HANDLE;
    PFN_vkCmdCopyAccelerationStructureNV vkCmdCopyAccelerationStructureNV = VK_NULL_HANDLE;
    PFN_vkBindAccelerationStructureMemoryNV vkBindAccelerationStructureMemoryNV = VK_NULL_HANDLE;
    PFN_vkCmdBuildAccelerationStructureNV vkCmdBuildAccelerationStructureNV = VK_NULL_HANDLE;
    PFN_vkCmdTraceRaysNV vkCmdTraceRaysNV = VK_NULL_HANDLE;
    PFN_vkGetRayTracingShaderGroupHandlesNV vkGetRayTracingShaderGroupHandlesNV = VK_NULL_HANDLE;
    PFN_vkCreateRayTracingPipelinesNV vkCreateRayTracingPipelinesNV = VK_NULL_HANDLE;
    PFN_vkGetAccelerationStructureHandleNV vkGetAccelerationStructureHandleNV = VK_NULL_HANDLE;

    VkPhysicalDeviceRayTracingPropertiesNV _ray_tracingProperties = { };

public:
    TutorialApplication();
    virtual ~TutorialApplication();

    virtual void CreateDevice() override;                        // Tutorial 01
    virtual void Init() override;                                // Tutorial 01
};

TutorialApplication::TutorialApplication()
{
    _appName = L"VkRay Tutorial 01: Initialization";
}

TutorialApplication::~TutorialApplication()
{

}

// ============================================================
// Tutorial 01: Create device with ray_tracing support
// ============================================================
void TutorialApplication::CreateDevice()
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

    // Request NV_RAY_TRACING_EXTENSION:
    std::vector<const char*> deviceExtensions({ VK_KHR_SWAPCHAIN_EXTENSION_NAME, VK_NV_RAY_TRACING_EXTENSION_NAME });
    VkPhysicalDeviceFeatures features = { };

    VkDeviceCreateInfo deviceCreateInfo;
    deviceCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    deviceCreateInfo.pNext = nullptr;
    deviceCreateInfo.flags = 0;
    deviceCreateInfo.queueCreateInfoCount = (uint32_t)deviceQueueCreateInfos.size();
    deviceCreateInfo.pQueueCreateInfos = deviceQueueCreateInfos.data();
    deviceCreateInfo.enabledLayerCount = 0;
    deviceCreateInfo.ppEnabledLayerNames = nullptr;
    deviceCreateInfo.enabledExtensionCount = (uint32_t)deviceExtensions.size();
    deviceCreateInfo.ppEnabledExtensionNames = deviceExtensions.data();
    deviceCreateInfo.pEnabledFeatures = &features;

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

void TutorialApplication::Init()
{
    // ============================================================
    // Get ray_tracing function pointers
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
    _ray_tracingProperties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PROPERTIES_NV;
    _ray_tracingProperties.pNext = nullptr;
    _ray_tracingProperties.maxRecursionDepth = 0;
    _ray_tracingProperties.shaderGroupHandleSize = 0;
    VkPhysicalDeviceProperties2 props;
    props.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
    props.pNext = &_ray_tracingProperties;
    props.properties = { };
    vkGetPhysicalDeviceProperties2(_physicalDevice, &props);
}

void main(int argc, const char* argv[])
{
    RunApplication<TutorialApplication>();
}

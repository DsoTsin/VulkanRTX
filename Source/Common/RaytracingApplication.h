#pragma once

#include "Application.h"
#include "vulkan/vulkan.h"

struct VkGeometryInstance
{
    float transform[12];
    uint32_t instanceId : 24;
    uint32_t mask : 8;
    uint32_t instanceOffset : 24;
    uint32_t flags : 8;
    uint64_t accelerationStructureHandle;
};

class RayTracingApplication : public Application
{
public:
    RayTracingApplication();

protected:
    std::vector<const char*> _deviceExtensions;

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

    VkPhysicalDeviceRayTracingPropertiesNV _rayTracingProperties = { };

    void InitRayTracing();
    virtual void CreateDevice() override; // Tutorial 01
};
#pragma once

#include <memory>
#include <numeric>
#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <array>

#define NOMINMAX
#include <Windows.h>
#include "Shlwapi.h"
#pragma comment(lib, "shlwapi.lib")

#include "vulkan/vulkan.h"

std::wstring ToString(VkResult value);
void LogError(const std::wstring& message, bool silent = false);
void ExitError(const std::wstring& message, bool silent = false);

struct Settings
{
    bool ValidationEnabled = false;
    uint32_t DesiredWindowWidth = 1280;
    uint32_t DesiredWindowHeight = 720;
    VkFormat DesiredSurfaceFormat = VK_FORMAT_B8G8R8A8_UNORM;
};

struct WindowInfo
{
    HWND Window;
    HINSTANCE WindowInstance;
};

struct QueueInfo
{
    int32_t QueueFamilyIndex;
    VkQueue Queue;
};

struct QueuesInfo
{
    QueueInfo Graphics;
    QueueInfo Compute;
    QueueInfo Transfer;
};

struct MsgInfo
{
    HWND hWnd;
    UINT uMsg;
    WPARAM wParam;
    LPARAM lParam;
};


class ResourceBase
{
protected:
    static VkPhysicalDevice _physicalDevice;
    static VkDevice _device;
    static VkPhysicalDeviceMemoryProperties _physicalDeviceMemoryProperties;
    static VkCommandPool _commandPool;
    static VkQueue _transferQueue;

public:
    static void Init(VkPhysicalDevice physicalDevice, VkDevice device, VkCommandPool commandPool, VkQueue transferQueue);
    static uint32_t GetMemoryType(VkMemoryRequirements& memoryRequiriments, VkMemoryPropertyFlags memoryProperties);
};

class ImageResource : public ResourceBase
{
private:
    static std::wstring _folderPath;

public:
    VkImage Image = VK_NULL_HANDLE;
    VkDeviceMemory Memory = VK_NULL_HANDLE;
    VkImageView ImageView = VK_NULL_HANDLE;
    VkSampler Sampler = VK_NULL_HANDLE;

public:
    ~ImageResource();

public:
    static void SetFolderPath(const std::wstring& folderPath);

    VkResult CreateImage(VkImageType imageType, VkFormat format, VkExtent3D extent, VkImageTiling tiling,
        VkImageUsageFlags usage, VkMemoryPropertyFlags memoryProperties);

    bool LoadTexture2DFromFile(const std::wstring& fileName, VkResult& vkResult);

    VkResult CreateImageView(VkImageViewType viewType, VkFormat format, VkImageSubresourceRange subresourceRange);

    VkResult CreateSampler(VkFilter magFilter, VkFilter minFilter, VkSamplerMipmapMode mipmapMode, VkSamplerAddressMode addressMode);

    void Cleanup();
};

class ShaderResource : public ResourceBase
{
private:
    static std::wstring _folderPath;
    VkShaderModule _module = VK_NULL_HANDLE;

public:
    ~ShaderResource();

public:
    static void SetFolderPath(const std::wstring& folderPath);

    VkResult LoadFromFile(const std::wstring& fileName, bool& cantOpenFile);
    void Cleanup();

    VkPipelineShaderStageCreateInfo GetShaderStage(VkShaderStageFlagBits stage);
};

class BufferResource : public ResourceBase
{
public:
    VkBuffer Buffer = VK_NULL_HANDLE;
    VkDeviceMemory Memory = VK_NULL_HANDLE;
    VkDeviceSize Size = 0;

public:
    ~BufferResource();

public:
    VkResult Create(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags memoryProperties);
    void Cleanup();

    void* Map(VkDeviceSize size) const;
    void Unmap() const;

    bool CopyToBufferUsingMapUnmap(const void* memoryToCopyFrom, VkDeviceSize size) const;
};


class Application
{
protected:
    static Application* _applicationInstance;

    std::wstring _appName;
    Settings _settings;
    std::wstring _basePath;
    WindowInfo _windowInfo = { };
    uint32_t _actualWindowWidth = 0;
    uint32_t _actualWindowHeight = 0;
    VkSurfaceFormatKHR _surfaceFormat = { };
    VkInstance _instance = VK_NULL_HANDLE;
    VkPhysicalDevice _physicalDevice = VK_NULL_HANDLE;
    QueuesInfo _queuesInfo = { };
    VkDevice _device = VK_NULL_HANDLE;
    VkDebugReportCallbackEXT _debugReportCallback = VK_NULL_HANDLE;
    VkSurfaceKHR _surface = VK_NULL_HANDLE;
    VkSwapchainKHR _swapchain = VK_NULL_HANDLE;
    std::vector<VkImage> _swapchainImages;
    std::vector<VkImageView> _swapchainImageViews;
    PFN_vkAcquireNextImageKHR vkAcquireNextImageKHR = VK_NULL_HANDLE;
    PFN_vkQueuePresentKHR vkQueuePresentKHR = VK_NULL_HANDLE;
    ImageResource _offsreenImageResource;
    VkCommandPool _commandPool = VK_NULL_HANDLE;
    std::vector<VkCommandBuffer> _commandBuffers;
    VkSemaphore _imageAcquiredSemaphore = VK_NULL_HANDLE;
    VkSemaphore _renderFinishedSemaphore = VK_NULL_HANDLE;
    std::vector<VkFence> _frameReadinessFences;
    uint32_t _bufferedFrameMaxNum = 0;

protected:
    Application();

public:
    virtual ~Application();

public:
    static Application* GetInstance();
    void Run();
    void HandleMessages(MsgInfo* info);

protected:
    void Initialize();
    void Loop();
    void Shutdown();
    void InitCommon();
    void CreateApplicationWindow();
    void GetSettings();
    void CreateInstance();
    void FindDeviceAndQueues();
    void PostCreateDevice();
    void CreateDebugReportCallback();
    void CreateSurface();
    void CreateSwapchain();
    void CreateFences();
    void CreateOffsreenBuffers();
    void CreateCommandPool();
    void CreateCommandBuffers();
    void CreateSynchronization();
    void ImageBarrier(VkCommandBuffer commandBuffer, VkImage image, VkImageSubresourceRange& subresourceRange,
        VkAccessFlags srcAccessMask, VkAccessFlags dstAccessMask, VkImageLayout oldLayout, VkImageLayout newLayout);
    void FillCommandBuffers();
    void DrawFrame();

    // ============================================================
    // Inherited application class can override the following methods
    // ============================================================

    virtual void CreateDevice();
    virtual void Init();
    virtual void RecordCommandBufferForFrame(VkCommandBuffer commandBuffer, uint32_t frameIndex);
    virtual void UpdateDataForFrame(uint32_t frameIndex);
};

template <class T>
void RunApplication()
{
    std::shared_ptr<T> application(new T());
    application->Run();
}

//#define NVVK_FORCE_VALIDATION
//#define NVVK_DISABLE_VSYNC

#define NVVK_RESOLVE_INSTANCE_FUNCTION_ADDRESS(instance, funcName) \
    { \
        funcName = reinterpret_cast<PFN_##funcName>(vkGetInstanceProcAddr(instance, ""#funcName)); \
        if (funcName == nullptr) \
        { \
            const std::string name = #funcName; \
            ExitError(std::wstring(L"Can't get instance function address: ") + std::wstring(name.begin(), name.end())); \
        } \
    }

#define NVVK_RESOLVE_DEVICE_FUNCTION_ADDRESS(device, funcName) \
    { \
        funcName = reinterpret_cast<PFN_##funcName>(vkGetDeviceProcAddr(device, ""#funcName)); \
        if (funcName == nullptr) \
        { \
            const std::string name = #funcName; \
            ExitError(std::wstring(L"Can't get device function address: ") + std::wstring(name.begin(), name.end())); \
        } \
    }

#define NVVK_CHECK_ERROR(code, message) \
    { \
        if (code != VK_SUCCESS) \
        { \
            ExitError(message + std::wstring(L" ErrorCode: ") + ToString(code)); \
        } \
    }

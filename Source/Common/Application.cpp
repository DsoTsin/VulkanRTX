#include "Application.h"
#define STB_IMAGE_IMPLEMENTATION
#include "stb\stb_image.h"

std::wstring ToString(VkResult value)
{
    return std::to_wstring(value);
}

void LogError(const std::wstring& message, bool silent)
{
    if (!silent)
    {
        MessageBox(nullptr, message.c_str(), L"Error", MB_OK | MB_ICONERROR);
    }
    std::cerr << message.c_str() << "\n";
}

void ExitError(const std::wstring& message, bool silent)
{
    LogError(message, silent);
    exit(1);
}


VkPhysicalDevice ResourceBase::_physicalDevice;
VkDevice ResourceBase::_device;
VkPhysicalDeviceMemoryProperties ResourceBase::_physicalDeviceMemoryProperties;
VkCommandPool ResourceBase::_commandPool;
VkQueue ResourceBase::_transferQueue;

std::wstring ShaderResource::_folderPath;
std::wstring ImageResource::_folderPath;

Application* Application::_applicationInstance = nullptr;


Application::Application() : _appName(L"Vk tutorial")
{
    _applicationInstance = this;
}

Application::~Application()
{
    if (_renderFinishedSemaphore)
    {
        vkDestroySemaphore(_device, _renderFinishedSemaphore, nullptr);
    }
    if (_imageAcquiredSemaphore)
    {
        vkDestroySemaphore(_device, _imageAcquiredSemaphore, nullptr);
    }
    vkFreeCommandBuffers(_device, _commandPool, (uint32_t)_commandBuffers.size(), (VkCommandBuffer*)_commandBuffers.data());
    if (_commandPool)
    {
        vkDestroyCommandPool(_device, _commandPool, nullptr);
    }
    _offsreenImageResource.Cleanup();

    for (auto& fence : _frameReadinessFences)
    {
        vkDestroyFence(_device, fence, nullptr);
    }
    _frameReadinessFences.clear();

    for (auto& imageView : _swapchainImageViews)
    {
        if (imageView)
        {
            vkDestroyImageView(_device, imageView, nullptr);
        }
    }
    if (_swapchain)
    {
        PFN_vkDestroySwapchainKHR vkDestroySwapchainKHR;
        NVVK_RESOLVE_DEVICE_FUNCTION_ADDRESS(_device, vkDestroySwapchainKHR);
        vkDestroySwapchainKHR(_device, _swapchain, nullptr);
    }
    if (_surface)
    {
        vkDestroySurfaceKHR(_instance, _surface, nullptr);
    }
    if (_debugReportCallback)
    {
        PFN_vkDestroyDebugReportCallbackEXT vkDestroyDebugReportCallbackEXT;
        NVVK_RESOLVE_INSTANCE_FUNCTION_ADDRESS(_instance, vkDestroyDebugReportCallbackEXT);
        vkDestroyDebugReportCallbackEXT(_instance, _debugReportCallback, nullptr);
    }
    if (_device)
    {
        vkDestroyDevice(_device, nullptr);
    }
    if (_instance)
    {
        vkDestroyInstance(_instance, nullptr);
    }
}

Application* Application::GetInstance()
{
    return _applicationInstance;
}

void Application::Run()
{
    Initialize();
    Loop();
    Shutdown();
}

void Application::HandleMessages(MsgInfo* info)
{
    switch (info->uMsg)
    {
    case WM_CLOSE:
        DestroyWindow(info->hWnd);
        PostQuitMessage(0);
        break;

    case WM_KEYDOWN:
        switch (info->wParam)
        {
        case VK_ESCAPE:
            PostQuitMessage(0);
            break;
        }
        break;
    }
}

VKAPI_ATTR VkBool32 VKAPI_CALL MessageCallback(VkDebugReportFlagsEXT flags, VkDebugReportObjectTypeEXT objType,
    uint64_t srcObject, size_t location, int32_t msgCode, const char* pLayerPrefix, const char* pMsg, void* pUserData)
{
    std::stringstream debugMessage;
    
    std::string flagText = "";
    if (flags & VK_DEBUG_REPORT_INFORMATION_BIT_EXT)
    {
        flagText = "Info";
    }
    if (flags & VK_DEBUG_REPORT_WARNING_BIT_EXT)
    {
        flagText = "Warning";
    }
    if (flags & VK_DEBUG_REPORT_PERFORMANCE_WARNING_BIT_EXT)
    {
        flagText = "PerfWarning";
    }
    if (flags & VK_DEBUG_REPORT_ERROR_BIT_EXT)
    {
        flagText = "Error";
    }
    if (flags & VK_DEBUG_REPORT_DEBUG_BIT_EXT)
    {
        flagText = "Debug";
    }

    debugMessage << flagText << " [" << pLayerPrefix << "] Code " << msgCode << " : " << pMsg;
    std::cout << debugMessage.str() << "\n";
    fflush(stdout);
    return VK_FALSE;
}

LRESULT CALLBACK WndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    Application* application = Application::GetInstance();
    if (application != nullptr)
    {
        MsgInfo info{ hWnd, uMsg, wParam, lParam };
        application->HandleMessages(&info);
    }
    return (DefWindowProc(hWnd, uMsg, wParam, lParam));
}

void Application::Initialize()
{
    InitCommon();
    CreateApplicationWindow();
    GetSettings();
    CreateInstance();
    CreateDebugReportCallback();
    FindDeviceAndQueues();
    CreateDevice();
    PostCreateDevice();
    CreateSurface();
    CreateSwapchain();
    CreateFences();
    CreateCommandPool();
    ResourceBase::Init(_physicalDevice, _device, _commandPool, _queuesInfo.Graphics.Queue);
    CreateOffsreenBuffers();
    CreateCommandBuffers();
    CreateSynchronization();

    Init(); // finally call user initialize code

    FillCommandBuffers();

    NVVK_RESOLVE_DEVICE_FUNCTION_ADDRESS(_device, vkAcquireNextImageKHR);
    NVVK_RESOLVE_DEVICE_FUNCTION_ADDRESS(_device, vkQueuePresentKHR);
}

void Application::Loop()
{
    MSG msg;
    bool quitMessageReceived = false;
    while (!quitMessageReceived)
    {
        while (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE))
        {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
            if (msg.message == WM_QUIT)
            {
                quitMessageReceived = true;
                break;
            }
        }
        if (!quitMessageReceived)
        {
            DrawFrame();
        }
    }
}

void Application::Shutdown()
{
    vkDeviceWaitIdle(_device);
}

void Application::InitCommon()
{
    TCHAR dest[MAX_PATH];
    const DWORD length = GetModuleFileName(nullptr, dest, MAX_PATH);
    PathRemoveFileSpec(dest);

    _basePath = std::wstring(dest);
    ShaderResource::SetFolderPath(_basePath + L"/Assets/Shaders/");
    ImageResource::SetFolderPath(_basePath + L"/Assets/Textures/");
}

void Application::CreateApplicationWindow()
{
    _windowInfo.WindowInstance = GetModuleHandle(0);

    WNDCLASSEX wndClass;
    wndClass.cbSize = sizeof(WNDCLASSEX);
    wndClass.style = CS_HREDRAW | CS_VREDRAW;
    wndClass.lpfnWndProc = WndProc;
    wndClass.cbClsExtra = 0;
    wndClass.cbWndExtra = 0;
    wndClass.hInstance = _windowInfo.WindowInstance;
    wndClass.hIcon = LoadIcon(nullptr, IDI_APPLICATION);
    wndClass.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wndClass.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);
    wndClass.lpszMenuName = nullptr;
    wndClass.lpszClassName = _appName.c_str();
    wndClass.hIconSm = LoadIcon(nullptr, IDI_WINLOGO);

    if (!RegisterClassEx(&wndClass))
    {
        ExitError(L"Failed RegisterClassEx");
    }

    const uint32_t screenWidth = (uint32_t)GetSystemMetrics(SM_CXSCREEN);
    const uint32_t screenHeight = (uint32_t)GetSystemMetrics(SM_CYSCREEN);

    _actualWindowWidth = _settings.DesiredWindowWidth;
    _actualWindowHeight = _settings.DesiredWindowHeight;

    const DWORD exStyle = WS_EX_APPWINDOW | WS_EX_WINDOWEDGE;
    const DWORD style = WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_CLIPSIBLINGS | WS_CLIPCHILDREN;

    RECT windowRect;
    windowRect.left = 0;
    windowRect.top = 0;
    windowRect.right = _actualWindowWidth;
    windowRect.bottom = _actualWindowHeight;
    AdjustWindowRectEx(&windowRect, style, FALSE, exStyle);

    _windowInfo.Window = CreateWindowEx(0,
        _appName.c_str(),
        _appName.c_str(),
        style | WS_CLIPSIBLINGS | WS_CLIPCHILDREN,
        0,
        0,
        windowRect.right - windowRect.left,
        windowRect.bottom - windowRect.top,
        nullptr,
        nullptr,
        _windowInfo.WindowInstance,
        nullptr);

    if (!_windowInfo.Window)
    {
        ExitError(L"Failed to create window");
    }

    const uint32_t x = (screenWidth - windowRect.right) / 2;
    const uint32_t y = (screenHeight - windowRect.bottom) / 2;
    SetWindowPos(_windowInfo.Window, 0, x, y, 0, 0, SWP_NOZORDER | SWP_NOSIZE);

    ShowWindow(_windowInfo.Window, SW_SHOW);
    SetForegroundWindow(_windowInfo.Window);
    SetFocus(_windowInfo.Window);
}

void Application::GetSettings()
{
#ifdef NVVK_FORCE_VALIDATION
    _settings.ValidationEnabled = true;
#endif
}

void Application::CreateInstance()
{
    const std::string appName(_appName.begin(), _appName.end());

    VkApplicationInfo applicationInfo;
    applicationInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    applicationInfo.pNext = nullptr;
    applicationInfo.pApplicationName = appName.c_str();
    applicationInfo.applicationVersion = 1;
    applicationInfo.pEngineName = nullptr;
    applicationInfo.engineVersion = 0;
    applicationInfo.apiVersion = VK_API_VERSION_1_1;

    std::vector<const char*> enabledExtensions = { VK_KHR_SURFACE_EXTENSION_NAME, VK_KHR_WIN32_SURFACE_EXTENSION_NAME };
    if (_settings.ValidationEnabled)
    {
        enabledExtensions.push_back(VK_EXT_DEBUG_REPORT_EXTENSION_NAME);
    }

    std::vector<const char*> enabledLayers;
    if (_settings.ValidationEnabled)
    {
        enabledLayers.push_back("VK_LAYER_LUNARG_standard_validation");
    }

    VkInstanceCreateInfo instanceCreateInfo;
    instanceCreateInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    instanceCreateInfo.pNext = nullptr;
    instanceCreateInfo.pApplicationInfo = &applicationInfo;
    instanceCreateInfo.enabledLayerCount = (uint32_t)enabledLayers.size();
    instanceCreateInfo.ppEnabledLayerNames = enabledLayers.data();
    instanceCreateInfo.enabledExtensionCount = (uint32_t)enabledExtensions.size();
    instanceCreateInfo.ppEnabledExtensionNames = enabledExtensions.data();
    instanceCreateInfo.flags = 0;

    const VkResult code = vkCreateInstance(&instanceCreateInfo, nullptr, &_instance);
    NVVK_CHECK_ERROR(code, L"vkCreateInstance");
}

void Application::FindDeviceAndQueues()
{
    uint32_t physicalDeviceCount;
    vkEnumeratePhysicalDevices(_instance, &physicalDeviceCount, nullptr);
    std::vector<VkPhysicalDevice> physicalDevices(physicalDeviceCount);
    vkEnumeratePhysicalDevices(_instance, &physicalDeviceCount, physicalDevices.data());

    if (physicalDevices.empty())
    {
        ExitError(L"No physical device found");
    }
    _physicalDevice = physicalDevices[0];

    auto getQueueFamilyIndex = [&](VkQueueFlags queueFlag)
    {
        uint32_t queueFamilyPropertyCount;
        vkGetPhysicalDeviceQueueFamilyProperties(_physicalDevice, &queueFamilyPropertyCount, nullptr);
        std::vector<VkQueueFamilyProperties> queueFamilyProperties(queueFamilyPropertyCount);
        vkGetPhysicalDeviceQueueFamilyProperties(_physicalDevice, &queueFamilyPropertyCount, queueFamilyProperties.data());

        if (queueFlag & VK_QUEUE_COMPUTE_BIT)
        {
            for (uint32_t i = 0; i < queueFamilyPropertyCount; ++i)
            {
                if ((queueFamilyProperties[i].queueFlags & VK_QUEUE_COMPUTE_BIT) &&
                    !(queueFamilyProperties[i].queueFlags & VK_QUEUE_GRAPHICS_BIT))
                {
                    return i;
                }
            }
        }

        if (queueFlag & VK_QUEUE_TRANSFER_BIT)
        {
            for (uint32_t i = 0; i < queueFamilyPropertyCount; ++i)
            {
                if ((queueFamilyProperties[i].queueFlags & VK_QUEUE_TRANSFER_BIT) &&
                    !(queueFamilyProperties[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) &&
                    !(queueFamilyProperties[i].queueFlags & VK_QUEUE_COMPUTE_BIT))
                {
                    return i;
                }
            }
        }

        for (uint32_t i = 0; i < queueFamilyPropertyCount; ++i)
        {
            if (queueFamilyProperties[i].queueFlags & queueFlag)
            {
                return i;
            }
        }

        return 0u;
    };

    _queuesInfo.Graphics.QueueFamilyIndex = getQueueFamilyIndex(VK_QUEUE_GRAPHICS_BIT);
    _queuesInfo.Compute.QueueFamilyIndex = getQueueFamilyIndex(VK_QUEUE_COMPUTE_BIT);
    _queuesInfo.Transfer.QueueFamilyIndex = getQueueFamilyIndex(VK_QUEUE_TRANSFER_BIT);
}

void Application::CreateDevice()
{
    std::vector<VkDeviceQueueCreateInfo> deviceQueueCreateInfos;
    const float priority = 0.0f;

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

    const std::vector<const char*> deviceExtensions({ VK_KHR_SWAPCHAIN_EXTENSION_NAME });
    const VkPhysicalDeviceFeatures features = { };

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

    const VkResult code = vkCreateDevice(_physicalDevice, &deviceCreateInfo, nullptr, &_device);
    NVVK_CHECK_ERROR(code, L"vkCreateDevice");
}

void Application::PostCreateDevice()
{
    vkGetDeviceQueue(_device, _queuesInfo.Graphics.QueueFamilyIndex, 0, &_queuesInfo.Graphics.Queue);
    vkGetDeviceQueue(_device, _queuesInfo.Compute.QueueFamilyIndex, 0, &_queuesInfo.Compute.Queue);
    vkGetDeviceQueue(_device, _queuesInfo.Transfer.QueueFamilyIndex, 0, &_queuesInfo.Transfer.Queue);
}

void Application::CreateDebugReportCallback()
{
    if (!_settings.ValidationEnabled)
    {
        return;
    }

    const VkDebugReportFlagsEXT flags = VK_DEBUG_REPORT_WARNING_BIT_EXT | VK_DEBUG_REPORT_ERROR_BIT_EXT |
        VK_DEBUG_REPORT_PERFORMANCE_WARNING_BIT_EXT;

    VkDebugReportCallbackCreateInfoEXT info;
    info.sType = VK_STRUCTURE_TYPE_DEBUG_REPORT_CALLBACK_CREATE_INFO_EXT;
    info.pNext = nullptr;
    info.flags = flags;
    info.pfnCallback = MessageCallback;
    info.pUserData = nullptr;

    PFN_vkCreateDebugReportCallbackEXT vkCreateDebugReportCallbackEXT;
    NVVK_RESOLVE_INSTANCE_FUNCTION_ADDRESS(_instance, vkCreateDebugReportCallbackEXT);

    const VkResult code = vkCreateDebugReportCallbackEXT(_instance, &info, nullptr, &_debugReportCallback);
    NVVK_CHECK_ERROR(code, L"vkCreateDebugReportCallbackEXT");
}

void Application::CreateSurface()
{
    VkWin32SurfaceCreateInfoKHR surfaceCreateInfo;
    surfaceCreateInfo.sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR;
    surfaceCreateInfo.pNext = nullptr;
    surfaceCreateInfo.flags = 0;
    surfaceCreateInfo.hinstance = _windowInfo.WindowInstance;
    surfaceCreateInfo.hwnd = _windowInfo.Window;

    VkResult code = vkCreateWin32SurfaceKHR(_instance, &surfaceCreateInfo, nullptr, &_surface);
    NVVK_CHECK_ERROR(code, L"vkCreateWin32SurfaceKHR");

    PFN_vkGetPhysicalDeviceSurfaceSupportKHR vkGetPhysicalDeviceSurfaceSupportKHR;
    NVVK_RESOLVE_INSTANCE_FUNCTION_ADDRESS(_instance, vkGetPhysicalDeviceSurfaceSupportKHR);

    VkBool32 supportPresent = VK_FALSE;
    code = vkGetPhysicalDeviceSurfaceSupportKHR(_physicalDevice, _queuesInfo.Graphics.QueueFamilyIndex, _surface, &supportPresent);
    NVVK_CHECK_ERROR(code, L"vkGetPhysicalDeviceSurfaceSupportKHR");

    if (!supportPresent)
    {
        ExitError(L"Graphics queue does not support presenting");
    }

    PFN_vkGetPhysicalDeviceSurfaceFormatsKHR vkGetPhysicalDeviceSurfaceFormatsKHR;
    NVVK_RESOLVE_INSTANCE_FUNCTION_ADDRESS(_instance, vkGetPhysicalDeviceSurfaceFormatsKHR);

    uint32_t formatCount;
    vkGetPhysicalDeviceSurfaceFormatsKHR(_physicalDevice, _surface, &formatCount, nullptr);
    std::vector<VkSurfaceFormatKHR> surfaceFormats(formatCount);
    vkGetPhysicalDeviceSurfaceFormatsKHR(_physicalDevice, _surface, &formatCount, surfaceFormats.data());

    if (formatCount == 1 && surfaceFormats[0].format == VK_FORMAT_UNDEFINED)
    {
        _surfaceFormat.format = _settings.DesiredSurfaceFormat;
        _surfaceFormat.colorSpace = surfaceFormats[0].colorSpace;
    }
    else
    {
        bool found = false;
        for (auto& surfaceFormat : surfaceFormats)
        {
            if (surfaceFormat.format == _settings.DesiredSurfaceFormat)
            {
                _surfaceFormat = surfaceFormat;
                found = true;
                break;
            }
        }
        if (!found)
        {
            _surfaceFormat = surfaceFormats[0];
        }
    }
}

void Application::CreateSwapchain()
{
    PFN_vkGetPhysicalDeviceSurfaceCapabilitiesKHR vkGetPhysicalDeviceSurfaceCapabilitiesKHR;
    NVVK_RESOLVE_INSTANCE_FUNCTION_ADDRESS(_instance, vkGetPhysicalDeviceSurfaceCapabilitiesKHR);

    VkSurfaceCapabilitiesKHR surfaceCapabilities;
    VkResult code = vkGetPhysicalDeviceSurfaceCapabilitiesKHR(_physicalDevice, _surface, &surfaceCapabilities);
    NVVK_CHECK_ERROR(code, L"vkGetPhysicalDeviceSurfaceCapabilitiesKHR");

    PFN_vkGetPhysicalDeviceSurfacePresentModesKHR vkGetPhysicalDeviceSurfacePresentModesKHR;
    NVVK_RESOLVE_INSTANCE_FUNCTION_ADDRESS(_instance, vkGetPhysicalDeviceSurfacePresentModesKHR);

    uint32_t presentModeCount;
    vkGetPhysicalDeviceSurfacePresentModesKHR(_physicalDevice, _surface, &presentModeCount, nullptr);
    std::vector<VkPresentModeKHR> presentModes(presentModeCount);
    vkGetPhysicalDeviceSurfacePresentModesKHR(_physicalDevice, _surface, &presentModeCount, presentModes.data());

    bool isMailboxModeSupported = std::find(presentModes.begin(), presentModes.end(), VK_PRESENT_MODE_MAILBOX_KHR) != presentModes.end();
#ifndef NVVK_DISABLE_VSYNC
    isMailboxModeSupported = false;
#endif
    VkPresentModeKHR presentMode = isMailboxModeSupported ? VK_PRESENT_MODE_MAILBOX_KHR : VK_PRESENT_MODE_FIFO_KHR;

    const VkSwapchainKHR prevSwapchain = _swapchain;

    VkSwapchainCreateInfoKHR swapchainCreateInfo;
    swapchainCreateInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    swapchainCreateInfo.pNext = nullptr;
    swapchainCreateInfo.flags = 0;
    swapchainCreateInfo.surface = _surface;
    swapchainCreateInfo.minImageCount = surfaceCapabilities.minImageCount;
    swapchainCreateInfo.imageFormat = _surfaceFormat.format;
    swapchainCreateInfo.imageColorSpace = _surfaceFormat.colorSpace;
    swapchainCreateInfo.imageExtent = { _actualWindowWidth, _actualWindowHeight };
    swapchainCreateInfo.imageArrayLayers = 1;
    swapchainCreateInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    swapchainCreateInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    swapchainCreateInfo.queueFamilyIndexCount = 0;
    swapchainCreateInfo.pQueueFamilyIndices = nullptr;
    swapchainCreateInfo.preTransform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
    swapchainCreateInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    swapchainCreateInfo.presentMode = presentMode;
    swapchainCreateInfo.clipped = VK_TRUE;
    swapchainCreateInfo.oldSwapchain = prevSwapchain;

    PFN_vkCreateSwapchainKHR vkCreateSwapchainKHR;
    NVVK_RESOLVE_DEVICE_FUNCTION_ADDRESS(_device, vkCreateSwapchainKHR);

    code = vkCreateSwapchainKHR(_device, &swapchainCreateInfo, nullptr, &_swapchain);
    NVVK_CHECK_ERROR(code, L"vkCreateSwapchainKHR");

    if (prevSwapchain)
    {
        for (uint32_t i = 0, len = (uint32_t)_swapchainImageViews.size(); i < len; ++i)
        {
            vkDestroyImageView(_device, _swapchainImageViews[i], nullptr);
            _swapchainImageViews[i] = VK_NULL_HANDLE;
        }
        vkDestroySwapchainKHR(_device, prevSwapchain, nullptr);
    }

    PFN_vkGetSwapchainImagesKHR vkGetSwapchainImagesKHR;
    NVVK_RESOLVE_DEVICE_FUNCTION_ADDRESS(_device, vkGetSwapchainImagesKHR);

    uint32_t imageCount;
    vkGetSwapchainImagesKHR(_device, _swapchain, &imageCount, nullptr);
    _swapchainImages.resize(imageCount);
    vkGetSwapchainImagesKHR(_device, _swapchain, &imageCount, _swapchainImages.data());

    _swapchainImageViews.resize(imageCount);
    for (uint32_t i = 0; i < imageCount; i++)
    {
        VkImageViewCreateInfo imageViewCreateInfo;
        imageViewCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        imageViewCreateInfo.pNext = nullptr;
        imageViewCreateInfo.format = _surfaceFormat.format;
        imageViewCreateInfo.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
        imageViewCreateInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        imageViewCreateInfo.image = _swapchainImages[i];
        imageViewCreateInfo.flags = 0;
        imageViewCreateInfo.components = { };

        code = vkCreateImageView(_device, &imageViewCreateInfo, nullptr, &_swapchainImageViews[i]);
        NVVK_CHECK_ERROR(code, L"vkCreateImageView " + std::to_wstring(i))
    }
}

void Application::CreateFences()
{
    VkFenceCreateInfo fenceCreateInfo;
    fenceCreateInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fenceCreateInfo.pNext = nullptr;
    fenceCreateInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

    _frameReadinessFences.resize(_swapchainImageViews.size());
    for (auto& fence : _frameReadinessFences)
        vkCreateFence(_device, &fenceCreateInfo, nullptr, &fence);

    _bufferedFrameMaxNum = static_cast<uint32_t>(_frameReadinessFences.size());
}

void Application::CreateOffsreenBuffers()
{
    const VkImageUsageFlags usageFlags = VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;

    VkResult code = _offsreenImageResource.CreateImage(VK_IMAGE_TYPE_2D, _surfaceFormat.format,
        { _actualWindowWidth, _actualWindowHeight, 1 },
        VK_IMAGE_TILING_OPTIMAL, usageFlags, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    NVVK_CHECK_ERROR(code, L"_offsreenImageResource.CreateImage");

    code = _offsreenImageResource.CreateImageView(VK_IMAGE_VIEW_TYPE_2D, _surfaceFormat.format,
        { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 });

    NVVK_CHECK_ERROR(code, L"_offsreenImageResource.CreateImageView");
}

void Application::CreateCommandPool()
{
    VkCommandPoolCreateInfo commandPoolCreateInfo;
    commandPoolCreateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    commandPoolCreateInfo.pNext = nullptr;
    commandPoolCreateInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    commandPoolCreateInfo.queueFamilyIndex = _queuesInfo.Graphics.QueueFamilyIndex;

    const VkResult code = vkCreateCommandPool(_device, &commandPoolCreateInfo, nullptr, &_commandPool);
    NVVK_CHECK_ERROR(code, L"vkCreateCommandPool");
}

void Application::CreateCommandBuffers()
{
    _commandBuffers.resize(_swapchainImages.size());

    VkCommandBufferAllocateInfo commandBufferAllocateInfo;
    commandBufferAllocateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    commandBufferAllocateInfo.pNext = nullptr;
    commandBufferAllocateInfo.commandPool = _commandPool;
    commandBufferAllocateInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    commandBufferAllocateInfo.commandBufferCount = (uint32_t)_swapchainImages.size();

    const VkResult code = vkAllocateCommandBuffers(_device, &commandBufferAllocateInfo, _commandBuffers.data());
    NVVK_CHECK_ERROR(code, L"vkAllocateCommandBuffers");
}

void Application::CreateSynchronization()
{
    VkSemaphoreCreateInfo semaphoreCreatInfo;
    semaphoreCreatInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
    semaphoreCreatInfo.pNext = nullptr;
    semaphoreCreatInfo.flags = 0;

    VkResult code = vkCreateSemaphore(_device, &semaphoreCreatInfo, nullptr, &_imageAcquiredSemaphore);
    NVVK_CHECK_ERROR(code, L"vkCreateSemaphore");

    code = vkCreateSemaphore(_device, &semaphoreCreatInfo, nullptr, &_renderFinishedSemaphore);
    NVVK_CHECK_ERROR(code, L"vkCreateSemaphore");
}

void Application::ImageBarrier(VkCommandBuffer commandBuffer, VkImage image, VkImageSubresourceRange& subresourceRange,
    VkAccessFlags srcAccessMask, VkAccessFlags dstAccessMask, VkImageLayout oldLayout, VkImageLayout newLayout)
{
    VkImageMemoryBarrier imageMemoryBarrier;
    imageMemoryBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    imageMemoryBarrier.pNext = nullptr;
    imageMemoryBarrier.srcAccessMask = srcAccessMask;
    imageMemoryBarrier.dstAccessMask = dstAccessMask;
    imageMemoryBarrier.oldLayout = oldLayout;
    imageMemoryBarrier.newLayout = newLayout;
    imageMemoryBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    imageMemoryBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    imageMemoryBarrier.image = image;
    imageMemoryBarrier.subresourceRange = subresourceRange;

    vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
        0, 0, nullptr, 0, nullptr, 1, &imageMemoryBarrier);
}

void Application::FillCommandBuffers()
{
    VkCommandBufferBeginInfo commandBufferBeginInfo;
    commandBufferBeginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    commandBufferBeginInfo.pNext = nullptr;
    commandBufferBeginInfo.flags = 0;
    commandBufferBeginInfo.pInheritanceInfo = nullptr;

    VkImageSubresourceRange subresourceRange;
    subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    subresourceRange.baseMipLevel = 0;
    subresourceRange.levelCount = 1;
    subresourceRange.baseArrayLayer = 0;
    subresourceRange.layerCount = 1;

    for (uint32_t i = 0; i < _commandBuffers.size(); i++)
    {
        const VkCommandBuffer commandBuffer = _commandBuffers[i];

        VkResult code = vkBeginCommandBuffer(commandBuffer, &commandBufferBeginInfo);
        NVVK_CHECK_ERROR(code, L"vkBeginCommandBuffer");

        ImageBarrier(commandBuffer, _offsreenImageResource.Image, subresourceRange,
            0, VK_ACCESS_SHADER_WRITE_BIT, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL);

        RecordCommandBufferForFrame(commandBuffer, i); // user draw code

        ImageBarrier(commandBuffer, _swapchainImages[i], subresourceRange,
            0, VK_ACCESS_TRANSFER_WRITE_BIT, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

        ImageBarrier(commandBuffer, _offsreenImageResource.Image, subresourceRange,
            VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);

        VkImageCopy copyRegion;
        copyRegion.srcSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 };
        copyRegion.srcOffset = { 0, 0, 0 };
        copyRegion.dstSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 };
        copyRegion.dstOffset = { 0, 0, 0 };
        copyRegion.extent = { _actualWindowWidth, _actualWindowHeight, 1 };
        vkCmdCopyImage(commandBuffer, _offsreenImageResource.Image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
            _swapchainImages[i], VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copyRegion);

        ImageBarrier(commandBuffer, _swapchainImages[i], subresourceRange,
            VK_ACCESS_TRANSFER_WRITE_BIT, 0, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);

        code = vkEndCommandBuffer(commandBuffer);
        NVVK_CHECK_ERROR(code, L"vkEndCommandBuffer");
    }
}

void Application::DrawFrame()
{
    uint32_t imageIndex;
    VkResult code = vkAcquireNextImageKHR(_device, _swapchain, UINT64_MAX, _imageAcquiredSemaphore, nullptr, &imageIndex);
    NVVK_CHECK_ERROR(code, L"Failed to acquire next image");

    const VkFence fence = _frameReadinessFences[imageIndex];
    code = vkWaitForFences(_device, 1, &fence, VK_TRUE, UINT64_MAX);
    NVVK_CHECK_ERROR(code, L"Failed to wait for fence");
    vkResetFences(_device, 1, &fence);

    UpdateDataForFrame(imageIndex);

    const VkPipelineStageFlags waitStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;

    VkSubmitInfo submitInfo;
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.pNext = nullptr;
    submitInfo.waitSemaphoreCount = 1;
    submitInfo.pWaitSemaphores = &_imageAcquiredSemaphore;
    submitInfo.pWaitDstStageMask = &waitStageMask;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &_commandBuffers[imageIndex];
    submitInfo.signalSemaphoreCount = 1;
    submitInfo.pSignalSemaphores = &_renderFinishedSemaphore;

    code = vkQueueSubmit(_queuesInfo.Graphics.Queue, 1, &submitInfo, fence);
    NVVK_CHECK_ERROR(code, L"vkQueueSubmit");

    VkPresentInfoKHR presentInfo;
    presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    presentInfo.pNext = nullptr;
    presentInfo.waitSemaphoreCount = 1;
    presentInfo.pWaitSemaphores = &_renderFinishedSemaphore;
    presentInfo.swapchainCount = 1;
    presentInfo.pSwapchains = &_swapchain;
    presentInfo.pImageIndices = &imageIndex;
    presentInfo.pResults = nullptr;

    code = vkQueuePresentKHR(_queuesInfo.Graphics.Queue, &presentInfo);
    NVVK_CHECK_ERROR(code, L"vkQueuePresentKHR");
}


void Application::Init()
{
}

void Application::RecordCommandBufferForFrame(VkCommandBuffer commandBuffer, uint32_t frameIndex)
{
}

void Application::UpdateDataForFrame(uint32_t frameIndex)
{
}

// ============================================================
// Resource base
// ============================================================

void ResourceBase::Init(VkPhysicalDevice physicalDevice, VkDevice device, VkCommandPool commandPool, VkQueue transferQueue)
{
    _physicalDevice = physicalDevice;
    _device = device;
    vkGetPhysicalDeviceMemoryProperties(_physicalDevice, &_physicalDeviceMemoryProperties);
    _commandPool = commandPool;
    _transferQueue = transferQueue;
}

uint32_t ResourceBase::GetMemoryType(VkMemoryRequirements& memoryRequiriments, VkMemoryPropertyFlags memoryProperties)
{
    uint32_t result = 0;
    for (uint32_t memoryTypeIndex = 0; memoryTypeIndex < VK_MAX_MEMORY_TYPES; ++memoryTypeIndex)
    {
        if (memoryRequiriments.memoryTypeBits & (1 << memoryTypeIndex))
        {
            if ((_physicalDeviceMemoryProperties.memoryTypes[memoryTypeIndex].propertyFlags & memoryProperties) == memoryProperties)
            {
                result = memoryTypeIndex;
                break;
            }
        }
    }
    return result;
}

// ============================================================
// Image resource
// ============================================================

ImageResource::~ImageResource()
{
    Cleanup();
}

void ImageResource::SetFolderPath(const std::wstring& folderPath)
{
    _folderPath = folderPath;
}

VkResult ImageResource::CreateImage(VkImageType imageType, VkFormat format, VkExtent3D extent, VkImageTiling tiling,
    VkImageUsageFlags usage, VkMemoryPropertyFlags memoryProperties)
{
    VkImageCreateInfo imageCreateInfo;
    imageCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageCreateInfo.pNext = nullptr;
    imageCreateInfo.flags = 0;
    imageCreateInfo.imageType = imageType;
    imageCreateInfo.format = format;
    imageCreateInfo.extent = extent;
    imageCreateInfo.mipLevels = 1;
    imageCreateInfo.arrayLayers = 1;
    imageCreateInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    imageCreateInfo.tiling = tiling;
    imageCreateInfo.usage = usage;
    imageCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    imageCreateInfo.queueFamilyIndexCount = 0;
    imageCreateInfo.pQueueFamilyIndices = nullptr;
    imageCreateInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

    VkResult code = vkCreateImage(_device, &imageCreateInfo, nullptr, &Image);
    if (code != VK_SUCCESS)
    {
        Image = VK_NULL_HANDLE;
        return code;
    }

    VkMemoryRequirements memoryRequirements;
    vkGetImageMemoryRequirements(_device, Image, &memoryRequirements);

    VkMemoryAllocateInfo memoryAllocateInfo;
    memoryAllocateInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    memoryAllocateInfo.pNext = nullptr;
    memoryAllocateInfo.allocationSize = memoryRequirements.size;
    memoryAllocateInfo.memoryTypeIndex = GetMemoryType(memoryRequirements, memoryProperties);

    code = vkAllocateMemory(_device, &memoryAllocateInfo, nullptr, &Memory);
    if (code != VK_SUCCESS)
    {
        vkDestroyImage(_device, Image, nullptr);
        Image = VK_NULL_HANDLE;
        Memory = VK_NULL_HANDLE;
        return code;
    }

    code = vkBindImageMemory(_device, Image, Memory, 0);
    if (code != VK_SUCCESS)
    {
        vkDestroyImage(_device, Image, nullptr);
        vkFreeMemory(_device, Memory, nullptr);
        Image = VK_NULL_HANDLE;
        Memory = VK_NULL_HANDLE;
        return code;
    }

    return VK_SUCCESS;
}

bool ImageResource::LoadTexture2DFromFile(const std::wstring& fileName, VkResult& code)
{
    code = VK_SUCCESS;

    const std::wstring filePath = _folderPath + fileName;
    FILE *file;
    if (_wfopen_s(&file, filePath.c_str(), L"rb") != 0)
    {
        return false;
    }

    int32_t textureWidth;
    int32_t textureHeight;
    int32_t textureChannels;
    stbi_uc* pixelData = stbi_load_from_file(file, &textureWidth, &textureHeight, &textureChannels, STBI_rgb_alpha);
    if (!pixelData)
    {
        stbi_image_free(pixelData);
        return false;
    }

    VkDeviceSize imageSize = textureWidth * textureHeight * 4;
    BufferResource stagingBuffer;
    code = stagingBuffer.Create(imageSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    if (code != VK_SUCCESS)
    {
        stbi_image_free(pixelData);
        return false;
    }

    if (!stagingBuffer.CopyToBufferUsingMapUnmap(pixelData, imageSize))
    {
        stbi_image_free(pixelData);
        return false;
    }

    stbi_image_free(pixelData);

    VkExtent3D imageExtent { (uint32_t)textureWidth, (uint32_t)textureHeight, 1 };
    code = CreateImage(VK_IMAGE_TYPE_2D, VK_FORMAT_R8G8B8A8_SRGB, imageExtent, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    if (code != VK_SUCCESS)
    {
        return false;
    }

    VkCommandBufferAllocateInfo allocInfo;
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.pNext = nullptr;
    allocInfo.commandPool = _commandPool;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandBufferCount = 1;

    VkCommandBuffer commandBuffer;
    code = vkAllocateCommandBuffers(_device, &allocInfo, &commandBuffer);
    if (code != VK_SUCCESS)
    {
        return false;
    }

    VkCommandBufferBeginInfo beginInfo;
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.pNext = nullptr;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    beginInfo.pInheritanceInfo = nullptr;

    code = vkBeginCommandBuffer(commandBuffer, &beginInfo);
    if (code != VK_SUCCESS)
    {
        vkFreeCommandBuffers(_device, _commandPool, 1, &commandBuffer);
        return false;
    }

    VkImageMemoryBarrier barrier;
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;;
    barrier.pNext = nullptr;
    barrier.srcAccessMask = 0;
    barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = Image;
    barrier.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };

    vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier);

    VkBufferImageCopy region;
    region.bufferOffset = 0;
    region.bufferRowLength = 0;
    region.bufferImageHeight = 0;
    region.imageSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 };
    region.imageOffset = { 0, 0, 0 };
    region.imageExtent = imageExtent;

    vkCmdCopyBufferToImage(commandBuffer, stagingBuffer.Buffer, Image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

    barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier);

    code = vkEndCommandBuffer(commandBuffer);
    if (code != VK_SUCCESS)
    {
        vkFreeCommandBuffers(_device, _commandPool, 1, &commandBuffer);
        return false;
    }

    VkSubmitInfo submitInfo;
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.pNext = nullptr;
    submitInfo.waitSemaphoreCount = 0;
    submitInfo.pWaitSemaphores = nullptr;
    submitInfo.pWaitDstStageMask = nullptr;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &commandBuffer;
    submitInfo.signalSemaphoreCount = 0;
    submitInfo.pSignalSemaphores = nullptr;

    code = vkQueueSubmit(_transferQueue, 1, &submitInfo, VK_NULL_HANDLE);
    if (code != VK_SUCCESS)
    {
        vkFreeCommandBuffers(_device, _commandPool, 1, &commandBuffer);
        return false;
    }

    code = vkQueueWaitIdle(_transferQueue);
    if (code != VK_SUCCESS)
    {
        vkFreeCommandBuffers(_device, _commandPool, 1, &commandBuffer);
        return false;
    }

    vkFreeCommandBuffers(_device, _commandPool, 1, &commandBuffer);

    return true;
}

VkResult ImageResource::CreateImageView(VkImageViewType viewType, VkFormat format, VkImageSubresourceRange subresourceRange)
{
    VkImageViewCreateInfo imageViewCreateInfo;
    imageViewCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    imageViewCreateInfo.pNext = nullptr;
    imageViewCreateInfo.viewType = viewType;
    imageViewCreateInfo.format = format;
    imageViewCreateInfo.subresourceRange = subresourceRange;
    imageViewCreateInfo.image = Image;
    imageViewCreateInfo.flags = 0;
    imageViewCreateInfo.components = { VK_COMPONENT_SWIZZLE_R, VK_COMPONENT_SWIZZLE_G, VK_COMPONENT_SWIZZLE_B, VK_COMPONENT_SWIZZLE_A };

    const VkResult code = vkCreateImageView(_device, &imageViewCreateInfo, nullptr, &ImageView);
    if (code != VK_SUCCESS)
    {
        ImageView = VK_NULL_HANDLE;
    }

    return code;
}

VkResult ImageResource::CreateSampler(VkFilter magFilter, VkFilter minFilter, VkSamplerMipmapMode mipmapMode, VkSamplerAddressMode addressMode)
{
    VkSamplerCreateInfo samplerCreateInfo;
    samplerCreateInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    samplerCreateInfo.pNext = nullptr;
    samplerCreateInfo.flags = 0;
    samplerCreateInfo.magFilter = magFilter;
    samplerCreateInfo.minFilter = minFilter;
    samplerCreateInfo.mipmapMode = mipmapMode;
    samplerCreateInfo.addressModeU = addressMode;
    samplerCreateInfo.addressModeV = addressMode;
    samplerCreateInfo.addressModeW = addressMode;
    samplerCreateInfo.mipLodBias = 0;
    samplerCreateInfo.anisotropyEnable = VK_FALSE;
    samplerCreateInfo.maxAnisotropy = 1;
    samplerCreateInfo.compareEnable = VK_FALSE;
    samplerCreateInfo.compareOp = VK_COMPARE_OP_ALWAYS;
    samplerCreateInfo.minLod = 0;
    samplerCreateInfo.maxLod = 0;
    samplerCreateInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
    samplerCreateInfo.unnormalizedCoordinates = VK_FALSE;

    VkResult code = vkCreateSampler(_device, &samplerCreateInfo, nullptr, &Sampler);
    return code;
}

void ImageResource::Cleanup()
{
    if (ImageView)
    {
        vkDestroyImageView(_device, ImageView, nullptr);
        ImageView = VK_NULL_HANDLE;
    }
    if (Memory)
    {
        vkFreeMemory(_device, Memory, nullptr);
        Memory = VK_NULL_HANDLE;
    }
    if (Image)
    {
        vkDestroyImage(_device, Image, nullptr);
        Image = VK_NULL_HANDLE;
    }
    if (Sampler)
    {
        vkDestroySampler(_device, Sampler, nullptr);
        Sampler = VK_NULL_HANDLE;
    }
}

// ============================================================
// Shader resource
// ============================================================

ShaderResource::~ShaderResource()
{
    Cleanup();
}

void ShaderResource::SetFolderPath(const std::wstring& folderPath)
{
    _folderPath = folderPath;
}

VkResult ShaderResource::LoadFromFile(const std::wstring& fileName, bool& cantOpenFile)
{
    cantOpenFile = false;

    const std::wstring filePath = _folderPath + fileName;
    std::ifstream fileStream(filePath, std::ios::binary | std::ios::in | std::ios::ate);
    if (!fileStream.is_open())
    {
        cantOpenFile = true;
        return VK_SUCCESS;
    }
    const size_t shaderSize = fileStream.tellg();
    fileStream.seekg(0, std::ios::beg);
    std::vector<char> bytecode(shaderSize);
    fileStream.read(bytecode.data(), shaderSize);
    fileStream.close();

    VkShaderModuleCreateInfo shaderModuleCreateInfo;
    shaderModuleCreateInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    shaderModuleCreateInfo.pNext = nullptr;
    shaderModuleCreateInfo.codeSize = shaderSize;
    shaderModuleCreateInfo.pCode = (uint32_t*)bytecode.data();
    shaderModuleCreateInfo.flags = 0;

    const VkResult code = vkCreateShaderModule(_device, &shaderModuleCreateInfo, nullptr, &_module);
    if (code != VK_SUCCESS)
    {
        _module = VK_NULL_HANDLE;
    }

    return code;
}

VkPipelineShaderStageCreateInfo ShaderResource::GetShaderStage(VkShaderStageFlagBits stage)
{
    VkPipelineShaderStageCreateInfo result;
    result.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    result.pNext = nullptr;
    result.stage = stage;
    result.module = _module;
    result.pName = "main";
    result.flags = 0;
    result.pSpecializationInfo = nullptr;
    return result;
}

void ShaderResource::Cleanup()
{
    if (_module)
    {
        vkDestroyShaderModule(_device, _module, nullptr);
        _module = VK_NULL_HANDLE;
    }
}

// ============================================================
// Buffer resource
// ============================================================

BufferResource::~BufferResource()
{
    Cleanup();
}

VkResult BufferResource::Create(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags memoryProperties)
{
    VkBufferCreateInfo bufferCreateInfo;
    bufferCreateInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferCreateInfo.pNext = nullptr;
    bufferCreateInfo.flags = 0;
    bufferCreateInfo.size = size;
    bufferCreateInfo.usage = usage;
    bufferCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    bufferCreateInfo.queueFamilyIndexCount = 0;
    bufferCreateInfo.pQueueFamilyIndices = nullptr;

    Size = size;

    VkResult code = vkCreateBuffer(_device, &bufferCreateInfo, nullptr, &Buffer);
    if (code != VK_SUCCESS)
    {
        Buffer = VK_NULL_HANDLE;
        return code;
    }

    VkMemoryRequirements memoryRequirements;
    vkGetBufferMemoryRequirements(_device, Buffer, &memoryRequirements);

    VkMemoryAllocateInfo memoryAllocateInfo;
    memoryAllocateInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    memoryAllocateInfo.pNext = nullptr;
    memoryAllocateInfo.allocationSize = memoryRequirements.size;
    memoryAllocateInfo.memoryTypeIndex = GetMemoryType(memoryRequirements, memoryProperties);

    code = vkAllocateMemory(_device, &memoryAllocateInfo, nullptr, &Memory);
    if (code != VK_SUCCESS)
    {
        vkDestroyBuffer(_device, Buffer, nullptr);
        Buffer = VK_NULL_HANDLE;
        Memory = VK_NULL_HANDLE;
        return code;
    }

    code = vkBindBufferMemory(_device, Buffer, Memory, 0);
    if (code != VK_SUCCESS)
    {
        vkDestroyBuffer(_device, Buffer, nullptr);
        vkFreeMemory(_device, Memory, nullptr);
        Buffer = VK_NULL_HANDLE;
        Memory = VK_NULL_HANDLE;
        return code;
    }

    return code;
}

void BufferResource::Cleanup()
{
    if (Buffer)
    {
        vkDestroyBuffer(_device, Buffer, nullptr);
        Buffer = VK_NULL_HANDLE;
    }
    if (Memory)
    {
        vkFreeMemory(_device, Memory, nullptr);
        Memory = VK_NULL_HANDLE;
    }
}

void* BufferResource::Map(VkDeviceSize size) const
{
    void* mappedMemory = nullptr;
    const VkResult code = vkMapMemory(_device, Memory, 0, size, 0, &mappedMemory);
    if (code != VK_SUCCESS)
    {
        LogError(L"vkMapMemory: " + ToString(code));
        return nullptr;
    }
    return mappedMemory;
}

void BufferResource::Unmap() const
{
    vkUnmapMemory(_device, Memory);
}

bool BufferResource::CopyToBufferUsingMapUnmap(const void* memoryToCopyFrom, VkDeviceSize size) const
{
    void* mappedMemory = Map(size);
    if (mappedMemory == nullptr)
    {
        return false;
    }

    memcpy(mappedMemory, memoryToCopyFrom, size);
    Unmap();
    return true;
}
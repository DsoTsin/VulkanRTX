#include "../Common/RayTracingApplication.h"

struct RenderObject
{
    VkGeometryNV geometry = { };
    VkAccelerationStructureNV bottomAS = VK_NULL_HANDLE;
    VkDeviceMemory bottomASMemory = VK_NULL_HANDLE;
    std::array<BufferResource, 3> vertexBuffers = { };
    BufferResource indexBuffer;
    BufferResource indexBufferCopy;
    BufferResource uniformBuffer;
    ImageResource texture;
    uint32_t vertexNum = 0;
    uint16_t indexNum = 0;
    uint32_t shaderIndex = 0;
};

struct VertexPosition
{
    float X, Y, Z;
};

struct UniformBufferContent
{
    uint32_t vertexBufferArrayOffset;
    uint32_t indexBufferArrayOffset;
    uint32_t textureArrayOffset;
    uint32_t padding;
};

class TutorialApplication : public RayTracingApplication
{
public:
    VkDeviceMemory _topASMemory = VK_NULL_HANDLE;
    VkAccelerationStructureNV _topAS = VK_NULL_HANDLE;
    VkPipelineLayout _rtPipelineLayout = VK_NULL_HANDLE;
    VkPipeline _rtPipeline = VK_NULL_HANDLE;
    BufferResource _shaderBindingTable;
    VkDescriptorPool _rtDescriptorPool = VK_NULL_HANDLE;

    std::array<VkDescriptorSetLayout, 4> _rtDescriptorSetLayouts = { };
    std::array<VkDescriptorSet, 4> _rtDescriptorSets = { };

    static constexpr uint32_t _objectNum = 5;
    std::array<RenderObject, _objectNum> _renderObjects = { };
    std::vector<VkBufferView> _vertexBufferViews = { };
    std::vector<VkBufferView> _indexBufferViews = { };
    std::vector<VkImageView> _imageViews = { };
    std::vector<VkSampler> _samplers = { };
 
public:
    TutorialApplication();
    ~TutorialApplication();

    virtual void Init() override;
    virtual void RecordCommandBufferForFrame(VkCommandBuffer commandBuffer, uint32_t frameIndex) override;

    void CreateAccelerationStructures();
    void CreateDescriptorSetLayouts();
    void CreatePipeline();
    void CreateShaderBindingTable();
    void CreatePoolAndAllocateDescriptorSets();
    void UpdateDescriptorSets();

    void CreateBox(RenderObject& object, const std::wstring& texturePath);
    void CreateBoxGeometry(RenderObject& object);
    void CreateBoxBufferViews(RenderObject& object);

    void CreateIcosahedron(RenderObject& object);
    void CreateIcosahedronGeometry(RenderObject& object);
    void CreateIcosahedronBufferViews(RenderObject& object);

    void LoadObjectTexture(RenderObject& object, const std::wstring& path);
    void CreateObjectBottomLevelAS(RenderObject& object);

    void CreateAccelerationStructure(VkAccelerationStructureTypeNV type, uint32_t geometryCount,
        VkGeometryNV* geometries, uint32_t instanceCount, VkAccelerationStructureNV& AS, VkDeviceMemory& memory);
};

TutorialApplication::TutorialApplication()
{
    _appName = L"VkRay Tutorial 11: Different Vertex Formats";
    _deviceExtensions.push_back(VK_KHR_SWAPCHAIN_EXTENSION_NAME);
    _deviceExtensions.push_back(VK_NV_RAY_TRACING_EXTENSION_NAME);
    _deviceExtensions.push_back(VK_EXT_DESCRIPTOR_INDEXING_EXTENSION_NAME);
}

TutorialApplication::~TutorialApplication()
{
    if (_topAS)
    {
        vkDestroyAccelerationStructureNV(_device, _topAS, nullptr);
    }
    if (_topASMemory)
    {
        vkFreeMemory(_device, _topASMemory, nullptr);
    }

    for (auto& bufferView : _vertexBufferViews)
    {
        vkDestroyBufferView(_device, bufferView, nullptr);
    }

    for (auto& bufferView : _indexBufferViews)
    {
        vkDestroyBufferView(_device, bufferView, nullptr);
    }

    for (auto& object : _renderObjects)
    {
        if (object.bottomAS)
        {
            vkDestroyAccelerationStructureNV(_device, object.bottomAS, nullptr);
        }
        if (object.bottomASMemory)
        {
            vkFreeMemory(_device, object.bottomASMemory, nullptr);
        }

        for (auto& vertexBuffer : object.vertexBuffers)
        {
            vertexBuffer.Cleanup();
        }

        object.indexBuffer.Cleanup();
        object.indexBufferCopy.Cleanup();
        object.uniformBuffer.Cleanup();
        object.texture.Cleanup();
    }

    if (_rtDescriptorPool)
    {
        vkDestroyDescriptorPool(_device, _rtDescriptorPool, nullptr);
    }

    _shaderBindingTable.Cleanup();

    if (_rtPipeline)
    {
        vkDestroyPipeline(_device, _rtPipeline, nullptr);
    }
    if (_rtPipelineLayout)
    {
        vkDestroyPipelineLayout(_device, _rtPipelineLayout, nullptr);
    }

    for (auto& item : _rtDescriptorSetLayouts)
    {
        vkDestroyDescriptorSetLayout(_device, item, nullptr);
    }
}

void TutorialApplication::Init()
{
    InitRayTracing();

    CreateBox(_renderObjects[0], L"cb0.bmp");
    CreateIcosahedron(_renderObjects[1]);
    CreateBox(_renderObjects[2], L"cb1.bmp");
    CreateBox(_renderObjects[3], L"cb2.bmp");
    CreateIcosahedron(_renderObjects[4]);

    CreateAccelerationStructures();
    CreateDescriptorSetLayouts();
    CreatePipeline();
    CreateShaderBindingTable();
    CreatePoolAndAllocateDescriptorSets();
    UpdateDescriptorSets();
}

template< typename T >
void CreateBufferAndUploadData( BufferResource& buffer, VkBufferUsageFlags usage, const std::vector<T>& content )
{
    const VkMemoryPropertyFlags memoryFlags = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;

    const VkDeviceSize size = content.size() * sizeof(T);

    VkResult code = buffer.Create(size, usage, memoryFlags);
    NVVK_CHECK_ERROR(code, L"rt BufferResource::Create");

    if (!buffer.CopyToBufferUsingMapUnmap(content.data(), size))
    {
        ExitError(L"Failed to copy data to buffer");
    }
}

void TutorialApplication::CreateAccelerationStructure(VkAccelerationStructureTypeNV type, uint32_t geometryCount,
    VkGeometryNV* geometries, uint32_t instanceCount, VkAccelerationStructureNV& AS, VkDeviceMemory& memory)
{
    VkAccelerationStructureCreateInfoNV accelerationStructureInfo;
    accelerationStructureInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_NV;
    accelerationStructureInfo.pNext = nullptr;
    accelerationStructureInfo.compactedSize = 0;
    accelerationStructureInfo.info.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_INFO_NV;
    accelerationStructureInfo.info.pNext = NULL;
    accelerationStructureInfo.info.type = type;
    accelerationStructureInfo.info.flags = 0;
    accelerationStructureInfo.info.instanceCount = instanceCount;
    accelerationStructureInfo.info.geometryCount = geometryCount;
    accelerationStructureInfo.info.pGeometries = geometries;

    VkResult code = vkCreateAccelerationStructureNV(_device, &accelerationStructureInfo, nullptr, &AS);
    NVVK_CHECK_ERROR(code, L"vkCreateAccelerationStructureNV");

    VkAccelerationStructureMemoryRequirementsInfoNV memoryRequirementsInfo;
    memoryRequirementsInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_MEMORY_REQUIREMENTS_INFO_NV;
    memoryRequirementsInfo.pNext = nullptr;
    memoryRequirementsInfo.accelerationStructure = AS;
    memoryRequirementsInfo.type = VK_ACCELERATION_STRUCTURE_MEMORY_REQUIREMENTS_TYPE_OBJECT_NV;

    VkMemoryRequirements2 memoryRequirements;
    vkGetAccelerationStructureMemoryRequirementsNV(_device, &memoryRequirementsInfo, &memoryRequirements);

    VkMemoryAllocateInfo memoryAllocateInfo;
    memoryAllocateInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    memoryAllocateInfo.pNext = nullptr;
    memoryAllocateInfo.allocationSize = memoryRequirements.memoryRequirements.size;
    memoryAllocateInfo.memoryTypeIndex = ResourceBase::GetMemoryType(memoryRequirements.memoryRequirements, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    code = vkAllocateMemory(_device, &memoryAllocateInfo, nullptr, &memory);
    NVVK_CHECK_ERROR(code, L"rt AS vkAllocateMemory");

    VkBindAccelerationStructureMemoryInfoNV bindInfo;
    bindInfo.sType = VK_STRUCTURE_TYPE_BIND_ACCELERATION_STRUCTURE_MEMORY_INFO_NV;
    bindInfo.pNext = nullptr;
    bindInfo.accelerationStructure = AS;
    bindInfo.memory = memory;
    bindInfo.memoryOffset = 0;
    bindInfo.deviceIndexCount = 0;
    bindInfo.pDeviceIndices = nullptr;

    code = vkBindAccelerationStructureMemoryNV(_device, 1, &bindInfo);
    NVVK_CHECK_ERROR(code, L"vkBindAccelerationStructureMemoryNV");
};

void TutorialApplication::CreateObjectBottomLevelAS(RenderObject& object)
{
    VkGeometryNV& geometry = object.geometry;
    geometry.sType = VK_STRUCTURE_TYPE_GEOMETRY_NV;
    geometry.pNext = nullptr;
    geometry.geometryType = VK_GEOMETRY_TYPE_TRIANGLES_NV;
    geometry.geometry.triangles.sType = VK_STRUCTURE_TYPE_GEOMETRY_TRIANGLES_NV;
    geometry.geometry.triangles.pNext = nullptr;
    geometry.geometry.triangles.vertexData = object.vertexBuffers[0].Buffer;
    geometry.geometry.triangles.vertexOffset = 0;
    geometry.geometry.triangles.vertexCount = object.vertexNum;
    geometry.geometry.triangles.vertexStride = sizeof(VertexPosition);
    geometry.geometry.triangles.vertexFormat = VK_FORMAT_R32G32B32_SFLOAT;
    geometry.geometry.triangles.indexData = object.indexBuffer.Buffer;
    geometry.geometry.triangles.indexOffset = 0;
    geometry.geometry.triangles.indexCount = object.indexNum;
    geometry.geometry.triangles.indexType = VK_INDEX_TYPE_UINT16;
    geometry.geometry.triangles.transformData = VK_NULL_HANDLE;
    geometry.geometry.triangles.transformOffset = 0;
    geometry.geometry.aabbs = { };
    geometry.geometry.aabbs.sType = VK_STRUCTURE_TYPE_GEOMETRY_AABB_NV;
    geometry.flags = VK_GEOMETRY_OPAQUE_BIT_NV;

    CreateAccelerationStructure(VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_NV, 1, &geometry, 0,
        object.bottomAS, object.bottomASMemory);
}

void TutorialApplication::CreateIcosahedron(RenderObject& object)
{
    object.shaderIndex = 1;

    UniformBufferContent content;
    content.vertexBufferArrayOffset = (uint32_t)_vertexBufferViews.size();
    content.indexBufferArrayOffset = (uint32_t)_indexBufferViews.size();
    content.textureArrayOffset = (uint32_t)_imageViews.size();

    const VkMemoryPropertyFlags memoryFlags = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
    object.uniformBuffer.Create(sizeof(content), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, memoryFlags);
    object.uniformBuffer.CopyToBufferUsingMapUnmap(&content, sizeof(content));

    CreateIcosahedronGeometry(object);
    CreateIcosahedronBufferViews(object);
    CreateObjectBottomLevelAS(object);
}

void TutorialApplication::CreateBox(RenderObject& object, const std::wstring& texturePath)
{
    object.shaderIndex = 0;

    UniformBufferContent content;
    content.vertexBufferArrayOffset = (uint32_t)_vertexBufferViews.size();
    content.indexBufferArrayOffset = (uint32_t)_indexBufferViews.size();
    content.textureArrayOffset = (uint32_t)_imageViews.size();

    const VkMemoryPropertyFlags memoryFlags = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
    object.uniformBuffer.Create(sizeof(content), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, memoryFlags);
    object.uniformBuffer.CopyToBufferUsingMapUnmap(&content, sizeof(content));

    CreateBoxGeometry(object);
    CreateBoxBufferViews(object);
    LoadObjectTexture(object, texturePath);
    CreateObjectBottomLevelAS(object);
}

void TutorialApplication::CreateIcosahedronGeometry(RenderObject& object)
{
    const float scale = 0.25f;
    const float d = (1.0f + sqrt(5.0f)) * 0.5f * scale;

    std::vector<VertexPosition> positions
    {
        { -scale, +d, 0 },
        { +scale, +d, 0 },
        { -scale, -d, 0 },
        { +scale, -d, 0 },
        { +0, -scale, +d },
        { +0, +scale, +d },
        { +0, -scale, -d },
        { +0, +scale, -d },
        { +d, 0, -scale },
        { +d, 0, +scale },
        { -d, 0, -scale },
        { -d, 0, +scale }
    };

    CreateBufferAndUploadData(object.vertexBuffers[0], VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, positions);

    std::vector<float> normals( positions.size() * 3 );
    for (size_t i = 0; i < positions.size(); i++)
    {
        const VertexPosition& pos = positions[i];
        const float invLength = 1.0f / sqrt(pos.X * pos.X + pos.Y * pos.Y + pos.Z * pos.Z);
        normals[i * 3] = pos.X * invLength;
        normals[i * 3 + 1] = pos.Y * invLength;
        normals[i * 3 + 2] = pos.Z * invLength;
    }

    CreateBufferAndUploadData(object.vertexBuffers[1], VK_BUFFER_USAGE_UNIFORM_TEXEL_BUFFER_BIT, normals);

    std::vector<uint16_t> indices
    {
        0, 11, 5, 0, 5, 1, 0, 1, 7, 0, 7, 10, 0, 10, 11,
        1, 5, 9, 5, 11, 4, 11, 10, 2, 10, 7, 6, 7, 1, 8,
        3, 9, 4, 3, 4, 2, 3, 2, 6, 3, 6, 8, 3, 8, 9,
        4, 9, 5, 2, 4, 11, 6, 2, 10, 8, 6, 7, 9, 8, 1
    };

    CreateBufferAndUploadData(object.indexBuffer, VK_BUFFER_USAGE_INDEX_BUFFER_BIT, indices);

    // Convert to R16G16B16A16_UINT
    std::vector<uint16_t> indicesWithPadding;
    for( size_t i = 0; i < indices.size(); i += 3 )
    {
        indicesWithPadding.push_back(indices[i]);
        indicesWithPadding.push_back(indices[i + 1]);
        indicesWithPadding.push_back(indices[i + 2]);
        indicesWithPadding.push_back(0);
    }

    CreateBufferAndUploadData(object.indexBufferCopy, VK_BUFFER_USAGE_UNIFORM_TEXEL_BUFFER_BIT, indicesWithPadding);

    object.vertexNum = (uint32_t)positions.size();
    object.indexNum = (uint16_t)indices.size();
}

void TutorialApplication::CreateBoxGeometry(RenderObject& object)
{
    const float boxHalfSize = 0.25f;

    std::vector<VertexPosition> positions
    {
        { -boxHalfSize, -boxHalfSize, -boxHalfSize }, { -boxHalfSize, -boxHalfSize,  boxHalfSize },
        { -boxHalfSize,  boxHalfSize, -boxHalfSize }, { -boxHalfSize,  boxHalfSize,  boxHalfSize },
        {  boxHalfSize, -boxHalfSize, -boxHalfSize }, {  boxHalfSize, -boxHalfSize,  boxHalfSize },
        {  boxHalfSize,  boxHalfSize, -boxHalfSize }, {  boxHalfSize,  boxHalfSize,  boxHalfSize },
        { -boxHalfSize, -boxHalfSize, -boxHalfSize }, { -boxHalfSize, -boxHalfSize,  boxHalfSize },
        {  boxHalfSize, -boxHalfSize, -boxHalfSize }, {  boxHalfSize, -boxHalfSize,  boxHalfSize },
        { -boxHalfSize,  boxHalfSize, -boxHalfSize }, { -boxHalfSize,  boxHalfSize,  boxHalfSize },
        {  boxHalfSize,  boxHalfSize, -boxHalfSize }, {  boxHalfSize,  boxHalfSize,  boxHalfSize },
        { -boxHalfSize, -boxHalfSize, -boxHalfSize }, { -boxHalfSize,  boxHalfSize, -boxHalfSize },
        {  boxHalfSize, -boxHalfSize, -boxHalfSize }, {  boxHalfSize,  boxHalfSize, -boxHalfSize },
        { -boxHalfSize, -boxHalfSize,  boxHalfSize }, { -boxHalfSize,  boxHalfSize,  boxHalfSize },
        {  boxHalfSize, -boxHalfSize,  boxHalfSize }, {  boxHalfSize,  boxHalfSize,  boxHalfSize },
    };

    CreateBufferAndUploadData(object.vertexBuffers[0], VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, positions);

    std::vector<float> texcoords
    {
        0.0f, 0.0f, 2.0f, 0.0f, 0.0f, 2.0f, 2.0f, 2.0f,
        0.0f, 0.0f, 2.0f, 0.0f, 0.0f, 2.0f, 2.0f, 2.0f,
        0.0f, 0.0f, 2.0f, 0.0f, 0.0f, 2.0f, 2.0f, 2.0f,
        0.0f, 0.0f, 2.0f, 0.0f, 0.0f, 2.0f, 2.0f, 2.0f,
        0.0f, 0.0f, 2.0f, 0.0f, 0.0f, 2.0f, 2.0f, 2.0f,
        0.0f, 0.0f, 2.0f, 0.0f, 0.0f, 2.0f, 2.0f, 2.0f,
    };

    CreateBufferAndUploadData(object.vertexBuffers[1], VK_BUFFER_USAGE_UNIFORM_TEXEL_BUFFER_BIT, texcoords);

    std::vector<float> normals
    {
        -1.0f,  0.0f,  0.0f,
        -1.0f,  0.0f,  0.0f,
        -1.0f,  0.0f,  0.0f,
        -1.0f,  0.0f,  0.0f,
        1.0f,  0.0f,  0.0f,
        1.0f,  0.0f,  0.0f,
        1.0f,  0.0f,  0.0f,
        1.0f,  0.0f,  0.0f,
        0.0f, -1.0f,  0.0f,
        0.0f, -1.0f,  0.0f,
        0.0f, -1.0f,  0.0f,
        0.0f, -1.0f,  0.0f,
        0.0f,  1.0f,  0.0f,
        0.0f,  1.0f,  0.0f,
        0.0f,  1.0f,  0.0f,
        0.0f,  1.0f,  0.0f,
        0.0f,  0.0f, -1.0f,
        0.0f,  0.0f, -1.0f,
        0.0f,  0.0f, -1.0f,
        0.0f,  0.0f, -1.0f,
        0.0f,  0.0f,  1.0f,
        0.0f,  0.0f,  1.0f,
        0.0f,  0.0f,  1.0f,
        0.0f,  0.0f,  1.0f,
    };

    CreateBufferAndUploadData(object.vertexBuffers[2], VK_BUFFER_USAGE_UNIFORM_TEXEL_BUFFER_BIT, normals);

    std::vector<uint16_t> indices
    {
        0, 1, 2, 1, 2, 3,
        4, 5, 6, 5, 6, 7,
        8, 9, 10, 9, 10, 11,
        12, 13, 14, 13, 14, 15,
        16, 17, 18, 17, 18, 19,
        20, 21, 22, 21, 22, 23
    };

    CreateBufferAndUploadData(object.indexBuffer, VK_BUFFER_USAGE_INDEX_BUFFER_BIT, indices);

    // Convert to R16G16B16A16_UINT
    std::vector<uint16_t> indicesWithPadding;
    for( size_t i = 0; i < indices.size(); i += 3 )
    {
        indicesWithPadding.push_back(indices[i]);
        indicesWithPadding.push_back(indices[i + 1]);
        indicesWithPadding.push_back(indices[i + 2]);
        indicesWithPadding.push_back(0);
    }

    CreateBufferAndUploadData(object.indexBufferCopy, VK_BUFFER_USAGE_UNIFORM_TEXEL_BUFFER_BIT, indicesWithPadding);

    object.vertexNum = (uint32_t)positions.size();
    object.indexNum = (uint16_t)indices.size();
}

void TutorialApplication::CreateIcosahedronBufferViews(RenderObject& object)
{
    VkBufferViewCreateInfo bufferViewInfo;
    bufferViewInfo.sType = VK_STRUCTURE_TYPE_BUFFER_VIEW_CREATE_INFO;
    bufferViewInfo.pNext = nullptr;
    bufferViewInfo.flags = 0;
    bufferViewInfo.buffer = object.vertexBuffers[1].Buffer;
    bufferViewInfo.format = VK_FORMAT_R32G32B32_SFLOAT; // Normals
    bufferViewInfo.offset = 0;
    bufferViewInfo.range = VK_WHOLE_SIZE;

    VkBufferView vertexBufferView;
    VkResult code = vkCreateBufferView(_device, &bufferViewInfo, nullptr, &vertexBufferView);
    NVVK_CHECK_ERROR(code, L"vkCreateBufferView");

    bufferViewInfo.buffer = object.indexBufferCopy.Buffer;
    bufferViewInfo.format = VK_FORMAT_R16G16B16A16_UINT; // Indices

    VkBufferView indexBufferView;
    code = vkCreateBufferView(_device, &bufferViewInfo, nullptr, &indexBufferView);
    NVVK_CHECK_ERROR(code, L"vkCreateBufferView");

    _vertexBufferViews.push_back(vertexBufferView);
    _indexBufferViews.push_back(indexBufferView);
}

void TutorialApplication::CreateBoxBufferViews(RenderObject& object)
{
    VkBufferViewCreateInfo bufferViewInfo;
    bufferViewInfo.sType = VK_STRUCTURE_TYPE_BUFFER_VIEW_CREATE_INFO;
    bufferViewInfo.pNext = nullptr;
    bufferViewInfo.flags = 0;
    bufferViewInfo.buffer = object.vertexBuffers[1].Buffer;
    bufferViewInfo.format = VK_FORMAT_R32G32_SFLOAT; // Texcoords
    bufferViewInfo.offset = 0;
    bufferViewInfo.range = VK_WHOLE_SIZE;

    VkBufferView vertexBufferViews[2];
    VkResult code = vkCreateBufferView(_device, &bufferViewInfo, nullptr, &vertexBufferViews[0]);
    NVVK_CHECK_ERROR(code, L"vkCreateBufferView");

    bufferViewInfo.buffer = object.vertexBuffers[2].Buffer;
    bufferViewInfo.format = VK_FORMAT_R32G32B32_SFLOAT; // Normals

    code = vkCreateBufferView(_device, &bufferViewInfo, nullptr, &vertexBufferViews[1]);
    NVVK_CHECK_ERROR(code, L"vkCreateBufferView");

    bufferViewInfo.buffer = object.indexBufferCopy.Buffer;
    bufferViewInfo.format = VK_FORMAT_R16G16B16A16_UINT; // Indices

    VkBufferView indexBufferView;
    code = vkCreateBufferView(_device, &bufferViewInfo, nullptr, &indexBufferView);
    NVVK_CHECK_ERROR(code, L"vkCreateBufferView");

    _vertexBufferViews.push_back(vertexBufferViews[0]);
    _vertexBufferViews.push_back(vertexBufferViews[1]);
    _indexBufferViews.push_back(indexBufferView);
}

void TutorialApplication::LoadObjectTexture(RenderObject& object, const std::wstring& path)
{
    VkResult code;
    if (!object.texture.LoadTexture2DFromFile(path, code))
    {
        ExitError(L"Failed to load texture. VkResult: " + std::to_wstring(code));
    }

    VkImageSubresourceRange subresourceRange;
    subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    subresourceRange.baseMipLevel = 0;
    subresourceRange.levelCount = VK_REMAINING_MIP_LEVELS;
    subresourceRange.baseArrayLayer = 0;
    subresourceRange.layerCount = VK_REMAINING_ARRAY_LAYERS;

    code = object.texture.CreateImageView(VK_IMAGE_VIEW_TYPE_2D, VK_FORMAT_R8G8B8A8_SRGB, subresourceRange);
    NVVK_CHECK_ERROR(code, L"Failed to create image view.");

    code = object.texture.CreateSampler(VK_FILTER_NEAREST, VK_FILTER_LINEAR, VK_SAMPLER_MIPMAP_MODE_LINEAR, VK_SAMPLER_ADDRESS_MODE_REPEAT);
    NVVK_CHECK_ERROR(code, L"Failed to create sampler.");

    _imageViews.push_back(object.texture.ImageView);
    _samplers.push_back(object.texture.Sampler);
}

void TutorialApplication::CreateAccelerationStructures()
{
    // ============================================================
    // 1. CREATE INSTANCE BUFFER
    // There can be many instances of the single geometry. Create
    // instances using various transforms.
    // ============================================================

    BufferResource instanceBuffer;

    {
        uint64_t accelerationStructureHandle;
        VkResult code;
 
        VkGeometryInstance instances[_objectNum] = { };

        const float width = 4.0f;
        const float height = 0.75f;
        const float depth = 0.75f;
        const float stepX = width / ( _objectNum );
        const float stepY = height / ( _objectNum );
        const float stepZ = depth / ( _objectNum );
        const float biasX = -stepX * float(_objectNum - 1.0f) * 0.5f;
        const float biasY = -stepY * float(_objectNum - 1.0f) * 0.5f - 0.75f;
        const float biasZ = -stepZ * float(_objectNum - 1.0f) * 0.5f;

        for (uint32_t i = 0; i < _objectNum; i++)
        {
            code = vkGetAccelerationStructureHandleNV(_device, _renderObjects[i].bottomAS, sizeof(uint64_t), &accelerationStructureHandle);
            NVVK_CHECK_ERROR(code, L"vkGetAccelerationStructureHandleNV");

            VkGeometryInstance& instance = instances[i];
            instance.instanceId = i;
            instance.mask = 0xff;
            instance.instanceOffset = _renderObjects[i].shaderIndex;
            instance.flags = VK_GEOMETRY_INSTANCE_TRIANGLE_CULL_DISABLE_BIT_NV;
            instance.accelerationStructureHandle = accelerationStructureHandle;

            float transform[12] =
            {
                1.0f, 0.0f, 0.0f, biasX + stepX * i,
                0.0f, 1.0f, 0.0f, biasY + stepY * i,
                0.0f, 0.0f, 1.0f, biasZ + stepZ * i,
            };
            memcpy(instance.transform, transform, sizeof(instance.transform));
        }
 
        const VkDeviceSize instanceBufferSize = _objectNum * sizeof(VkGeometryInstance);
        const VkMemoryPropertyFlags memoryFlags = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;

        code = instanceBuffer.Create(instanceBufferSize, VK_BUFFER_USAGE_RAY_TRACING_BIT_NV, memoryFlags);
        NVVK_CHECK_ERROR(code, L"rt instanceBuffer.Create");
        instanceBuffer.CopyToBufferUsingMapUnmap(instances, instanceBufferSize);
    }

    // ============================================================
    // 2. CREATE TOP LEVEL ACCELERATION STRUCTURES
    // Top level AS encompasses bottom level acceleration structures.
    // ============================================================

    CreateAccelerationStructure(VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_NV,
        0, nullptr, _objectNum, _topAS, _topASMemory);

    // ============================================================
    // 3. BUILD ACCELERATION STRUCTURES
    // Finally fill acceleration structures using all the data.
    // ============================================================

    auto GetScratchBufferSize = [&](VkAccelerationStructureNV handle)
    {
        VkAccelerationStructureMemoryRequirementsInfoNV memoryRequirementsInfo;
        memoryRequirementsInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_MEMORY_REQUIREMENTS_INFO_NV;
        memoryRequirementsInfo.pNext = nullptr;
        memoryRequirementsInfo.accelerationStructure = handle;
        memoryRequirementsInfo.type = VK_ACCELERATION_STRUCTURE_MEMORY_REQUIREMENTS_TYPE_BUILD_SCRATCH_NV;

        VkMemoryRequirements2 memoryRequirements;
        vkGetAccelerationStructureMemoryRequirementsNV(_device, &memoryRequirementsInfo, &memoryRequirements);

        VkDeviceSize result = memoryRequirements.memoryRequirements.size;
        return result;
    };

    {
        VkDeviceSize bottomAccelerationStructureBufferSize = 0; 
        
        for (size_t i = 0; i < _renderObjects.size(); i++)
            bottomAccelerationStructureBufferSize = std::max(bottomAccelerationStructureBufferSize, GetScratchBufferSize(_renderObjects[i].bottomAS));

        VkDeviceSize topAccelerationStructureBufferSize = GetScratchBufferSize(_topAS);
        VkDeviceSize scratchBufferSize = std::max(bottomAccelerationStructureBufferSize, topAccelerationStructureBufferSize);

        BufferResource scratchBuffer;
        scratchBuffer.Create(scratchBufferSize, VK_BUFFER_USAGE_RAY_TRACING_BIT_NV, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);


        VkCommandBufferAllocateInfo commandBufferAllocateInfo;
        commandBufferAllocateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        commandBufferAllocateInfo.pNext = nullptr;
        commandBufferAllocateInfo.commandPool = _commandPool;
        commandBufferAllocateInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        commandBufferAllocateInfo.commandBufferCount = 1;

        VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
        VkResult code = vkAllocateCommandBuffers(_device, &commandBufferAllocateInfo, &commandBuffer);
        NVVK_CHECK_ERROR(code, L"rt vkAllocateCommandBuffers");

        VkCommandBufferBeginInfo beginInfo;
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        beginInfo.pNext = nullptr;
        beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        beginInfo.pInheritanceInfo = nullptr;
        vkBeginCommandBuffer(commandBuffer, &beginInfo);

        VkMemoryBarrier memoryBarrier;
        memoryBarrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
        memoryBarrier.pNext = nullptr;
        memoryBarrier.srcAccessMask = VK_ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT_NV | VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_NV;
        memoryBarrier.dstAccessMask = VK_ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT_NV | VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_NV;

        for (size_t i = 0; i < _renderObjects.size(); i++)
        {
            {
                VkAccelerationStructureInfoNV asInfo;
                asInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_INFO_NV;
                asInfo.pNext = NULL;
                asInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_NV;
                asInfo.flags = 0;
                asInfo.instanceCount = 0;
                asInfo.geometryCount = 1;
                asInfo.pGeometries = &_renderObjects[i].geometry;

                vkCmdBuildAccelerationStructureNV(commandBuffer, &asInfo, VK_NULL_HANDLE, 0, VK_FALSE, _renderObjects[i].bottomAS, VK_NULL_HANDLE, scratchBuffer.Buffer, 0);
            }

            vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_NV, VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_NV, 0, 1, &memoryBarrier, 0, 0, 0, 0);
        }

        {
            VkAccelerationStructureInfoNV asInfo;
            asInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_INFO_NV;
            asInfo.pNext = NULL;
            asInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_NV;
            asInfo.flags = 0;
            asInfo.instanceCount = _objectNum;
            asInfo.geometryCount = 0;
            asInfo.pGeometries = nullptr;

            vkCmdBuildAccelerationStructureNV(commandBuffer, &asInfo, instanceBuffer.Buffer, 0, VK_FALSE, _topAS, VK_NULL_HANDLE, scratchBuffer.Buffer, 0);
        }

        vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_NV, VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_NV, 0, 1, &memoryBarrier, 0, 0, 0, 0);
        vkEndCommandBuffer(commandBuffer);

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

        vkQueueSubmit(_queuesInfo.Graphics.Queue, 1, &submitInfo, VK_NULL_HANDLE);
        vkQueueWaitIdle(_queuesInfo.Graphics.Queue);
        vkFreeCommandBuffers(_device, _commandPool, 1, &commandBuffer);
    }
}

void TutorialApplication::CreateDescriptorSetLayouts()
{
    {
        VkDescriptorSetLayoutBinding accelerationStructureLayoutBinding;
        accelerationStructureLayoutBinding.binding = 0;
        accelerationStructureLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_NV;
        accelerationStructureLayoutBinding.descriptorCount = 1;
        accelerationStructureLayoutBinding.stageFlags = VK_SHADER_STAGE_RAYGEN_BIT_NV;
        accelerationStructureLayoutBinding.pImmutableSamplers = nullptr;

        VkDescriptorSetLayoutBinding outputImageLayoutBinding;
        outputImageLayoutBinding.binding = 1;
        outputImageLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
        outputImageLayoutBinding.descriptorCount = 1;
        outputImageLayoutBinding.stageFlags = VK_SHADER_STAGE_RAYGEN_BIT_NV;
        outputImageLayoutBinding.pImmutableSamplers = nullptr;

        VkDescriptorSetLayoutBinding uniformBufferLayoutBinding;
        uniformBufferLayoutBinding.binding = 2;
        uniformBufferLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        uniformBufferLayoutBinding.descriptorCount = _objectNum; // _instanceNum is an upper bound
        uniformBufferLayoutBinding.stageFlags = VK_SHADER_STAGE_CLOSEST_HIT_BIT_NV;
        uniformBufferLayoutBinding.pImmutableSamplers = nullptr;

        std::vector<VkDescriptorSetLayoutBinding> bindings({ accelerationStructureLayoutBinding, outputImageLayoutBinding,
            uniformBufferLayoutBinding });

        // Variable number of uniform buffers
        std::array<VkDescriptorBindingFlagsEXT, 3> flags =
            { 0, 0, VK_DESCRIPTOR_BINDING_VARIABLE_DESCRIPTOR_COUNT_BIT_EXT };

        VkDescriptorSetLayoutBindingFlagsCreateInfoEXT bindingFlags;
        bindingFlags.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO_EXT;
        bindingFlags.pNext = nullptr;
        bindingFlags.pBindingFlags = flags.data();
        bindingFlags.bindingCount = (uint32_t)flags.size();

        VkDescriptorSetLayoutCreateInfo layoutInfo;
        layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        layoutInfo.pNext = &bindingFlags;
        layoutInfo.flags = 0;
        layoutInfo.bindingCount = (uint32_t)bindings.size();
        layoutInfo.pBindings = bindings.data();

        VkResult code = vkCreateDescriptorSetLayout(_device, &layoutInfo, nullptr, &_rtDescriptorSetLayouts[0]);
        NVVK_CHECK_ERROR(code, L"vkCreateDescriptorSetLayout");
    }

    {
        const VkDescriptorBindingFlagsEXT flag = VK_DESCRIPTOR_BINDING_VARIABLE_DESCRIPTOR_COUNT_BIT_EXT;

        VkDescriptorSetLayoutBindingFlagsCreateInfoEXT bindingFlags;
        bindingFlags.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO_EXT;
        bindingFlags.pNext = nullptr;
        bindingFlags.pBindingFlags = &flag;
        bindingFlags.bindingCount = 1;

        VkDescriptorSetLayoutBinding texelBufferBinding;
        texelBufferBinding.binding = 0;
        texelBufferBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER;
        texelBufferBinding.descriptorCount = _objectNum * 2; // 2 vertex buffers per object
        texelBufferBinding.stageFlags = VK_SHADER_STAGE_CLOSEST_HIT_BIT_NV;
        texelBufferBinding.pImmutableSamplers = nullptr;

        VkDescriptorSetLayoutCreateInfo layoutInfo;
        layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        layoutInfo.pNext = &bindingFlags;
        layoutInfo.flags = 0;
        layoutInfo.bindingCount = 1;
        layoutInfo.pBindings = &texelBufferBinding;

        VkResult code = vkCreateDescriptorSetLayout(_device, &layoutInfo, nullptr, &_rtDescriptorSetLayouts[1]);
        NVVK_CHECK_ERROR(code, L"vkCreateDescriptorSetLayout");
 
        texelBufferBinding.descriptorCount = _objectNum; // 1 index buffer per object

        code = vkCreateDescriptorSetLayout(_device, &layoutInfo, nullptr, &_rtDescriptorSetLayouts[2]);
        NVVK_CHECK_ERROR(code, L"vkCreateDescriptorSetLayout");
    }

    {
        const VkDescriptorBindingFlagsEXT flag = VK_DESCRIPTOR_BINDING_VARIABLE_DESCRIPTOR_COUNT_BIT_EXT;

        VkDescriptorSetLayoutBindingFlagsCreateInfoEXT bindingFlags;
        bindingFlags.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO_EXT;
        bindingFlags.pNext = nullptr;
        bindingFlags.pBindingFlags = &flag;
        bindingFlags.bindingCount = 1;

        VkDescriptorSetLayoutBinding textureBinding;
        textureBinding.binding = 0;
        textureBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        textureBinding.descriptorCount = _objectNum; // 1 texture per object
        textureBinding.stageFlags = VK_SHADER_STAGE_CLOSEST_HIT_BIT_NV;
        textureBinding.pImmutableSamplers = nullptr;

        VkDescriptorSetLayoutCreateInfo layoutInfo;
        layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        layoutInfo.pNext = &bindingFlags;
        layoutInfo.flags = 0;
        layoutInfo.bindingCount = 1;
        layoutInfo.pBindings = &textureBinding;

        VkResult code = vkCreateDescriptorSetLayout(_device, &layoutInfo, nullptr, &_rtDescriptorSetLayouts[3]);
        NVVK_CHECK_ERROR(code, L"vkCreateDescriptorSetLayout");
    }
}

void TutorialApplication::CreatePipeline()
{
    auto LoadShader = [](ShaderResource& shader, std::wstring shaderName)
    {
        bool fileError;
        VkResult code = shader.LoadFromFile(shaderName, fileError);
        if (fileError)
        {
            ExitError(L"Failed to read " + shaderName + L" file");
        }
        NVVK_CHECK_ERROR(code, shaderName);
    };

    ShaderResource rgenShader;
    ShaderResource missShader;
    std::array<ShaderResource, 2> chitShaders;

    LoadShader(rgenShader, L"rt_11_shaders.rgen.spv");
    LoadShader(missShader, L"rt_11_shaders.rmiss.spv");
    LoadShader(chitShaders[0], L"rt_11_box.rchit.spv");
    LoadShader(chitShaders[1], L"rt_11_icosahedron.rchit.spv");
 
    std::vector<VkPipelineShaderStageCreateInfo> shaderStages
    {
        rgenShader.GetShaderStage(VK_SHADER_STAGE_RAYGEN_BIT_NV),
        missShader.GetShaderStage(VK_SHADER_STAGE_MISS_BIT_NV),
        chitShaders[0].GetShaderStage(VK_SHADER_STAGE_CLOSEST_HIT_BIT_NV),
        chitShaders[1].GetShaderStage(VK_SHADER_STAGE_CLOSEST_HIT_BIT_NV),
    };

    VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo;
    pipelineLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutCreateInfo.pNext = nullptr;
    pipelineLayoutCreateInfo.flags = 0;
    pipelineLayoutCreateInfo.setLayoutCount = (uint32_t)_rtDescriptorSetLayouts.size();
    pipelineLayoutCreateInfo.pSetLayouts = _rtDescriptorSetLayouts.data();
    pipelineLayoutCreateInfo.pushConstantRangeCount = 0;
    pipelineLayoutCreateInfo.pPushConstantRanges = nullptr;

    VkResult code = vkCreatePipelineLayout(_device, &pipelineLayoutCreateInfo, nullptr, &_rtPipelineLayout);
    NVVK_CHECK_ERROR(code, L"rt vkCreatePipelineLayout");

    std::vector<VkRayTracingShaderGroupCreateInfoNV> shaderGroups({
        // group0 = [ raygen ]
        { VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_NV, nullptr, VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_NV, 0, VK_SHADER_UNUSED_NV, VK_SHADER_UNUSED_NV, VK_SHADER_UNUSED_NV },
        // group1 = [ miss ]
        { VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_NV, nullptr, VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_NV, 1, VK_SHADER_UNUSED_NV, VK_SHADER_UNUSED_NV, VK_SHADER_UNUSED_NV },
        // group2 = [ chit ]
        { VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_NV, nullptr, VK_RAY_TRACING_SHADER_GROUP_TYPE_TRIANGLES_HIT_GROUP_NV, VK_SHADER_UNUSED_NV, 2, VK_SHADER_UNUSED_NV, VK_SHADER_UNUSED_NV },
        // group3 = [ chit ]
        { VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_NV, nullptr, VK_RAY_TRACING_SHADER_GROUP_TYPE_TRIANGLES_HIT_GROUP_NV, VK_SHADER_UNUSED_NV, 3, VK_SHADER_UNUSED_NV, VK_SHADER_UNUSED_NV },
    });

    VkRayTracingPipelineCreateInfoNV rayPipelineInfo;
    rayPipelineInfo.sType = VK_STRUCTURE_TYPE_RAY_TRACING_PIPELINE_CREATE_INFO_NV;
    rayPipelineInfo.pNext = nullptr;
    rayPipelineInfo.flags = 0;
    rayPipelineInfo.stageCount = (uint32_t)shaderStages.size();
    rayPipelineInfo.pStages = shaderStages.data();
    rayPipelineInfo.groupCount = (uint32_t)shaderGroups.size();
    rayPipelineInfo.pGroups = shaderGroups.data();
    rayPipelineInfo.maxRecursionDepth = 1;
    rayPipelineInfo.layout = _rtPipelineLayout;
    rayPipelineInfo.basePipelineHandle = VK_NULL_HANDLE;
    rayPipelineInfo.basePipelineIndex = 0;

    code = vkCreateRayTracingPipelinesNV(_device, nullptr, 1, &rayPipelineInfo, nullptr, &_rtPipeline);
    NVVK_CHECK_ERROR(code, L"vkCreateRayTracingPipelinesNV");
}

void TutorialApplication::CreateShaderBindingTable()
{
    const uint32_t groupNum = 4;
    const VkDeviceSize shaderBindingTableSize = _rayTracingProperties.shaderGroupHandleSize * groupNum;

    VkResult code = _shaderBindingTable.Create(shaderBindingTableSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);
    NVVK_CHECK_ERROR(code, L"_shaderBindingTable.Create");

    uint8_t* mappedMemory = (uint8_t*)_shaderBindingTable.Map(shaderBindingTableSize);

    code = vkGetRayTracingShaderGroupHandlesNV(_device, _rtPipeline, 0, groupNum, shaderBindingTableSize, mappedMemory);
    NVVK_CHECK_ERROR(code, L"_shaderBindingTable.Create");

    _shaderBindingTable.Unmap();
}

void TutorialApplication::CreatePoolAndAllocateDescriptorSets()
{
    std::vector<VkDescriptorPoolSize> poolSizes
    ({
        { VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1 },
        { VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_NV, 1 },
        { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, _objectNum },
        { VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, _objectNum * 3 }, // 2 vertex buffers + index buffer
        { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, _objectNum }
    });

    VkDescriptorPoolCreateInfo descriptorPoolCreateInfo;
    descriptorPoolCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    descriptorPoolCreateInfo.pNext = nullptr;
    descriptorPoolCreateInfo.flags = 0;
    descriptorPoolCreateInfo.maxSets = (uint32_t)_rtDescriptorSetLayouts.size();
    descriptorPoolCreateInfo.poolSizeCount = (uint32_t)poolSizes.size();
    descriptorPoolCreateInfo.pPoolSizes = poolSizes.data();

    VkResult code = vkCreateDescriptorPool(_device, &descriptorPoolCreateInfo, nullptr, &_rtDescriptorPool);
    NVVK_CHECK_ERROR(code, L"vkCreateDescriptorPool");

    const uint32_t variableDescriptorCounts[4] = {
        _objectNum, // uniform buffers
        (uint32_t)_vertexBufferViews.size(), // vertex buffers
        (uint32_t)_indexBufferViews.size(), // index buffers
        (uint32_t)_imageViews.size() // combined image samplers
    };

    VkDescriptorSetVariableDescriptorCountAllocateInfoEXT variableDescriptorCountInfo;
    variableDescriptorCountInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_VARIABLE_DESCRIPTOR_COUNT_ALLOCATE_INFO_EXT;
    variableDescriptorCountInfo.pNext = nullptr;
    variableDescriptorCountInfo.descriptorSetCount = (uint32_t)_rtDescriptorSetLayouts.size();
    variableDescriptorCountInfo.pDescriptorCounts = variableDescriptorCounts; // actual number of descriptors

    VkDescriptorSetAllocateInfo descriptorSetAllocateInfo;
    descriptorSetAllocateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    descriptorSetAllocateInfo.pNext = &variableDescriptorCountInfo;
    descriptorSetAllocateInfo.descriptorPool = _rtDescriptorPool;
    descriptorSetAllocateInfo.descriptorSetCount = (uint32_t)_rtDescriptorSetLayouts.size();
    descriptorSetAllocateInfo.pSetLayouts = _rtDescriptorSetLayouts.data();

    code = vkAllocateDescriptorSets(_device, &descriptorSetAllocateInfo, _rtDescriptorSets.data());
    NVVK_CHECK_ERROR(code, L"vkAllocateDescriptorSets");
}

void TutorialApplication::UpdateDescriptorSets()
{
    VkWriteDescriptorSetAccelerationStructureNV descriptorAccelerationStructureInfo;
    descriptorAccelerationStructureInfo.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_ACCELERATION_STRUCTURE_NV;
    descriptorAccelerationStructureInfo.pNext = nullptr;
    descriptorAccelerationStructureInfo.accelerationStructureCount = 1;
    descriptorAccelerationStructureInfo.pAccelerationStructures = &_topAS;

    VkWriteDescriptorSet accelerationStructureWrite;
    accelerationStructureWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    accelerationStructureWrite.pNext = &descriptorAccelerationStructureInfo; // Notice that pNext is assigned here!
    accelerationStructureWrite.dstSet = _rtDescriptorSets[0];
    accelerationStructureWrite.dstBinding = 0;
    accelerationStructureWrite.dstArrayElement = 0;
    accelerationStructureWrite.descriptorCount = 1;
    accelerationStructureWrite.descriptorType = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_NV;
    accelerationStructureWrite.pImageInfo = nullptr;
    accelerationStructureWrite.pBufferInfo = nullptr;
    accelerationStructureWrite.pTexelBufferView = nullptr;

    VkDescriptorImageInfo descriptorOutputImageInfo;
    descriptorOutputImageInfo.sampler = nullptr;
    descriptorOutputImageInfo.imageView = _offsreenImageResource.ImageView;
    descriptorOutputImageInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;

    VkWriteDescriptorSet outputImageWrite;
    outputImageWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    outputImageWrite.pNext = nullptr;
    outputImageWrite.dstSet = _rtDescriptorSets[0];
    outputImageWrite.dstBinding = 1;
    outputImageWrite.dstArrayElement = 0;
    outputImageWrite.descriptorCount = 1;
    outputImageWrite.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    outputImageWrite.pImageInfo = &descriptorOutputImageInfo;
    outputImageWrite.pBufferInfo = nullptr;
    outputImageWrite.pTexelBufferView = nullptr;

    std::array<VkDescriptorBufferInfo, _objectNum> bufferInfo;

    for (uint32_t i = 0; i < _renderObjects.size(); i++)
    {
        bufferInfo[i].buffer = _renderObjects[i].uniformBuffer.Buffer;
        bufferInfo[i].offset = 0;
        bufferInfo[i].range = _renderObjects[i].uniformBuffer.Size;
    }

    VkWriteDescriptorSet uniformBuffers;
    uniformBuffers.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    uniformBuffers.pNext = nullptr;
    uniformBuffers.dstSet = _rtDescriptorSets[0];
    uniformBuffers.dstBinding = 2;
    uniformBuffers.dstArrayElement = 0;
    uniformBuffers.descriptorCount = (uint32_t)bufferInfo.size();
    uniformBuffers.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    uniformBuffers.pImageInfo = nullptr;
    uniformBuffers.pBufferInfo = bufferInfo.data();
    uniformBuffers.pTexelBufferView = nullptr;

    VkWriteDescriptorSet vertexBuffers;
    vertexBuffers.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    vertexBuffers.pNext = nullptr;
    vertexBuffers.dstSet = _rtDescriptorSets[1];
    vertexBuffers.dstBinding = 0;
    vertexBuffers.dstArrayElement = 0;
    vertexBuffers.descriptorCount = (uint32_t)_vertexBufferViews.size();
    vertexBuffers.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER;
    vertexBuffers.pImageInfo = nullptr;
    vertexBuffers.pBufferInfo = nullptr;
    vertexBuffers.pTexelBufferView = _vertexBufferViews.data();

    VkWriteDescriptorSet indexBuffers;
    indexBuffers.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    indexBuffers.pNext = nullptr;
    indexBuffers.dstSet = _rtDescriptorSets[2];
    indexBuffers.dstBinding = 0;
    indexBuffers.dstArrayElement = 0;
    indexBuffers.descriptorCount = (uint32_t)_indexBufferViews.size();
    indexBuffers.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER;
    indexBuffers.pImageInfo = nullptr;
    indexBuffers.pBufferInfo = nullptr;
    indexBuffers.pTexelBufferView = _indexBufferViews.data();

    std::vector<VkDescriptorImageInfo> imageInfoArray;
    for (size_t i = 0; i < _imageViews.size(); i++)
    {
        VkDescriptorImageInfo info;
        info.sampler = _samplers[i];
        info.imageView = _imageViews[i];
        info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        imageInfoArray.push_back(info);
    }

    VkWriteDescriptorSet imageWrite;
    imageWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    imageWrite.pNext = nullptr;
    imageWrite.dstSet = _rtDescriptorSets[3];
    imageWrite.dstBinding = 0;
    imageWrite.dstArrayElement = 0;
    imageWrite.descriptorCount = (uint32_t)imageInfoArray.size();
    imageWrite.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    imageWrite.pImageInfo = imageInfoArray.data();
    imageWrite.pBufferInfo = nullptr;
    imageWrite.pTexelBufferView = nullptr;

    const std::vector<VkWriteDescriptorSet> descriptorWrites
    {
        accelerationStructureWrite,
        outputImageWrite,
        uniformBuffers,
        vertexBuffers,
        indexBuffers,
        imageWrite
    };

    vkUpdateDescriptorSets(_device, (uint32_t)descriptorWrites.size(), descriptorWrites.data(), 0, nullptr);
}

void TutorialApplication::RecordCommandBufferForFrame(VkCommandBuffer commandBuffer, uint32_t frameIndex)
{
    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_RAY_TRACING_NV, _rtPipeline);
    vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_RAY_TRACING_NV, _rtPipelineLayout, 0,
        (uint32_t)_rtDescriptorSets.size(), _rtDescriptorSets.data(), 0, 0);

    // Here's how the shader binding table looks like in this tutorial:
    // |[ raygen shader ]|[ miss shader ]|[ hit shader ][ hit shader ]|
    // |                 |               |                            |
    // | 0               | 1             | 2                          | 4

    vkCmdTraceRaysNV(commandBuffer,
        _shaderBindingTable.Buffer, 0,
        _shaderBindingTable.Buffer, 1 * _rayTracingProperties.shaderGroupHandleSize, _rayTracingProperties.shaderGroupHandleSize,
        _shaderBindingTable.Buffer, 2 * _rayTracingProperties.shaderGroupHandleSize, _rayTracingProperties.shaderGroupHandleSize,
        VK_NULL_HANDLE, 0, 0,
        _actualWindowWidth, _actualWindowHeight, 1);
}


void main(int argc, const char* argv[])
{
    RunApplication<TutorialApplication>();
}
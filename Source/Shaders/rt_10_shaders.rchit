#version 460
#extension GL_NV_ray_tracing : require
#extension GL_EXT_nonuniform_qualifier : require

layout(binding = 0) uniform accelerationStructureNV topLevelAS;

layout(binding = 2) uniform UniformBuffer
{
    vec3 color;
} uniformBuffers[];

layout(location = 0) rayPayloadInNV vec3 hitValue;
layout(location = 1) hitAttributeNV vec3 attribs;

layout(shaderRecordNV) buffer InlineData {
    vec4 inlineData;
};

void main()
{
    // gl_InstanceCustomIndex = VkGeometryInstance::instanceId
    hitValue = inlineData.rgb + uniformBuffers[nonuniformEXT(gl_InstanceCustomIndexNV)].color;
}
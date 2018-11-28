#version 460
#extension GL_NV_ray_tracing : require
#extension GL_EXT_nonuniform_qualifier : require

layout(location = 0) rayPayloadInNV vec3 hitValue;
layout(location = 1) hitAttributeNV vec2 attribs;

layout(set = 0, binding = 0) uniform accelerationStructureNV topLevelAS;

layout(set = 0, binding = 2) uniform UniformBuffer
{
    uvec4 offsets;
} uniformBuffers[];

layout(set = 1, binding = 0) uniform samplerBuffer vertexBuffers[];
layout(set = 2, binding = 0) uniform usamplerBuffer indexBuffers[];
layout(set = 3, binding = 0) uniform sampler2D textures[];

vec3 CalculateLighting(in vec3 N, in vec3 L, in vec3 V, in vec3 albedo, in vec3 lightColor)
{
    vec3 directLighting = vec3(0.0);

    float dotNL = dot(N, L);
    if (dotNL > 0.0)
    {
        const vec3 H = normalize(V + L);

        dotNL = clamp(dotNL, 0.0, 1.0);
        const float dotNV = clamp(dot(N, V), 0.0, 1.0);
        const float dotNH = clamp(dot(N, H), 0.0, 1.0);
        const float dotLH = clamp(dot(L, H), 0.0, 1.0);
        const float dotVH = clamp(dot(V, H), 0.0, 1.0);

        const float roughness = 0.1 + dot(albedo, albedo) * 0.3;
        const float F0 = 0.15 - dot(albedo, albedo) * 0.05;

        // Direct diffuse
        const float f = 2.0 * dotVH * dotVH * roughness - 0.5;
        const float FdV = f * pow(dotNV , 5.0) + 1.0;
        const float FdL = f * pow(dotNL, 5.0) + 1.0;
        const vec3 diffuse = FdV * FdL * dotNL * albedo * lightColor;

        // Direct specular
        const float alpha = roughness * roughness;
        const float alphaSqr = alpha * alpha;
        const float denom = dotNH * dotNH * (alphaSqr - 1.0) + 1.0;
        const float D = alphaSqr / (denom * denom);
        const float F_a = 1.0;
        const float F_b = pow(1.0 - dotLH, 5.0);
        const float F = mix(F_b, F_a, F0);
        const float k = (alpha + 2.0 * roughness + 1.0) / 8.0;
        const float G = dotNL / (mix(dotNL, 1.0, k) * mix(dotNV , 1.0, k));
        const vec3 specular = D * F * G * 0.25 * lightColor;

        directLighting = diffuse + specular;
    }

    // Indirect diffuse
    const vec3 colorBottom = vec3(0.0, 0.5, 1.0) * 0.005;
    const vec3 colorTop = vec3(0.25, 0.75, 1.0) * 0.05;
    const vec3 indirectLighting = albedo * mix(colorBottom, colorTop, N.y * 0.5 + 0.5);

    return directLighting + indirectLighting;
}


void main()
{
    // gl_InstanceCustomIndex = VkGeometryInstance::instanceId
    const uvec3 offsets = uniformBuffers[nonuniformEXT(gl_InstanceCustomIndexNV)].offsets.xyz;
    const uint vbArrayOffset = offsets.x;
    const uint ibArrayOffset = offsets.y;
    const uint texArrayOffset = offsets.z;

    // Fetch indices from R16G16B16A16 uniform texel buffer
    const ivec3 indices = ivec3(texelFetch(indexBuffers[nonuniformEXT(ibArrayOffset)], gl_PrimitiveID).rgb);

    // Fetch vertex attributes of the primitive
    const vec2 texcoords0 = texelFetch(vertexBuffers[nonuniformEXT(vbArrayOffset)], indices.x).rg;
    const vec2 texcoords1 = texelFetch(vertexBuffers[nonuniformEXT(vbArrayOffset)], indices.y).rg;
    const vec2 texcoords2 = texelFetch(vertexBuffers[nonuniformEXT(vbArrayOffset)], indices.z).rg;
    const vec3 normals0 = texelFetch(vertexBuffers[nonuniformEXT(vbArrayOffset + 1)], indices.x).rgb;
    const vec3 normals1 = texelFetch(vertexBuffers[nonuniformEXT(vbArrayOffset + 1)], indices.y).rgb;
    const vec3 normals2 = texelFetch(vertexBuffers[nonuniformEXT(vbArrayOffset + 1)], indices.z).rgb;

    const vec3 barycentrics = vec3(1.0 - attribs.x - attribs.y, attribs.x, attribs.y);

    // Interpolate vertex attributes
    const vec2 texcoords = barycentrics.x * texcoords0 + barycentrics.y * texcoords1 + barycentrics.z * texcoords2;
    const vec3 normal = normalize(barycentrics.x * normals0 + barycentrics.y * normals1 + barycentrics.z * normals2);

    const vec3 N = mat3(gl_ObjectToWorldNV ) * normal;
    const vec3 L = normalize(vec3(-0.85, 0.5, 1.0));
    const vec3 V = normalize(-gl_WorldRayDirectionNV );

    // Sample texture using the interpolated texture coordinates
    const vec3 albedo = texture(textures[nonuniformEXT(texArrayOffset)], texcoords).rgb;

    const vec3 lightColor = vec3(1.0, 1.0, 0.6) * 1.5;
    const vec3 lighting = CalculateLighting(N, L, V, albedo, lightColor);

    hitValue = pow(lighting, 1.0 / vec3(2.2));
}
#version 460
#extension GL_NV_ray_tracing : require

layout(binding = 0) uniform accelerationStructureNV topLevelAS;

layout(location = 0) rayPayloadInNV vec3 hitValue;
layout(location = 1) hitAttributeNV vec3 attribs;
layout(location = 2) rayPayloadNV float secondaryRayHitValue;

void main()
{
    const vec3 barycentrics = vec3(1.0 - attribs.x - attribs.y, attribs.x, attribs.y);

    vec3 origin = gl_WorldRayOriginNV + gl_WorldRayDirectionNV * gl_HitTNV;
    vec3 direction = normalize(vec3(0.8, 1, 0));
    uint rayFlags = gl_RayFlagsOpaqueNV | gl_RayFlagsTerminateOnFirstHitNV;
    uint cullMask = 0xff;
    float tmin = 0.001;
    float tmax = 100.0;

    traceNV(topLevelAS, rayFlags, cullMask, 1 /* sbtRecordOffset */, 0 /* sbtRecordStride */, 1 /* missIndex */, origin, tmin, direction, tmax, 2 /*payload location*/);

    hitValue = barycentrics * ( secondaryRayHitValue < tmax ? 0.25 : 1.0 );
}
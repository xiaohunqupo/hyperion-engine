#ifndef VCT_UNIFORMS_GLSL
#define VCT_UNIFORMS_GLSL

#include "../defines.inc"

layout(std140, set = HYP_DESCRIPTOR_SET_VOXELIZER, binding = 1) uniform VctParams
{
    uvec4 extent;
    vec4 aabb_max;
    vec4 aabb_min;
    uint num_mipmaps;

    uint _pad0;
    uint _pad1;
    uint _pad2;
} vct_params;

#endif

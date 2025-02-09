#ifndef HYP_SHADOWS_GLSL
#define HYP_SHADOWS_GLSL

#include "noise.inc"

#define HYP_SHADOW_BIAS 0.005
#define HYP_SHADOW_VARIABLE_BIAS 1
// #define HYP_SHADOW_PENUMBRA 1
#define HYP_SHADOW_PENUMBRA_MIN 0.01
#define HYP_SHADOW_PENUMBRA_MAX 8.0

#define HYP_SHADOW_VARIANCE_LIGHT_BLEED_REDUCTION 0.35

#define HYP_SHADOW_SAMPLES_4
//#define HYP_SHADOW_SAMPLES_8

#define HYP_PENUMBRA_SAMPLES_4
//#define HYP_PENUMBRA_SAMPLES_8

#define HYP_SHADOW_VARIANCE 1

const vec2 shadow_pcf_kernel[16] = vec2[16](
    vec2(-0.94201624, -0.39906216),
    vec2(0.94558609, -0.76890725),
    vec2(-0.094184101, -0.92938870),
    vec2(0.34495938, 0.29387760),
    vec2(-0.91588581, 0.45771432),
    vec2(-0.81544232, -0.87912464),
    vec2(-0.38277543, 0.27676845),
    vec2(0.97484398, 0.75648379),
    vec2(0.44323325, -0.97511554),
    vec2(0.53742981, -0.47373420),
    vec2(-0.26496911, -0.41893023),
    vec2(0.79197514, 0.19090188),
    vec2(-0.24188840, 0.99706507),
    vec2(-0.81409955, 0.91437590),
    vec2(0.19984126, 0.78641367),
    vec2(0.14383161, -0.14100790)
);

const mat4 shadow_bias_matrix = mat4( 
    0.5, 0.0, 0.0, 0.0,
    0.0, 0.5, 0.0, 0.0,
    0.0, 0.0, 1.0, 0.0,
    0.5, 0.5, 0.0, 1.0
);

vec3 GetShadowCoord(uint index, vec3 pos)
{
    mat4 shadow_matrix = shadow_bias_matrix * shadow_data.matrices[index].projection * shadow_data.matrices[index].view;
    vec4 shadow_position = shadow_matrix * vec4(pos, 1.0);
    shadow_position.xyz /= shadow_position.w;
  
    return shadow_position.xyz;
}

float GetShadowStandard(uint index, vec3 pos, vec2 offset, float NdotL)
{
    const vec3 coord = GetShadowCoord(index, pos);
    const vec4 shadow_sample = texture(shadow_maps[index], coord.xy + offset);
    const float shadow_depth = shadow_sample.r;

    float bias = HYP_SHADOW_BIAS;

#if HYP_SHADOW_VARIABLE_BIAS
    bias *= tan(acos(NdotL));
    bias = clamp(bias, 0.0, 0.01);
#endif

    return max(step(coord.z - bias, shadow_depth), 0.0);
}

float GetShadowVariance(uint index, vec3 pos, float NdotL)
{
    const vec3 coord = GetShadowCoord(index, pos);
    const vec4 shadow_sample = Texture2D(shadow_maps[index], coord.xy);
    const float moment = shadow_sample.r;

    if (coord.z <= moment) {
        return 1.0;
    }

    const float moment2 = shadow_sample.g;

    const float variance = max(moment2 - HYP_FMATH_SQR(moment), HYP_FMATH_EPSILON);
    const float d = coord.z - moment;
    
    float percent_in_shadow = variance / (variance + HYP_FMATH_SQR(d));
    percent_in_shadow = smoothstep(HYP_SHADOW_VARIANCE_LIGHT_BLEED_REDUCTION, 1.0, percent_in_shadow);
    
    return percent_in_shadow;
}

float AvgBlockerDepthToPenumbra(float light_size, float avg_blocker_depth, float shadow_map_coord_z)
{
    float penumbra = (shadow_map_coord_z - avg_blocker_depth) * light_size / avg_blocker_depth;
    penumbra += HYP_SHADOW_PENUMBRA_MIN;
    penumbra = min(HYP_SHADOW_PENUMBRA_MAX, penumbra);
    return penumbra;
}

float GetShadowContactHardened(uint index, vec3 pos, float NdotL)
{
    const vec3 coord = GetShadowCoord(index, pos);

    const float shadow_map_depth = coord.z;

    const float shadow_filter_size = 0.0025;
    const float penumbra_filter_size = 0.008;
    const float light_size = 3.0; // affects how quickly shadows become soft

    const float gradient_noise = InterleavedGradientNoise(texcoord * vec2(uvec2(scene.resolution_x, scene.resolution_y)));

    float total_blocker_depth = 0.0;
    float num_blockers = 0.0;


#if defined(HYP_SHADOW_SAMPLES_16) || defined(HYP_PENUMBRA_SAMPLES_16)
    #define HYP_DEF_VOGEL_DISK(iter_index) \
        vec2 vogel_##iter_index = VogelDisk(iter_index, 16, gradient_noise)
#else
    #define HYP_DEF_VOGEL_DISK(iter_index) \
        vec2 vogel_##iter_index = VogelDisk(iter_index, 8, gradient_noise)
#endif

    HYP_DEF_VOGEL_DISK(0);
    HYP_DEF_VOGEL_DISK(2);
    HYP_DEF_VOGEL_DISK(4);
    HYP_DEF_VOGEL_DISK(6);
    HYP_DEF_VOGEL_DISK(8);
    HYP_DEF_VOGEL_DISK(10);
    HYP_DEF_VOGEL_DISK(12);
    HYP_DEF_VOGEL_DISK(14);

#if defined(HYP_SHADOW_SAMPLES_16) || defined(HYP_PENUMBRA_SAMPLES_16)
    HYP_DEF_VOGEL_DISK(1);
    HYP_DEF_VOGEL_DISK(3);
    HYP_DEF_VOGEL_DISK(5);
    HYP_DEF_VOGEL_DISK(7);
    HYP_DEF_VOGEL_DISK(9);
    HYP_DEF_VOGEL_DISK(11);
    HYP_DEF_VOGEL_DISK(13);
    HYP_DEF_VOGEL_DISK(15);
#endif

#undef HYP_DEF_VOGEL_DISK

#if defined(HYP_SHADOW_PENUMBRA) && HYP_SHADOW_PENUMBRA

// vectorization
#define HYP_DO_SHADOW_PENUMBRA(iter_index0, iter_index1, iter_index2, iter_index3) \
    { \
        vec4 blocker_samples = vec4( \
            texture(shadow_maps[index], coord.xy + (vogel_##iter_index0 * /*shadow_pcf_kernel[iter_index0] **/ penumbra_filter_size)).r, \
            texture(shadow_maps[index], coord.xy + (vogel_##iter_index1 * /*shadow_pcf_kernel[iter_index1] **/ penumbra_filter_size)).r, \
            texture(shadow_maps[index], coord.xy + (vogel_##iter_index2 * /*shadow_pcf_kernel[iter_index2] **/ penumbra_filter_size)).r, \
            texture(shadow_maps[index], coord.xy + (vogel_##iter_index3 * /*shadow_pcf_kernel[iter_index3] **/ penumbra_filter_size)).r \
        ); \
        vec4 are_samples_blocking = vec4(lessThan(blocker_samples, vec4(shadow_map_depth))); \
        total_blocker_depth += dot(blocker_samples * are_samples_blocking, vec4(1.0)); \
        num_blockers += dot(are_samples_blocking, vec4(1.0)); \
    }
    
#ifdef HYP_PENUMBRA_SAMPLES_8
    HYP_DO_SHADOW_PENUMBRA(0, 2, 4, 6)
    HYP_DO_SHADOW_PENUMBRA(8, 10, 12, 14)
#elif defined(HYP_PENUMBRA_SAMPLES_4)
    HYP_DO_SHADOW_PENUMBRA(0, 4, 8, 12)
#endif

#undef HYP_DO_SHADOW_PENUMBRA

    float penumbra_mask = num_blockers > 0.0 ? AvgBlockerDepthToPenumbra(light_size, total_blocker_depth / max(num_blockers, 0.0001), shadow_map_depth) : 0.0;
#else
    float penumbra_mask = 1.0;
#endif
    
    float shadowness = 0.0;

#define HYP_DO_SHADOW(iter_index) \
    { \
        shadowness += GetShadowStandard(index, pos, /*vogel_##iter_index **/ shadow_pcf_kernel[iter_index] * 0.0008 + (vogel_##iter_index * penumbra_mask * shadow_filter_size), NdotL); \
    }

#ifdef HYP_SHADOW_SAMPLES_8
    HYP_DO_SHADOW(0)
    HYP_DO_SHADOW(2)
    HYP_DO_SHADOW(4)
    HYP_DO_SHADOW(6)
    HYP_DO_SHADOW(8)
    HYP_DO_SHADOW(10)
    HYP_DO_SHADOW(12)
    HYP_DO_SHADOW(14)

    shadowness /= 8.0;
#elif defined(HYP_SHADOW_SAMPLES_4)
    HYP_DO_SHADOW(0)
    HYP_DO_SHADOW(4)
    HYP_DO_SHADOW(8)
    HYP_DO_SHADOW(12)

    shadowness /= 4.0;
#endif

#undef HYP_DO_SHADOW

    return shadowness;
}


float GetShadow(uint index, vec3 position, float NdotL)
{
#if HYP_SHADOW_PENUMBRA
    return GetShadowContactHardened(index, position, NdotL);
#elif HYP_SHADOW_VARIANCE
    return GetShadowVariance(index, position, NdotL);
#else
    return GetShadowStandard(index, position, vec2(0.0), NdotL);
#endif
}

#endif
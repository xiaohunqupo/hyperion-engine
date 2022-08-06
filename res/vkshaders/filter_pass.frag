#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_EXT_scalar_block_layout : enable
#extension GL_EXT_nonuniform_qualifier : enable

layout(location=0) in vec3 v_position;
layout(location=1) in vec3 v_normal;
layout(location=2) in vec2 v_texcoord0;

// TODO: read in from prev pass.

layout(location=0) out vec4 color_output;

#include "include/gbuffer.inc"
#include "include/scene.inc"
#include "include/noise.inc"
#include "include/shared.inc"

const float diffarea = 0.3; //self-shadowing reduction
const float gdisplace = 0.4; //gauss bell center //0.4

vec2 texcoord = v_texcoord0;//vec2(v_texcoord0.x, 1.0 - v_texcoord0.y);

#define PI 3.14159265359
#define SSAO_NOISE 1
#define SSAO_NOISE_AMOUNT 0.0002
#define SSAO_MIST 0
#define SSAO_MIST_START 0.0
#define SSAO_MIST_END 0.01
#define CAP_MIN_DISTANCE 0.0001
#define CAP_MAX_DISTANCE 0.1
#define SSAO_SAMPLES 35 // NOTE: Even numbers breaking on linux nvidia drivers ??
#define SSAO_STRENGTH 0.65
#define SSAO_CLAMP_AMOUNT 0.125
#define SSAO_RADIUS 3.0
#define SSAO_ENABLED 1

vec2 GetNoise(vec2 coord) //generating noise/pattern texture for dithering
{
    float width = float(scene.resolution_x);
    float height = float(scene.resolution_y);
    
    float noise_x = ((fract(1.0 - coord.s * (width / 2.0)) * 0.25) + (fract(coord.t * (height / 2.0)) * 0.75)) * 2.0 - 1.0;
    float noise_y = ((fract(1.0 - coord.s * (width / 2.0)) * 0.75) + (fract(coord.t * (height / 2.0)) * 0.25)) * 2.0 - 1.0;

#if SSAO_NOISE
    noise_x = clamp(fract(sin(dot(coord, vec2(12.9898, 78.233))) * 43758.5453), 0.0, 1.0) * 2.0 - 1.0;
    noise_y = clamp(fract(sin(dot(coord, vec2(12.9898, 78.233) * 2.0)) * 43758.5453), 0.0, 1.0) * 2.0 - 1.0;
#endif

    return vec2(noise_x, noise_y) * SSAO_NOISE_AMOUNT;
}

#if SSAO_MIST
float CalculateMist()
{
    float depth = SampleGBuffer(gbuffer_depth_texture, texcoord).r;
    float depth_mist = -CAP_MAX_DISTANCE * CAP_MIN_DISTANCE / (depth * (CAP_MAX_DISTANCE - CAP_MIN_DISTANCE) - CAP_MAX_DISTANCE);
    
    return clamp((depth_mist - SSAO_MIST_START) / SSAO_MIST_END, 0.0, 1.0);
}
#endif

float ReadDepth(vec2 coord)
{
    float depth = SampleGBuffer(gbuffer_depth_texture, coord).r;
    
    if (coord.x < 0.0 || coord.y < 0.0) {
        return 1.0;
    }
    
    float z_n = 2.0 * depth - 1.0;
        
    return (2.0 * CAP_MIN_DISTANCE) / (CAP_MAX_DISTANCE + CAP_MIN_DISTANCE - z_n * (CAP_MAX_DISTANCE - CAP_MIN_DISTANCE));
}

int CompareDepthsFar(float depth1, float depth2)
{
    float garea = 2.0; //gauss bell width
    float diff = (depth1 - depth2) * 100.0; //depth difference (0-100)
    //reduce left bell width to avoid self-shadowing
    return int(diff >= gdisplace);
}

float CompareDepths(float depth1, float depth2)
{
    float diff = (depth1 - depth2) * 100.0; //depth difference (0-100)
    //reduce left bell width to avoid self-shadowing
    float garea = (diff < gdisplace) ? diffarea : 2.0; // gauss bell width

    return pow(2.7182, -2.0 * (diff - gdisplace) * (diff - gdisplace) / (garea * garea));
}

float CalculateAO(float depth, float dw, float dh)
{
    float dd = (1.0-depth)*SSAO_RADIUS;

    float temp = 0.0;
    float temp2 = 0.0;
    float coordw = texcoord.x + dw*dd;
    float coordh = texcoord.y + dh*dd;
    float coordw2 = texcoord.x - dw*dd;
    float coordh2 = texcoord.y - dh*dd;

    vec2 coord = vec2(coordw, coordh);
    vec2 coord2 = vec2(coordw2, coordh2);

    float cd = ReadDepth(coord);
    int far = CompareDepthsFar(depth, cd);
    temp = CompareDepths(depth, cd);
    //DEPTH EXTRAPOLATION:
    if (far > 0) {
        temp2 = CompareDepths(ReadDepth(coord2), depth);
        temp += (1.0-temp)*temp2;
    }

    return temp;
}

#define HYP_GTAO_NUM_CIRCLES 2
#define HYP_GTAO_NUM_SLICES  2
#define HYP_GTAO_RADIUS      80.0
#define HYP_GTAO_THICKNESS   0.75
#define HYP_GTAO_POWER       4.0

const float spatial_offsets[] = { 0.0, 0.5, 0.25, 0.75 };
const float temporal_rotations[] = { 60, 300, 180, 240, 120, 0 };

float fov_rad = HYP_FMATH_DEG2RAD(scene.camera_fov);

float tan_half_fov = tan(fov_rad * 0.5);
float inv_tan_half_fov = 1.0 / tan_half_fov; //-scene.view[1][1];
vec2 focal_len = vec2(inv_tan_half_fov * (float(scene.resolution_y) / float(scene.resolution_x)), inv_tan_half_fov);
vec2 inv_focal_len = vec2(1.0) / focal_len;

vec4 uv_to_view = vec4(2.0 * inv_focal_len.x, 2.0 * inv_focal_len.y, -1.0 * inv_focal_len.x, -1.0 * inv_focal_len.y);

float GTAOOffsets(vec2 uv)
{
    ivec2 position = ivec2(uv * vec2(scene.resolution_x, scene.resolution_y));
    return 0.25 * float((position.y - position.x) & 3);
}

mat4 inv_view_proj = inverse(scene.projection * scene.view);

vec3 GTAOGetPosition(vec2 uv)
{
    const float depth = SampleGBuffer(gbuffer_depth_texture, uv).r;

    const float linear_depth = LinearDepth(scene.projection, depth);
    return vec3((vec2(uv.x, 1.0 - uv.y) * uv_to_view.xy + uv_to_view.zw) * linear_depth, linear_depth);
}

vec3 GTAOGetNormal(vec2 uv)
{
    vec3 normal = DecodeNormal(SampleGBuffer(gbuffer_normals_texture, uv));
    vec3 view_normal = normalize((scene.view * vec4(normal, 0.0)).xyz);
    return view_normal;
}

float GTAOIntegrateUniformWeight(vec2 h)
{
    vec2 arc = vec2(1.0) - cos(h);

    return arc.x + arc.y;
}

float GTAOIntegrateArcCosWeight(vec2 h, float n)
{
    vec2 arc = -cos(2.0 * h - n) + cos(n) + 2.0 * h * sin(n);
    return 0.25 * (arc.x + arc.y);
}

vec4 GTAOGetDetails(vec2 uv)
{
    
    const float projected_scale = float(scene.resolution_y) / (tan_half_fov * 2.0) * 0.5;

    /*! TODO! global counter variable for temporal stuff */
    const float temporal_offset = spatial_offsets[0];
    // const float temporal_noise = InterleavedGradientNoise(uv * vec2(scene.resolution_x, scene.resolution_y)); // TODO!
    const float temporal_rotation = temporal_rotations[0]; // TODO!

    const float noise_offset = GTAOOffsets(vec2(uv.x, 1.0 - uv.y));
    const float noise_direction = InterleavedGradientNoise(vec2(uv.x, 1.0 - uv.y) * vec2(scene.resolution_x, scene.resolution_y));
    float ray_step = fract(noise_offset + temporal_offset);

    const vec3 position = GTAOGetPosition(uv);
    const float depth = SampleGBuffer(gbuffer_depth_texture, uv).r;
    const vec3 world_position = ReconstructWorldSpacePositionFromDepth(inv_view_proj, uv, depth).xyz;
    const vec3 N = GTAOGetNormal(uv);
    const vec3 V = normalize(vec3(0.0) - position);

    const vec2 texel_size = vec2(1.0) / vec2(scene.resolution_x, scene.resolution_y);

    float step_radius = max(min((HYP_GTAO_RADIUS * projected_scale) / position.z, 512.0), float(HYP_GTAO_NUM_SLICES)) / float(HYP_GTAO_NUM_SLICES + 1);

    vec3 bent_normal = vec3(0.0);
    float occlusion = 0.0;

    vec2 falloff;

    for (int i = 0; i < HYP_GTAO_NUM_CIRCLES; i++) {
        const float angle = (float(i) + noise_direction + (temporal_rotation / 360.0)) * (HYP_FMATH_PI / float(HYP_GTAO_NUM_CIRCLES));
        const vec3 slice_dir = vec3(cos(angle), sin(angle), 0.0);

        vec2 h = vec2(-1.0);

        vec2 uv_offset;
        vec4 uv_slice;
        vec3 ds, dt;
        vec2 dsdt;
        vec2 dsdt_length;
        vec2 H;

        for (int j = 0; j < HYP_GTAO_NUM_SLICES; j++) {
            uv_offset = (slice_dir.xy * texel_size) * max(step_radius * (float(j) + ray_step), float(j + 1)) * vec2(1.0, -1.0);
            uv_slice = uv.xyxy + vec4(uv_offset, -uv_offset);

            ds = GTAOGetPosition(uv_slice.xy) - position;
            dt = GTAOGetPosition(uv_slice.zw) - position;

            dsdt = vec2(dot(ds, ds), dot(dt, dt));
            dsdt_length = inversesqrt(dsdt);

            falloff = Saturate(dsdt * (2.0 / max(HYP_FMATH_SQR(HYP_GTAO_RADIUS), 0.0001)));

            H = vec2(dot(ds, V), dot(dt, V)) * dsdt_length;
            h = mix(
                mix(H, h, falloff),
                mix(H, h, HYP_GTAO_THICKNESS),
                greaterThan(H, h)
            );

            // h.x = max(h.x, H.x - falloff.x);
            // h.y = max(h.y, H.y - falloff.y);
        }

        vec3 plane_normal = normalize(cross(slice_dir, V));
        vec3 tangent = cross(V, plane_normal);
        vec3 projected_normal = N - plane_normal * dot(N, plane_normal);
        float projected_length = length(projected_normal);

        float cos_n = clamp(dot(normalize(projected_normal), V), -1.0, 1.0);
        float n = -sign(dot(projected_normal, tangent)) * acos(cos_n);

        h = acos(clamp(h, vec2(-1.0), vec2(1.0)));
        h.x = n + max(-h.x - n, -HYP_FMATH_PI_HALF);
        h.y = n + min(h.y - n, HYP_FMATH_PI_HALF);

        const float bent_angle = (h.x + h.y) * 0.5;

        bent_normal += V * cos(bent_angle) - tangent * sin(bent_angle);
        occlusion += projected_length * GTAOIntegrateArcCosWeight(h, n);
    }

    bent_normal = normalize(normalize(bent_normal) - (V * 0.5));
    occlusion = Saturate(pow(occlusion / float(HYP_GTAO_NUM_CIRCLES), HYP_GTAO_POWER));

    return vec4(bent_normal, occlusion);
}

void main()
{
#if SSAO_ENABLED
    
    // color_output = vec4((inverse(scene.view) * (vec4(GTAOGetDetails(texcoord).xyz, 0.0)) * 0.5 + 0.5).xyz, 1.0);
    color_output = vec4(GTAOGetDetails(texcoord).aaa, 1.0);
    return;


    float width = float(scene.resolution_x);
    float height = float(scene.resolution_y);

    vec2 noise = GetNoise(texcoord);
    float depth = ReadDepth(texcoord);

    float w = (1.0 / width) / clamp(depth, SSAO_CLAMP_AMOUNT, 1.0) + (noise.x * (1.0 - noise.x));
    float h = (1.0 / height) / clamp(depth, SSAO_CLAMP_AMOUNT, 1.0) + (noise.y * (1.0 - noise.y));

    float pw = 0.0;
    float ph = 0.0;

    float ao = 0.0;

    float dl = PI * (3.0 - sqrt(5.0));
    float dz = 1.0 / float(SSAO_SAMPLES);
    float l = 0.0;
    float z = 1.0 - dz/2.0;

    for (int i = 0; i < SSAO_SAMPLES; i++) {
        float r = sqrt(1.0 - z);

        pw = cos(l) * r;
        ph = sin(l) * r;
        ao += CalculateAO(depth, pw * w, ph * h);
        z = z - dz;
        l = l + dl;
    }

    ao /= float(SSAO_SAMPLES);
    ao *= SSAO_STRENGTH;
    ao = 1.0-ao;

#if SSAO_MIST
    ao = mix(ao, 1.0, CalculateMist());
#endif
    
#else
    float ao = 1.0;
#endif
    
    color_output = vec4(ao);
}

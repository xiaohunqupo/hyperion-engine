#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_EXT_nonuniform_qualifier : enable
#extension GL_EXT_scalar_block_layout : enable

#include "include/defines.inc"

layout(location=0) in vec3 v_position;
layout(location=1) in vec3 v_normal;
layout(location=2) in vec2 v_texcoord0;
layout(location=4) in vec3 v_tangent;
layout(location=5) in vec3 v_bitangent;
layout(location=7) in flat vec3 v_camera_position;
layout(location=8) in mat3 v_tbn_matrix;

layout(location=0) out vec4 output_color;

#include "include/scene.inc"
#include "include/material.inc"
#include "include/object.inc"
#include "include/packing.inc"

#define HYP_CUBEMAP_AMBIENT 0.1

void main()
{
    vec3 view_vector = normalize(v_camera_position - v_position);
    vec3 N = normalize(v_normal);

    output_color = material.albedo;

    vec2 texcoord = v_texcoord0 * material.uv_scale;

    if (HAS_TEXTURE(MATERIAL_TEXTURE_ALBEDO_map)) {
        vec4 albedo_texture = SAMPLE_TEXTURE(MATERIAL_TEXTURE_ALBEDO_map, texcoord);

        if (albedo_texture.a < 0.5) {
            discard;
        }

        output_color *= albedo_texture;
    }

    vec3 L = light.position.xyz;
    L -= v_position.xyz * float(min(light.type, 1));
    L = normalize(L);

    float NdotL = max(0.0001, dot(N, L));

    output_color *= clamp(NdotL + HYP_CUBEMAP_AMBIENT, 0.0, 1.0);
    // output_color.rgb = pow(output_color.rgb, vec3(1.0 / 2.2));
}

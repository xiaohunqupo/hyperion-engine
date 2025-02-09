#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_EXT_scalar_block_layout : enable
#extension GL_EXT_nonuniform_qualifier : enable

#include "include/defines.inc"

layout(location=0) in vec3 v_position;
layout(location=1) in vec2 v_texcoord0;

layout(location=0) out vec4 output_color;

#include "include/env_probe.inc"
#include "include/gbuffer.inc"
#include "include/material.inc"
#include "include/PostFXSample.inc"
#include "include/scene.inc"
#include "include/brdf.inc"

vec2 texcoord = v_texcoord0;

#include "include/shadows.inc"
#include "include/PhysicalCamera.inc"

void main()
{
    vec4 albedo = SampleGBuffer(gbuffer_albedo_texture, texcoord);
    vec4 normal = vec4(DecodeNormal(SampleGBuffer(gbuffer_normals_texture, texcoord)), 1.0);

    vec4 tangents_buffer = SampleGBuffer(gbuffer_tangents_texture, texcoord);

    vec3 tangent = UnpackNormalVec2(tangents_buffer.xy);
    vec3 bitangent = UnpackNormalVec2(tangents_buffer.zw);

    float depth = SampleGBuffer(gbuffer_depth_texture, texcoord).r;
    vec4 position = ReconstructWorldSpacePositionFromDepth(inverse(scene.projection), inverse(scene.view), texcoord, depth);
    vec4 material = SampleGBuffer(gbuffer_material_texture, texcoord); /* r = roughness, g = metalness, b = ?, a = AO */

    const float roughness = material.r;
    const float metalness = material.g;

    bool perform_lighting = albedo.a > 0.0;

    const float material_reflectance = 0.5;
    // dialetric f0
    const float reflectance = 0.16 * material_reflectance * material_reflectance;
    vec4 F0 = vec4(albedo.rgb * metalness + (reflectance * (1.0 - metalness)), 1.0);

    vec3 N = normalize(normal.xyz);
    vec3 T = normalize(tangent.xyz);
    vec3 B = normalize(bitangent.xyz);
    vec3 V = normalize(scene.camera_position.xyz - position.xyz);

    float NdotV = max(0.0001, dot(N, V));
 
    vec4 result = vec4(0.0);
    
    float ao = 1.0;
    float shadow = 1.0;
    
    if (perform_lighting) {
        ao = SampleEffectPre(0, v_texcoord0, vec4(1.0)).r * material.a;

        vec3 L = light.position.xyz;
        L -= position.xyz * float(min(light.type, 1));
        L = normalize(L);

        const vec3 H = normalize(L + V);

        const float NdotL = max(0.0001, dot(N, L));
        const float NdotH = max(0.0001, dot(N, H));
        const float LdotH = max(0.0001, dot(L, H));
        const float HdotV = max(0.0001, dot(H, V));

        if (light.shadow_map_index != ~0u) {
            shadow = GetShadow(light.shadow_map_index, position.xyz, NdotL);
        }

        vec4 light_color = unpackUnorm4x8(light.color_encoded);

        vec3 F90 = vec3(clamp(dot(F0.rgb, vec3(50.0 * 0.33)), 0.0, 1.0));
        const float D = Trowbridge(NdotH, roughness);//DistributionGGX(N, H, roughness);
        const float G = CookTorranceG(NdotL, NdotV, HdotV, NdotH);//V_SmithGGXCorrelated(roughness, NdotV, NdotL);
        const vec4 F = vec4(SchlickFresnel(F0.rgb, F90, LdotH), 1.0);
        // const vec4 F = vec4(SchlickFresnelRoughness(F0.rgb, roughness, LdotH), 1.0);

        const float perceptual_roughness = sqrt(roughness);
        const vec2 AB = BRDFMap(perceptual_roughness, NdotV);
        const vec4 dfg = F * AB.x + AB.y;
        const vec4 E = vec4(mix(dfg.xxx, dfg.yyy, F0.rgb), 1.0);
        const vec4 energy_compensation = vec4(1.0 + F0.rgb * (1.0 / dfg.y - 1.0), 1.0);

        const vec4 diffuse_color = vec4(albedo.rgb * (1.0 - metalness), albedo.a);
        const vec4 specular_lobe = D * G * F;

        float dist = max(length(light.position.xyz - position.xyz), light.radius);
        float attenuation = (light.type == HYP_LIGHT_TYPE_POINT) ?
            (light.intensity / max(dist * dist, 0.0001)) : 1.0;
        
        const vec4 kD = vec4((1.0 - F.rgb) * (1.0 - metalness), 1.0);

        vec4 specular = specular_lobe;

        vec4 diffuse_lobe = diffuse_color * (1.0 / HYP_FMATH_PI);
        vec4 diffuse = diffuse_lobe;

        vec4 direct_component = diffuse + specular * energy_compensation;
        
        direct_component = CalculateFogLinear(direct_component, vec4(0.7, 0.8, 1.0, 1.0), position.xyz, scene.camera_position.xyz, (scene.camera_near + scene.camera_far) * 0.5, scene.camera_far);
        direct_component.rgb *= (exposure * light.intensity);
        result += direct_component * (light_color * ao * NdotL * shadow * attenuation);
    } else {
        result = albedo;
    }

    output_color = result;
}

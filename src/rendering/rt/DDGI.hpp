/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_DDGI_HPP
#define HYPERION_DDGI_HPP

#include <rendering/Shader.hpp>
#include <rendering/Buffers.hpp>

#include <rendering/backend/RendererStructs.hpp>
#include <rendering/backend/RenderObject.hpp>
#include <rendering/backend/rt/RendererRaytracingPipeline.hpp>

#include <math/BoundingBox.hpp>

#include <Types.hpp>

#include <random>

namespace hyperion {

using renderer::RTUpdateStateFlags;

class Engine;

enum ProbeSystemFlags : uint32
{
    PROBE_SYSTEM_FLAGS_NONE = 0x0,
    PROBE_SYSTEM_FLAGS_FIRST_RUN = 0x1
};

struct ProbeRayData
{
    Vec4f   direction_depth;
    Vec4f   origin;
    Vec4f   normal;
    Vec4f   color;
};

static_assert(sizeof(ProbeRayData) == 64);

struct DDGIInfo
{
    static constexpr uint32 irradiance_octahedron_size = 8;
    static constexpr uint32 depth_octahedron_size = 16;
    static constexpr Vec3u  probe_border = Vec3u { 2, 0, 2 };

    BoundingBox                 aabb;
    float                       probe_distance = 3.2f;
    uint32                      num_rays_per_probe = 64;

    HYP_FORCE_INLINE const Vec3f &GetOrigin() const
        { return aabb.min; }

    HYP_FORCE_INLINE Vec3u NumProbesPerDimension() const
    {
        const Vec3f probes_per_dimension = MathUtil::Ceil((aabb.GetExtent() / probe_distance) + Vec3f(probe_border));

        return Vec3u(probes_per_dimension);
    }
    
    HYP_FORCE_INLINE uint32 NumProbes() const
    {
        const Vec3u per_dimension = NumProbesPerDimension();

        return per_dimension.x * per_dimension.y * per_dimension.z;
    }
    
    HYP_FORCE_INLINE Vec2u GetImageDimensions() const
    {
        return { uint32(MathUtil::NextPowerOf2(NumProbes())), num_rays_per_probe };
    }
};

struct RotationMatrixGenerator
{
    Matrix4                                 matrix;
    std::random_device                      random_device;
    std::mt19937                            mt { random_device() };
    std::uniform_real_distribution<float>   angle { 0.0f, 359.0f };
    std::uniform_real_distribution<float>   axis { -1.0f, 1.0f };

    const Matrix4 &Next()
    {
        return matrix = Matrix4::Rotation({
            Vec3f { axis(mt), axis(mt), axis(mt) }.Normalize(),
            MathUtil::DegToRad(angle(mt))
        });
    }
};

struct Probe
{
    Vec3f   position;
};

class DDGI
{
public:
    HYP_API DDGI(DDGIInfo &&grid_info);
    DDGI(const DDGI &other)             = delete;
    DDGI &operator=(const DDGI &other)  = delete;
    HYP_API ~DDGI();

    const Array<Probe> &GetProbes() const
        { return m_probes; }

    void SetTLAS(const TLASRef &tlas)
        { m_tlas = tlas; }

    HYP_API void ApplyTLASUpdates(RTUpdateStateFlags flags);

    const GPUBufferRef &GetRadianceBuffer() const
        { return m_radiance_buffer; }

    const ImageRef &GetIrradianceImage() const
        { return m_irradiance_image; }

    const ImageViewRef &GetIrradianceImageView() const
        { return m_irradiance_image_view; }

    HYP_API void Init();
    HYP_API void Destroy();

    HYP_API void RenderProbes(Frame *frame);
    HYP_API void ComputeIrradiance(Frame *frame);

private:
    void CreatePipelines();
    void CreateUniformBuffer();
    void CreateStorageBuffers();
    void UpdateUniforms(Frame *frame);

    DDGIInfo                                    m_grid_info;
    Array<Probe>                                m_probes;
    
    FixedArray<uint32, max_frames_in_flight>    m_updates;

    ComputePipelineRef                          m_update_irradiance;
    ComputePipelineRef                          m_update_depth;
    ComputePipelineRef                          m_copy_border_texels_irradiance;
    ComputePipelineRef                          m_copy_border_texels_depth;

    ShaderRef                                   m_shader;

    RaytracingPipelineRef                       m_pipeline;

    GPUBufferRef                                m_uniform_buffer;
    GPUBufferRef                                m_radiance_buffer;

    ImageRef                                    m_irradiance_image;
    ImageViewRef                                m_irradiance_image_view;
    ImageRef                                    m_depth_image;
    ImageViewRef                                m_depth_image_view;

    TLASRef                                     m_tlas;

    DDGIUniforms                                m_uniforms;

    RotationMatrixGenerator                     m_random_generator;
    uint32                                      m_time;
};

} // namespace hyperion

#endif
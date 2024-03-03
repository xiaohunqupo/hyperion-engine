#ifndef HYPERION_V2_LIGHTMAP_RENDERER_HPP
#define HYPERION_V2_LIGHTMAP_RENDERER_HPP

#include <core/Base.hpp>
#include <core/lib/Queue.hpp>
#include <core/lib/Mutex.hpp>
#include <core/lib/AtomicVar.hpp>

#include <math/Triangle.hpp>
#include <math/Ray.hpp>

#include <scene/Scene.hpp>

#include <rendering/lightmapper/LightmapUVBuilder.hpp>

#include <rendering/RenderGroup.hpp>
#include <rendering/RenderComponent.hpp>
#include <rendering/EnvProbe.hpp>
#include <rendering/Mesh.hpp>

#include <rendering/backend/RendererFrame.hpp>
#include <rendering/backend/RendererBuffer.hpp>
#include <rendering/backend/rt/RendererRaytracingPipeline.hpp>

namespace hyperion::v2 {

using renderer::Frame;
using renderer::Image;
using renderer::ImageView;

struct LightmapHitsBuffer;

enum LightmapTraceMode
{
    LIGHTMAP_TRACE_MODE_GPU,
    LIGHTMAP_TRACE_MODE_CPU
};

struct LightmapRay
{
    Ray         ray;
    ID<Mesh>    mesh_id;
    uint        triangle_index;
    uint        texel_index;

    bool operator==(const LightmapRay &other) const
    {
        return ray == other.ray
            && mesh_id == other.mesh_id
            && triangle_index == other.triangle_index
            && texel_index == other.texel_index;
    }

    bool operator!=(const LightmapRay &other) const
        { return !(*this == other); }
};

constexpr uint max_ray_hits_gpu = 512 * 512;
constexpr uint max_ray_hits_cpu = 64 * 64;

struct alignas(16) LightmapHit
{
    Vec4f   color;
};

static_assert(sizeof(LightmapHit) == 16);

struct alignas(16) LightmapHitsBuffer
{
    FixedArray<LightmapHit, max_ray_hits_gpu>   hits;
};

static_assert(sizeof(LightmapHitsBuffer) == max_ray_hits_gpu * 16);

class LightmapPathTracer
{
public:
    LightmapPathTracer(Handle<TLAS> tlas);
    LightmapPathTracer(const LightmapPathTracer &other)                 = delete;
    LightmapPathTracer &operator=(const LightmapPathTracer &other)      = delete;
    LightmapPathTracer(LightmapPathTracer &&other) noexcept             = delete;
    LightmapPathTracer &operator=(LightmapPathTracer &&other) noexcept  = delete;
    ~LightmapPathTracer();

    const RaytracingPipelineRef &GetPipeline() const
        { return m_raytracing_pipeline; }

    void Create();
    
    void ReadHitsBuffer(LightmapHitsBuffer *ptr, uint frame_index);
    void Trace(Frame *frame, const Array<LightmapRay> &rays, uint32 ray_offset);

private:
    void CreateUniformBuffer();
    void UpdateUniforms(Frame *frame, uint32 ray_offset);

    Handle<TLAS>                                        m_tlas;
    
    FixedArray<GPUBufferRef, max_frames_in_flight>      m_uniform_buffers;
    FixedArray<GPUBufferRef, max_frames_in_flight>      m_rays_buffers;
    FixedArray<GPUBufferRef, max_frames_in_flight>      m_hits_buffers;
    HeapArray<LightmapHitsBuffer, max_frames_in_flight> m_previous_hits_buffers;
    RaytracingPipelineRef                               m_raytracing_pipeline;
};

class LightmapJob
{
public:
    static constexpr uint num_multisamples = 8;

    LightmapJob(Scene *scene, Array<LightmapEntity> entities);
    LightmapJob(Scene *scene, Array<LightmapEntity> entities, HashMap<ID<Mesh>, Array<Triangle>> triangle_cache);

    LightmapJob(const LightmapJob &other)                   = delete;
    LightmapJob &operator=(const LightmapJob &other)        = delete;
    LightmapJob(LightmapJob &&other) noexcept               = delete;
    LightmapJob &operator=(LightmapJob &&other) noexcept    = delete;
    ~LightmapJob()                                          = default;
    
    LightmapUVMap &GetUVMap()
        { return m_uv_map; }

    const LightmapUVMap &GetUVMap() const
        { return m_uv_map; }

    Scene *GetScene() const
        { return m_scene; }

    const Array<LightmapEntity> &GetEntities() const
        { return m_entities; }

    uint32 GetTexelIndex() const
        { return m_texel_index; }

    const Array<uint> &GetTexelIndices() const
        { return m_texel_indices; }

    const Array<LightmapRay> &GetPreviousFrameRays(uint frame_index) const
        { return m_previous_frame_rays[frame_index]; }

    void SetPreviousFrameRays(uint frame_index, Array<LightmapRay> rays)
        { m_previous_frame_rays[frame_index] = std::move(rays); }

    void Start();

    void GatherRays(uint max_ray_hits, Array<LightmapRay> &out_rays);

    /*! \brief Trace rays on the CPU.
     *  \param rays The rays to trace.    
     */
    void TraceRaysOnCPU(const Array<LightmapRay> &rays);

    /*! \brief Integrate ray hits into the lightmap.
     *  \param rays The rays that were traced.
     *  \param hits The hits to integrate.
     *  \param num_hits The number of hits (must be the same as the number of rays).
     */
    void IntegrateRayHits(const LightmapRay *rays, const LightmapHit *hits, uint num_hits);

    bool IsCompleted() const;
    bool IsStarted() const;
    bool IsReady() const
        { return m_is_ready.Get(MemoryOrder::RELAXED); }

private:
    void BuildUVMap();
    Optional<LightmapHit> TraceSingleRayOnCPU(const LightmapRay &ray);

    Scene                                                   *m_scene;
    Array<LightmapEntity>                                   m_entities;

    LightmapUVMap                                           m_uv_map;

    Array<uint>                                             m_texel_indices; // flattened texel indices, flattened so that meshes are grouped together

    HashMap<ID<Mesh>, Array<Triangle>>                      m_triangle_cache; // for cpu tracing

    FixedArray<Array<LightmapRay>, max_frames_in_flight>    m_previous_frame_rays;

    AtomicVar<bool>                                         m_is_ready;
    AtomicVar<bool>                                         m_is_started;
    uint                                                    m_texel_index;
};

class LightmapRenderer : public RenderComponent<LightmapRenderer>
{
public:
    LightmapRenderer(Name name);
    virtual ~LightmapRenderer() override = default;

    void AddJob(UniquePtr<LightmapJob> &&job)
    {
        Mutex::Guard guard(m_queue_mutex);

        m_queue.Push(std::move(job));

        m_num_jobs.Increment(1u, MemoryOrder::RELAXED);
    }

    void Init();
    void InitGame();

    void OnRemoved();
    void OnUpdate(GameCounter::TickUnit delta);
    void OnRender(Frame *frame);

private:
    void HandleCompletedJob(LightmapJob *job);

    virtual void OnComponentIndexChanged(RenderComponentBase::Index new_index, RenderComponentBase::Index prev_index) override
        { }

    LightmapTraceMode               m_trace_mode;

    UniquePtr<LightmapPathTracer>   m_path_tracer;

    Queue<UniquePtr<LightmapJob>>   m_queue;
    Mutex                           m_queue_mutex;
    AtomicVar<uint>                 m_num_jobs;
};

} // namespace hyperion::v2

#endif
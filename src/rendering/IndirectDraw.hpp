/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */
#ifndef HYPERION_INDIRECT_DRAW_HPP
#define HYPERION_INDIRECT_DRAW_HPP

#include <rendering/Buffers.hpp>
#include <rendering/CullData.hpp>

#include <core/containers/FixedArray.hpp>
#include <core/containers/Array.hpp>

#include <rendering/backend/RendererStructs.hpp>
#include <rendering/backend/RenderObject.hpp>

#include <Constants.hpp>
#include <Types.hpp>

namespace hyperion {

using renderer::IndirectDrawCommand;
using renderer::Result;

class Mesh;
class Material;
class Engine;
class Entity;

struct RenderCommand_CreateIndirectRenderer;
struct RenderCommand_DestroyIndirectRenderer;

struct DrawCall;

struct DrawCommandData
{
    uint32  draw_command_index;
};

class IndirectDrawState
{
public:
    static constexpr uint batch_size = 256u;
    static constexpr uint initial_count = batch_size;
    // should sizes be scaled up to the next power of 2?
    static constexpr bool use_next_pow2_size = true;

    IndirectDrawState();
    ~IndirectDrawState();

    const GPUBufferRef &GetInstanceBuffer(uint frame_index) const
        { return m_instance_buffers[frame_index]; }

    const GPUBufferRef &GetIndirectBuffer(uint frame_index) const
        { return m_indirect_buffers[frame_index]; }

    const Array<ObjectInstance> &GetInstances() const
        { return m_object_instances; }

    void Create();
    void Destroy();

    void PushDrawCall(const DrawCall &draw_call, DrawCommandData &out);
    void ResetDrawState();

    void UpdateBufferData(Frame *frame, bool *out_was_resized);

private:
    Array<ObjectInstance>                           m_object_instances;
    Array<IndirectDrawCommand>                      m_draw_commands;

    FixedArray<GPUBufferRef, max_frames_in_flight>  m_indirect_buffers;
    FixedArray<GPUBufferRef, max_frames_in_flight>  m_instance_buffers;
    FixedArray<GPUBufferRef, max_frames_in_flight>  m_staging_buffers;
    uint32                                          m_num_draw_commands;
    uint8                                           m_dirty_bits;
};

class IndirectRenderer
{
public:
    friend struct RenderCommand_CreateIndirectRenderer;
    friend struct RenderCommand_DestroyIndirectRenderer;

    IndirectRenderer();
    IndirectRenderer(const IndirectRenderer &)                  = delete;
    IndirectRenderer &operator=(const IndirectRenderer &)       = delete;
    IndirectRenderer(IndirectRenderer &&) noexcept              = delete;
    IndirectRenderer &operator=(IndirectRenderer &&) noexcept   = delete;
    ~IndirectRenderer();

    IndirectDrawState &GetDrawState()
        { return m_indirect_draw_state; }

    const IndirectDrawState &GetDrawState() const
        { return m_indirect_draw_state; }

    void Create();
    void Destroy();

    void ExecuteCullShaderInBatches(Frame *frame, const CullData &cull_data);

private:
    void RebuildDescriptors(Frame *frame);

    IndirectDrawState                                   m_indirect_draw_state;
    ComputePipelineRef                                  m_object_visibility;
    CullData                                            m_cached_cull_data;
    uint8                                               m_cached_cull_data_updated_bits;
};

} // namespace hyperion

#endif
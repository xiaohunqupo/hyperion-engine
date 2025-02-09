#ifndef HYPERION_V2_SVO_H
#define HYPERION_V2_SVO_H

#include "Base.hpp"
#include "Voxelizer.hpp"
#include "Compute.hpp"
#include <rendering/RenderComponent.hpp>
#include <rendering/backend/RendererStructs.hpp>

#include <core/Containers.hpp>

namespace hyperion::v2 {

using renderer::IndirectBuffer;
using renderer::StorageBuffer;
using renderer::DescriptorSet;
using renderer::ShaderVec2;

class SparseVoxelOctree
    : public EngineComponentBase<STUB_CLASS(SparseVoxelOctree)>,
      public RenderComponent<SparseVoxelOctree>
{
    static constexpr UInt32 min_nodes = 10000;
    static constexpr UInt32 max_nodes = 10000000;

    using OctreeNode = ShaderVec2<UInt32>;

public:
    static constexpr RenderComponentName component_name = RENDER_COMPONENT_SVO;

    SparseVoxelOctree();
    SparseVoxelOctree(const SparseVoxelOctree &other) = delete;
    SparseVoxelOctree &operator=(const SparseVoxelOctree &other) = delete;
    ~SparseVoxelOctree();

    Voxelizer *GetVoxelizer() const { return m_voxelizer.get(); }

    void Init(Engine *engine);
    void InitGame(Engine *engine); // init on game thread

    void OnUpdate(Engine *engine, GameCounter::TickUnit delta);
    void OnRender(Engine *engine, Frame *frame);

private:
    UInt32 CalculateNumNodes() const;
    void CreateBuffers(Engine *engine);
    void CreateDescriptors(Engine *engine);
    void CreateComputePipelines(Engine *engine);

    virtual void OnEntityAdded(Handle<Entity> &entity) override;
    virtual void OnEntityRemoved(Handle<Entity> &entity) override;
    virtual void OnEntityRenderableAttributesChanged(Handle<Entity> &entity) override;
    virtual void OnComponentIndexChanged(RenderComponentBase::Index new_index, RenderComponentBase::Index prev_index) override;

    void WriteMipmaps(Engine *engine);

    void BindDescriptorSets(
        Engine *engine,
        CommandBuffer *command_buffer,
        UInt frame_index,
        const ComputePipeline *pipeline
    ) const;

    FixedArray<UniquePtr<DescriptorSet>, max_frames_in_flight> m_descriptor_sets;

    std::unique_ptr<Voxelizer> m_voxelizer;
    std::unique_ptr<AtomicCounter> m_counter;

    std::unique_ptr<IndirectBuffer> m_indirect_buffer;
    std::unique_ptr<StorageBuffer> m_build_info_buffer;
    std::unique_ptr<StorageBuffer> m_octree_buffer;
    
    Handle<ComputePipeline> m_init_nodes;
    Handle<ComputePipeline> m_tag_nodes;
    Handle<ComputePipeline> m_alloc_nodes;
    Handle<ComputePipeline> m_modify_args;
    Handle<ComputePipeline> m_write_mipmaps;
};

} // namespace hyperion::v2

#endif
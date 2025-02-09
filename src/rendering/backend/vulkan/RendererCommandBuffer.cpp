#include <rendering/backend/RendererCommandBuffer.hpp>
#include <rendering/backend/RendererComputePipeline.hpp>
#include <rendering/backend/RendererGraphicsPipeline.hpp>
#include <rendering/backend/rt/RendererRaytracingPipeline.hpp>
#include <rendering/backend/RendererStructs.hpp>

#include <Types.hpp>

#include <rendering/backend/RendererFeatures.hpp>

namespace hyperion {
namespace renderer {

CommandBuffer::CommandBuffer(Type type)
    : m_type(type),
      m_command_buffer(VK_NULL_HANDLE),
      m_command_pool(VK_NULL_HANDLE)
{
}

CommandBuffer::~CommandBuffer()
{
    AssertThrowMsg(m_command_buffer == VK_NULL_HANDLE, "command buffer should have been destroyed");
}

Result CommandBuffer::Create(Device *device, VkCommandPool command_pool)
{
    AssertThrow(command_pool != VK_NULL_HANDLE);

    VkCommandBufferLevel level;

    switch (m_type) {
    case COMMAND_BUFFER_PRIMARY:
        level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        break;
    case COMMAND_BUFFER_SECONDARY:
        level = VK_COMMAND_BUFFER_LEVEL_SECONDARY;
        break;
    default:
        AssertThrowMsg(false, "Unsupported command buffer type");
    }

    VkCommandBufferAllocateInfo alloc_info{VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
    alloc_info.level = level;
    alloc_info.commandPool = command_pool;
    alloc_info.commandBufferCount = 1;

    m_command_pool = command_pool;

    HYPERION_VK_CHECK_MSG(
        vkAllocateCommandBuffers(device->GetDevice(), &alloc_info, &m_command_buffer),
        "Failed to allocate command buffer"
    );

    HYPERION_RETURN_OK;
}

Result CommandBuffer::Destroy(Device *device)
{
    AssertThrow(m_command_buffer != VK_NULL_HANDLE);
    AssertThrow(m_command_pool != VK_NULL_HANDLE);

    auto result = Result::OK;

    vkFreeCommandBuffers(device->GetDevice(), m_command_pool, 1, &m_command_buffer);
    m_command_buffer = VK_NULL_HANDLE;
    m_command_pool = VK_NULL_HANDLE;

    return result;
}

Result CommandBuffer::Begin(Device *device, const RenderPass *render_pass)
{
    VkCommandBufferInheritanceInfo inheritance_info { VK_STRUCTURE_TYPE_COMMAND_BUFFER_INHERITANCE_INFO };
    inheritance_info.subpass = 0;
    inheritance_info.framebuffer = VK_NULL_HANDLE;

    VkCommandBufferBeginInfo begin_info { VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };

    if (m_type == COMMAND_BUFFER_SECONDARY) {
        AssertThrowMsg(render_pass != nullptr, "Render pass not provided for secondary command buffer!");
        inheritance_info.renderPass = render_pass->GetHandle();

        begin_info.pInheritanceInfo = &inheritance_info;
        begin_info.flags |= VK_COMMAND_BUFFER_USAGE_RENDER_PASS_CONTINUE_BIT;
    }

    HYPERION_VK_CHECK_MSG(
        vkBeginCommandBuffer(m_command_buffer, &begin_info),
        "Failed to begin command buffer"
    );

    HYPERION_RETURN_OK;
}

Result CommandBuffer::End(Device *device)
{
    HYPERION_VK_CHECK_MSG(
        vkEndCommandBuffer(m_command_buffer),
        "Failed to end command buffer"
    );

    HYPERION_RETURN_OK;
}

Result CommandBuffer::Reset(Device *device)
{
    HYPERION_VK_CHECK_MSG(
        vkResetCommandBuffer(m_command_buffer, 0),
        "Failed to reset command buffer"
    );

    HYPERION_RETURN_OK;
}

Result CommandBuffer::SubmitSecondary(
    CommandBuffer *primary,
    const std::vector<std::unique_ptr<CommandBuffer>> &command_buffers
)
{
    const auto *vk_command_buffers = static_cast<VkCommandBuffer *>(alloca(sizeof(VkCommandBuffer) * command_buffers.size()));

    vkCmdExecuteCommands(
        primary->GetCommandBuffer(),
        static_cast<UInt32>(command_buffers.size()),
        vk_command_buffers
    );
    
    HYPERION_RETURN_OK;
}

Result CommandBuffer::SubmitPrimary(
    VkQueue queue,
    Fence *fence,
    SemaphoreChain *semaphore_chain
)
{
    VkSubmitInfo submit_info { VK_STRUCTURE_TYPE_SUBMIT_INFO };

    if (semaphore_chain != nullptr) {
        submit_info.waitSemaphoreCount = static_cast<UInt32>(semaphore_chain->m_wait_semaphores_view.size());
        submit_info.pWaitSemaphores = semaphore_chain->m_wait_semaphores_view.data();
        submit_info.signalSemaphoreCount = static_cast<UInt32>(semaphore_chain->m_signal_semaphores_view.size());
        submit_info.pSignalSemaphores = semaphore_chain->m_signal_semaphores_view.data();
        submit_info.pWaitDstStageMask = semaphore_chain->m_wait_semaphores_stage_view.data();
    } else {
        submit_info.waitSemaphoreCount = 0;
        submit_info.pWaitSemaphores = nullptr;
        submit_info.signalSemaphoreCount = 0;
        submit_info.pSignalSemaphores = nullptr;
        submit_info.pWaitDstStageMask = nullptr;
    }

    submit_info.commandBufferCount = 1;
    submit_info.pCommandBuffers = &m_command_buffer;

    HYPERION_VK_CHECK_MSG(
        vkQueueSubmit(queue, 1, &submit_info, fence->GetHandle()),
        "Failed to submit command"
    );

    HYPERION_RETURN_OK;
}

Result CommandBuffer::SubmitSecondary(CommandBuffer *primary)
{
    vkCmdExecuteCommands(
        primary->GetCommandBuffer(),
        1,
        &m_command_buffer
    );

    HYPERION_RETURN_OK;
}

void CommandBuffer::DrawIndexed(
    UInt32 num_indices,
    UInt32 num_instances,
    UInt32 instance_index
) const
{
    vkCmdDrawIndexed(
        m_command_buffer,
        num_indices,
        num_instances,
        0,
        0,
        instance_index
    );
}

void CommandBuffer::DrawIndexedIndirect(
    const GPUBuffer *buffer,
    UInt32 buffer_offset
) const
{
    vkCmdDrawIndexedIndirect(
        m_command_buffer,
        buffer->buffer,
        buffer_offset,
        1,
        static_cast<UInt32>(sizeof(IndirectDrawCommand))
    );
}

void CommandBuffer::BindDescriptorSet(
    const DescriptorPool &pool,
    const GraphicsPipeline *pipeline,
    DescriptorSet::Index set
) const
{
    BindDescriptorSet(
        pool,
        static_cast<const Pipeline *>(pipeline),
        VK_PIPELINE_BIND_POINT_GRAPHICS,
        set,
        set,
        nullptr,
        0
    );
}

void CommandBuffer::BindDescriptorSet(
    const DescriptorPool &pool,
    const GraphicsPipeline *pipeline,
    DescriptorSet::Index set,
    DescriptorSet::Index binding
) const
{
    BindDescriptorSet(
        pool,
        static_cast<const Pipeline *>(pipeline),
        VK_PIPELINE_BIND_POINT_GRAPHICS,
        set,
        binding,
        nullptr,
        0
    );
}

void CommandBuffer::BindDescriptorSet(
    const DescriptorPool &pool,
    const GraphicsPipeline *pipeline,
    DescriptorSet::Index set,
    DescriptorSet::Index binding,
    const UInt32 *offsets,
    size_t num_offsets
) const
{
    BindDescriptorSet(
        pool,
        static_cast<const Pipeline *>(pipeline),
        VK_PIPELINE_BIND_POINT_GRAPHICS,
        set,
        binding,
        offsets,
        num_offsets
    );
}

void CommandBuffer::BindDescriptorSets(
    const DescriptorPool &pool,
    const GraphicsPipeline *pipeline,
    const DescriptorSet::Index *sets,
    const DescriptorSet::Index *bindings,
    size_t num_descriptor_sets,
    const UInt32 *offsets,
    size_t num_offsets
) const
{
    BindDescriptorSets(
        pool,
        static_cast<const Pipeline *>(pipeline),
        VK_PIPELINE_BIND_POINT_GRAPHICS,
        sets,
        bindings,
        num_descriptor_sets,
        offsets,
        num_offsets
    );
}

void CommandBuffer::BindDescriptorSet(
    const DescriptorPool &pool,
    const GraphicsPipeline *pipeline,
    const DescriptorSet *descriptor_set,
    DescriptorSet::Index binding,
    const UInt32 *offsets,
    SizeType num_offsets
) const
{
    BindDescriptorSet(
        pool,
        static_cast<const Pipeline *>(pipeline),
        VK_PIPELINE_BIND_POINT_GRAPHICS,
        descriptor_set,
        binding,
        offsets,
        num_offsets
    );
}

void CommandBuffer::BindDescriptorSet(
    const DescriptorPool &pool,
    const ComputePipeline *pipeline,
    const DescriptorSet *descriptor_set,
    DescriptorSet::Index binding
) const
{
    BindDescriptorSet(
        pool,
        static_cast<const Pipeline *>(pipeline),
        VK_PIPELINE_BIND_POINT_COMPUTE,
        descriptor_set,
        binding,
        nullptr,
        0
    );
}

void CommandBuffer::BindDescriptorSet(
    const DescriptorPool &pool,
    const ComputePipeline *pipeline,
    const DescriptorSet *descriptor_set,
    DescriptorSet::Index binding,
    const UInt32 *offsets,
    SizeType num_offsets
) const
{
    BindDescriptorSet(
        pool,
        static_cast<const Pipeline *>(pipeline),
        VK_PIPELINE_BIND_POINT_COMPUTE,
        descriptor_set,
        binding,
        offsets,
        num_offsets
    );
}

void CommandBuffer::BindDescriptorSet(
    const DescriptorPool &pool,
    const ComputePipeline *pipeline,
    DescriptorSet::Index set
) const
{
    BindDescriptorSet(
        pool,
        static_cast<const Pipeline *>(pipeline),
        VK_PIPELINE_BIND_POINT_COMPUTE,
        set,
        set,
        nullptr,
        0
    );
}

void CommandBuffer::BindDescriptorSet(
    const DescriptorPool &pool,
    const ComputePipeline *pipeline,
    DescriptorSet::Index set,
    DescriptorSet::Index binding
) const
{
    BindDescriptorSet(
        pool,
        static_cast<const Pipeline *>(pipeline),
        VK_PIPELINE_BIND_POINT_COMPUTE,
        set,
        binding,
        nullptr,
        0
    );
}

void CommandBuffer::BindDescriptorSet(
    const DescriptorPool &pool,
    const ComputePipeline *pipeline,
    DescriptorSet::Index set,
    DescriptorSet::Index binding,
    const UInt32 *offsets,
    SizeType num_offsets
) const
{
    BindDescriptorSet(
        pool,
        static_cast<const Pipeline *>(pipeline),
        VK_PIPELINE_BIND_POINT_COMPUTE,
        set,
        binding,
        offsets,
        num_offsets
    );
}

void CommandBuffer::BindDescriptorSets(
    const DescriptorPool &pool,
    const ComputePipeline *pipeline,
    const DescriptorSet::Index *sets,
    const DescriptorSet::Index *bindings,
    SizeType num_descriptor_sets,
    const UInt32 *offsets,
    SizeType num_offsets
) const
{
    BindDescriptorSets(
        pool,
        static_cast<const Pipeline *>(pipeline),
        VK_PIPELINE_BIND_POINT_COMPUTE,
        sets,
        bindings,
        num_descriptor_sets,
        offsets,
        num_offsets
    );
}

void CommandBuffer::BindDescriptorSet(
    const DescriptorPool &pool,
    const RaytracingPipeline *pipeline,
    const DescriptorSet *descriptor_set,
    DescriptorSet::Index binding
) const
{
    BindDescriptorSet(
        pool,
        static_cast<const Pipeline *>(pipeline),
        VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR,
        descriptor_set,
        binding,
        nullptr,
        0
    );
}

void CommandBuffer::BindDescriptorSet(
    const DescriptorPool &pool,
    const RaytracingPipeline *pipeline,
    const DescriptorSet *descriptor_set,
    DescriptorSet::Index binding,
    const UInt32 *offsets,
    SizeType num_offsets
) const
{
    BindDescriptorSet(
        pool,
        static_cast<const Pipeline *>(pipeline),
        VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR,
        descriptor_set,
        binding,
        offsets,
        num_offsets
    );
}

void CommandBuffer::BindDescriptorSet(
    const DescriptorPool &pool,
    const RaytracingPipeline *pipeline,
    DescriptorSet::Index set
) const
{
    BindDescriptorSet(
        pool,
        static_cast<const Pipeline *>(pipeline),
        VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR,
        set,
        set,
        nullptr,
        0
    );
}

void CommandBuffer::BindDescriptorSet(
    const DescriptorPool &pool,
    const RaytracingPipeline *pipeline,
    DescriptorSet::Index set,
    DescriptorSet::Index binding
) const
{
    BindDescriptorSet(
        pool,
        static_cast<const Pipeline *>(pipeline),
        VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR,
        set,
        binding,
        nullptr,
        0
    );
}

void CommandBuffer::BindDescriptorSet(
    const DescriptorPool &pool,
    const RaytracingPipeline *pipeline,
    DescriptorSet::Index set,
    DescriptorSet::Index binding,
    const UInt32 *offsets,
    SizeType num_offsets
) const
{
    BindDescriptorSet(
        pool,
        static_cast<const Pipeline *>(pipeline),
        VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR,
        set,
        binding,
        offsets,
        num_offsets
    );
}

void CommandBuffer::BindDescriptorSets(
    const DescriptorPool &pool,
    const RaytracingPipeline *pipeline,
    const DescriptorSet::Index *sets,
    const DescriptorSet::Index *bindings,
    SizeType num_descriptor_sets,
    const UInt32 *offsets,
    SizeType num_offsets
) const
{
    BindDescriptorSets(
        pool,
        static_cast<const Pipeline *>(pipeline),
        VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR,
        sets,
        bindings,
        num_descriptor_sets,
        offsets,
        num_offsets
    );
}

void CommandBuffer::BindDescriptorSet(
    const DescriptorPool &pool,
    const Pipeline *pipeline,
    VkPipelineBindPoint bind_point,
    DescriptorSet::Index set,
    DescriptorSet::Index binding,
    const UInt32 *offsets,
    SizeType num_offsets
) const
{
    const auto set_index = static_cast<UInt>(set);
    const auto binding_index = DescriptorSet::GetDesiredIndex(binding);

    const auto &descriptor_sets = pool.GetDescriptorSets();

    AssertThrowMsg(
        set_index < descriptor_sets.Size(),
        "Attempt to bind invalid descriptor set (%u) (at index %u) -- out of bounds (max is %llu)\n",
        static_cast<UInt>(set),
        set_index,
        descriptor_sets.Size()
    );

    const auto &bind_set = descriptor_sets[set_index];

    AssertThrowMsg(
        bind_set != nullptr,
        "Attempt to bind invalid descriptor set %u (at index %u) -- set is null\n",
        static_cast<UInt>(set),
        set_index
    );

    BindDescriptorSet(
        pool,
        pipeline,
        bind_point,
        bind_set.get(),
        binding,
        offsets,
        num_offsets
    );
}

void CommandBuffer::BindDescriptorSet(
    const DescriptorPool &pool,
    const Pipeline *pipeline,
    VkPipelineBindPoint bind_point,
    const DescriptorSet *descriptor_set,
    DescriptorSet::Index binding,
    const UInt32 *offsets,
    SizeType num_offsets
) const
{
    const auto binding_index = DescriptorSet::GetDesiredIndex(binding);
    const auto handle        = descriptor_set->m_set;

    vkCmdBindDescriptorSets(
        m_command_buffer,
        bind_point,
        pipeline->layout,
        binding_index,
        1,
        &handle,
        static_cast<UInt32>(num_offsets),
        offsets
    );
}

void CommandBuffer::BindDescriptorSets(
    const DescriptorPool &pool,
    const Pipeline *pipeline,
    VkPipelineBindPoint bind_point,
    const DescriptorSet::Index *sets,
    const DescriptorSet::Index *bindings,
    SizeType num_descriptor_sets,
    const UInt32 *offsets,
    SizeType num_offsets
) const
{
    constexpr UInt max_bound_descriptor_sets = 8;
    VkDescriptorSet descriptor_sets_buffer[max_bound_descriptor_sets];

    AssertThrow(num_descriptor_sets <= max_bound_descriptor_sets);

    const auto &descriptor_sets = pool.GetDescriptorSets();

    UInt32 binding_index = 0;

    for (UInt i = 0; i < num_descriptor_sets; i++) {
        const auto set_index = static_cast<UInt>(sets[i]);
        const auto current_binding_index = DescriptorSet::GetDesiredIndex(bindings[i]);

        if (i == 0) {
            binding_index = current_binding_index;
        } else {
            AssertThrowMsg(
                current_binding_index == binding_index + i,
                "Cannot bind multiple descriptor sets: binding for set %u is %u, but must fall into the pattern [firstSet..firstSet+descriptorSetCount-1], where firstSet is %u.",
                set_index,
                current_binding_index,
                binding_index
            );
        }

        AssertThrowMsg(
            set_index < descriptor_sets.Size(),
            "Attempt to bind invalid descriptor set (%u) (at index %u) -- out of bounds (max is %llu)\n",
            static_cast<UInt>(sets[i]),
            set_index,
            descriptor_sets.Size()
        );

        const auto &bind_set = descriptor_sets[set_index];

        AssertThrowMsg(
            bind_set != nullptr,
            "Attempt to bind invalid descriptor set %u (at index %u) -- set is null\n",
            static_cast<UInt>(sets[i]),
            set_index
        );

        descriptor_sets_buffer[i] = bind_set->m_set;
    }

    vkCmdBindDescriptorSets(
        m_command_buffer,
        bind_point,
        pipeline->layout,
        binding_index,
        static_cast<UInt32>(num_descriptor_sets),
        descriptor_sets_buffer,
        static_cast<UInt32>(num_offsets),
        offsets
    );
}

void CommandBuffer::BindDescriptorSets(
    const DescriptorPool &pool,
    const Pipeline *pipeline,
    VkPipelineBindPoint bind_point,
    const DescriptorSet *descriptor_sets,
    const DescriptorSet::Index *bindings,
    SizeType num_descriptor_sets,
    const UInt32 *offsets,
    SizeType num_offsets
) const
{
    constexpr UInt max_bound_descriptor_sets = 8;
    VkDescriptorSet descriptor_sets_buffer[max_bound_descriptor_sets];

    AssertThrow(num_descriptor_sets <= max_bound_descriptor_sets);

    UInt32 binding_index = 0;

    for (UInt i = 0; i < num_descriptor_sets; i++) {
        const auto current_binding_index = DescriptorSet::GetDesiredIndex(bindings[i]);

        if (i == 0) {
            binding_index = current_binding_index;
        } else {
            AssertThrowMsg(
                current_binding_index == binding_index + i,
                "Cannot bind multiple descriptor sets: binding for set is %u, but must fall into the pattern [firstSet..firstSet+descriptorSetCount-1], where firstSet is %u.",
                current_binding_index,
                binding_index
            );
        }

        descriptor_sets_buffer[i] = descriptor_sets[i].m_set;
    }

    vkCmdBindDescriptorSets(
        m_command_buffer,
        bind_point,
        pipeline->layout,
        binding_index,
        static_cast<UInt32>(num_descriptor_sets),
        descriptor_sets_buffer,
        static_cast<UInt32>(num_offsets),
        offsets
    );
}



void CommandBuffer::DebugMarkerBegin(const char *marker_name) const
{
    if (Features::dyn_functions.vkCmdDebugMarkerBeginEXT) {
        const VkDebugMarkerMarkerInfoEXT marker {
            .sType       = VK_STRUCTURE_TYPE_DEBUG_MARKER_MARKER_INFO_EXT,
            .pNext       = nullptr,
            .pMarkerName = marker_name
        };

        Features::dyn_functions.vkCmdDebugMarkerBeginEXT(m_command_buffer, &marker);
    }
}

void CommandBuffer::DebugMarkerEnd() const
{
    if (Features::dyn_functions.vkCmdDebugMarkerEndEXT) {
        Features::dyn_functions.vkCmdDebugMarkerEndEXT(m_command_buffer);
    }
}

} // namespace renderer
} // namespace hyperion
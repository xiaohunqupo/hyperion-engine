#ifndef HYPERION_RENDERER_RENDER_PASS_H
#define HYPERION_RENDERER_RENDER_PASS_H

#include <rendering/backend/RendererImage.hpp>
#include <rendering/backend/RendererImageView.hpp>
#include <rendering/backend/RendererSampler.hpp>
#include <rendering/backend/RendererAttachment.hpp>

#include <Types.hpp>

#include <vulkan/vulkan.h>

namespace hyperion {
namespace renderer {

class CommandBuffer;
class FramebufferObject;

class RenderPass
{
    friend class FramebufferObject;
    friend class GraphicsPipeline;

public:
    enum class Mode
    {
        RENDER_PASS_INLINE = 0,
        RENDER_PASS_SECONDARY_COMMAND_BUFFER = 1
    };

    RenderPass(RenderPassStage stage, Mode mode);
    RenderPass(RenderPassStage stage, Mode mode, UInt num_multiview_layers);
    RenderPass(const RenderPass &other) = delete;
    RenderPass &operator=(const RenderPass &other) = delete;
    ~RenderPass();

    RenderPassStage GetStage() const { return m_stage; }

    bool IsMultiview() const { return m_num_multiview_layers != 0; }
    UInt NumMultiviewLayers() const { return m_num_multiview_layers; }

    void AddAttachmentRef(AttachmentRef *attachment_ref)
    {
        attachment_ref->IncRef(HYP_ATTACHMENT_REF_INSTANCE);

        m_render_pass_attachment_refs.push_back(attachment_ref);
    }

    bool RemoveAttachmentRef(const Attachment *attachment)
    {
        const auto it = std::find_if(
            m_render_pass_attachment_refs.begin(),
            m_render_pass_attachment_refs.end(),
            [attachment](const AttachmentRef *item) {
                return item->GetAttachment() == attachment;
            }
        );

        if (it == m_render_pass_attachment_refs.end()) {
            return false;
        }

        (*it)->DecRef(HYP_ATTACHMENT_REF_INSTANCE);

        m_render_pass_attachment_refs.erase(it);

        return true;
    }

    auto &GetAttachmentRefs() { return m_render_pass_attachment_refs; }
    const auto &GetAttachmentRefs() const { return m_render_pass_attachment_refs; }

    VkRenderPass GetHandle() const { return m_handle; }

    Result Create(Device *device);
    Result Destroy(Device *device);

    void Begin(CommandBuffer *cmd, FramebufferObject *framebuffer);
    void End(CommandBuffer *cmd);

private:
    void CreateDependencies();

    void AddDependency(const VkSubpassDependency &dependency)
        { m_dependencies.push_back(dependency); }

    RenderPassStage m_stage;
    Mode m_mode;
    UInt m_num_multiview_layers;

    std::vector<AttachmentRef *> m_render_pass_attachment_refs;

    std::vector<VkSubpassDependency> m_dependencies;
    std::vector<VkClearValue> m_clear_values;

    VkRenderPass m_handle;
};

} // namespace renderer
} // namespace hyperion

#endif
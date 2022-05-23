#include "render_list.h"

#include "../engine.h"

namespace hyperion::v2 {

RenderListContainer::RenderListContainer()
{
    for (size_t i = 0; i < m_buckets.size(); i++) {
        m_buckets[i] = {
            .bucket = Bucket(i)
        };
    }
}

void RenderListContainer::AddFramebuffersToPipelines(Engine *engine)
{
    for (auto &bucket : m_buckets) {
        bucket.AddFramebuffersToPipelines(engine);
    }
}

void RenderListContainer::Create(Engine *engine)
{
    for (auto &bucket : m_buckets) {
        bucket.CreateRenderPass(engine);
        bucket.CreateFramebuffers(engine);
    }
}

void RenderListContainer::Destroy(Engine *engine)
{
    for (auto &bucket : m_buckets) {
        bucket.Destroy(engine);
    }
}

void RenderListContainer::RenderListBucket::AddFramebuffersToPipelines(Engine *engine)
{
    for (auto &pipeline : graphics_pipelines) {
        for (auto &framebuffer : framebuffers) {
            pipeline->AddFramebuffer(framebuffer.IncRef());
        }
    }
}

void RenderListContainer::RenderListBucket::CreateRenderPass(Engine *engine)
{
    AssertThrow(render_pass == nullptr);

    auto mode = renderer::RenderPass::Mode::RENDER_PASS_SECONDARY_COMMAND_BUFFER;

    if (bucket == BUCKET_SWAPCHAIN) {
        mode = renderer::RenderPass::Mode::RENDER_PASS_INLINE;
    }
    
    render_pass = engine->resources.render_passes.Add(std::make_unique<RenderPass>(
        RenderPassStage::SHADER,
        mode
    ));

    if (IsRenderableBucket()) { // add gbuffer attachments
        renderer::AttachmentRef *attachment_ref;

        attachments.push_back(std::make_unique<renderer::Attachment>(
            std::make_unique<renderer::FramebufferImage2D>(
                engine->GetInstance()->swapchain->extent,
                engine->GetDefaultFormat(Engine::TEXTURE_FORMAT_DEFAULT_COLOR),
                nullptr
            ),
            RenderPassStage::SHADER
        ));

        HYPERION_ASSERT_RESULT(attachments.back()->AddAttachmentRef(
            engine->GetInstance()->GetDevice(),
            renderer::LoadOperation::CLEAR,
            renderer::StoreOperation::STORE,
            &attachment_ref
        ));

        render_pass->GetRenderPass().AddAttachmentRef(attachment_ref);

        for (uint i = 0; i < num_gbuffer_attachments - 2 /* -2 because color and depth already accounted for*/; i++) {
            attachments.push_back(std::make_unique<renderer::Attachment>(
                std::make_unique<renderer::FramebufferImage2D>(
                    engine->GetInstance()->swapchain->extent,
                    engine->GetDefaultFormat(Engine::TEXTURE_FORMAT_DEFAULT_GBUFFER),
                    nullptr
                ),
                RenderPassStage::SHADER
            ));

            HYPERION_ASSERT_RESULT(attachments.back()->AddAttachmentRef(
                engine->GetInstance()->GetDevice(),
                renderer::LoadOperation::CLEAR,
                renderer::StoreOperation::STORE,
                &attachment_ref
            ));

            render_pass->GetRenderPass().AddAttachmentRef(attachment_ref);
        }

        /* Add depth attachment */
        if (bucket == BUCKET_TRANSLUCENT) { // translucent reuses the opaque bucket's depth buffer.
            auto &forward_fbo = engine->GetRenderListContainer()[BUCKET_OPAQUE].framebuffers[0];
            AssertThrow(forward_fbo != nullptr);

            renderer::AttachmentRef *depth_attachment;

            HYPERION_ASSERT_RESULT(forward_fbo->GetFramebuffer().GetAttachmentRefs().at(num_gbuffer_attachments - 1)->AddAttachmentRef(
                engine->GetInstance()->GetDevice(),
                renderer::StoreOperation::STORE,
                &depth_attachment
            ));

            depth_attachment->SetBinding(num_gbuffer_attachments - 1);

            render_pass->GetRenderPass().AddAttachmentRef(depth_attachment);
        } else {
            attachments.push_back(std::make_unique<renderer::Attachment>(
                std::make_unique<renderer::FramebufferImage2D>(
                    engine->GetInstance()->swapchain->extent,
                    engine->GetDefaultFormat(Engine::TEXTURE_FORMAT_DEFAULT_DEPTH),
                    nullptr
                ),
                RenderPassStage::SHADER
            ));

            HYPERION_ASSERT_RESULT(attachments.back()->AddAttachmentRef(
                engine->GetInstance()->GetDevice(),
                renderer::LoadOperation::CLEAR,
                renderer::StoreOperation::STORE,
                &attachment_ref
            ));

            render_pass->GetRenderPass().AddAttachmentRef(attachment_ref);
        }
    }

    for (auto &attachment : attachments) {
        HYPERION_ASSERT_RESULT(attachment->Create(engine->GetInstance()->GetDevice()));
    }
    
    render_pass.Init();
}

void RenderListContainer::RenderListBucket::CreateFramebuffers(Engine *engine)
{
    AssertThrow(framebuffers.empty());

    const uint num_frames = engine->GetInstance()->GetFrameHandler()->NumFrames();

    for (uint i = 0; i < 1/*num_frames*/; i++) {
        auto framebuffer = std::make_unique<Framebuffer>(engine->GetInstance()->swapchain->extent, render_pass.IncRef());

        for (auto *attachment_ref : render_pass->GetRenderPass().GetAttachmentRefs()) {
            framebuffer->GetFramebuffer().AddAttachmentRef(attachment_ref);
        }

        framebuffers.push_back(engine->resources.framebuffers.Add(
            std::move(framebuffer)
        ));

        framebuffers.back().Init();
    }
}

void RenderListContainer::RenderListBucket::Destroy(Engine *engine)
{
    auto result = renderer::Result::OK;

    graphics_pipelines.clear();
    framebuffers.clear();

    for (const auto &attachment : attachments) {
        HYPERION_PASS_ERRORS(
            attachment->Destroy(engine->GetInstance()->GetDevice()),
            result
        );
    }

    HYPERION_ASSERT_RESULT(result);
}

} // namespace hyperion::v2
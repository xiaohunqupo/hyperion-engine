#include "deferred.h"
#include "../engine.h"

#include <asset/byte_reader.h>
#include <util/fs/fs_util.h>

namespace hyperion::v2 {

using renderer::ImageDescriptor;
using renderer::ImageSamplerDescriptor;
using renderer::DescriptorKey;
using renderer::Rect;

DeferredPass::DeferredPass()
    : FullScreenPass()
{
}

DeferredPass::~DeferredPass() = default;

void DeferredPass::CreateShader(Engine *engine)
{
    m_shader = engine->resources.shaders.Add(std::make_unique<Shader>(
        std::vector<SubShader>{
            SubShader{ShaderModule::Type::VERTEX, {
                FileByteReader(FileSystem::Join(engine->assets.GetBasePath(), "vkshaders/deferred_vert.spv")).Read(),
                {.name = "deferred vert"}
            }},
            SubShader{ShaderModule::Type::FRAGMENT, {
                FileByteReader(FileSystem::Join(engine->assets.GetBasePath(), "vkshaders/deferred_frag.spv")).Read(),
                {.name = "deferred frag"}
            }}
        }
    ));

    m_shader->Init(engine);
}

void DeferredPass::CreateComputePipelines(Engine *engine)
{
    m_ssr_write_uvs = engine->resources.compute_pipelines.Add(std::make_unique<ComputePipeline>(
        engine->resources.shaders.Add(std::make_unique<Shader>(
            std::vector<SubShader>{
                { ShaderModule::Type::COMPUTE, {FileByteReader(FileSystem::Join(engine->assets.GetBasePath(), "vkshaders/ssr/ssr_write_uvs.comp.spv")).Read()}}
            }
        ))
    ));

    m_ssr_write_uvs.Init();

    m_ssr_blur = engine->resources.compute_pipelines.Add(std::make_unique<ComputePipeline>(
        engine->resources.shaders.Add(std::make_unique<Shader>(
            std::vector<SubShader>{
                { ShaderModule::Type::COMPUTE, {FileByteReader(FileSystem::Join(engine->assets.GetBasePath(), "vkshaders/ssr/ssr_blur.comp.spv")).Read()}}
            }
        ))
    ));

    m_ssr_blur.Init();
}

void DeferredPass::CreateRenderPass(Engine *engine)
{
    m_render_pass = engine->GetRenderListContainer()[Bucket::BUCKET_TRANSLUCENT].GetRenderPass().IncRef();
}

void DeferredPass::CreateDescriptors(Engine *engine)
{
    for (UInt i = 0; i < max_frames_in_flight; i++) {
        auto &framebuffer = m_framebuffers[i]->GetFramebuffer();

        if (!framebuffer.GetAttachmentRefs().empty()) {
            auto *descriptor_set = engine->GetInstance()->GetDescriptorPool().GetDescriptorSet(DescriptorSet::global_buffer_mapping[i]);
            auto *descriptor = descriptor_set->GetOrAddDescriptor<ImageSamplerDescriptor>(DescriptorKey::DEFERRED_RESULT);

            for (auto *attachment_ref : framebuffer.GetAttachmentRefs()) {
                descriptor->SetSubDescriptor({
                    .element_index = ~0u,
                    .image_view    = attachment_ref->GetImageView(),
                    .sampler       = attachment_ref->GetSampler(),
                });
            }
        }
    }
}

void DeferredPass::Create(Engine *engine)
{
    for (UInt i = 0; i < max_frames_in_flight; i++) {
        m_mipmapped_results[i] = engine->resources.textures.Add(std::make_unique<Texture2D>(
            Extent2D { 512, 512 },
            Image::InternalFormat::TEXTURE_INTERNAL_FORMAT_BGRA8_SRGB,
            Image::FilterMode::TEXTURE_FILTER_LINEAR_MIPMAP,
            Image::WrapMode::TEXTURE_WRAP_CLAMP_TO_BORDER,
            nullptr
        ));

        m_mipmapped_results[i].Init();

        for (UInt j = 0; j < static_cast<UInt>(m_ssr_image_outputs[i].size()); j++) {
            m_ssr_image_outputs[i][j] = SSRImageOutput {
                .image = std::make_unique<StorageImage>(
                    m_mipmapped_results[i]->GetExtent(),
                    Image::InternalFormat::TEXTURE_INTERNAL_FORMAT_RGBA16F,
                    Image::Type::TEXTURE_TYPE_2D,
                    nullptr
                ),
                .image_view = std::make_unique<ImageView>()
            };

            m_ssr_image_outputs[i][j].Create(engine->GetDevice());
        }

        m_framebuffers[i] = engine->GetRenderListContainer()[Bucket::BUCKET_TRANSLUCENT].GetFramebuffers()[i].IncRef();
        
        auto command_buffer = std::make_unique<CommandBuffer>(CommandBuffer::COMMAND_BUFFER_SECONDARY);

        HYPERION_ASSERT_RESULT(command_buffer->Create(
            engine->GetInstance()->GetDevice(),
            engine->GetInstance()->GetGraphicsCommandPool()
        ));

        m_command_buffers[i] = std::move(command_buffer);
    }

    m_sampler = std::make_unique<Sampler>();
    HYPERION_ASSERT_RESULT(m_sampler->Create(engine->GetDevice()));
    
    CreatePipeline(engine);
}

void DeferredPass::Destroy(Engine *engine)
{
    m_ssr_write_uvs.Reset();
    m_ssr_blur.Reset();

    for (UInt i = 0; i < max_frames_in_flight; i++) {
        for (UInt j = 0; j < static_cast<UInt>(m_ssr_image_outputs[i].size()); j++) {
            m_ssr_image_outputs[i][j].Destroy(engine->GetDevice());
        }

        engine->SafeReleaseRenderable(std::move(m_mipmapped_results[i]));
    }

    HYPERION_ASSERT_RESULT(m_sampler->Destroy(engine->GetDevice()));

    FullScreenPass::Destroy(engine); // flushes render queue
}

void DeferredPass::Render(Engine *engine, Frame *frame)
{
}

DeferredRenderer::DeferredRenderer() = default;
DeferredRenderer::~DeferredRenderer() = default;

void DeferredRenderer::Create(Engine *engine)
{
    Threads::AssertOnThread(THREAD_RENDER);

    m_post_processing.Create(engine);

    m_pass.CreateShader(engine);
    m_pass.CreateComputePipelines(engine);
    m_pass.CreateRenderPass(engine);
    m_pass.Create(engine);

    for (UInt i = 0; i < max_frames_in_flight; i++) {
        auto &opaque_fbo = engine->GetRenderListContainer()[Bucket::BUCKET_OPAQUE].GetFramebuffers()[i];
        
        auto *descriptor_set_pass = engine->GetInstance()->GetDescriptorPool()
            .GetDescriptorSet(DescriptorSet::global_buffer_mapping[i]);
        
        descriptor_set_pass->AddDescriptor<ImageDescriptor>(0);

        UInt attachment_index = 0;

        /* Gbuffer textures */
        for (; attachment_index < RenderListContainer::gbuffer_textures.size() - 1; attachment_index++) {
            descriptor_set_pass
                ->GetDescriptor(0)
                ->SetSubDescriptor({
                    .image_view = opaque_fbo->GetFramebuffer().GetAttachmentRefs()[attachment_index]->GetImageView()
                });
        }

        /* Depth texture */
        descriptor_set_pass
            ->AddDescriptor<ImageDescriptor>(1)
            ->SetSubDescriptor({
                .image_view = opaque_fbo->GetFramebuffer().GetAttachmentRefs()[attachment_index]->GetImageView()
            });

        /* Mip chain */
        descriptor_set_pass
            ->AddDescriptor<ImageSamplerDescriptor>(2)
            ->SetSubDescriptor({
                .image_view = &m_pass.GetMipmappedResults()[i]->GetImageView(),
                .sampler    = &m_pass.GetMipmappedResults()[i]->GetSampler()
            });

        /* Gbuffer sampler */
        descriptor_set_pass
            ->AddDescriptor<renderer::SamplerDescriptor>(3)
            ->SetSubDescriptor({
                .sampler = m_pass.GetSampler().get()
            });

        /* SSR Data */
        descriptor_set_pass // 1st output -- trace, write UVs
            ->AddDescriptor<renderer::StorageImageDescriptor>(12)
            ->SetSubDescriptor({
                .image_view = m_pass.GetSSRImageOutputs()[i][0].image_view.get()
            });

        descriptor_set_pass // 2nd output -- blurred sample
            ->AddDescriptor<renderer::StorageImageDescriptor>(13)
            ->SetSubDescriptor({
                .image_view = m_pass.GetSSRImageOutputs()[i][1].image_view.get()
            });
    }
    
    m_pass.CreateDescriptors(engine);

    HYP_FLUSH_RENDER_QUEUE(engine);
}

void DeferredRenderer::Destroy(Engine *engine)
{
    Threads::AssertOnThread(THREAD_RENDER);

    //! TODO: remove all descriptors

    m_post_processing.Destroy(engine);
    m_pass.Destroy(engine);  // flushes render queue
}

void DeferredRenderer::Render(Engine *engine, Frame *frame)
{
    Threads::AssertOnThread(THREAD_RENDER);

    auto *primary = frame->GetCommandBuffer();
    const auto frame_index = frame->GetFrameIndex();
    auto &mipmapped_result = m_pass.GetMipmappedResults()[frame_index]->GetImage();

    m_pass.Record(engine, frame_index);

    auto &render_list = engine->GetRenderListContainer();
    auto &bucket = render_list.Get(BUCKET_OPAQUE);
    
    // begin opaque objs
    bucket.GetFramebuffers()[frame_index]->BeginCapture(primary);
    RenderOpaqueObjects(engine, frame);
    bucket.GetFramebuffers()[frame_index]->EndCapture(primary);
    // end opaque objs
    
    /* ========== BEGIN MIP CHAIN GENERATION ========== */
    auto *framebuffer_image = bucket.GetFramebuffers()[frame_index]->GetFramebuffer()
        .GetAttachmentRefs()[0]->GetAttachment()->GetImage();
    
    framebuffer_image->GetGPUImage()->InsertBarrier(primary, renderer::GPUMemory::ResourceState::COPY_SRC);
    mipmapped_result.GetGPUImage()->InsertBarrier(primary, renderer::GPUMemory::ResourceState::COPY_DST);

    // Blit into the mipmap chain img
    mipmapped_result.Blit(
        primary,
        framebuffer_image,
        Rect { 0, 0, framebuffer_image->GetExtent().width, framebuffer_image->GetExtent().height },
        Rect { 0, 0, mipmapped_result.GetExtent().width, mipmapped_result.GetExtent().height }
    );

    HYPERION_ASSERT_RESULT(mipmapped_result.GenerateMipmaps(engine->GetDevice(), primary));

    framebuffer_image->GetGPUImage()->InsertBarrier(primary, renderer::GPUMemory::ResourceState::SHADER_RESOURCE);

    /* ==========  END MIP CHAIN GENERATION ========== */

    /* ========== BEGIN SSR ========== */

    // PASS 1 -- write UVs

    // start by putting the UV image in a writeable state
    const Pipeline::PushConstantData ssr_push_constant_data {
        .ssr_data = {
            .width            = mipmapped_result.GetExtent().width,
            .height           = mipmapped_result.GetExtent().height,
            .ray_step         = 0.35f,
            .num_iterations   = 100.0f,
            .max_ray_distance = 64.0f
        }
    };

    m_pass.m_ssr_image_outputs[frame_index][0].image->GetGPUImage()
        ->InsertBarrier(primary, renderer::GPUMemory::ResourceState::UNORDERED_ACCESS);

    m_pass.m_ssr_write_uvs->GetPipeline()->Bind(primary, ssr_push_constant_data);

    // bind `global` descriptor set
    engine->GetInstance()->GetDescriptorPool().Bind(
        engine->GetDevice(), primary, m_pass.m_ssr_write_uvs->GetPipeline(),
        {
            {.set = DescriptorSet::global_buffer_mapping[frame->GetFrameIndex()], .count = 1},
            {.binding = DescriptorSet::DESCRIPTOR_SET_INDEX_GLOBAL}
        }
    );

    engine->GetInstance()->GetDescriptorPool().Bind(
        engine->GetDevice(), primary, m_pass.m_ssr_write_uvs->GetPipeline(),
        {
            {.set = DescriptorSet::scene_buffer_mapping[frame->GetFrameIndex()], .count = 1},
            {.binding = DescriptorSet::DESCRIPTOR_SET_INDEX_SCENE}
        }
    );

    m_pass.m_ssr_write_uvs->GetPipeline()->Dispatch(primary, mipmapped_result.GetExtent() / Extent3D{8, 8, 1});

    // transition the UV image back into read state
    m_pass.m_ssr_image_outputs[frame_index][0].image->GetGPUImage()
        ->InsertBarrier(primary, renderer::GPUMemory::ResourceState::SHADER_RESOURCE);

    // PASS 2 - sample textures, blur image

    // put blur image in writeable state
    m_pass.m_ssr_image_outputs[frame_index][1].image->GetGPUImage()
        ->InsertBarrier(primary, renderer::GPUMemory::ResourceState::UNORDERED_ACCESS);

    m_pass.m_ssr_blur->GetPipeline()->Bind(primary, ssr_push_constant_data);

    // bind `global` descriptor set
    engine->GetInstance()->GetDescriptorPool().Bind(
        engine->GetDevice(), primary, m_pass.m_ssr_blur->GetPipeline(),
        {
            {.set = DescriptorSet::global_buffer_mapping[frame->GetFrameIndex()], .count = 1},
            {.binding = DescriptorSet::DESCRIPTOR_SET_INDEX_GLOBAL}
        }
    );

    engine->GetInstance()->GetDescriptorPool().Bind(
        engine->GetDevice(), primary, m_pass.m_ssr_blur->GetPipeline(),
        {
            {.set = DescriptorSet::scene_buffer_mapping[frame->GetFrameIndex()], .count = 1},
            {.binding = DescriptorSet::DESCRIPTOR_SET_INDEX_SCENE}
        }
    );

    m_pass.m_ssr_blur->GetPipeline()->Dispatch(primary, mipmapped_result.GetExtent() / Extent3D{8, 8, 1});

    // transition blur image back into read state
    m_pass.m_ssr_image_outputs[frame_index][1].image->GetGPUImage()
        ->InsertBarrier(primary, renderer::GPUMemory::ResourceState::SHADER_RESOURCE);

    /* ==========  END SSR  ========== */

    m_post_processing.RenderPre(engine, frame);

    // begin translucent objs
    m_pass.GetFramebuffer(frame_index)->BeginCapture(primary);
    /* Render deferred shading onto full screen quad */
    HYPERION_ASSERT_RESULT(m_pass.GetCommandBuffer(frame_index)->SubmitSecondary(primary));

    RenderTranslucentObjects(engine, frame);
    m_pass.GetFramebuffer(frame_index)->EndCapture(primary);
    // end translucent objs

    m_post_processing.RenderPost(engine, frame);
}

void DeferredRenderer::RenderOpaqueObjects(Engine *engine, Frame *frame)
{
    for (auto &pipeline : engine->GetRenderListContainer().Get(Bucket::BUCKET_SKYBOX).GetGraphicsPipelines()) {
        pipeline->Render(engine, frame);
    }
    
    for (auto &pipeline : engine->GetRenderListContainer().Get(Bucket::BUCKET_OPAQUE).GetGraphicsPipelines()) {
        pipeline->Render(engine, frame);
    }
}

void DeferredRenderer::RenderTranslucentObjects(Engine *engine, Frame *frame)
{
    for (auto &pipeline : engine->GetRenderListContainer().Get(Bucket::BUCKET_TRANSLUCENT).GetGraphicsPipelines()) {
        pipeline->Render(engine, frame);
    }
}

} // namespace hyperion::v2

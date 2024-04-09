#include <rendering/HBAO.hpp>
#include <rendering/RenderEnvironment.hpp>
#include <Engine.hpp>

namespace hyperion::v2 {

using renderer::CommandBuffer;

#pragma region Render commands

struct RENDER_COMMAND(AddHBAOFinalImagesToGlobalDescriptorSet) : renderer::RenderCommand
{
    FixedArray<ImageViewRef, max_frames_in_flight> pass_image_views;

    RENDER_COMMAND(AddHBAOFinalImagesToGlobalDescriptorSet)(FixedArray<ImageViewRef, max_frames_in_flight> &&pass_image_views)
        : pass_image_views(std::move(pass_image_views))
    {
    }

    virtual ~RENDER_COMMAND(AddHBAOFinalImagesToGlobalDescriptorSet)() override = default;

    virtual Result operator()() override
    {
        for (uint frame_index = 0; frame_index < max_frames_in_flight; frame_index++) {
            g_engine->GetGlobalDescriptorTable()->GetDescriptorSet(HYP_NAME(Global), frame_index)
                ->SetElement(HYP_NAME(SSAOResultTexture), pass_image_views[frame_index]);
        }

        HYPERION_RETURN_OK;
    }
};

struct RENDER_COMMAND(RemoveHBAODescriptors) : renderer::RenderCommand
{
    RENDER_COMMAND(RemoveHBAODescriptors)()
    {
    }

    virtual ~RENDER_COMMAND(RemoveHBAODescriptors)() override = default;

    virtual Result operator()() override
    {
        auto result = Result::OK;

        for (uint frame_index = 0; frame_index < max_frames_in_flight; frame_index++) {
            g_engine->GetGlobalDescriptorTable()->GetDescriptorSet(HYP_NAME(Global), frame_index)
                ->SetElement(HYP_NAME(SSAOResultTexture), g_engine->GetPlaceholderData()->GetImageView2D1x1R8());
        }

        return result;
    }
};

#pragma endregion

HBAO::HBAO(const Extent2D &extent)
    : m_extent(extent)
{
}

HBAO::~HBAO() = default;

void HBAO::Create()
{
    CreatePass();
    CreateTemporalBlending();

    PUSH_RENDER_COMMAND(
        AddHBAOFinalImagesToGlobalDescriptorSet,
        FixedArray<ImageViewRef, max_frames_in_flight> {
            m_temporal_blending ? m_temporal_blending->GetImageOutput(0).image_view : m_hbao_pass->GetAttachmentUsage(0)->GetImageView(),
            m_temporal_blending ? m_temporal_blending->GetImageOutput(1).image_view : m_hbao_pass->GetAttachmentUsage(0)->GetImageView()
        }
    );
}

void HBAO::Destroy()
{
    m_temporal_blending->Destroy();

    m_hbao_pass->Destroy();

    PUSH_RENDER_COMMAND(RemoveHBAODescriptors);
}

void HBAO::CreatePass()
{
    ShaderProperties shader_properties;
    shader_properties.Set("HBIL_ENABLED", g_engine->GetConfig().Get(CONFIG_HBIL));

    Handle<Shader> hbao_shader = g_shader_manager->GetOrCreate(
        HYP_NAME(HBAO),
        shader_properties
    );

    g_engine->InitObject(hbao_shader);

    renderer::DescriptorTableDeclaration descriptor_table_decl = hbao_shader->GetCompiledShader().GetDescriptorUsages().BuildDescriptorTable();

    DescriptorTableRef descriptor_table = MakeRenderObject<renderer::DescriptorTable>(descriptor_table_decl);
    DeferCreate(descriptor_table, g_engine->GetGPUDevice());

    m_hbao_pass.Reset(new FullScreenPass(
        hbao_shader,
        std::move(descriptor_table),
        InternalFormat::RGBA8,
        m_extent
    ));

    m_hbao_pass->Create();
}

void HBAO::CreateTemporalBlending()
{
    AssertThrow(m_hbao_pass != nullptr);

    m_temporal_blending.Reset(new TemporalBlending(
        m_hbao_pass->GetFramebuffer()->GetExtent(),
        InternalFormat::RGBA8,
        TemporalBlendTechnique::TECHNIQUE_3,
        TemporalBlendFeedback::LOW,
        m_hbao_pass->GetFramebuffer()
    ));

    m_temporal_blending->Create();
}

void HBAO::Render(Frame *frame)
{
    const uint frame_index = frame->GetFrameIndex();
    const CommandBufferRef &command_buffer = frame->GetCommandBuffer();

    {
        struct alignas(128)
        {
            Vec2u   dimension;
        } push_constants;

        push_constants.dimension = m_extent;

        m_hbao_pass->GetRenderGroup()->GetPipeline()->SetPushConstants(&push_constants, sizeof(push_constants));
        m_hbao_pass->Begin(frame);

        Frame temporary_frame = Frame::TemporaryFrame(m_hbao_pass->GetCommandBuffer(frame_index), frame_index);
        
        m_hbao_pass->GetRenderGroup()->GetPipeline()->GetDescriptorTable()->Bind(
            &temporary_frame,
            m_hbao_pass->GetRenderGroup()->GetPipeline(),
            {
                {
                    HYP_NAME(Scene),
                    {
                        { HYP_NAME(ScenesBuffer), HYP_RENDER_OBJECT_OFFSET(Scene, g_engine->GetRenderState().GetScene().id.ToIndex()) },
                        { HYP_NAME(CamerasBuffer), HYP_RENDER_OBJECT_OFFSET(Camera, g_engine->GetRenderState().GetCamera().id.ToIndex()) },
                        { HYP_NAME(LightsBuffer), HYP_RENDER_OBJECT_OFFSET(Light, 0) },
                        { HYP_NAME(EnvGridsBuffer), HYP_RENDER_OBJECT_OFFSET(EnvGrid, 0) },
                        { HYP_NAME(CurrentEnvProbe), HYP_RENDER_OBJECT_OFFSET(EnvProbe, 0) }
                    }
                }
            }
        );
        
        m_hbao_pass->GetQuadMesh()->Render(m_hbao_pass->GetCommandBuffer(frame_index));
        m_hbao_pass->End(frame);
    }
    
    m_temporal_blending->Render(frame);
}

} // namespace hyperion::v2
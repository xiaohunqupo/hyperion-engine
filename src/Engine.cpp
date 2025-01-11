/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <Engine.hpp>

#include <rendering/PostFX.hpp>
#include <rendering/RenderEnvironment.hpp>
#include <rendering/RenderGroup.hpp>
#include <rendering/ShaderGlobals.hpp>
#include <rendering/GBuffer.hpp>
#include <rendering/EntityInstanceBatchHolderMap.hpp>
#include <rendering/Deferred.hpp>
#include <rendering/DefaultFormats.hpp>
#include <rendering/PlaceholderData.hpp>
#include <rendering/FinalPass.hpp>
#include <rendering/World.hpp>

#include <rendering/debug/DebugDrawer.hpp>

#include <rendering/backend/AsyncCompute.hpp>
#include <rendering/backend/RendererInstance.hpp>
#include <rendering/backend/RendererFeatures.hpp>
#include <rendering/backend/RendererDescriptorSet.hpp>
#include <rendering/backend/RendererDevice.hpp>

#include <asset/Assets.hpp>

#include <util/profiling/ProfileScope.hpp>
#include <util/MeshBuilder.hpp>
#include <util/fs/FsUtil.hpp>

#include <audio/AudioManager.hpp>

#include <core/system/StackDump.hpp>
#include <core/system/SystemEvent.hpp>

#include <core/threading/Threads.hpp>
#include <core/threading/TaskSystem.hpp>

#include <core/utilities/DeferredScope.hpp>

#include <core/logging/LogChannels.hpp>
#include <core/logging/Logger.hpp>

#include <core/net/NetRequestThread.hpp>

#include <core/system/CommandLine.hpp>

#include <core/object/HypClassUtils.hpp>

#include <scripting/ScriptingService.hpp>

#include <Game.hpp>

#define HYP_LOG_FRAMES_PER_SECOND

namespace hyperion {

using renderer::FillMode;
using renderer::GPUBufferType;

Handle<Engine>          g_engine = { };
Handle<AssetManager>    g_asset_manager { };
ShaderManagerSystem     *g_shader_manager = nullptr;
MaterialCache           *g_material_system = nullptr;
SafeDeleter             *g_safe_deleter = nullptr;

/* \brief Should the swapchain be rebuilt on the next frame? */
static bool g_should_recreate_swapchain = false;

class RenderThread final : public Thread<Scheduler>
{
public:
    RenderThread(const RC<AppContext> &app_context)
        : Thread(ThreadName::THREAD_RENDER, ThreadPriorityValue::HIGHEST),
          m_app_context(app_context),
          m_is_running(false)
    {
    }

    // Overrides Start() to not create a thread object. Runs the render loop on the main thread.
    bool Start()
    {
        AssertThrow(m_is_running.Exchange(true, MemoryOrder::ACQUIRE_RELEASE) == false);

        // Must be current thread
        Threads::AssertOnThread(ThreadName::THREAD_RENDER);

        SetCurrentThreadObject(this);
        m_scheduler.SetOwnerThread(GetID());

        (*this)();

        return true;
    }

    void Stop()
    {
        m_is_running.Set(false, MemoryOrder::RELEASE);
    }

    HYP_FORCE_INLINE bool IsRunning() const
        { return m_is_running.Get(MemoryOrder::ACQUIRE); }

private:
    virtual void operator()() override
    {
        AssertThrow(m_app_context != nullptr);
        AssertThrow(m_app_context->GetGame() != nullptr);

        SystemEvent event;

#ifdef HYP_LOG_FRAMES_PER_SECOND
        uint32 num_frames = 0;
        float delta_time_accum = 0.0f;

        GameCounter counter;
#endif
    
        Queue<Scheduler::ScheduledTask> tasks;

        while (m_is_running.Get(MemoryOrder::RELAXED)) {
            // input manager stuff
            while (m_app_context->PollEvent(event)) {
                m_app_context->GetGame()->PushEvent(std::move(event));
            }

#ifdef HYP_LOG_FRAMES_PER_SECOND
            counter.NextTick();
            delta_time_accum += counter.delta;
            num_frames++;

            if (delta_time_accum >= 1.0f) {
                DebugLog(
                    LogType::Debug,
                    "Render FPS: %f\n",
                    1.0f / (delta_time_accum / float(num_frames))
                );

                delta_time_accum = 0.0f;
                num_frames = 0;
            }
#endif

            if (uint32 num_enqueued = m_scheduler.NumEnqueued()) {
                m_scheduler.AcceptAll(tasks);

                while (tasks.Any()) {
                    tasks.Pop().Execute();
                }
            }

            g_engine->RenderNextFrame(m_app_context->GetGame());
        }
    }

    RC<AppContext>  m_app_context;
    AtomicVar<bool> m_is_running;
};

#pragma region Render commands

struct RENDER_COMMAND(CopyBackbufferToCPU) : renderer::RenderCommand
{
    ImageRef        image;
    GPUBufferRef    buffer;

    RENDER_COMMAND(CopyBackbufferToCPU)(const ImageRef &image, const GPUBufferRef &buffer)
        : image(image),
          buffer(buffer)
    {
    }

    virtual ~RENDER_COMMAND(CopyBackbufferToCPU)() override = default;

    virtual RendererResult operator()() override
    {
        AssertThrow(image.IsValid());
        AssertThrow(buffer.IsValid());


        HYPERION_RETURN_OK;
    }
};

struct RENDER_COMMAND(RecreateSwapchain) : renderer::RenderCommand
{
    virtual ~RENDER_COMMAND(RecreateSwapchain)() override = default;

    virtual RendererResult operator()() override
    {
        g_should_recreate_swapchain = true;

        HYPERION_RETURN_OK;
    }
};

#pragma endregion Render commands

#pragma region Engine

const Handle<Engine> &Engine::GetInstance()
{
    return g_engine;
}

Engine::Engine()
    : m_is_initialized(false)
{
}

Engine::~Engine()
{
    AssertThrowMsg(m_instance == nullptr, "Engine instance must be destroyed before Engine object is destroyed");
}

void Engine::FindTextureFormatDefaults()
{
    Threads::AssertOnThread(ThreadName::THREAD_RENDER);

    const Device *device = m_instance->GetDevice();

    m_texture_format_defaults.Set(
        TextureFormatDefault::TEXTURE_FORMAT_DEFAULT_COLOR,
        device->GetFeatures().FindSupportedFormat(
            std::array{ InternalFormat::RGBA8_SRGB,
                        InternalFormat::R10G10B10A2,
                        InternalFormat::RGBA16F,
                        InternalFormat::RGBA8 },
            renderer::ImageSupportType::SRV
        )
    );

    m_texture_format_defaults.Set(
        TextureFormatDefault::TEXTURE_FORMAT_DEFAULT_DEPTH,
        device->GetFeatures().FindSupportedFormat(
            std::array{ InternalFormat::DEPTH_32F, InternalFormat::DEPTH_24,
                        InternalFormat::DEPTH_16
                         },
            renderer::ImageSupportType::DEPTH
        )
    );

    m_texture_format_defaults.Set(
        TextureFormatDefault::TEXTURE_FORMAT_DEFAULT_NORMALS,
        device->GetFeatures().FindSupportedFormat(
            std::array{ InternalFormat::RGBA16F,
                        InternalFormat::RGBA32F,
                        InternalFormat::RGBA8 },
            renderer::ImageSupportType::SRV
        )
    );

    m_texture_format_defaults.Set(
        TextureFormatDefault::TEXTURE_FORMAT_DEFAULT_STORAGE,
        device->GetFeatures().FindSupportedFormat(
            std::array{ InternalFormat::RGBA16F },
            renderer::ImageSupportType::UAV
        )
    );
}

HYP_API void Engine::Initialize(const RC<AppContext> &app_context)
{
    HYP_SCOPE;
    Threads::AssertOnThread(ThreadName::THREAD_MAIN);

    AssertThrow(!m_is_initialized);
    m_is_initialized = true;

    HYP_DEFER({
        m_is_initialized = false;
    });

    AssertThrow(app_context != nullptr);
    m_app_context = app_context;

    m_render_thread = MakeUnique<RenderThread>(m_app_context);

    m_app_context->GetMainWindow()->OnWindowSizeChanged.Bind([this](Vec2i new_window_size)
    {
        HYP_LOG(Engine, Info, "Resize window to {}", new_window_size);

        PUSH_RENDER_COMMAND(RecreateSwapchain);
    }).Detach();
    
    RenderObjectDeleter<renderer::Platform::CURRENT>::Initialize();

    m_crash_handler.Initialize();

    TaskSystem::GetInstance().Start();

    AssertThrow(m_instance == nullptr);
    m_instance = MakeUnique<Instance>();
    
#ifdef HYP_DEBUG_MODE
    constexpr bool use_debug_layers = false;// true;
#else
    constexpr bool use_debug_layers = false;
#endif

    HYPERION_ASSERT_RESULT(m_instance->Initialize(*app_context, use_debug_layers));
    
    m_global_descriptor_table = MakeRenderObject<DescriptorTable>(renderer::GetStaticDescriptorTableDeclaration());

    // Update app configuration to reflect device, after instance is created (e.g RT is not supported)
    m_app_context->UpdateConfigurationOverrides();

    FindTextureFormatDefaults();

    m_configuration.SetToDefaultConfiguration();
    m_configuration.LoadFromDefinitionsFile();

    if (!m_shader_compiler.LoadShaderDefinitions()) {
        HYP_BREAKPOINT;
    }

    m_render_data = MakeUnique<ShaderGlobals>();
    m_render_data->Create();

    m_placeholder_data = MakeUnique<PlaceholderData>();
    m_placeholder_data->Create();

    m_render_state = CreateObject<RenderState>();
    InitObject(m_render_state);

    // Create script compilation service
    m_scripting_service = MakeUnique<ScriptingService>(
        g_asset_manager->GetBasePath() / "scripts" / "src",
        g_asset_manager->GetBasePath() / "scripts" / "projects",
        g_asset_manager->GetBasePath() / "scripts" / "bin"
    );

    m_scripting_service->Start();

    m_net_request_thread = MakeUnique<net::NetRequestThread>();
    m_net_request_thread->Start();

    // must start after net request thread
    if (m_app_context->GetArguments()["Profile"]) {
        StartProfilerConnectionThread();
    }

    m_entity_instance_batch_holder_map = MakeUnique<EntityInstanceBatchHolderMap>();

    for (uint frame_index = 0; frame_index < max_frames_in_flight; frame_index++) {
        // Global

        for (uint i = 0; i < num_gbuffer_textures; i++) {
            m_global_descriptor_table->GetDescriptorSet(NAME("Global"), frame_index)->SetElement(NAME("GBufferTextures"), i, GetPlaceholderData()->GetImageView2D1x1R8());
        }

        m_global_descriptor_table->GetDescriptorSet(NAME("Global"), frame_index)->SetElement(NAME("GBufferDepthTexture"), GetPlaceholderData()->GetImageView2D1x1R8());
        m_global_descriptor_table->GetDescriptorSet(NAME("Global"), frame_index)->SetElement(NAME("GBufferMipChain"), GetPlaceholderData()->GetImageView2D1x1R8());

        m_global_descriptor_table->GetDescriptorSet(NAME("Global"), frame_index)->SetElement(NAME("BlueNoiseBuffer"), GetPlaceholderData()->GetOrCreateBuffer(GetGPUDevice(), GPUBufferType::STORAGE_BUFFER, sizeof(BlueNoiseBuffer), true /* exact size */));

        m_global_descriptor_table->GetDescriptorSet(NAME("Global"), frame_index)->SetElement(NAME("DeferredResult"), GetPlaceholderData()->GetImageView2D1x1R8());

        m_global_descriptor_table->GetDescriptorSet(NAME("Global"), frame_index)->SetElement(NAME("PostFXPreStack"), 0, GetPlaceholderData()->GetImageView2D1x1R8());
        m_global_descriptor_table->GetDescriptorSet(NAME("Global"), frame_index)->SetElement(NAME("PostFXPreStack"), 1, GetPlaceholderData()->GetImageView2D1x1R8());
        m_global_descriptor_table->GetDescriptorSet(NAME("Global"), frame_index)->SetElement(NAME("PostFXPreStack"), 2, GetPlaceholderData()->GetImageView2D1x1R8());
        m_global_descriptor_table->GetDescriptorSet(NAME("Global"), frame_index)->SetElement(NAME("PostFXPreStack"), 3, GetPlaceholderData()->GetImageView2D1x1R8());
        m_global_descriptor_table->GetDescriptorSet(NAME("Global"), frame_index)->SetElement(NAME("PostFXPostStack"), 0, GetPlaceholderData()->GetImageView2D1x1R8());
        m_global_descriptor_table->GetDescriptorSet(NAME("Global"), frame_index)->SetElement(NAME("PostFXPostStack"), 1, GetPlaceholderData()->GetImageView2D1x1R8());
        m_global_descriptor_table->GetDescriptorSet(NAME("Global"), frame_index)->SetElement(NAME("PostFXPostStack"), 2, GetPlaceholderData()->GetImageView2D1x1R8());
        m_global_descriptor_table->GetDescriptorSet(NAME("Global"), frame_index)->SetElement(NAME("PostFXPostStack"), 3, GetPlaceholderData()->GetImageView2D1x1R8());
        m_global_descriptor_table->GetDescriptorSet(NAME("Global"), frame_index)->SetElement(NAME("PostProcessingUniforms"), GetPlaceholderData()->GetOrCreateBuffer(GetGPUDevice(), GPUBufferType::CONSTANT_BUFFER, sizeof(PostProcessingUniforms), true /* exact size */));

        m_global_descriptor_table->GetDescriptorSet(NAME("Global"), frame_index)->SetElement(NAME("SSAOResultTexture"), GetPlaceholderData()->GetImageView2D1x1R8());
        m_global_descriptor_table->GetDescriptorSet(NAME("Global"), frame_index)->SetElement(NAME("SSRResultTexture"), GetPlaceholderData()->GetImageView2D1x1R8());
        m_global_descriptor_table->GetDescriptorSet(NAME("Global"), frame_index)->SetElement(NAME("TAAResultTexture"), GetPlaceholderData()->GetImageView2D1x1R8());
        m_global_descriptor_table->GetDescriptorSet(NAME("Global"), frame_index)->SetElement(NAME("RTRadianceResultTexture"), GetPlaceholderData()->GetImageView2D1x1R8());
        m_global_descriptor_table->GetDescriptorSet(NAME("Global"), frame_index)->SetElement(NAME("EnvGridIrradianceResultTexture"), GetPlaceholderData()->GetImageView2D1x1R8());
        m_global_descriptor_table->GetDescriptorSet(NAME("Global"), frame_index)->SetElement(NAME("EnvGridRadianceResultTexture"), GetPlaceholderData()->GetImageView2D1x1R8());
        m_global_descriptor_table->GetDescriptorSet(NAME("Global"), frame_index)->SetElement(NAME("ReflectionProbeResultTexture"), GetPlaceholderData()->GetImageView2D1x1R8());

        m_global_descriptor_table->GetDescriptorSet(NAME("Global"), frame_index)->SetElement(NAME("DeferredIndirectResultTexture"), GetPlaceholderData()->GetImageView2D1x1R8());
        m_global_descriptor_table->GetDescriptorSet(NAME("Global"), frame_index)->SetElement(NAME("DeferredDirectResultTexture"), GetPlaceholderData()->GetImageView2D1x1R8());

        m_global_descriptor_table->GetDescriptorSet(NAME("Global"), frame_index)->SetElement(NAME("DepthPyramidResult"), GetPlaceholderData()->GetImageView2D1x1R8());

        m_global_descriptor_table->GetDescriptorSet(NAME("Global"), frame_index)->SetElement(NAME("DDGIUniforms"), GetPlaceholderData()->GetOrCreateBuffer(GetGPUDevice(), GPUBufferType::CONSTANT_BUFFER, sizeof(DDGIUniforms), true /* exact size */));
        m_global_descriptor_table->GetDescriptorSet(NAME("Global"), frame_index)->SetElement(NAME("DDGIIrradianceTexture"), GetPlaceholderData()->GetImageView2D1x1R8());
        m_global_descriptor_table->GetDescriptorSet(NAME("Global"), frame_index)->SetElement(NAME("DDGIDepthTexture"), GetPlaceholderData()->GetImageView2D1x1R8());

        m_global_descriptor_table->GetDescriptorSet(NAME("Global"), frame_index)->SetElement(NAME("SamplerNearest"), GetPlaceholderData()->GetSamplerNearest());
        m_global_descriptor_table->GetDescriptorSet(NAME("Global"), frame_index)->SetElement(NAME("SamplerLinear"), GetPlaceholderData()->GetSamplerLinearMipmap());

        m_global_descriptor_table->GetDescriptorSet(NAME("Global"), frame_index)->SetElement(NAME("UITexture"), GetPlaceholderData()->GetImageView2D1x1R8());

        m_global_descriptor_table->GetDescriptorSet(NAME("Global"), frame_index)->SetElement(NAME("FinalOutputTexture"), GetPlaceholderData()->GetImageView2D1x1R8());

        // Scene
        m_global_descriptor_table->GetDescriptorSet(NAME("Scene"), frame_index)->SetElement(NAME("ScenesBuffer"), m_render_data->scenes->GetBuffer(frame_index));
        m_global_descriptor_table->GetDescriptorSet(NAME("Scene"), frame_index)->SetElement(NAME("LightsBuffer"), m_render_data->lights->GetBuffer(frame_index));
        m_global_descriptor_table->GetDescriptorSet(NAME("Scene"), frame_index)->SetElement(NAME("ObjectsBuffer"), m_render_data->objects->GetBuffer(frame_index));
        m_global_descriptor_table->GetDescriptorSet(NAME("Scene"), frame_index)->SetElement(NAME("CamerasBuffer"), m_render_data->cameras->GetBuffer(frame_index));
        m_global_descriptor_table->GetDescriptorSet(NAME("Scene"), frame_index)->SetElement(NAME("EnvGridsBuffer"), m_render_data->env_grids->GetBuffer(frame_index));
        m_global_descriptor_table->GetDescriptorSet(NAME("Scene"), frame_index)->SetElement(NAME("EnvProbesBuffer"), m_render_data->env_probes->GetBuffer(frame_index));
        m_global_descriptor_table->GetDescriptorSet(NAME("Scene"), frame_index)->SetElement(NAME("CurrentEnvProbe"), m_render_data->env_probes->GetBuffer(frame_index));
        m_global_descriptor_table->GetDescriptorSet(NAME("Scene"), frame_index)->SetElement(NAME("ShadowMapsBuffer"), m_render_data->shadow_map_data->GetBuffer(frame_index));
        m_global_descriptor_table->GetDescriptorSet(NAME("Scene"), frame_index)->SetElement(NAME("SHGridBuffer"), GetRenderData()->spherical_harmonics_grid.sh_grid_buffer);

        for (uint shadow_map_index = 0; shadow_map_index < max_shadow_maps; shadow_map_index++) {
            m_global_descriptor_table->GetDescriptorSet(NAME("Scene"), frame_index)->SetElement(NAME("ShadowMapTextures"), shadow_map_index, GetPlaceholderData()->GetImageView2D1x1R8());
        }

        for (uint shadow_map_index = 0; shadow_map_index < max_bound_point_shadow_maps; shadow_map_index++) {
            m_global_descriptor_table->GetDescriptorSet(NAME("Scene"), frame_index)->SetElement(NAME("PointLightShadowMapTextures"), shadow_map_index, GetPlaceholderData()->GetImageViewCube1x1R8());
        }

        for (uint i = 0; i < max_bound_reflection_probes; i++) {
            m_global_descriptor_table->GetDescriptorSet(NAME("Scene"), frame_index)->SetElement(NAME("EnvProbeTextures"), i, GetPlaceholderData()->GetImageViewCube1x1R8());
        }

        m_global_descriptor_table->GetDescriptorSet(NAME("Scene"), frame_index)->SetElement(NAME("VoxelGridTexture"), GetPlaceholderData()->GetImageView3D1x1x1R8());

        // Object
        m_global_descriptor_table->GetDescriptorSet(NAME("Object"), frame_index)->SetElement(NAME("MaterialsBuffer"), m_render_data->materials->GetBuffer(frame_index));
        m_global_descriptor_table->GetDescriptorSet(NAME("Object"), frame_index)->SetElement(NAME("SkeletonsBuffer"), m_render_data->skeletons->GetBuffer(frame_index));
        // m_global_descriptor_table->GetDescriptorSet(NAME("Object"), frame_index)->SetElement(NAME("EntityInstanceBatchesBuffer"), GetPlaceholderData()->GetOrCreateBuffer(GetGPUDevice(), GPUBufferType::STORAGE_BUFFER, sizeof(EntityInstanceBatch)));

        // Material
#ifdef HYP_FEATURES_BINDLESS_TEXTURES
        for (uint texture_index = 0; texture_index < max_bindless_resources; texture_index++) {
            m_global_descriptor_table->GetDescriptorSet(NAME("Material"), frame_index)
                ->SetElement(NAME("Textures"), texture_index, GetPlaceholderData()->GetImageView2D1x1R8());
        }
#else
        for (uint texture_index = 0; texture_index < max_bound_textures; texture_index++) {
            m_global_descriptor_table->GetDescriptorSet(NAME("Material"), frame_index)
                ->SetElement(NAME("Textures"), texture_index, GetPlaceholderData()->GetImageView2D1x1R8());
        }
#endif
    }

    // m_global_descriptor_set_manager.Initialize(this);

    HYPERION_ASSERT_RESULT(m_global_descriptor_table->Create(m_instance->GetDevice()));

    m_material_descriptor_set_manager.Initialize();

    AssertThrowMsg(AudioManager::GetInstance()->Initialize(), "Failed to initialize audio device");

    m_deferred_renderer = MakeUnique<DeferredRenderer>();
    m_deferred_renderer->Create();

    m_final_pass = MakeUnique<FinalPass>();
    m_final_pass->Create();

    m_debug_drawer = MakeUnique<DebugDrawer>();
    m_debug_drawer->Create();

    m_world = CreateObject<World>();
    InitObject(m_world);
    
    HYP_SYNC_RENDER();

    AssertThrowMsg(m_app_context->GetGame() != nullptr, "Game not set on AppContext!");
    m_app_context->GetGame()->Init_Internal();

    // RenderThread::Start() is blocking, runs until exit
    AssertThrowMsg(m_render_thread->Start(), "Failed to start render thread!");
}

bool Engine::IsRenderLoopActive() const
{
    return m_render_thread != nullptr
        && m_render_thread->IsRunning();
}

void Engine::RequestStop()
{
    if (m_render_thread != nullptr) {
        if (m_render_thread->IsRunning()) {
            m_render_thread->Stop();
        }
    }
}

void Engine::FinalizeStop()
{
    HYP_SCOPE;
    Threads::AssertOnThread(ThreadName::THREAD_MAIN);

    m_is_shutting_down.Set(true, MemoryOrder::SEQUENTIAL);

    HYP_LOG(Engine, Info, "Stopping all engine processes");

    m_delegates.OnShutdown();

    m_scripting_service->Stop();
    m_scripting_service.Reset();

    // must stop before net request thread
    StopProfilerConnectionThread();

    if (m_net_request_thread != nullptr) {
        if (m_net_request_thread->IsRunning()) {
            m_net_request_thread->Stop();
        }

        if (m_net_request_thread->CanJoin()) {
            m_net_request_thread->Join();
        }
    }

    m_world.Reset();

    if (TaskSystem::GetInstance().IsRunning()) { // Stop task system
        HYP_LOG(Tasks, Info, "Stopping task system");

        TaskSystem::GetInstance().Stop();

        HYP_LOG(Tasks, Info, "Task system stopped");
    }

    m_deferred_renderer->Destroy();
    m_deferred_renderer.Reset();

    m_debug_drawer.Reset();

    m_final_pass.Reset();

    m_entity_instance_batch_holder_map.Reset();

    m_render_state.Reset();

    m_render_data->Destroy();

    HYPERION_ASSERT_RESULT(m_global_descriptor_table->Destroy(m_instance->GetDevice()));

    m_placeholder_data->Destroy();

    HYPERION_ASSERT_RESULT(m_instance->GetDevice()->Wait());

    g_safe_deleter->ForceDeleteAll();
    RemoveAllEnqueuedRenderObjectsNow<renderer::Platform::CURRENT>(/* force */true);

    HYPERION_ASSERT_RESULT(m_instance->GetDevice()->Wait());
    HYPERION_ASSERT_RESULT(m_instance->Destroy());

    m_render_thread->Join();
    m_render_thread.Reset();
}

HYP_API void Engine::RenderNextFrame(Game *game)
{
    HYP_PROFILE_BEGIN;

    RendererResult frame_result = GetGPUInstance()->GetFrameHandler()->PrepareFrame(
        GetGPUInstance()->GetDevice(),
        GetGPUInstance()->GetSwapchain()
    );

    if (!frame_result) {
        m_crash_handler.HandleGPUCrash(frame_result);

        RequestStop();

        return;
    }

    Frame *frame = GetGPUInstance()->GetFrameHandler()->GetCurrentFrame();

    if (g_should_recreate_swapchain) {
        HYP_LOG(Rendering, Info, "Recreating swapchain - New size: {}",
            Vec2i(GetGPUInstance()->GetSwapchain()->extent));

        GetDelegates().OnBeforeSwapchainRecreated();

        HYPERION_ASSERT_RESULT(GetGPUDevice()->Wait());
        HYPERION_ASSERT_RESULT(GetGPUInstance()->RecreateSwapchain());
        HYPERION_ASSERT_RESULT(GetGPUDevice()->Wait());

        HYPERION_ASSERT_RESULT(GetGPUInstance()->GetFrameHandler()->GetCurrentFrame()->RecreateFence(
            GetGPUInstance()->GetDevice()
        ));

        // Need to prepare frame again now that swapchain has been recreated.
        HYPERION_ASSERT_RESULT(GetGPUInstance()->GetFrameHandler()->PrepareFrame(
            GetGPUInstance()->GetDevice(),
            GetGPUInstance()->GetSwapchain()
        ));

        m_deferred_renderer->Resize(GetGPUInstance()->GetSwapchain()->extent);

        m_final_pass = MakeUnique<FinalPass>();
        m_final_pass->Create();

        // HYPERION_ASSERT_RESULT(frame->EndCapture(GetGPUInstance()->GetDevice()));
        frame = GetGPUInstance()->GetFrameHandler()->GetCurrentFrame();

        GetDelegates().OnAfterSwapchainRecreated();

        g_should_recreate_swapchain = false;
    }

    HYPERION_ASSERT_RESULT(GetGPUDevice()->GetAsyncCompute()->PrepareForFrame(GetGPUDevice(), frame));
    HYPERION_ASSERT_RESULT(frame->BeginCapture(GetGPUInstance()->GetDevice()));

    PreFrameUpdate(frame);
    
    m_world->GetRenderResources().PreRender(frame);

    game->OnFrameBegin(frame);

    m_world->GetRenderResources().Render(frame);

    RenderDeferred(frame);

    m_final_pass->Render(frame);

    HYPERION_ASSERT_RESULT(frame->EndCapture(GetGPUInstance()->GetDevice()));

    m_world->GetRenderResources().PostRender(frame);

    UpdateBuffersAndDescriptors((GetGPUInstance()->GetFrameHandler()->GetCurrentFrameIndex()));

    game->OnFrameEnd(frame);

    frame_result = frame->Submit(&GetGPUDevice()->GetGraphicsQueue());

    if (!frame_result) {
        //m_crash_handler.HandleGPUCrash(frame_result);

        //m_is_render_loop_active = false;
        //RequestStop();

        g_should_recreate_swapchain = true;

        return;
    }

    HYPERION_ASSERT_RESULT(GetGPUDevice()->GetAsyncCompute()->Submit(GetGPUDevice(), frame));

    GetGPUInstance()->GetFrameHandler()->PresentFrame(&GetGPUDevice()->GetGraphicsQueue(), GetGPUInstance()->GetSwapchain());
    GetGPUInstance()->GetFrameHandler()->NextFrame();
}

void Engine::PreFrameUpdate(Frame *frame)
{
    HYP_SCOPE;

    Threads::AssertOnThread(ThreadName::THREAD_RENDER);

    // Add/remove pending descriptor sets before flushing render commands or updating buffers and descriptor sets.
    // otherwise we'll have an issue when render commands expect the descriptor sets to be there.
    m_material_descriptor_set_manager.UpdatePendingDescriptorSets(frame);

    m_material_descriptor_set_manager.Update(frame);

    HYPERION_ASSERT_RESULT(m_global_descriptor_table->Update(m_instance->GetDevice(), frame->GetFrameIndex()));

    m_deferred_renderer->GetPostProcessing().PerformUpdates();

    HYPERION_ASSERT_RESULT(renderer::RenderCommands::Flush());

    RenderObjectDeleter<renderer::Platform::CURRENT>::Iterate();

    g_safe_deleter->PerformEnqueuedDeletions();

    m_render_state->ResetStates(RENDER_STATE_ACTIVE_ENV_PROBE | RENDER_STATE_ACTIVE_LIGHT);
}

void Engine::UpdateBuffersAndDescriptors(uint32 frame_index)
{
    HYP_SCOPE;

    m_render_data->UpdateBuffers(frame_index);

    for (auto &it : m_entity_instance_batch_holder_map->GetItems()) {
        it.second->UpdateBuffer(m_instance->GetDevice(), frame_index);
    }
}

void Engine::RenderDeferred(Frame *frame)
{
    HYP_SCOPE;

    Threads::AssertOnThread(ThreadName::THREAD_RENDER);

    m_deferred_renderer->Render(frame, m_render_state->GetScene().render_environment.Get());
}

#pragma endregion Engine

#pragma region GlobalDescriptorSetManager

GlobalDescriptorSetManager::GlobalDescriptorSetManager(Engine *engine)
{
    Mutex::Guard guard(m_mutex);

    for (auto &it : renderer::GetStaticDescriptorTableDeclaration().GetElements()) {
        renderer::DescriptorSetLayout layout(it);

        DescriptorSetRef ref = layout.CreateDescriptorSet();
        AssertThrow(ref.IsValid());

        HYP_LOG(Engine, Debug, "Num elements for descriptor set {}: {}", ref.GetName(), ref->GetLayout().GetElements().Size());
        HYP_BREAKPOINT;

        // Init with placeholder data
        for (const auto &layout_it : ref->GetLayout().GetElements()) {
            switch (layout_it.second.type) {
            case renderer::DescriptorSetElementType::UNIFORM_BUFFER: // Fallthrough
            case renderer::DescriptorSetElementType::UNIFORM_BUFFER_DYNAMIC: {
                AssertThrowMsg(layout_it.second.size != uint(-1), "No size set for descriptor %s", layout_it.first.LookupString());

                for (uint i = 0; i < layout_it.second.count; i++) {
                    ref->SetElement(
                        layout_it.first,
                        i,
                        engine->GetPlaceholderData()->GetOrCreateBuffer(
                            engine->GetGPUDevice(),
                            renderer::GPUBufferType::CONSTANT_BUFFER,
                            layout_it.second.size,
                            true // exact_size
                        )
                    );
                }

                break;
            }
            case renderer::DescriptorSetElementType::STORAGE_BUFFER: // Fallthrough
            case renderer::DescriptorSetElementType::STORAGE_BUFFER_DYNAMIC: {
                AssertThrowMsg(layout_it.second.size != uint(-1), "No size set for descriptor %s", layout_it.first.LookupString());

                for (uint i = 0; i < layout_it.second.count; i++) {
                    ref->SetElement(
                        layout_it.first,
                        i,
                        engine->GetPlaceholderData()->GetOrCreateBuffer(
                            engine->GetGPUDevice(),
                            renderer::GPUBufferType::STORAGE_BUFFER,
                            layout_it.second.size,
                            true // exact_size
                        )
                    );
                }

                break;
            }
            case renderer::DescriptorSetElementType::IMAGE: {
                // @TODO: Differentiate between 2D, 3D, and Cubemap
                for (uint i = 0; i < layout_it.second.count; i++) {
                    ref->SetElement(
                        layout_it.first,
                        i,
                        engine->GetPlaceholderData()->GetImageView2D1x1R8()
                    );
                }

                break;
            }
            case renderer::DescriptorSetElementType::IMAGE_STORAGE: {
                // @TODO: Differentiate between 2D, 3D, and Cubemap
                for (uint i = 0; i < layout_it.second.count; i++) {
                    ref->SetElement(
                        layout_it.first,
                        i,
                        engine->GetPlaceholderData()->GetImageView2D1x1R8Storage()
                    );
                }

                break;
            }
            case renderer::DescriptorSetElementType::SAMPLER: {
                for (uint i = 0; i < layout_it.second.count; i++) {
                    ref->SetElement(
                        layout_it.first,
                        i,
                        engine->GetPlaceholderData()->GetSamplerNearest()
                    );
                }

                break;
            }
            case renderer::DescriptorSetElementType::TLAS: {
                // Do nothing, must be manually set.
                break;
            }
            default:
                HYP_LOG(Engine, Error, "Unhandled descriptor type %d", layout_it.second.type);
                break;
            }
        }

        m_descriptor_sets.Insert(it.name, std::move(ref));
    }
}

GlobalDescriptorSetManager::~GlobalDescriptorSetManager() = default;

void GlobalDescriptorSetManager::Initialize(Engine *engine)
{
    Threads::AssertOnThread(ThreadName::THREAD_RENDER);

    Mutex::Guard guard(m_mutex);

    for (auto &it : m_descriptor_sets) {
        HYPERION_ASSERT_RESULT(it.second->Create(engine->GetGPUDevice()));
    }
}

void GlobalDescriptorSetManager::AddDescriptorSet(Name name, const DescriptorSetRef &ref)
{
    Mutex::Guard guard(m_mutex);

    const auto insert_result = m_descriptor_sets.Insert(name, std::move(ref));
    AssertThrowMsg(insert_result.second, "Failed to insert descriptor set, item %s already exists", name.LookupString());
}

DescriptorSetRef GlobalDescriptorSetManager::GetDescriptorSet(Name name) const
{
    Mutex::Guard guard(m_mutex);

    auto it = m_descriptor_sets.Find(name);

    if (it != m_descriptor_sets.End()) {
        return it->second;
    }

    return DescriptorSetRef { };
}

#pragma endregion GlobalDescriptorSetManager

} // namespace hyperion

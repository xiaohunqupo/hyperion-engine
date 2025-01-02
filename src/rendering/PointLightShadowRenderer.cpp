/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <rendering/PointLightShadowRenderer.hpp>
#include <rendering/RenderEnvironment.hpp>
#include <rendering/Light.hpp>
#include <rendering/EnvProbe.hpp>

#include <rendering/backend/RendererFeatures.hpp>

#include <core/logging/LogChannels.hpp>
#include <core/logging/Logger.hpp>

#include <util/profiling/ProfileScope.hpp>

#include <Engine.hpp>

namespace hyperion {

PointLightShadowRenderer::PointLightShadowRenderer(
    Name name,
    Handle<Light> light,
    const Vec2u &extent
) : RenderSubsystem(name),
    m_light(std::move(light)),
    m_extent(extent)
{
}

PointLightShadowRenderer::~PointLightShadowRenderer() = default;

void PointLightShadowRenderer::Init()
{
    if (!InitObject(m_light)) {
        HYP_LOG(Shadows, LogLevel::WARNING, "Point shadow renderer attached to invalid Light");

        return;
    }

    m_aabb = m_light->GetAABB();

    m_env_probe = CreateObject<EnvProbe>(
        Handle<Scene>(GetParent()->GetScene()->GetID()),
        m_aabb,
        m_extent,
        ENV_PROBE_TYPE_SHADOW
    );

    InitObject(m_env_probe);

    m_light->SetShadowMapIndex(m_env_probe->GetID().ToIndex());
    m_env_probe->EnqueueBind();
    m_last_visibility_state = true;
}

// called from game thread
void PointLightShadowRenderer::InitGame()
{
    Threads::AssertOnThread(ThreadName::THREAD_GAME);

    AssertThrow(m_env_probe.IsValid());
}

void PointLightShadowRenderer::OnRemoved()
{
    if (m_env_probe.IsValid()) {
        m_env_probe->EnqueueUnbind();
    }

    m_env_probe.Reset();
}

void PointLightShadowRenderer::OnUpdate(GameCounter::TickUnit delta)
{
    HYP_SCOPE;

    Threads::AssertOnThread(ThreadName::THREAD_GAME);

    AssertThrow(m_env_probe.IsValid());
    AssertThrow(m_light.IsValid());

    const BoundingBox &env_probe_aabb = m_env_probe->GetAABB();
    const BoundingBox light_aabb = m_light->GetAABB();

    if (env_probe_aabb != light_aabb) {
        m_env_probe->SetAABB(light_aabb);
    }

    m_env_probe->Update(delta);
}

void PointLightShadowRenderer::OnRender(Frame *frame)
{
    HYP_SCOPE;

    Threads::AssertOnThread(ThreadName::THREAD_RENDER);

    if (!m_env_probe.IsValid() || !m_light.IsValid()) {
        HYP_LOG(Shadows, LogLevel::WARNING, "Point shadow renderer attached to invalid Light or EnvProbe");

        return;
    }

    LightRenderResources &light_render_resources = m_light->GetRenderResources();

    if (light_render_resources.GetVisibilityBits().Test(GetParent()->GetScene()->GetCamera().GetID().ToIndex())) {
        if (!m_last_visibility_state) {
            g_engine->GetRenderState()->BindEnvProbe(m_env_probe->GetEnvProbeType(), m_env_probe->GetID());

            m_last_visibility_state = true;
        }

        m_env_probe->Render(frame);
    } else {
        // No point in keeping it bound if the light is not visible on the screen.
        if (m_last_visibility_state) {
            g_engine->GetRenderState()->UnbindEnvProbe(m_env_probe->GetEnvProbeType(), m_env_probe->GetID());

            m_last_visibility_state = false;
        }
    }
}

void PointLightShadowRenderer::OnComponentIndexChanged(RenderSubsystem::Index new_index, RenderSubsystem::Index /*prev_index*/)
{
    AssertThrowMsg(false, "Not implemented");
}

} // namespace hyperion

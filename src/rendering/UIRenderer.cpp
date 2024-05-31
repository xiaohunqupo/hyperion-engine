/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <rendering/UIRenderer.hpp>
#include <rendering/RenderEnvironment.hpp>
#include <rendering/RenderGroup.hpp>

#include <scene/ecs/EntityManager.hpp>
#include <scene/ecs/components/UIComponent.hpp>
#include <scene/ecs/components/BoundingBoxComponent.hpp>
#include <scene/ecs/components/TransformComponent.hpp>
#include <scene/ecs/components/MeshComponent.hpp>

#include <core/logging/Logger.hpp>

#include <ui/UIStage.hpp>

#include <util/fs/FsUtil.hpp>

#include <Engine.hpp>

namespace hyperion {

HYP_DECLARE_LOG_CHANNEL(UI);

using renderer::Result;

#pragma region Render commands

struct RENDER_COMMAND(RebuildProxyGroups_UI) : renderer::RenderCommand
{
    RC<EntityDrawCollection>            collection;
    Array<RenderProxy>                  added_proxies;
    Array<ID<Entity>>                   removed_proxies;

    Array<ID<Entity>>                   proxy_ordering;

    FramebufferRef                      framebuffer;
    Optional<RenderableAttributeSet>    override_attributes;

    RENDER_COMMAND(RebuildProxyGroups_UI)(
        const RC<EntityDrawCollection> &collection,
        Array<RenderProxy> &&added_proxies,
        Array<ID<Entity>> &&removed_proxies,
        const Array<ID<Entity>> &proxy_ordering,
        const FramebufferRef &framebuffer = nullptr,
        const Optional<RenderableAttributeSet> &override_attributes = { }
    ) : collection(collection),
        added_proxies(std::move(added_proxies)),
        removed_proxies(std::move(removed_proxies)),
        proxy_ordering(proxy_ordering),
        framebuffer(framebuffer),
        override_attributes(override_attributes)
    {
    }

    virtual ~RENDER_COMMAND(RebuildProxyGroups_UI)() override = default;

    RenderableAttributeSet GetMergedRenderableAttributes(const RenderableAttributeSet &entity_attributes) const
    {
        RenderableAttributeSet attributes = entity_attributes;

        // @FIXME: This is going to be quite slow, adding a reference for each item.
        if (framebuffer.IsValid()) {
            attributes.SetFramebuffer(framebuffer);
        }

        if (override_attributes.HasValue()) {
            if (const ShaderDefinition &override_shader_definition = override_attributes->GetShaderDefinition()) {
                attributes.SetShaderDefinition(override_shader_definition);
            }
            
            ShaderDefinition shader_definition = override_attributes->GetShaderDefinition().IsValid()
                ? override_attributes->GetShaderDefinition()
                : attributes.GetShaderDefinition();

#ifdef HYP_DEBUG_MODE
            AssertThrow(shader_definition.IsValid());
#endif

            // Check for varying vertex attributes on the override shader compared to the entity's vertex
            // attributes. If there is not a match, we should switch to a version of the override shader that
            // has matching vertex attribs.
            const VertexAttributeSet mesh_vertex_attributes = attributes.GetMeshAttributes().vertex_attributes;

            if (mesh_vertex_attributes != shader_definition.GetProperties().GetRequiredVertexAttributes()) {
                shader_definition.properties.SetRequiredVertexAttributes(mesh_vertex_attributes);
            }

            MaterialAttributes new_material_attributes = override_attributes->GetMaterialAttributes();
            new_material_attributes.shader_definition = shader_definition;
            // do not override bucket!
            new_material_attributes.bucket = attributes.GetMaterialAttributes().bucket;

            attributes.SetMaterialAttributes(new_material_attributes);
        }

        return attributes;
    }

    void BuildProxyGroupsInOrder()
    {
        collection->ClearProxyGroups();

        struct
        {
            HashCode    attributes_hash_code;
            uint32      drawable_layer = ~0u;
        } last_render_proxy_group;

        RenderProxyList &proxy_list = collection->GetProxyList(ThreadType::THREAD_TYPE_RENDER);

        for (const ID<Entity> entity : proxy_ordering)
        {
            RenderProxy *proxy = proxy_list.GetProxyForEntity(entity);

            if (!proxy) {
                continue;
            }

            const Handle<Mesh> &mesh = proxy->mesh;
            const Handle<Material> &material = proxy->material;

            if (!mesh.IsValid() || !material.IsValid()) {
                continue;
            }

            RenderableAttributeSet attributes = GetMergedRenderableAttributes(RenderableAttributeSet {
                mesh->GetMeshAttributes(),
                material->GetRenderAttributes()
            });

            const PassType pass_type = BucketToPassType(attributes.GetMaterialAttributes().bucket);

            // skip non-UI items
            if (pass_type != PASS_TYPE_UI) {
                continue;
            }

            if (last_render_proxy_group.drawable_layer != ~0u && last_render_proxy_group.attributes_hash_code == attributes.GetHashCode()) {
                // Set drawable layer on the attributes so it is grouped properly.
                attributes.SetDrawableLayer(last_render_proxy_group.drawable_layer);

                RenderProxyGroup &render_proxy_group = collection->GetProxyGroups()[pass_type][attributes];
                AssertThrow(render_proxy_group.GetRenderGroup().IsValid());

                render_proxy_group.AddRenderProxy(*proxy);
            } else {
                last_render_proxy_group = {
                    attributes.GetHashCode(),
                    last_render_proxy_group.drawable_layer + 1
                };

                attributes.SetDrawableLayer(last_render_proxy_group.drawable_layer);

                RenderProxyGroup &render_proxy_group = collection->GetProxyGroups()[pass_type][attributes];

                if (!render_proxy_group.GetRenderGroup().IsValid()) {
                    // Create RenderGroup
                    Handle<RenderGroup> render_group = g_engine->CreateRenderGroup(attributes);

                    HYP_LOG(UI, LogLevel::DEBUG, "Create render group {} (#{})", attributes.GetHashCode().Value(), render_group.GetID().Value());

#ifdef HYP_DEBUG_MODE
                    if (!render_group.IsValid()) {
                        HYP_LOG(UI, LogLevel::ERROR, "Render group not valid for attribute set {}!", attributes.GetHashCode().Value());

                        continue;
                    }
#endif

                    InitObject(render_group);

                    render_proxy_group.SetRenderGroup(render_group);
                }

                render_proxy_group.AddRenderProxy(*proxy);
            }
        }

        collection->RemoveEmptyProxyGroups();
    }

    virtual Result operator()() override
    {
        RenderProxyList &proxy_list = collection->GetProxyList(ThreadType::THREAD_TYPE_RENDER);

        for (RenderProxy &proxy : added_proxies) {
            const ID<Entity> entity = proxy.entity;

            proxy_list.Add(entity, std::move(proxy));
        }

        for (const ID<Entity> &entity : removed_proxies) {
            proxy_list.MarkToRemove(entity);
        }

        proxy_list.Advance(RenderProxyListAdvanceAction::PERSIST);

        BuildProxyGroupsInOrder();

        HYPERION_RETURN_OK;
    }
};

#pragma endregion Render commands

#pragma region UIRenderList

UIRenderList::UIRenderList()
    : RenderList()
{
}

UIRenderList::UIRenderList(const Handle<Camera> &camera)
    : RenderList(camera)
{
}

UIRenderList::~UIRenderList() = default;

void UIRenderList::ResetOrdering()
{
    m_proxy_ordering.Clear();
}

void UIRenderList::PushEntityToRender(ID<Entity> entity, const RenderProxy &proxy)
{
    RenderList::PushEntityToRender(entity, proxy);

    m_proxy_ordering.PushBack(entity);
}

void UIRenderList::UpdateOnRenderThread(const FramebufferRef &framebuffer, const Optional<RenderableAttributeSet> &override_attributes)
{
    Threads::AssertOnThread(ThreadName::THREAD_GAME);
    AssertThrow(m_draw_collection != nullptr);

    RenderProxyList &proxy_list = m_draw_collection->GetProxyList(ThreadType::THREAD_TYPE_GAME);

    Array<ID<Entity>> removed_proxies;
    proxy_list.GetRemovedEntities(removed_proxies);

    Array<RenderProxy *> added_proxies_ptrs;
    proxy_list.GetAddedEntities(added_proxies_ptrs, true);

    // @TODO Changed

    if (added_proxies_ptrs.Any() || removed_proxies.Any()) {
        Array<RenderProxy> added_proxies;
        added_proxies.Resize(added_proxies_ptrs.Size());

        for (uint i = 0; i < added_proxies_ptrs.Size(); i++) {
            AssertThrow(added_proxies_ptrs[i] != nullptr);

            // Copy the proxy to be added on the render thread
            added_proxies[i] = *added_proxies_ptrs[i];
        }

        PUSH_RENDER_COMMAND(
            RebuildProxyGroups_UI,
            m_draw_collection,
            std::move(added_proxies),
            std::move(removed_proxies),
            m_proxy_ordering,
            framebuffer,
            override_attributes
        );
    }

    proxy_list.Advance(RenderProxyListAdvanceAction::CLEAR);
}

void UIRenderList::CollectDrawCalls(Frame *frame)
{
    Threads::AssertOnThread(ThreadName::THREAD_RENDER);

    using IteratorType = FlatMap<RenderableAttributeSet, RenderProxyGroup>::Iterator;

    Array<IteratorType> iterators;

    for (auto &proxy_groups : m_draw_collection->GetProxyGroups()) {
        for (auto &it : proxy_groups) {
            const RenderableAttributeSet &attributes = it.first;

            if (attributes.GetMaterialAttributes().bucket != BUCKET_UI) {
                continue;
            }

            iterators.PushBack(&it);
        }
    }

    TaskSystem::GetInstance().ParallelForEach(
        TaskThreadPoolName::THREAD_POOL_RENDER,
        iterators,
        [this](IteratorType it, uint, uint)
        {
            RenderProxyGroup &proxy_group = it->second;

            const Handle<RenderGroup> &render_group = proxy_group.GetRenderGroup();
            AssertThrow(render_group.IsValid());

            render_group->CollectDrawCalls(proxy_group.GetRenderProxies());
        }
    );
}

void UIRenderList::ExecuteDrawCalls(Frame *frame) const
{
    Threads::AssertOnThread(ThreadName::THREAD_RENDER);
    
    AssertThrow(m_draw_collection != nullptr);

    const CommandBufferRef &command_buffer = frame->GetCommandBuffer();
    const uint frame_index = frame->GetFrameIndex();

    AssertThrowMsg(m_camera.IsValid(), "Cannot render with invalid Camera");

    const FramebufferRef &framebuffer = m_camera->GetFramebuffer();
    AssertThrow(framebuffer.IsValid());

    framebuffer->BeginCapture(command_buffer, frame_index);

    g_engine->GetRenderState().BindCamera(m_camera.Get());

    using IteratorType = FlatMap<RenderableAttributeSet, RenderProxyGroup>::ConstIterator;
    Array<IteratorType> iterators;

    for (const auto &proxy_groups : m_draw_collection->GetProxyGroups()) {
        for (const auto &it : proxy_groups) {
            iterators.PushBack(&it);
        }
    }

    std::sort(iterators.Begin(), iterators.End(), [](IteratorType lhs, IteratorType rhs) -> bool
    {
        return lhs->first.GetDrawableLayer() < rhs->first.GetDrawableLayer();
    });

    for (SizeType index = 0; index < iterators.Size(); index++) {
        const auto &it = *iterators[index];

        const RenderableAttributeSet &attributes = it.first;
        const RenderProxyGroup &proxy_group = it.second;

        if (attributes.GetMaterialAttributes().bucket != BUCKET_UI) {
            continue;
        }

        AssertThrow(proxy_group.GetRenderGroup().IsValid());

        if (framebuffer.IsValid()) {
            AssertThrowMsg(
                attributes.GetFramebuffer() == framebuffer,
                "Given Framebuffer does not match RenderList item's framebuffer -- invalid data passed?"
            );
        }

        proxy_group.GetRenderGroup()->PerformRendering(frame);
    }

    g_engine->GetRenderState().UnbindCamera();

    framebuffer->EndCapture(command_buffer, frame_index);
}

#pragma endregion UIRenderList

#pragma region UIRenderer

UIRenderer::UIRenderer(Name name, RC<UIStage> ui_stage)
    : RenderComponent(name),
      m_ui_stage(std::move(ui_stage))
{
}

UIRenderer::~UIRenderer()
{
    SafeRelease((std::move(m_framebuffer)));

    g_engine->GetFinalPass()->SetUITexture(Handle<Texture>::empty);
}

void UIRenderer::Init()
{
    CreateFramebuffer();

    AssertThrow(m_ui_stage != nullptr);

    AssertThrow(m_ui_stage->GetScene() != nullptr);
    AssertThrow(m_ui_stage->GetScene()->GetCamera().IsValid());

    m_ui_stage->GetScene()->GetCamera()->SetFramebuffer(m_framebuffer);

    m_render_list.SetCamera(m_ui_stage->GetScene()->GetCamera());

    g_engine->GetFinalPass()->SetUITexture(CreateObject<Texture>(
        m_framebuffer->GetAttachment(0)->GetImage(),
        m_framebuffer->GetAttachment(0)->GetImageView()
    ));
}

void UIRenderer::CreateFramebuffer()
{
    m_framebuffer = g_engine->GetGBuffer()[Bucket::BUCKET_UI].GetFramebuffer();
}

// called from game thread
void UIRenderer::InitGame() { }

void UIRenderer::OnRemoved()
{
    g_engine->GetFinalPass()->SetUITexture(Handle<Texture>::empty);
}

void UIRenderer::OnUpdate(GameCounter::TickUnit delta)
{
    m_render_list.ResetOrdering();

    m_ui_stage->CollectObjects([this](UIObject *object)
    {
        AssertThrow(object != nullptr);

        const NodeProxy &node = object->GetNode();
        AssertThrow(node.IsValid());

        const ID<Entity> entity = node->GetEntity();

        MeshComponent *mesh_component = node->GetScene()->GetEntityManager()->TryGetComponent<MeshComponent>(entity);
        AssertThrow(mesh_component != nullptr);
        AssertThrow(mesh_component->proxy != nullptr);

        m_render_list.PushEntityToRender(
            entity,
            *mesh_component->proxy
        );
    });

    m_render_list.UpdateOnRenderThread(m_ui_stage->GetScene()->GetCamera()->GetFramebuffer());
}

void UIRenderer::OnRender(Frame *frame)
{
    g_engine->GetRenderState().BindScene(m_ui_stage->GetScene());

    m_render_list.CollectDrawCalls(
        frame
    );

    m_render_list.ExecuteDrawCalls(
        frame
    );

    g_engine->GetRenderState().UnbindScene();
}

#pragma endregion UIRenderer

} // namespace hyperion

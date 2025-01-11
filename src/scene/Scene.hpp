/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_SCENE_HPP
#define HYPERION_SCENE_HPP

#include <scene/Node.hpp>
#include <scene/NodeProxy.hpp>
#include <scene/Entity.hpp>
#include <scene/Octree.hpp>
#include <scene/camera/Camera.hpp>

#include <core/Base.hpp>
#include <core/Name.hpp>

#include <core/utilities/DataMutationState.hpp>

#include <core/logging/LoggerFwd.hpp>

#include <core/object/HypObject.hpp>

#include <rendering/Shader.hpp>
#include <rendering/RenderCollection.hpp>

#include <rendering/backend/RenderObject.hpp>

#include <math/Color.hpp>

#include <GameCounter.hpp>
#include <Types.hpp>

namespace hyperion {

HYP_DECLARE_LOG_CHANNEL(Scene);

class RenderEnvironment;
class World;
class Scene;
class EntityManager;
class WorldGrid;

struct FogParams
{
    Color   color = Color(0xF2F8F7FF);
    float   start_distance = 250.0f;
    float   end_distance = 1000.0f;
};

enum class SceneFlags : uint32
{
    NONE        = 0x0,
    HAS_TLAS    = 0x1,
    NON_WORLD   = 0x2,
    DETACHED    = 0x4
};

HYP_MAKE_ENUM_FLAGS(SceneFlags);

struct SceneDrawProxy
{
    uint32 frame_counter;
};

HYP_CLASS()
class HYP_API Scene : public HypObject<Scene>
{
    friend class World;
    friend class UIStage;

    HYP_OBJECT_BODY(Scene);

    HYP_PROPERTY(ID, &Scene::GetID, { { "serialize", false } });

public:
    Scene();
    
    Scene(EnumFlags<SceneFlags> flags);
    
    Scene(
        World *world,
        EnumFlags<SceneFlags> flags = SceneFlags::NONE
    );

    Scene(
        World *world,
        const Handle<Camera> &camera,
        EnumFlags<SceneFlags> flags = SceneFlags::NONE
    );

    Scene(
        World *world,
        const Handle<Camera> &camera,
        ThreadID owner_thread_id,
        EnumFlags<SceneFlags> flags = SceneFlags::NONE
    );

    Scene(const Scene &other)               = delete;
    Scene &operator=(const Scene &other)    = delete;
    ~Scene();

    /*! \brief Get the thread ID that owns this Scene. */
    HYP_FORCE_INLINE ThreadID GetOwnerThreadID() const
        { return m_owner_thread_id; }

    /*! \brief Set the thread ID that owns this Scene.
     *  This is used to assert that the Scene is being accessed from the correct thread.
     *  \note Only call this if you know what you are doing. */
    void SetOwnerThreadID(ThreadID owner_thread_id);

    HYP_FORCE_INLINE EnumFlags<SceneFlags> GetFlags() const
        { return m_flags; }

    HYP_METHOD(Property="Name", Serialize=true, Editor=true)
    HYP_FORCE_INLINE Name GetName() const
        { return m_name; }

    HYP_METHOD(Property="Name", Serialize=true, Editor=true)
    HYP_FORCE_INLINE void SetName(Name name)
        { m_name = name; }

    /*! \brief Get the camera that is used to render this Scene and perform frustum culling. */
    HYP_METHOD(Property="Camera", Serialize=true, Editor=true)
    HYP_FORCE_INLINE const Handle<Camera> &GetCamera() const
        { return m_camera; }

    /*! \brief Set the camera that is used to render this Scene. */
    HYP_METHOD(Property="Camera", Serialize=true, Editor=true)
    void SetCamera(const Handle<Camera> &camera);

    HYP_FORCE_INLINE RenderCollector &GetRenderCollector()
        { return m_render_collector; }

    HYP_FORCE_INLINE const RenderCollector &GetRenderCollector() const
        { return m_render_collector; }

    HYP_METHOD()
    HYP_NODISCARD NodeProxy FindNodeWithEntity(ID<Entity> entity) const;

    HYP_METHOD()
    HYP_NODISCARD NodeProxy FindNodeByName(UTF8StringView name) const;
    
    /*! \brief Get the top level acceleration structure for this Scene, if it exists. */
    HYP_FORCE_INLINE const TLASRef &GetTLAS() const
        { return m_tlas; }

    /*! \brief Creates a top level acceleration structure for this Scene. If one already exists on this Scene,
     *  no action is performed and true is returned. If the TLAS could not be created, false is returned.
     *  The Scene must have had Init() called on it before calling this.
     */
    bool CreateTLAS();

    HYP_METHOD(Property="Root", Serialize=true, Editor=true)
    HYP_FORCE_INLINE const NodeProxy &GetRoot() const
        { return m_root_node_proxy; }

    /*! \brief Set the root node of this Scene, discarding the current.
     *  \internal For internal use only. Should not be called from user code. */
    HYP_METHOD(Property="Root", Serialize=true, Editor=true)
    HYP_FORCE_INLINE void SetRoot(NodeProxy root)
    {
        if (m_root_node_proxy.IsValid() && m_root_node_proxy->GetScene() == this) {
            m_root_node_proxy->SetScene(nullptr);
        }

        m_root_node_proxy = std::move(root);

        if (m_root_node_proxy.IsValid()) {
            m_root_node_proxy->SetScene(this);
        }
    }

    HYP_METHOD()
    HYP_FORCE_INLINE const RC<EntityManager> &GetEntityManager() const
        { return m_entity_manager; }

    HYP_FORCE_INLINE Octree &GetOctree()
        { return m_octree; }

    HYP_FORCE_INLINE const Octree &GetOctree() const
        { return m_octree; }

    HYP_FORCE_INLINE const Handle<RenderEnvironment> &GetEnvironment() const
        { return m_environment; }

    HYP_METHOD()
    HYP_FORCE_INLINE bool IsAttachedToWorld() const
        { return m_world != nullptr; }

    HYP_METHOD()
    HYP_FORCE_INLINE World *GetWorld() const
        { return m_world; }

    void SetWorld(World *world);

    HYP_METHOD()
    HYP_FORCE_INLINE bool IsNonWorldScene() const
        { return m_flags & SceneFlags::NON_WORLD; }

    HYP_METHOD(Property="IsAudioListener", Serialize=true)
    HYP_FORCE_INLINE bool IsAudioListener() const
        { return m_is_audio_listener; }

    HYP_METHOD(Property="IsAudioListener", Serialize=true)
    HYP_FORCE_INLINE void SetIsAudioListener(bool is_audio_listener)
        { m_is_audio_listener = is_audio_listener; }

    HYP_FORCE_INLINE const SceneDrawProxy &GetProxy() const
        { return m_proxy; }

    WorldGrid *GetWorldGrid() const;
    
    void Init();

    void Update(GameCounter::TickUnit delta);

    RenderCollector::CollectionResult CollectEntities(
        RenderCollector &render_collector, 
        const Handle<Camera> &camera,
        const Optional<RenderableAttributeSet> &override_attributes = { },
        bool skip_frustum_culling = false
    ) const;

    RenderCollector::CollectionResult CollectDynamicEntities(
        RenderCollector &render_collector, 
        const Handle<Camera> &camera,
        const Optional<RenderableAttributeSet> &override_attributes = { },
        bool skip_frustum_culling = false
    ) const;

    RenderCollector::CollectionResult CollectStaticEntities(
        RenderCollector &render_collector, 
        const Handle<Camera> &camera,
        const Optional<RenderableAttributeSet> &override_attributes = { },
        bool skip_frustum_culling = false
    ) const;

    bool AddToWorld(World *world);
    bool RemoveFromWorld();
    
    void EnqueueRenderUpdates();

private:
    Name                            m_name;

    ThreadID                        m_owner_thread_id;

    EnumFlags<SceneFlags>           m_flags;

    World                           *m_world;

    Handle<Camera>                  m_camera;
    RenderCollector                 m_render_collector;

    Handle<RenderEnvironment>       m_environment;

    FogParams                       m_fog_params;

    NodeProxy                       m_root_node_proxy;
    RC<EntityManager>               m_entity_manager;

    Octree                          m_octree;

    TLASRef                         m_tlas;

    Matrix4                         m_last_view_projection_matrix;

    bool                            m_is_audio_listener;

    GameCounter::TickUnit           m_previous_delta;
                                 
    mutable DataMutationState       m_mutation_state;

    SceneDrawProxy                  m_proxy;
};

} // namespace hyperion

#endif

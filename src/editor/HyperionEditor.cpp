#include <editor/HyperionEditor.hpp>
#include <editor/EditorObjectProperties.hpp>
#include <editor/EditorDelegates.hpp>
#include <editor/EditorSubsystem.hpp>
#include <editor/EditorProject.hpp>

#include <rendering/Texture.hpp>
#include <rendering/RenderEnvironment.hpp>

#include <rendering/debug/DebugDrawer.hpp>

#include <scene/World.hpp>

#include <scene/ecs/EntityManager.hpp>
#include <scene/ecs/components/MeshComponent.hpp>
#include <scene/ecs/components/SkyComponent.hpp>
#include <scene/ecs/components/TransformComponent.hpp>
#include <scene/ecs/components/AudioComponent.hpp>
#include <scene/ecs/components/LightComponent.hpp>
#include <scene/ecs/components/ShadowMapComponent.hpp>
#include <scene/ecs/components/BoundingBoxComponent.hpp>
#include <scene/ecs/components/VisibilityStateComponent.hpp>
#include <scene/ecs/components/TerrainComponent.hpp>
#include <scene/ecs/components/EnvGridComponent.hpp>
#include <scene/ecs/components/ReflectionProbeComponent.hpp>
#include <scene/ecs/components/RigidBodyComponent.hpp>
#include <scene/ecs/components/BLASComponent.hpp>
#include <scene/ecs/components/ScriptComponent.hpp>
#include <scene/ecs/ComponentInterface.hpp>

#include <scene/world_grid/terrain/TerrainWorldGridPlugin.hpp>
#include <scene/world_grid/WorldGrid.hpp>

#include <asset/AssetBatch.hpp>
#include <asset/Assets.hpp>
#include <asset/serialization/fbom/FBOMWriter.hpp>
#include <asset/serialization/fbom/FBOMReader.hpp>

#include <ui/UIObject.hpp>
#include <ui/UIText.hpp>
#include <ui/UIButton.hpp>
#include <ui/UIPanel.hpp>
#include <ui/UITabView.hpp>
#include <ui/UIMenuBar.hpp>
#include <ui/UIGrid.hpp>
#include <ui/UIImage.hpp>
#include <ui/UIDockableContainer.hpp>
#include <ui/UIListView.hpp>
#include <ui/UITextbox.hpp>
#include <ui/UIDataSource.hpp>
#include <ui/UIWindow.hpp>

#include <core/logging/Logger.hpp>

#include <core/net/HTTPRequest.hpp>

#include <scripting/Script.hpp>
#include <scripting/ScriptingService.hpp>

#include <util/profiling/Profile.hpp>

#include <util/MeshBuilder.hpp>

#include <rendering/lightmapper/LightmapperSubsystem.hpp>
#include <rendering/lightmapper/LightmapUVBuilder.hpp>

#include <system/SystemEvent.hpp>

#include <core/config/Config.hpp>

#include <HyperionEngine.hpp>

// temp
#include <core/object/HypClass.hpp>
#include <core/object/HypProperty.hpp>
#include <core/object/HypMethod.hpp>
#include <core/object/HypField.hpp>
#include <core/object/HypData.hpp>
#include <core/object/HypClassUtils.hpp>

#include <core/containers/Forest.hpp>

namespace hyperion {

HYP_DECLARE_LOG_CHANNEL(Editor);

namespace editor {

#pragma region HyperionEditor

HyperionEditor::HyperionEditor()
    : Game(ManagedGameInfo { "GameName.dll", "TestGame1" })
{
}

HyperionEditor::~HyperionEditor()
{
}

void HyperionEditor::Init()
{
    Game::Init();

#if 0
#if 1
    const HypClass *cls = GetClass<LightComponent>();
    HYP_LOG(Editor, Info, "my class: {}", cls->GetName());

    LightComponent light_component;
    light_component.light = CreateObject<Light>(LightType::POINT, Vec3f { 0.0f, 1.0f, 0.0f }, Color{}, 1.0f, 100.0f);

    if (HypProperty *property = cls->GetProperty("Light")) {
        // property->Set(light_component, CreateObject<Light>(LightType::POINT, Vec3f { 0.0f, 1.0f, 0.0f }, Color{}, 1.0f, 100.0f));

        HYP_LOG(Editor, Info, "LightComponent Light: {}", property->Get(light_component).ToString());

        if (const HypClass *light_class = property->GetHypClass()) {
            AssertThrow(property->GetTypeID() == TypeID::ForType<Light>());
            HYP_LOG(Editor, Info, "light_class: {}", light_class->GetName());
            HypProperty *light_radius_property = light_class->GetProperty("radius");
            AssertThrow(light_radius_property != nullptr);

            light_radius_property->Set(property->Get(light_component), 123.4f);
        }

        HYP_LOG(Editor, Info, "LightComponent Light: {}", property->Get<Handle<Light>>(light_component)->GetRadius());
    }
#endif

    // if (HypProperty *property = cls->GetProperty(NAME("VertexAttributes"))) {
    //     HYP_LOG(Core, Info, "Mesh Vertex Attributes: {}", property->getter.Invoke(m).Get<VertexAttributeSet>().flag_mask);
    // }

    // if (HypProperty *property = cls->GetProperty(NAME("VertexAttributes"))) {
    //     HYP_LOG(Core, Info, "Mesh Vertex Attributes: {}", property->getter.Invoke(m).Get<VertexAttributeSet>().flag_mask);
    // }

    HYP_BREAKPOINT;
#endif

    EditorSubsystem *editor_subsystem = g_engine->GetWorld()->AddSubsystem<EditorSubsystem>(
        GetAppContext(),
        GetUIStage()
    );

    m_scene = editor_subsystem->GetScene();

    editor_subsystem->OnProjectOpened.Bind([this](EditorProject *project)
    {
        m_scene = project->GetScene();
    }).Detach();

    // fbom::FBOMDeserializedObject obj;
    // fbom::FBOMReader reader({});
    // if (auto err = reader.LoadFromFile("Scene.hypscene", obj)) {
    //     HYP_FAIL("failed to load: %s", *err.message);
    // }

    // Handle<Scene> loaded_scene = obj.Get<Scene>();
    // m_scene->SetRoot(loaded_scene->GetRoot());

    // return;

    // if (false) {
        

    //     Array<Handle<Light>> point_lights;

    //     point_lights.PushBack(CreateObject<Light>(PointLight(
    //         Vector3(-5.0f, 0.5f, 0.0f),
    //         Color(1.0f, 0.0f, 0.0f),
    //         1.0f,
    //         5.0f
    //     )));
    //     point_lights.PushBack(CreateObject<Light>(PointLight(
    //         Vector3(5.0f, 2.0f, 0.0f),
    //         Color(0.0f, 1.0f, 0.0f),
    //         1.0f,
    //         15.0f
    //     )));

    //     for (auto &light : point_lights) {
    //         auto point_light_entity = m_scene->GetEntityManager()->AddEntity();

    //         m_scene->GetEntityManager()->AddComponent(point_light_entity, ShadowMapComponent { });

    //         m_scene->GetEntityManager()->AddComponent(point_light_entity, TransformComponent {
    //             Transform(
    //                 light->GetPosition(),
    //                 Vec3f(1.0f),
    //                 Quaternion::Identity()
    //             )
    //         });

    //         m_scene->GetEntityManager()->AddComponent(point_light_entity, LightComponent {
    //             light
    //         });
    //     }
    // }


    // {
    //     Array<Handle<Light>> point_lights;

    //     point_lights.PushBack(CreateObject<Light>(PointLight(
    //        Vector3(0.0f, 1.5f, 2.0f),
    //        Color(0.0f, 1.0f, 0.0f),
    //        10.0f,
    //        15.0f
    //     )));

    //     for (auto &light : point_lights) {
    //         auto point_light_entity = m_scene->GetEntityManager()->AddEntity();

    //         m_scene->GetEntityManager()->AddComponent(point_light_entity, ShadowMapComponent { });

    //         m_scene->GetEntityManager()->AddComponent(point_light_entity, TransformComponent {
    //             Transform(
    //                 light->GetPosition(),
    //                 Vec3f(1.0f),
    //                 Quaternion::Identity()
    //             )
    //         });

    //         m_scene->GetEntityManager()->AddComponent(point_light_entity, LightComponent {
    //             light
    //         });
    //     }
    // }

    // { // add test area light
    //     auto light = CreateObject<Light>(RectangleLight(
    //         Vec3f(0.0f, 1.25f, 0.0f),
    //         Vec3f(0.0f, 0.0f, -1.0f).Normalize(),
    //         Vec2f(2.0f, 2.0f),
    //         Color(1.0f, 0.0f, 0.0f),
    //         1.0f
    //     ));

    //     light->SetMaterial(MaterialCache::GetInstance()->GetOrCreate(
    //         {
    //            .shader_definition = ShaderDefinition {
    //                 NAME("Forward"),
    //                 ShaderProperties(static_mesh_vertex_attributes)
    //             },
    //            .bucket = Bucket::BUCKET_OPAQUE
    //         },
    //         {
    //         }
    //     ));
    //     AssertThrow(light->GetMaterial().IsValid());

    //     InitObject(light);

    //     auto area_light_entity = m_scene->GetEntityManager()->AddEntity();

    //     m_scene->GetEntityManager()->AddComponent(area_light_entity, TransformComponent {
    //         Transform(
    //             light->GetPosition(),
    //             Vec3f(1.0f),
    //             Quaternion::Identity()
    //         )
    //     });

    //     m_scene->GetEntityManager()->AddComponent(area_light_entity, LightComponent {
    //         light
    //     });
    // }

    // { // test terrain
    //     if (WorldGrid *world_grid = m_scene->GetWorldGrid()) {
    //         world_grid->AddPlugin(0, MakeRefCountedPtr<TerrainWorldGridPlugin>());
    //     } else {
    //         HYP_FAIL("Failed to get world grid");
    //     }
    // }

    if (true) { // add test area light
        Handle<Light> light = CreateObject<Light>(
            LightType::AREA_RECT,
            Vec3f(0.0f, 1.25f, 0.0f),
            Vec3f(0.0f, 0.0f, -1.0f).Normalize(),
            Vec2f(2.0f, 2.0f),
            Color(1.0f, 0.0f, 0.0f),
            1.0f,
            1.0f
        );

        Handle<Texture> dummy_light_texture;

        if (auto dummy_light_texture_asset = AssetManager::GetInstance()->Load<Texture>("textures/dummy.jpg")) {
            dummy_light_texture = dummy_light_texture_asset.Result();
        }

        light->SetMaterial(MaterialCache::GetInstance()->GetOrCreate(
            {
               .shader_definition = ShaderDefinition {
                    HYP_NAME(Forward),
                    ShaderProperties(static_mesh_vertex_attributes)
                },
               .bucket = Bucket::BUCKET_OPAQUE
            },
            {
            },
            {
                {
                    MaterialTextureKey::ALBEDO_MAP,
                    std::move(dummy_light_texture)
                }
            }
        ));
        AssertThrow(light->GetMaterial().IsValid());

        InitObject(light);

        auto area_light_entity = m_scene->GetEntityManager()->AddEntity();

        m_scene->GetEntityManager()->AddComponent<TransformComponent>(area_light_entity, {
            Transform(
                light->GetPosition(),
                Vec3f(1.0f),
                Quaternion::Identity()
            )
        });

        m_scene->GetEntityManager()->AddComponent<LightComponent>(area_light_entity, {
            light
        });
    }

#if 1
    // add sun
    
    auto sun = CreateObject<Light>(
        LightType::DIRECTIONAL,
        Vec3f(-0.4f, 0.65f, 0.1f).Normalize(),
        Color(Vec4f(1.0f)),
        4.0f,
        0.0f
    );

    InitObject(sun);

    NodeProxy sun_node = m_scene->GetRoot()->AddChild();
    sun_node.SetName("Sun");

    auto sun_entity = m_scene->GetEntityManager()->AddEntity();
    sun_node.SetEntity(sun_entity);
    sun_node.SetWorldTranslation(Vec3f { -0.1f, 0.65f, 0.1f });

    m_scene->GetEntityManager()->AddComponent<LightComponent>(sun_entity, LightComponent {
        sun
    });

    m_scene->GetEntityManager()->AddComponent<ShadowMapComponent>(sun_entity, ShadowMapComponent {
        .mode       = ShadowMode::PCF,
        .radius     = 35.0f,
        .resolution = { 1024, 1024 }
    });


    // Add Skybox
    if (true) {
        Handle<Entity> skybox_entity = m_scene->GetEntityManager()->AddEntity();

        m_scene->GetEntityManager()->AddComponent<TransformComponent>(skybox_entity, TransformComponent {
            Transform(
                Vec3f::Zero(),
                Vec3f(1000.0f),
                Quaternion::Identity()
            )
        });

        m_scene->GetEntityManager()->AddComponent<SkyComponent>(skybox_entity, SkyComponent { });
        m_scene->GetEntityManager()->AddComponent<VisibilityStateComponent>(skybox_entity, VisibilityStateComponent {
            VISIBILITY_STATE_FLAG_ALWAYS_VISIBLE
        });
        m_scene->GetEntityManager()->AddComponent<BoundingBoxComponent>(skybox_entity, BoundingBoxComponent {
            BoundingBox(Vec3f(-1000.0f), Vec3f(1000.0f))
        });

        NodeProxy skydome_node = m_scene->GetRoot()->AddChild();
        skydome_node.SetEntity(skybox_entity);
        skydome_node.SetName("Sky");
    }

#if 1
    // temp
    RC<AssetBatch> batch = AssetManager::GetInstance()->CreateBatch();
    batch->Add("test_model", "models/sponza/sponza.obj");
    // batch->Add("zombie", "models/ogrexml/dragger_Body.mesh.xml");
    // batch->Add("house", "models/house.obj");

    HYP_LOG(Editor, Debug, "Loading assets, scene ID = {}", GetScene()->GetID().Value());

    Handle<Entity> root_entity = GetScene()->GetEntityManager()->AddEntity();
    GetScene()->GetRoot()->SetEntity(root_entity);

    GetScene()->GetEntityManager()->AddComponent<ScriptComponent>(root_entity, ScriptComponent {
        {
            .assembly_path  = "GameName.dll",
            .class_name     = "FizzBuzzTest"
        }
    });

    batch->OnComplete.Bind([this](AssetMap &results)
    {
#if 1
        NodeProxy node = results["test_model"].ExtractAs<Node>();

        node.Scale(0.02f);
        node.SetName("test_model");
        node.LockTransform();

        if (node.IsValid()) {
            Handle<Entity> env_grid_entity = m_scene->GetEntityManager()->AddEntity();

            m_scene->GetEntityManager()->AddComponent<TransformComponent>(env_grid_entity, TransformComponent {
                node.GetWorldTransform()
            });

            m_scene->GetEntityManager()->AddComponent<BoundingBoxComponent>(env_grid_entity, BoundingBoxComponent {
                node.GetLocalAABB() * 2.0f,
                node.GetWorldAABB() * 2.0f
            });

            // Add env grid component
            m_scene->GetEntityManager()->AddComponent<EnvGridComponent>(env_grid_entity, EnvGridComponent {
                EnvGridType::ENV_GRID_TYPE_SH,
                Vec3u { 24, 4, 24 },
                EnvGridMobility::FOLLOW_CAMERA_X | EnvGridMobility::FOLLOW_CAMERA_Z
            });

            NodeProxy env_grid_node = m_scene->GetRoot()->AddChild();
            env_grid_node.SetEntity(env_grid_entity);
            env_grid_node.SetName("EnvGrid");
        }

        GetScene()->GetRoot()->AddChild(node);
        
        for (auto &node : node.GetChildren()) {
            if (auto child_entity = node.GetEntity()) {
                // Add BLASComponent

                m_scene->GetEntityManager()->AddComponent<BLASComponent>(child_entity, BLASComponent { });
            }
        }
#endif

        if (auto &zombie_asset = results["zombie"]; zombie_asset.IsOK()) {
            auto zombie = NodeProxy(zombie_asset.ExtractAs<Node>());
            zombie.Scale(0.25f);
            zombie.Translate(Vec3f(0, 2.0f, -1.0f));
            auto zombie_entity = zombie[0].GetEntity();

            m_scene->GetRoot()->AddChild(zombie);

            if (zombie_entity.IsValid()) {
                if (auto *mesh_component = m_scene->GetEntityManager()->TryGetComponent<MeshComponent>(zombie_entity)) {
                    mesh_component->material = mesh_component->material->Clone();
                    mesh_component->material->SetParameter(Material::MaterialKey::MATERIAL_KEY_ALBEDO, Vector4(1.0f, 0.0f, 0.0f, 1.0f));
                    mesh_component->material->SetParameter(Material::MaterialKey::MATERIAL_KEY_ROUGHNESS, 0.05f);
                    mesh_component->material->SetParameter(Material::MaterialKey::MATERIAL_KEY_METALNESS, 1.0f);
                    InitObject(mesh_component->material);
                }

                m_scene->GetEntityManager()->AddComponent<AudioComponent>(zombie_entity, AudioComponent {
                    .audio_source = AssetManager::GetInstance()->Load<AudioSource>("sounds/taunt.wav").Result(),
                    .playback_state = {
                        .loop_mode = AudioLoopMode::AUDIO_LOOP_MODE_ONCE,
                        .speed = 2.0f
                    }
                });
            }

            zombie.SetName("zombie");
        }

#if 1
        // testing serialization / deserialization
        FileByteWriter byte_writer("Scene2.hyp");
        fbom::FBOMWriter writer { fbom::FBOMWriterConfig { } };
        writer.Append(*GetScene());
        auto write_err = writer.Emit(&byte_writer);
        byte_writer.Close();

        if (write_err != fbom::FBOMResult::FBOM_OK) {
            HYP_FAIL("Failed to save scene: %s", write_err.message.Data());
        }
#endif
    }).Detach();

    batch->LoadAsync();
#endif

#else
    HypData loaded_scene_data;
    fbom::FBOMReader reader({});
    if (auto err = reader.LoadFromFile("Scene2.hyp", loaded_scene_data)) {
        HYP_FAIL("failed to load: %s", *err.message);
    }
    DebugLog(LogType::Debug, "static data buffer size: %u\n", reader.m_static_data_buffer.Size());

    Handle<Scene> loaded_scene = loaded_scene_data.Get<Handle<Scene>>();
    
    DebugLog(LogType::Debug, "Loaded scene root node : %s\n", *loaded_scene->GetRoot().GetName());

    Proc<void, const NodeProxy &, int> DebugPrintNode;

    DebugPrintNode = [this, &DebugPrintNode](const NodeProxy &node, int depth)
    {
        if (!node.IsValid()) {
            return;
        }

        String str;

        for (int i = 0; i < depth; i++) {
            str += "  ";
        }
        
        json::JSONObject node_json;
        node_json["name"] = node.GetName();

        json::JSONValue entity_json = json::JSONUndefined();
        if (auto entity = node.GetEntity()) {
            json::JSONObject entity_json_object;
            entity_json_object["id"] = entity.GetID().Value();

            EntityManager *entity_manager = EntityManager::GetEntityToEntityManagerMap().GetEntityManager(entity);
            AssertThrow(entity_manager != nullptr);

            Optional<const TypeMap<ComponentID> &> all_components = entity_manager->GetAllComponents(entity);
            AssertThrow(all_components.HasValue());
            
            json::JSONArray components_json;

            for (const KeyValuePair<TypeID, ComponentID> &it : *all_components) {
                const ComponentInterface *component_interface = ComponentInterfaceRegistry::GetInstance().GetComponentInterface(it.first);

                json::JSONObject component_json;
                component_json["type"] = component_interface->GetTypeName();
                component_json["id"] = it.second;

                if (component_interface->GetTypeID() == TypeID::ForType<MeshComponent>()) {
                    const MeshComponent *mesh_component = entity_manager->TryGetComponent<MeshComponent>(entity);
                    AssertThrow(mesh_component != nullptr);

                    json::JSONObject mesh_component_json;
                    mesh_component_json["mesh_id"] = mesh_component->mesh.GetID().Value();
                    mesh_component_json["material_id"] = mesh_component->material.GetID().Value();
                    mesh_component_json["skeleton_id"] = mesh_component->skeleton.GetID().Value();

                    json::JSONObject mesh_json;
                    mesh_json["type"] = "Mesh";
                    mesh_json["num_indices"] = mesh_component->mesh->NumIndices();

                    mesh_component_json["mesh"] = std::move(mesh_json);

                    component_json["data"] = std::move(mesh_component_json);
                } else if (component_interface->GetTypeID() == TypeID::ForType<TransformComponent>()) {
                    const TransformComponent *transform_component = entity_manager->TryGetComponent<TransformComponent>(entity);
                    AssertThrow(transform_component != nullptr);

                    json::JSONObject transform_component_json;
                    transform_component_json["translation"] = HYP_FORMAT("{}", transform_component->transform.GetTranslation());
                    transform_component_json["scale"] = HYP_FORMAT("{}", transform_component->transform.GetScale());
                    transform_component_json["rotation"] = HYP_FORMAT("{}", transform_component->transform.GetRotation());

                    component_json["data"] = std::move(transform_component_json);
                }

                components_json.PushBack(std::move(component_json));
            }

            entity_json_object["components"] = std::move(components_json);

            entity_json = std::move(entity_json_object);
        }

        node_json["entity"] = std::move(entity_json);

        DebugLog(LogType::Debug, "%s\n", (str + json::JSONValue(node_json).ToString()).Data());

        for (auto &child : node->GetChildren()) {
            DebugPrintNode(child, depth + 1);
        }
    };

    // m_scene->GetRoot()->AddChild(loaded_scene->GetRoot());

    // DebugPrintNode(m_scene->GetRoot(), 0);

    // HYP_BREAKPOINT;


    NodeProxy root = loaded_scene->GetRoot();

    loaded_scene->SetRoot(NodeProxy::empty);

    m_scene->GetRoot()->AddChild(root);


    if (false) {
        // testing reflection capture
        Handle<Entity> reflection_probe_entity = m_scene->GetEntityManager()->AddEntity();

        m_scene->GetEntityManager()->AddComponent<TransformComponent>(reflection_probe_entity, TransformComponent { });

        m_scene->GetEntityManager()->AddComponent<BoundingBoxComponent>(reflection_probe_entity, BoundingBoxComponent {
            m_scene->GetRoot()->GetWorldAABB(),
            m_scene->GetRoot()->GetWorldAABB()
        });

        m_scene->GetEntityManager()->AddComponent<ReflectionProbeComponent>(reflection_probe_entity, ReflectionProbeComponent { });

        NodeProxy reflection_probe_node = m_scene->GetRoot()->AddChild();
        reflection_probe_node.SetEntity(reflection_probe_entity);
        reflection_probe_node.SetName("ReflectionProbeTest");
    }
#endif
}

void HyperionEditor::Teardown()
{
}

    
void HyperionEditor::Logic(GameCounter::TickUnit delta)
{
    // auto env_grid_node = m_scene->FindNodeByName("EnvGrid");

    // if (env_grid_node) {
    //     env_grid_node.SetWorldTranslation(Vec3f {
    //         m_scene->GetCamera()->GetTranslation().x,
    //         0.0f,
    //         m_scene->GetCamera()->GetTranslation().z
    //     });
    // }
}

void HyperionEditor::OnInputEvent(const SystemEvent &event)
{
    Game::OnInputEvent(event);

    if (event.GetType() == SystemEventType::EVENT_KEYDOWN && event.GetNormalizedKeyCode() == KeyCode::KEY_M) {
        NodeProxy test_model = m_scene->FindNodeByName("test_model");

        if (test_model) {
            test_model->UnlockTransform();
            test_model->Translate(Vec3f { 0.01f });
            test_model->LockTransform();
        }
    }
}

void HyperionEditor::OnFrameEnd(Frame *frame)
{
    Game::OnFrameEnd(frame);
}

#pragma endregion HyperionEditor

} // namespace editor
} // namespace hyperion
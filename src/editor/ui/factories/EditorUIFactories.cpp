#include <editor/ui/EditorUI.hpp>
#include <editor/EditorAction.hpp>
#include <editor/EditorSubsystem.hpp>

#include <asset/Assets.hpp>
#include <asset/AssetRegistry.hpp>

#include <scene/Node.hpp>
#include <scene/World.hpp>
#include <scene/ecs/EntityManager.hpp>
#include <scene/ecs/ComponentInterface.hpp>

#include <ui/UIText.hpp>
#include <ui/UITextbox.hpp>
#include <ui/UIStage.hpp>
#include <ui/UIGrid.hpp>
#include <ui/UIButton.hpp>
#include <ui/UIDataSource.hpp>

#include <core/memory/RefCountedPtr.hpp>

#include <core/threading/Task.hpp>

#include <core/object/HypClass.hpp>
#include <core/object/HypData.hpp>
#include <core/object/HypProperty.hpp>

#include <core/logging/Logger.hpp>

#include <core/Defines.hpp>

namespace hyperion {

HYP_DECLARE_LOG_CHANNEL(Editor);

class HypDataUIElementFactory : public UIElementFactory<HypData, HypDataUIElementFactory>
{
public:
    RC<UIObject> Create(UIObject *parent, const HypData &value) const
    {
        const HypClass *hyp_class = GetClass(value.GetTypeID());
        AssertThrowMsg(hyp_class != nullptr, "No HypClass registered for TypeID %u", value.GetTypeID().Value());

        if (value.IsNull()) {
            RC<UIText> empty_value_text = parent->CreateUIObject<UIText>(Vec2i { 0, 0 }, UIObjectSize(UIObjectSize::AUTO));
            empty_value_text->SetText("Object is null");

            return empty_value_text;
        }

        RC<UIGrid> grid = parent->CreateUIObject<UIGrid>(Vec2i { 0, 0 }, UIObjectSize({ 100, UIObjectSize::PERCENT }, { 0, UIObjectSize::AUTO }));

        HashMap<String, HypProperty *> properties_by_name;

        for (auto it = hyp_class->GetMembers(HypMemberType::TYPE_PROPERTY).Begin(); it != hyp_class->GetMembers(HypMemberType::TYPE_PROPERTY).End(); ++it) {
            if (HypProperty *property = dynamic_cast<HypProperty *>(&*it)) {
                if (!property->GetAttribute("editor")) {
                    continue;
                }

                if (!property->CanGet() || !property->CanSet()) {
                    continue;
                }

                properties_by_name[property->GetName().LookupString()] = property;
            } else {
                HYP_UNREACHABLE();
            }
        }

        for (auto &it : properties_by_name) {
            RC<UIGridRow> row = grid->AddRow();

            RC<UIGridColumn> column = row->AddColumn();

            RC<UIPanel> panel = parent->CreateUIObject<UIPanel>(Vec2i { 0, 0 }, UIObjectSize({ 100, UIObjectSize::PERCENT }, { 0, UIObjectSize::AUTO }));
            panel->SetPadding({ 1, 1 });

            HypData getter_result = it.second->Get(value);
            
            UIElementFactoryBase *factory = GetEditorUIElementFactory(getter_result.GetTypeID());
            
            if (!factory) {
                HYP_LOG(Editor, Warning, "No factory registered for TypeID {} when creating UI element for attribute \"{}\"", getter_result.GetTypeID().Value(), it.first);

                continue;
            }

            RC<UIObject> element = factory->CreateUIObject(parent, getter_result, { });
            AssertThrow(element != nullptr);

            HYP_LOG(Editor, Debug, "Element for attribute \"{}\": {}\tsize: {}", it.first, GetClass(element.GetTypeID())->GetName(), element->GetActualSize());
            
            panel->AddChildUIObject(element);

            column->AddChildUIObject(panel);
        }

        return grid;
    }

    void Update(UIObject *ui_object, const HypData &value) const
    {
    }
};

HYP_DEFINE_UI_ELEMENT_FACTORY(HypData, HypDataUIElementFactory);


template <int StringType>
class StringUIElementFactory : public UIElementFactory<containers::detail::String<StringType>, StringUIElementFactory<StringType>>
{
public:
    RC<UIObject> Create(UIObject *parent, const containers::detail::String<StringType> &value) const
    {
        RC<UITextbox> textbox = parent->CreateUIObject<UITextbox>(Vec2i { 0, 0 }, UIObjectSize({ 100, UIObjectSize::PERCENT }, { 20, UIObjectSize::PIXEL }));
        textbox->SetText(value.ToUTF8());
        
        return textbox;
    }

    void Update(UIObject *ui_object, const containers::detail::String<StringType> &value) const
    {
        ui_object->SetText(value.ToUTF8());
    }
};

HYP_DEFINE_UI_ELEMENT_FACTORY(containers::detail::String<StringType::ANSI>, StringUIElementFactory<StringType::ANSI>);
HYP_DEFINE_UI_ELEMENT_FACTORY(containers::detail::String<StringType::UTF8>, StringUIElementFactory<StringType::UTF8>);
HYP_DEFINE_UI_ELEMENT_FACTORY(containers::detail::String<StringType::UTF16>, StringUIElementFactory<StringType::UTF16>);
HYP_DEFINE_UI_ELEMENT_FACTORY(containers::detail::String<StringType::UTF32>, StringUIElementFactory<StringType::UTF32>);
HYP_DEFINE_UI_ELEMENT_FACTORY(containers::detail::String<StringType::WIDE_CHAR>, StringUIElementFactory<StringType::WIDE_CHAR>);

class Vec3fUIElementFactory : public UIElementFactory<Vec3f, Vec3fUIElementFactory>
{
public:
    RC<UIObject> Create(UIObject *parent, const Vec3f &value) const
    {
        RC<UIGrid> grid = parent->CreateUIObject<UIGrid>(Vec2i { 0, 0 }, UIObjectSize({ 100, UIObjectSize::PERCENT }, { 0, UIObjectSize::AUTO }));

        RC<UIGridRow> row = grid->AddRow();

        {
            RC<UIGridColumn> col = row->AddColumn();

            RC<UIPanel> panel = parent->CreateUIObject<UIPanel>(NAME("Vec3fPanel_X"), Vec2i { 0, 0 }, UIObjectSize({ 100, UIObjectSize::PERCENT }, { 0, UIObjectSize::AUTO }));
            panel->SetPadding({ 1, 1 });
            
            RC<UITextbox> textbox = parent->CreateUIObject<UITextbox>(NAME("Vec3fPanel_X_Value"), Vec2i { 0, 0 }, UIObjectSize({ 100, UIObjectSize::PERCENT }, { 20, UIObjectSize::PIXEL }));
            textbox->SetText(HYP_FORMAT("{}", value.x));
            panel->AddChildUIObject(textbox); 

            col->AddChildUIObject(panel);
        }

        {
            RC<UIGridColumn> col = row->AddColumn();

            RC<UIPanel> panel = parent->CreateUIObject<UIPanel>(NAME("Vec3fPanel_Y"), Vec2i { 0, 0 }, UIObjectSize({ 100, UIObjectSize::PERCENT }, { 0, UIObjectSize::AUTO }));
            panel->SetPadding({ 1, 1 });

            RC<UITextbox> textbox = parent->CreateUIObject<UITextbox>(NAME("Vec3fPanel_Y_Value"), Vec2i { 0, 0 }, UIObjectSize({ 100, UIObjectSize::PERCENT }, { 20, UIObjectSize::PIXEL }));
            textbox->SetText(HYP_FORMAT("{}", value.y));
            panel->AddChildUIObject(textbox);

            col->AddChildUIObject(panel);
        }

        {
            RC<UIGridColumn> col = row->AddColumn();

            RC<UIPanel> panel = parent->CreateUIObject<UIPanel>(NAME("Vec3fPanel_Z"), Vec2i { 0, 0 }, UIObjectSize({ 100, UIObjectSize::PERCENT }, { 0, UIObjectSize::AUTO }));
            panel->SetPadding({ 1, 1 });

            RC<UITextbox> textbox = parent->CreateUIObject<UITextbox>(NAME("Vec3fPanel_Z_Value"), Vec2i { 0, 0 }, UIObjectSize({ 100, UIObjectSize::PERCENT }, { 20, UIObjectSize::PIXEL }));
            textbox->SetText(HYP_FORMAT("{}", value.z));
            panel->AddChildUIObject(textbox);

            col->AddChildUIObject(panel);
        }

        return grid;
    }

    void Update(UIObject *ui_object, const Vec3f &value) const
    {
        ui_object->FindChildUIObject(NAME("Vec3fPanel_X_Value"))
            .Cast<UITextbox>()
            ->SetText(HYP_FORMAT("{}", value.x));

        ui_object->FindChildUIObject(NAME("Vec3fPanel_Y_Value"))
            .Cast<UITextbox>()
            ->SetText(HYP_FORMAT("{}", value.y));

        ui_object->FindChildUIObject(NAME("Vec3fPanel_Z_Value"))
            .Cast<UITextbox>()
            ->SetText(HYP_FORMAT("{}", value.z));
    }
};

HYP_DEFINE_UI_ELEMENT_FACTORY(Vec3f, Vec3fUIElementFactory);


class Uint32UIElementFactory : public UIElementFactory<uint32, Uint32UIElementFactory>
{
public:
    RC<UIObject> Create(UIObject *parent, const uint32 &value) const
    {
        RC<UIGrid> grid = parent->CreateUIObject<UIGrid>(Vec2i { 0, 0 }, UIObjectSize({ 100, UIObjectSize::PERCENT }, { 0, UIObjectSize::AUTO }));

        RC<UIGridRow> row = grid->AddRow();

        {
            RC<UIGridColumn> col = row->AddColumn();
            
            RC<UITextbox> textbox = parent->CreateUIObject<UITextbox>(NAME("Value"), Vec2i { 0, 0 }, UIObjectSize({ 100, UIObjectSize::PERCENT }, { 20, UIObjectSize::PIXEL }));
            textbox->SetText(HYP_FORMAT("{}", value));

            col->AddChildUIObject(textbox);
        }

        return grid;
    }

    void Update(UIObject *ui_object, const uint32 &value) const
    {
        ui_object->FindChildUIObject(NAME("Value"))
            .Cast<UITextbox>()
            ->SetText(HYP_FORMAT("{}", value));
    }
};

HYP_DEFINE_UI_ELEMENT_FACTORY(uint32, Uint32UIElementFactory);

class QuaternionUIElementFactory : public UIElementFactory<Quaternion, QuaternionUIElementFactory>
{
public:
    RC<UIObject> Create(UIObject *parent, const Quaternion &value) const
    {
        RC<UIGrid> grid = parent->CreateUIObject<UIGrid>(Vec2i { 0, 0 }, UIObjectSize({ 100, UIObjectSize::PERCENT }, { 0, UIObjectSize::AUTO }));

        RC<UIGridRow> row = grid->AddRow();

        {
            RC<UIGridColumn> col = row->AddColumn();

            RC<UIPanel> panel = parent->CreateUIObject<UIPanel>(NAME("QuaternionPanel_Roll"), Vec2i { 0, 0 }, UIObjectSize({ 100, UIObjectSize::PERCENT }, { 0, UIObjectSize::AUTO }));
            panel->SetPadding({ 1, 1 });
            
            RC<UITextbox> textbox = parent->CreateUIObject<UITextbox>(NAME("QuaternionPanel_Roll_Value"), Vec2i { 0, 0 }, UIObjectSize({ 100, UIObjectSize::PERCENT }, { 20, UIObjectSize::PIXEL }));
            textbox->SetText(HYP_FORMAT("{}", value.Roll()));
            panel->AddChildUIObject(textbox); 

            col->AddChildUIObject(panel);
        }

        {
            RC<UIGridColumn> col = row->AddColumn();

            RC<UIPanel> panel = parent->CreateUIObject<UIPanel>(NAME("QuaternionPanel_Pitch"), Vec2i { 0, 0 }, UIObjectSize({ 100, UIObjectSize::PERCENT }, { 0, UIObjectSize::AUTO }));
            panel->SetPadding({ 1, 1 });

            RC<UITextbox> textbox = parent->CreateUIObject<UITextbox>(NAME("QuaternionPanel_Pitch_Value"), Vec2i { 0, 0 }, UIObjectSize({ 100, UIObjectSize::PERCENT }, { 20, UIObjectSize::PIXEL }));
            textbox->SetText(HYP_FORMAT("{}", value.Pitch()));
            panel->AddChildUIObject(textbox);

            col->AddChildUIObject(panel);
        }

        {
            RC<UIGridColumn> col = row->AddColumn();

            RC<UIPanel> panel = parent->CreateUIObject<UIPanel>(NAME("QuaternionPanel_Yaw"), Vec2i { 0, 0 }, UIObjectSize({ 100, UIObjectSize::PERCENT }, { 0, UIObjectSize::AUTO }));
            panel->SetPadding({ 1, 1 });

            RC<UITextbox> textbox = parent->CreateUIObject<UITextbox>(NAME("QuaternionPanel_Yaw_Value"), Vec2i { 0, 0 }, UIObjectSize({ 100, UIObjectSize::PERCENT }, { 20, UIObjectSize::PIXEL }));
            textbox->SetText(HYP_FORMAT("{}", value.Yaw()));
            panel->AddChildUIObject(textbox);

            col->AddChildUIObject(panel);
        }

        return grid;
    }

    void Update(UIObject *ui_object, const Quaternion &value) const
    {
        ui_object->FindChildUIObject(NAME("QuaternionPanel_Roll_Value"))
            .Cast<UITextbox>()
            ->SetText(HYP_FORMAT("{}", value.Roll()));

        ui_object->FindChildUIObject(NAME("QuaternionPanel_Pitch_Value"))
            .Cast<UITextbox>()
            ->SetText(HYP_FORMAT("{}", value.Pitch()));

        ui_object->FindChildUIObject(NAME("QuaternionPanel_Yaw_Value"))
            .Cast<UITextbox>()
            ->SetText(HYP_FORMAT("{}", value.Yaw()));
    }
};

HYP_DEFINE_UI_ELEMENT_FACTORY(Quaternion, QuaternionUIElementFactory);

class TransformUIElementFactory : public UIElementFactory<Transform, TransformUIElementFactory>
{
public:
    RC<UIObject> Create(UIObject *parent, const Transform &value) const
    {
        const HypClass *hyp_class = GetClass<Transform>();

        RC<UIGrid> grid = parent->CreateUIObject<UIGrid>(Vec2i { 0, 0 }, UIObjectSize({ 100, UIObjectSize::PERCENT }, { 0, UIObjectSize::AUTO }));

        {
            RC<UIGridRow> translation_header_row = grid->AddRow();
            RC<UIGridColumn> translation_header_column = translation_header_row->AddColumn();

            RC<UIText> translation_header = parent->CreateUIObject<UIText>(Vec2i { 0, 0 }, UIObjectSize(UIObjectSize::AUTO));
            translation_header->SetText("Translation");
            translation_header_column->AddChildUIObject(translation_header);

            RC<UIGridRow> translation_value_row = grid->AddRow();
            RC<UIGridColumn> translation_value_column = translation_value_row->AddColumn();
            
            if (UIElementFactoryBase *factory = GetEditorUIElementFactory<Vec3f>()) {
                RC<UIObject> translation_element = factory->CreateUIObject(parent, HypData(value.GetTranslation()), { });
                translation_value_column->AddChildUIObject(translation_element);
            }
        }

        {
            RC<UIGridRow> rotation_header_row = grid->AddRow();
            RC<UIGridColumn> rotation_header_column = rotation_header_row->AddColumn();

            RC<UIText> rotation_header = parent->CreateUIObject<UIText>(Vec2i { 0, 0 }, UIObjectSize(UIObjectSize::AUTO));
            rotation_header->SetText("Rotation");
            rotation_header_column->AddChildUIObject(rotation_header);

            RC<UIGridRow> rotation_value_row = grid->AddRow();
            RC<UIGridColumn> rotation_value_column = rotation_value_row->AddColumn();
            
            if (UIElementFactoryBase *factory = GetEditorUIElementFactory<Quaternion>()) {
                RC<UIObject> rotation_element = factory->CreateUIObject(parent, HypData(value.GetRotation()), { });
                rotation_value_column->AddChildUIObject(rotation_element);
            }
        }

        {
            RC<UIGridRow> scale_header_row = grid->AddRow();
            RC<UIGridColumn> scale_header_column = scale_header_row->AddColumn();

            RC<UIText> scale_header = parent->CreateUIObject<UIText>(Vec2i { 0, 0 }, UIObjectSize(UIObjectSize::AUTO));
            scale_header->SetText("Scale");
            scale_header_column->AddChildUIObject(scale_header);

            RC<UIGridRow> scale_value_row = grid->AddRow();
            RC<UIGridColumn> scale_value_column = scale_value_row->AddColumn();
            
            if (UIElementFactoryBase *factory = GetEditorUIElementFactory<Vec3f>()) {
                RC<UIObject> scale_element = factory->CreateUIObject(parent, HypData(value.GetScale()), { });
                scale_value_column->AddChildUIObject(scale_element);
            }
        }

        return grid;
    }

    void Update(UIObject *ui_object, const Transform &value) const
    {
        HYP_NOT_IMPLEMENTED_VOID();

        // ui_object->FindChildUIObject(NAME("TranslationValue"))
        //     .Cast<UITextbox>()
        //     ->SetText(HYP_FORMAT("{}", value.GetTranslation()));

        // ui_object->FindChildUIObject(NAME("RotationValue"))
        //     .Cast<UITextbox>()
        //     ->SetText(HYP_FORMAT("{}", value.GetRotation()));

        // ui_object->FindChildUIObject(NAME("ScaleValue"))
        //     .Cast<UITextbox>()
        //     ->SetText(HYP_FORMAT("{}", value.GetScale()));
    }
};

HYP_DEFINE_UI_ELEMENT_FACTORY(Transform, TransformUIElementFactory);

class EditorWeakNodeFactory : public UIElementFactory<Weak<Node>, EditorWeakNodeFactory>
{
public:
    RC<UIObject> Create(UIObject *parent, const Weak<Node> &value) const
    {
        String node_name = "Invalid";
        UUID node_uuid = UUID::Invalid();

        if (RC<Node> node_rc = value.Lock()) {
            node_name = node_rc->GetName();
            node_uuid = node_rc->GetUUID();
        } else {
            node_uuid = UUID();
        }

        RC<UIText> text = parent->CreateUIObject<UIText>(CreateNameFromDynamicString(ANSIString("Node_") + node_uuid.ToString()), Vec2i { 0, 0 }, UIObjectSize(UIObjectSize::AUTO));
        text->SetText(node_name);
        
        return text;
    }

    void Update(UIObject *ui_object, const Weak<Node> &value) const
    {
        static const String invalid_node_name = "<Invalid>";

        if (UIText *text = dynamic_cast<UIText *>(ui_object)) {
            if (RC<Node> node_rc = value.Lock()) {
                text->SetText(node_rc->GetName());
            } else {
                text->SetText(invalid_node_name);
            }
        }
    }
};

HYP_DEFINE_UI_ELEMENT_FACTORY(Weak<Node>, EditorWeakNodeFactory);

class EntityUIElementFactory : public UIElementFactory<Handle<Entity>, EntityUIElementFactory>
{
public:
    RC<UIObject> Create(UIObject *parent, const Handle<Entity> &entity) const
    {
        const EditorNodePropertyRef *context = GetContext<EditorNodePropertyRef>();
        AssertThrow(context != nullptr);

        if (!entity.IsValid()) {
            RC<UIGrid> grid = parent->CreateUIObject<UIGrid>(Vec2i { 0, 0 }, UIObjectSize({ 100, UIObjectSize::PERCENT }, { 0, UIObjectSize::AUTO }));

            RC<UIGridRow> row = grid->AddRow();
            RC<UIGridColumn> column = row->AddColumn();

            RC<UIButton> add_entity_button = parent->CreateUIObject<UIButton>(NAME("Add_Entity_Button"), Vec2i { 0, 0 }, UIObjectSize({ 100, UIObjectSize::PERCENT }, { 0, UIObjectSize::AUTO }));
            add_entity_button->SetText("Add Entity");

            add_entity_button->OnClick.Bind([world = Handle<World>(parent->GetWorld()), node_weak = context->node](...) -> UIEventHandlerResult
            {
                HYP_LOG(Editor, Debug, "Add Entity clicked");

                if (RC<Node> node_rc = node_weak.Lock()) {
                    world->GetSubsystem<EditorSubsystem>()->GetActionStack()->Push(MakeRefCountedPtr<FunctionalEditorAction>(
                        NAME("NodeSetEntity"),
                        [node_rc, entity = Handle<Entity>::empty]() mutable -> EditorActionFunctions
                        {
                            return {
                                [&]()
                                {
                                    Scene *scene = node_rc->GetScene();

                                    if (!scene) {
                                        HYP_LOG(Editor, Error, "GetScene() returned null for Node with name \"{}\", cannot add Entity", node_rc->GetName());

                                        return;
                                    }

                                    if (!entity.IsValid()) {
                                        entity = scene->GetEntityManager()->AddEntity();
                                    }

                                    node_rc->SetEntity(entity);
                                },
                                [&]()
                                {
                                    node_rc->SetEntity(Handle<Entity>::empty);
                                }
                            };
                        }
                    ));

                    return UIEventHandlerResult::STOP_BUBBLING;
                }

                HYP_LOG(Editor, Error, "Cannot add Entity to Node, Node reference could not be obtained");

                return UIEventHandlerResult::ERR;
            }).Detach();

            column->AddChildUIObject(add_entity_button);

            return grid;
        }

        EntityManager *entity_manager = EntityManager::GetEntityToEntityManagerMap().GetEntityManager(entity);

        if (!entity_manager) {
            HYP_LOG(Editor, Error, "No EntityManager found for entity #{}", entity.GetID().Value());
            
            return nullptr;
        }

        auto CreateComponentsGrid = [&]() -> RC<UIObject>
        {
            Optional<const TypeMap<ComponentID> &> all_components = entity_manager->GetAllComponents(entity);

            if (!all_components.HasValue()) {
                HYP_LOG(Editor, Error, "No component map found for Entity #{}", entity.GetID().Value());

                return nullptr;
            }

            RC<UIGrid> grid = parent->CreateUIObject<UIGrid>(Vec2i { 0, 0 }, UIObjectSize({ 100, UIObjectSize::PERCENT }, { 0, UIObjectSize::AUTO }));

            for (const auto &it : *all_components) {
                const TypeID component_type_id = it.first;

                const ComponentInterface *component_interface = ComponentInterfaceRegistry::GetInstance().GetComponentInterface(component_type_id);

                if (!component_interface) {
                    HYP_LOG(Editor, Error, "No ComponentInterface registered for component with TypeID {}", component_type_id.Value());

                    continue;
                }

                if (component_interface->GetClass() && !component_interface->GetClass()->GetAttribute("editor", true)) {
                    // Skip components that are not meant to be edited in the editor
                    continue;
                }

                UIElementFactoryBase *factory = GetEditorUIElementFactory(component_type_id);

                if (!factory) {
                    HYP_LOG(Editor, Error, "No editor UI component factory registered for component of type \"{}\"", component_interface->GetTypeName());

                    continue;
                }

                ComponentContainerBase *component_container = entity_manager->TryGetContainer(component_type_id);
                AssertThrow(component_container != nullptr);

                HypData component_hyp_data;

                if (!component_container->TryGetComponent(it.second, component_hyp_data)) {
                    HYP_LOG(Editor, Error, "Failed to get component of type \"{}\" with ID {} for Entity #{}", component_interface->GetTypeName(), it.second, entity.GetID().Value());

                    continue;
                }

                RC<UIGridRow> header_row = grid->AddRow();
                RC<UIGridColumn> header_column = header_row->AddColumn();

                RC<UIText> component_header = parent->CreateUIObject<UIText>(Vec2i { 0, 0 }, UIObjectSize(UIObjectSize::AUTO));

                Optional<String> component_header_text_opt;
                Optional<String> component_description_opt;

                if (component_interface->GetClass()) {
                    if (const HypClassAttributeValue &attr = component_interface->GetClass()->GetAttribute("label")) {
                        component_header_text_opt = attr.GetString();
                    }

                    if (const HypClassAttributeValue &attr = component_interface->GetClass()->GetAttribute("description")) {
                        component_description_opt = attr.GetString();
                    }
                }
            
                if (!component_header_text_opt.HasValue()) {
                    component_header_text_opt = component_interface->GetTypeName();
                }

                component_header->SetText(*component_header_text_opt);
                component_header->SetTextSize(12);
                header_column->AddChildUIObject(component_header);

                if (component_description_opt.HasValue()) {
                    RC<UIGridRow> description_row = grid->AddRow();
                    RC<UIGridColumn> description_column = description_row->AddColumn();

                    RC<UIText> component_description = parent->CreateUIObject<UIText>(Vec2i { 0, 0 }, UIObjectSize(UIObjectSize::AUTO));
                    component_description->SetTextSize(10);
                    component_description->SetText(*component_description_opt);

                    description_column->AddChildUIObject(component_description);
                }

                RC<UIGridRow> content_row = grid->AddRow();
                RC<UIGridColumn> content_column = content_row->AddColumn();

                RC<UIPanel> component_content = parent->CreateUIObject<UIPanel>(Vec2i { 0, 30 }, UIObjectSize({ 100, UIObjectSize::PERCENT }, { 0, UIObjectSize::AUTO }));

                RC<UIObject> element = factory->CreateUIObject(parent, component_hyp_data, { });
                AssertThrow(element != nullptr);

                component_content->AddChildUIObject(element);

                content_column->AddChildUIObject(component_content);
            }

            return grid;
        };

        RC<UIGrid> components_grid_container = parent->CreateUIObject<UIGrid>(Vec2i { 0, 0 }, UIObjectSize({ 100, UIObjectSize::PERCENT }, { 0, UIObjectSize::AUTO }));
        
        RC<UIGridRow> components_grid_container_header_row = components_grid_container->AddRow();
        RC<UIGridColumn> components_grid_container_header_column = components_grid_container_header_row->AddColumn();

        RC<UIText> components_grid_container_header_text = parent->CreateUIObject<UIText>(Vec2i { 0, 0 }, UIObjectSize({ 0, UIObjectSize::AUTO }, { 0, UIObjectSize::AUTO }));
        components_grid_container_header_text->SetText("Components");
        components_grid_container_header_column->AddChildUIObject(components_grid_container_header_text);

        RC<UIButton> add_component_button = parent->CreateUIObject<UIButton>(Vec2i { 0, 0 }, UIObjectSize(UIObjectSize::AUTO));
        add_component_button->SetText("Add Component");
        add_component_button->OnClick.Bind([stage_weak = parent->GetStage()->WeakRefCountedPtrFromThis()](...)
        {
            HYP_LOG(Editor, Debug, "Add Component clicked");

            if (RC<UIStage> stage = stage_weak.Lock().Cast<UIStage>()) {
                auto loaded_ui_asset = AssetManager::GetInstance()->Load<RC<UIObject>>("ui/dialog/Component.Add.ui.xml");
                
                if (loaded_ui_asset.IsOK()) {
                    auto loaded_ui = loaded_ui_asset.Result();

                    if (RC<UIObject> add_component_window = loaded_ui->FindChildUIObject("Add_Component_Window")) {
                        stage->AddChildUIObject(add_component_window);
                
                        return UIEventHandlerResult::STOP_BUBBLING;
                    }
                }

                HYP_LOG(Editor, Error, "Failed to load add component ui dialog! Error: {}", loaded_ui_asset.result.message);

                return UIEventHandlerResult::ERR;

                // RC<UIWindow> window = parent->CreateUIObject<UIWindow>(Vec2i { 0, 0 }, UIObjectSize(Vec2i { 250, 500 }));
                // window->SetOriginAlignment(UIObjectAlignment::CENTER);
                // window->SetParentAlignment(UIObjectAlignment::CENTER);
                // window->SetText("Add Component");

                // RC<UIGrid> window_content_grid = parent->CreateUIObject<UIGrid>(Vec2i { 0, 0 }, UIObjectSize(UIObjectSize::AUTO));
                // RC<UIGridRow> window_content_grid_row = window_content_grid->AddRow();
                // RC<UIGridColumn> window_content_grid_column = window_content_grid_row->AddColumn();

                // RC<UIGrid> window_footer_grid = parent->CreateUIObject<UIGrid>(Vec2i { 0, 0 }, UIObjectSize(UIObjectSize::AUTO));

                // RC<UIGridRow> window_footer_grid_row = window_footer_grid->AddRow();

                // RC<UIButton> add_button = parent->CreateUIObject<UIButton>(Vec2i { 0, 0 }, UIObjectSize(UIObjectSize::AUTO));
                // add_button->SetText("Add");
                // add_button->OnClick.Bind([window_weak = window.ToWeak()](...)
                // {
                //     // @TODO

                //     return UIEventHandlerResult::ERR;
                // }).Detach();
                // window_footer_grid_row->AddColumn()->AddChildUIObject(add_button);

                // RC<UIButton> cancel_button = parent->CreateUIObject<UIButton>(Vec2i { 0, 0 }, UIObjectSize(UIObjectSize::AUTO));
                // cancel_button->SetText("Cancel");
                // cancel_button->OnClick.Bind([window_weak = window.ToWeak()](...)
                // {
                //     if (RC<UIWindow> window = window_weak.Lock()) {
                //         if (window->RemoveFromParent()) {
                //             return UIEventHandlerResult::STOP_BUBBLING;
                //         }
                //     }

                //     return UIEventHandlerResult::ERR;
                // }).Detach();

                // window_footer_grid_row->AddColumn()->AddChildUIObject(cancel_button);

                // window_content_grid->AddRow()->AddColumn()->AddChildUIObject(window_footer_grid);

                // window->AddChildUIObject(window_content_grid);

                // window_content_grid_row->SetSize(UIObjectSize({ 100, UIObjectSize::PERCENT }, { 100, UIObjectSize::FILL }));

                // stage->AddChildUIObject(window);
            }

            return UIEventHandlerResult::ERR;
        }).Detach();

        components_grid_container_header_column->AddChildUIObject(add_component_button);

        RC<UIGridRow> components_grid_container_content_row = components_grid_container->AddRow();
        RC<UIGridColumn> components_grid_container_content_column = components_grid_container_content_row->AddColumn();

        if (entity_manager->GetOwnerThreadMask() & Threads::CurrentThreadID()) {
            components_grid_container_content_column->AddChildUIObject(CreateComponentsGrid());
        } else {
            HYP_NAMED_SCOPE("Awaiting async component UI element creation");

            Task<RC<UIObject>> task;

            entity_manager->PushCommand([&CreateComponentsGrid, executor = task.Initialize()](EntityManager &mgr, GameCounter::TickUnit delta)
            {
                executor->Fulfill(CreateComponentsGrid());
            });

            components_grid_container_content_column->AddChildUIObject(task.Await());
        }

        return components_grid_container;
    }

    void Update(UIObject *ui_object, const Handle<Entity> &entity) const
    {
        // @TODO

        HYP_NOT_IMPLEMENTED_VOID();
    }
};

HYP_DEFINE_UI_ELEMENT_FACTORY(Handle<Entity>, EntityUIElementFactory);

class EditorNodePropertyFactory : public UIElementFactory<EditorNodePropertyRef, EditorNodePropertyFactory>
{
public:
    RC<UIObject> Create(UIObject *parent, const EditorNodePropertyRef &value) const
    {
        RC<Node> node_rc = value.node.Lock();
        
        if (!node_rc) {
            HYP_LOG(Editor, Error, "Node reference is invalid, cannot create UI element for property \"{}\"", value.title);

            return nullptr;
        }

        UIElementFactoryBase *factory = GetEditorUIElementFactory(value.property->GetTypeID());

        if (!factory) {
            HYP_LOG(Editor, Error, "No factory registered for TypeID {} when creating UI element for property \"{}\"", value.property->GetTypeID().Value(), value.title);

            return nullptr;
        }

        // Create panel
        RC<UIPanel> panel = parent->CreateUIObject<UIPanel>(Vec2i { 0, 0 }, UIObjectSize({ 100, UIObjectSize::PERCENT }, { 0, UIObjectSize::AUTO }));

        {
            RC<UIGrid> grid = parent->CreateUIObject<UIGrid>(Vec2i { 0, 0 }, UIObjectSize({ 100, UIObjectSize::PERCENT }, { 0, UIObjectSize::AUTO }));

            RC<UIGridRow> header_row = grid->AddRow();
            RC<UIGridColumn> header_column = header_row->AddColumn();

            RC<UIText> component_header = parent->CreateUIObject<UIText>(Vec2i { 0, 0 }, UIObjectSize(UIObjectSize::AUTO));

            component_header->SetText(value.title);
            component_header->SetTextSize(12);
            header_column->AddChildUIObject(component_header);

            if (value.description.HasValue()) {
                RC<UIGridRow> description_row = grid->AddRow();
                RC<UIGridColumn> description_column = description_row->AddColumn();

                RC<UIText> component_description = parent->CreateUIObject<UIText>(Vec2i { 0, 0 }, UIObjectSize(UIObjectSize::AUTO));
                component_description->SetTextSize(10);
                component_description->SetText(*value.description);

                description_column->AddChildUIObject(component_description);
            }

            RC<UIGridRow> content_row = grid->AddRow();
            RC<UIGridColumn> content_column = content_row->AddColumn();

            panel->AddChildUIObject(grid);
        }

        {
            RC<UIPanel> content = parent->CreateUIObject<UIPanel>(NAME("PropertyPanel_Content"), Vec2i { 0, 25 }, UIObjectSize({ 100, UIObjectSize::PERCENT }, { 0, UIObjectSize::AUTO }));

            if (RC<UIObject> element = factory->CreateUIObject(parent, value.property->Get(HypData(node_rc)), AnyRef(const_cast<EditorNodePropertyRef &>(value)))) {
                content->AddChildUIObject(element);
            }

            panel->AddChildUIObject(content);
        }

        return panel;
    }

    void Update(UIObject *ui_object, const EditorNodePropertyRef &value) const
    {
        RC<Node> node_rc = value.node.Lock();
        AssertThrow(node_rc != nullptr);

        UIElementFactoryBase *factory = GetEditorUIElementFactory(value.property->GetTypeID());
        AssertThrow(factory != nullptr);

        RC<UIPanel> content = ui_object->FindChildUIObject(WeakName("PropertyPanel_Content")).Cast<UIPanel>();
        AssertThrow(content != nullptr);
        
        content->RemoveAllChildUIObjects();

        RC<UIObject> element = factory->CreateUIObject(ui_object, value.property->Get(HypData(node_rc)), AnyRef(const_cast<EditorNodePropertyRef &>(value)));
        AssertThrow(element != nullptr);

        content->AddChildUIObject(element);
    }
};

HYP_DEFINE_UI_ELEMENT_FACTORY(EditorNodePropertyRef, EditorNodePropertyFactory);

class AssetPackageUIElementFactory : public UIElementFactory<AssetPackage, AssetPackageUIElementFactory>
{
public:
    RC<UIObject> Create(UIObject *parent, const AssetPackage &value) const
    {
        RC<UIText> text = parent->CreateUIObject<UIText>();
        text->SetText(value.GetName().LookupString());

        parent->SetNodeTag(NodeTag(NAME("AssetPackage"), value.HandleFromThis()));

        return text;
    }

    void Update(UIObject *ui_object, const AssetPackage &value) const
    {
        ui_object->SetText(value.GetName().LookupString());
    }
};

HYP_DEFINE_UI_ELEMENT_FACTORY(AssetPackage, AssetPackageUIElementFactory);

class AssetObjectUIElementFactory : public UIElementFactory<AssetObject, AssetObjectUIElementFactory>
{
public:
    RC<UIObject> Create(UIObject *parent, const AssetObject &value) const
    {
        RC<UIText> text = parent->CreateUIObject<UIText>();
        text->SetText(value.GetName().LookupString());

        parent->SetNodeTag(NodeTag(NAME("AssetObject"), value.HandleFromThis()));

        return text;
    }

    void Update(UIObject *ui_object, const AssetObject &value) const
    {
        ui_object->SetText(value.GetName().LookupString());
    }
};

HYP_DEFINE_UI_ELEMENT_FACTORY(AssetObject, AssetObjectUIElementFactory);

} // namespace hyperion
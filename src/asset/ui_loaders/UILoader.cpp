/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <asset/ui_loaders/UILoader.hpp>

#include <ui/UIStage.hpp>
#include <ui/UIObject.hpp>
#include <ui/UIText.hpp>
#include <ui/UIButton.hpp>
#include <ui/UIPanel.hpp>
#include <ui/UITabView.hpp>
#include <ui/UIMenuBar.hpp>
#include <ui/UIGrid.hpp>
#include <ui/UIImage.hpp>
#include <ui/UIDockableContainer.hpp>

#include <util/xml/SAXParser.hpp>

#include <core/containers/Stack.hpp>
#include <core/containers/FlatMap.hpp>
#include <core/containers/String.hpp>
#include <core/Name.hpp>

#include <math/Vector2.hpp>

#include <algorithm>

namespace hyperion {

#define UI_OBJECT_CREATE_FUNCTION(name) \
    { \
        String(HYP_STR(name)).ToUpper(), \
        [](UIStage *stage, Name name, Vec2i position, UIObjectSize size) -> RC<UIObject> \
            { return stage->CreateUIObject<UI##name>(name, position, size, false); } \
    }

static const FlatMap<String, std::add_pointer_t<RC<UIObject>(UIStage *, Name, Vec2i, UIObjectSize)>> g_node_create_functions {
    UI_OBJECT_CREATE_FUNCTION(Button),
    UI_OBJECT_CREATE_FUNCTION(Text),
    UI_OBJECT_CREATE_FUNCTION(Panel),
    UI_OBJECT_CREATE_FUNCTION(Image),
    UI_OBJECT_CREATE_FUNCTION(TabView),
    UI_OBJECT_CREATE_FUNCTION(Tab),
    UI_OBJECT_CREATE_FUNCTION(Grid),
    UI_OBJECT_CREATE_FUNCTION(GridRow),
    UI_OBJECT_CREATE_FUNCTION(GridColumn),
    UI_OBJECT_CREATE_FUNCTION(MenuBar),
    UI_OBJECT_CREATE_FUNCTION(MenuItem),
    UI_OBJECT_CREATE_FUNCTION(DockableContainer),
    UI_OBJECT_CREATE_FUNCTION(DockableItem)
};

#undef UI_OBJECT_CREATE_FUNCTION

static const HashMap<String, UIObjectAlignment> g_ui_alignment_strings {
    { "TOPLEFT", UIObjectAlignment::TOP_LEFT },
    { "TOPRIGHT", UIObjectAlignment::TOP_RIGHT },
    { "CENTER", UIObjectAlignment::CENTER },
    { "BOTTOMLEFT", UIObjectAlignment::BOTTOM_LEFT },
    { "BOTTOMRIGHT", UIObjectAlignment::BOTTOM_RIGHT }
};

static UIObjectAlignment ParseUIObjectAlignment(const String &str)
{
    const String str_upper = str.ToUpper();

    const auto alignment_it = g_ui_alignment_strings.Find(str_upper);

    if (alignment_it != g_ui_alignment_strings.End()) {
        return alignment_it->second;
    }

    return UIObjectAlignment::TOP_LEFT;
}

static Vec2i ParseVec2i(const String &str)
{
    Vec2i result = Vec2i::Zero();

    Array<String> split = str.Split(' ');

    for (uint i = 0; i < split.Size() && i < result.size; i++) {
        split[i] = split[i].Trimmed();

        result[i] = StringUtil::Parse<int32>(split[i]);
    }

    return result;
}

static Optional<bool> ParseBool(const String &str)
{
    const String str_upper = str.ToUpper();

    if (str_upper == "TRUE") {
        return true;
    }

    if (str_upper == "FALSE") {
        return false;
    }

    return { };
}

static Optional<Pair<int32, UIObjectSize::Flags>> ParseUIObjectSizeElement(String str)
{
    str = str.Trimmed();
    str = str.ToUpper();

    if (str == "auto") {
        return Pair<int32, UIObjectSize::Flags> { 0, UIObjectSize::AUTO };
    }

    const SizeType percent_index = str.FindIndex("%");

    int32 parsed_int;

    if (percent_index != String::not_found) {
        String sub = str.Substr(0, percent_index);

        if (!StringUtil::Parse<int32>(sub, &parsed_int)) {
            return { };
        }

        return Pair<int32, UIObjectSize::Flags> { parsed_int, UIObjectSize::PERCENT };
    }

    if (!StringUtil::Parse<int32>(str, &parsed_int)) {
        return { };
    }

    return Pair<int32, UIObjectSize::Flags> { parsed_int, UIObjectSize::PIXEL };
}

static Optional<UIObjectSize> ParseUIObjectSize(const String &str)
{
    Array<String> split = str.Split(' ');

    if (split.Empty()) {
        return { };
    }

    if (split.Size() == 1) {
        Optional<Pair<int32, UIObjectSize::Flags>> parse_result = ParseUIObjectSizeElement(split[0]);

        if (!parse_result.HasValue()) {
            return { };
        }
        
        return UIObjectSize(*parse_result, *parse_result);
    }

    if (split.Size() == 2) {
        Optional<Pair<int32, UIObjectSize::Flags>> width_parse_result = ParseUIObjectSizeElement(split[0]);
        Optional<Pair<int32, UIObjectSize::Flags>> height_parse_result = ParseUIObjectSizeElement(split[1]);

        if (!width_parse_result.HasValue() || !height_parse_result.HasValue()) {
            return { };
        }
        
        return UIObjectSize(*width_parse_result, *height_parse_result);
    }

    return { };
}

class UISAXHandler : public xml::SAXHandler
{
public:
    UISAXHandler(LoaderState *state, UIStage *ui_stage)
        : m_ui_stage(ui_stage)
    {
        AssertThrow(ui_stage != nullptr);

        m_ui_object_stack.Push(ui_stage);
    }

    UIObject *LastObject()
    {
        AssertThrow(m_ui_object_stack.Any());

        return m_ui_object_stack.Top();
    }

    virtual void Begin(const String &name, const xml::AttributeMap &attributes) override
    {
        const String node_name_upper = name.ToUpper();

        const auto node_create_functions_it = g_node_create_functions.Find(node_name_upper);

        if (node_create_functions_it != g_node_create_functions.End()) {
            // Attributes needed for constructor of UIObject
            Name ui_object_name;

            if (attributes.Contains("name")) {
                ui_object_name = CreateNameFromDynamicString(ANSIString(attributes.At("name")));
            } else {
                ui_object_name = Name::Unique("UIObject");
            }

            Vec2i position = Vec2i::Zero();

            if (attributes.Contains("position")) {
                position = ParseVec2i(attributes.At("position"));
            }

            UIObjectSize size;

            if (attributes.Contains("size")) {
                if (Optional<UIObjectSize> parsed_size = ParseUIObjectSize(attributes.At("size")); parsed_size.HasValue()) {
                    size = *parsed_size;
                } else {
                    DebugLog(LogType::Warn, "UI object has invalid size property: %s\n", attributes.At("size").Data());
                }
            }

            RC<UIObject> ui_object = node_create_functions_it->second(m_ui_stage, ui_object_name, position, size);

            // Set properties based on attributes
            if (attributes.Contains("parentalignment")) {
                UIObjectAlignment alignment = ParseUIObjectAlignment(attributes.At("parentalignment"));

                ui_object->SetParentAlignment(alignment);
            }

            if (attributes.Contains("originalignment")) {
                UIObjectAlignment alignment = ParseUIObjectAlignment(attributes.At("originalignment"));

                ui_object->SetOriginAlignment(alignment);
            }

            if (attributes.Contains("visible")) {
                if (Optional<bool> parsed_bool = ParseBool(attributes.At("visible")); parsed_bool.HasValue()) {
                    ui_object->SetIsVisible(*parsed_bool);
                }
            }

            if (attributes.Contains("padding")) {
                ui_object->SetPadding(ParseVec2i(attributes.At("padding")));
            }

            LastObject()->AddChildUIObject(ui_object.Get());

            m_ui_object_stack.Push(ui_object);
        }
    }

    virtual void End(const String &name) override
    {
        const String node_name_upper = name.ToUpper();

        const auto node_create_functions_it = g_node_create_functions.Find(node_name_upper);

        if (node_create_functions_it != g_node_create_functions.End()) {
            UIObject *last_object = LastObject();

            // @TODO: Type check to ensure proper structure.

            // must always have one object in stack.
            if (m_ui_object_stack.Size() <= 1) {
                DebugLog(LogType::Warn, "Invalid UI object structure\n");

                return;
            }

            m_ui_object_stack.Pop();

            return;
        }
    }

    virtual void Characters(const String &value) override {}
    virtual void Comment(const String &comment) override {}

private:
    UIStage             *m_ui_stage;
    Stack<UIObject *>   m_ui_object_stack;
};

LoadedAsset UILoader::LoadAsset(LoaderState &state) const
{
    AssertThrow(state.asset_manager != nullptr);

    RC<UIObject> ui_stage(new UIStage(ThreadID::Current()));
    ui_stage->Init();

    UISAXHandler handler(&state, static_cast<UIStage *>(ui_stage.Get()));

    xml::SAXParser parser(&handler);
    auto sax_result = parser.Parse(&state.stream);

    if (!sax_result) {
        DebugLog(LogType::Warn, "Failed to parse UI stage: %s\n", sax_result.message.Data());

        return { { LoaderResult::Status::ERR, sax_result.message } };
    }

    return { { LoaderResult::Status::OK }, ui_stage };
}

} // namespace hyperion

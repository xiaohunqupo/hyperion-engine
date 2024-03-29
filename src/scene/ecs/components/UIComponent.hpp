#ifndef HYPERION_V2_ECS_UI_COMPONENT_HPP
#define HYPERION_V2_ECS_UI_COMPONENT_HPP

#include <core/lib/Proc.hpp>
#include <core/lib/Optional.hpp>
#include <core/lib/String.hpp>
#include <core/lib/RefCountedPtr.hpp>
#include <core/lib/Variant.hpp>

#include <core/Name.hpp>
#include <math/Vector2.hpp>

namespace hyperion::v2 {

class UIObject;

enum UIComponentType
{
    UI_COMPONENT_TYPE_NONE = 0,
    UI_COMPONENT_TYPE_BUTTON,
    UI_COMPONENT_TYPE_TEXT,
    UI_COMPONENT_TYPE_IMAGE,
    UI_COMPONENT_TYPE_MAX
};

struct UIComponentBounds
{
    Vec2i   position;
    Vec2i   size;

    bool ContainsPoint(Vec2i point) const
    {
        return point.x >= position.x && point.x <= position.x + size.x &&
               point.y >= position.y && point.y <= position.y + size.y;
    }
};

enum UIComponentEvent
{
    UI_COMPONENT_EVENT_NONE = 0,
    UI_COMPONENT_EVENT_MOUSE_DOWN,
    UI_COMPONENT_EVENT_MOUSE_UP,
    UI_COMPONENT_EVENT_MOUSE_DRAG,
    UI_COMPONENT_EVENT_MOUSE_HOVER,
    UI_COMPONENT_EVENT_CLICK,
    UI_COMPONENT_EVENT_MAX
};

struct UIComponentEventData
{
    UIComponentEvent    event = UI_COMPONENT_EVENT_NONE;
    Vec2f               mouse_position;
};

struct UIMouseEventData
{
    Vec2f   position;
    int     button = 0;
    bool    is_down = false;
};

struct UIComponent
{
    RC<UIObject>    ui_object;
};

template <UIComponentEvent>
struct UIEventHandlerComponent
{
    Proc<bool, const UIComponentEventData &> handler;
};

} // namespace hyperion::v2

#endif
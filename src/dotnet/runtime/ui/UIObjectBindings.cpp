#include <dotnet/core/RefCountedPtrBindings.hpp>

#include <ui/UIObject.hpp>

#include <core/lib/RefCountedPtr.hpp>

#include <math/Vector2.hpp>

#include <Engine.hpp>
#include <Types.hpp>

using namespace hyperion::v2;

extern "C" {

Vec2i UIObject_GetPosition(ManagedRefCountedPtr obj)
{
    RC<UIObject> ui_object = GetRefCountedPtrFromManaged<UIObject>(obj);

    if (!ui_object) {
        return { };
    }

    return ui_object->GetPosition();
}

void UIObject_SetPosition(ManagedRefCountedPtr obj, Vec2i position)
{
    RC<UIObject> ui_object = GetRefCountedPtrFromManaged<UIObject>(obj);

    if (!ui_object) {
        return;
    }

    ui_object->SetPosition(position);
}

Vec2i UIObject_GetSize(ManagedRefCountedPtr obj)
{
    RC<UIObject> ui_object = GetRefCountedPtrFromManaged<UIObject>(obj);

    if (!ui_object) {
        return { };
    }

    return ui_object->GetSize();
}

void UIObject_SetSize(ManagedRefCountedPtr obj, Vec2i size)
{
    RC<UIObject> ui_object = GetRefCountedPtrFromManaged<UIObject>(obj);

    if (!ui_object) {
        return;
    }

    ui_object->SetSize(size);
}

}
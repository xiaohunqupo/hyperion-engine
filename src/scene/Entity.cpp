#include <scene/Entity.hpp>

namespace hyperion::v2 {

Entity::Entity()
    : BasicObject()
{
}

void Entity::Init()
{
    if (IsInitCalled()) {
        return;
    }

    BasicObject::Init();

    SetReady(true);
}

bool Entity::IsReady() const
{
    return Base::IsReady();
}

} // namespace hyperion::v2

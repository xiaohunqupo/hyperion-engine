/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <scene/Entity.hpp>

#include <scene/ecs/EntityManager.hpp>

#include <core/logging/Logger.hpp>
#include <core/logging/LogChannels.hpp>

namespace hyperion {

HYP_DEFINE_LOG_SUBCHANNEL(Entity, Scene);

Entity::Entity() = default;

Entity::~Entity()
{
    const ID<Entity> id = GetID();

    if (!id.IsValid()) {
        return;
    }

    Task<bool> success = EntityManager::GetEntityToEntityManagerMap().PerformActionWithEntity(id, [](EntityManager *entity_manager, ID<Entity> id)
    {
        entity_manager->RemoveEntity(id);
    });

    if (!success.Await()) {
        HYP_LOG(Entity, LogLevel::ERR, "Failed to remove Entity with ID #{} from EntityManager", id.Value());
    }
}

} // namespace hyperion
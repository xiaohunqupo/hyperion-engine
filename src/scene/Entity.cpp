/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <scene/Entity.hpp>

#include <scene/ecs/EntityManager.hpp>

#include <core/logging/Logger.hpp>
#include <core/logging/LogChannels.hpp>

#include <util/profiling/ProfileScope.hpp>

namespace hyperion {

HYP_DEFINE_LOG_SUBCHANNEL(Entity, Scene);

Entity::Entity()
{
    HYP_LOG(Entity, LogLevel::DEBUG, "Creating Entity with ID #{}", GetID().Value());
}

Entity::~Entity()
{
    const ID<Entity> id = GetID();

    if (!id.IsValid()) {
        return;
    }

    Task<bool> success = EntityManager::GetEntityToEntityManagerMap().PerformActionWithEntity(id, [](EntityManager *entity_manager, ID<Entity> id)
    {
        HYP_NAMED_SCOPE("Remove Entity from EntityManager (task)");

        HYP_LOG(Entity, LogLevel::DEBUG, "Removing Entity #{} from EntityManager on thread '{}'", id.Value(), Threads::CurrentThreadID().name);

        entity_manager->RemoveEntity(id);
    });

    if (!success.Await()) {
        HYP_LOG(Entity, LogLevel::ERR, "Failed to remove Entity with ID #{} from EntityManager", id.Value());
    }
}

} // namespace hyperion
/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <scene/ecs/systems/VisibilityStateUpdaterSystem.hpp>
#include <scene/ecs/EntityManager.hpp>
#include <scene/ecs/components/LightComponent.hpp>

#include <scene/Scene.hpp>
#include <scene/Octree.hpp>

#include <core/logging/Logger.hpp>

#include <Engine.hpp>

namespace hyperion {

HYP_DEFINE_LOG_CHANNEL(Visibility);

void VisibilityStateUpdaterSystem::OnEntityAdded(EntityManager &entity_manager, ID<Entity> entity)
{
    SystemBase::OnEntityAdded(entity_manager, entity);

    VisibilityStateComponent &visibility_state_component = entity_manager.GetComponent<VisibilityStateComponent>(entity);

    if (visibility_state_component.octant_id != OctantID::Invalid()) {
        return;
    }

    visibility_state_component.visibility_state = nullptr;

    // This system must be ran before WorldAABBUpdaterSystem so that the bounding box is up to date

    BoundingBoxComponent &bounding_box_component = entity_manager.GetComponent<BoundingBoxComponent>(entity);

    if (bounding_box_component.world_aabb.IsValid()) {
        Octree &octree = entity_manager.GetScene()->GetOctree();

        const Octree::InsertResult insert_result = octree.Insert(entity, bounding_box_component.world_aabb);

        if (insert_result.first) {
            AssertThrowMsg(insert_result.second != OctantID::Invalid(), "Invalid octant ID returned from Insert()");

            visibility_state_component.octant_id = insert_result.second;

            if (Octree *octant = octree.GetChildOctant(visibility_state_component.octant_id)) {
                visibility_state_component.visibility_state = octant->GetVisibilityState().Get();
            }

            HYP_LOG(Visibility, LogLevel::DEBUG, "Inserted entity {} into octree, inserted at {}, {}", entity.Value(), visibility_state_component.octant_id.GetIndex(), visibility_state_component.octant_id.GetDepth());
        } else {
            HYP_LOG(Visibility, LogLevel::WARNING, "Failed to insert entity {} into octree: {}", entity.Value(), insert_result.first.message);
        }
    } else {
        HYP_LOG(Visibility, LogLevel::WARNING, "Entity {} has invalid bounding box, skipping octree insertion", entity.Value());
    }

    visibility_state_component.last_aabb_hash = bounding_box_component.world_aabb.GetHashCode();
}

void VisibilityStateUpdaterSystem::OnEntityRemoved(EntityManager &entity_manager, ID<Entity> entity)
{
    SystemBase::OnEntityRemoved(entity_manager, entity);

    VisibilityStateComponent &visibility_state_component = entity_manager.GetComponent<VisibilityStateComponent>(entity);
    visibility_state_component.visibility_state = nullptr;

    if (visibility_state_component.octant_id != OctantID::Invalid()) {
        Octree &octree = entity_manager.GetScene()->GetOctree();

        const Octree::Result remove_result = octree.Remove(entity);

        if (!remove_result) {
            HYP_LOG(Visibility, LogLevel::WARNING,  "Failed to remove entity {} from octree: {}", entity.Value(), remove_result.message);
        }

        visibility_state_component.octant_id = OctantID::Invalid();
    }
}

void VisibilityStateUpdaterSystem::Process(EntityManager &entity_manager, GameCounter::TickUnit delta)
{
    Octree &octree = entity_manager.GetScene()->GetOctree();

    for (auto [entity_id, visibility_state_component, bounding_box_component] : entity_manager.GetEntitySet<VisibilityStateComponent, BoundingBoxComponent>()) {
        bool needs_octree_update = false;

        const bool visibility_state_invalidated = visibility_state_component.flags & VISIBILITY_STATE_FLAG_INVALIDATED;

        const HashCode aabb_hash_code = bounding_box_component.world_aabb.GetHashCode();

        needs_octree_update |= (aabb_hash_code != visibility_state_component.last_aabb_hash);
        needs_octree_update |= visibility_state_invalidated;

        visibility_state_component.flags &= ~VISIBILITY_STATE_FLAG_INVALIDATED;

        // if entity is not in the octree, try to insert it
        if (visibility_state_component.octant_id == OctantID::Invalid()) {

            if (!bounding_box_component.world_aabb.IsValid()) {
                visibility_state_component.visibility_state = nullptr;

                continue;
            }

            const Octree::InsertResult insert_result = octree.Insert(entity_id, bounding_box_component.world_aabb);

            if (insert_result.first) {
                AssertThrowMsg(insert_result.second != OctantID::Invalid(), "Invalid octant ID returned from Insert()");

                visibility_state_component.octant_id = insert_result.second;
                visibility_state_component.last_aabb_hash = aabb_hash_code;

                if (Octree *octant = octree.GetChildOctant(visibility_state_component.octant_id)) {
                    visibility_state_component.visibility_state = octant->GetVisibilityState().Get();
                }

                HYP_LOG(Visibility, LogLevel::DEBUG, "Inserted entity {} into octree, inserted at {}, {}", entity_id.Value(), visibility_state_component.octant_id.GetIndex(), visibility_state_component.octant_id.GetDepth());
            }

            continue;
        }

        if (needs_octree_update) {
            // force entry invalidation if the bounding box is not finite,
            // so directional lights changing cause the octree to be updated
            const bool force_entry_invalidation = visibility_state_invalidated;

            const Octree::InsertResult update_result = octree.Update(entity_id, bounding_box_component.world_aabb, force_entry_invalidation);

            if (!update_result.first) {
                HYP_LOG(Visibility, LogLevel::WARNING, "Failed to update entity {} in octree: {}", entity_id.Value(), update_result.first.message);

                continue;
            }

            AssertThrowMsg(update_result.second != OctantID::Invalid(), "Invalid octant ID returned from Update()");

            visibility_state_component.octant_id = update_result.second;
            visibility_state_component.last_aabb_hash = aabb_hash_code;
        }

        if (visibility_state_component.octant_id != OctantID::Invalid()) {
            Octree *octant = octree.GetChildOctant(visibility_state_component.octant_id);

            if (octant) {
                visibility_state_component.visibility_state = octant->GetVisibilityState().Get();

                continue;
            }
        }

        visibility_state_component.visibility_state = nullptr;
    }
}

} // namespace hyperion

/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_OCTREE_HPP
#define HYPERION_OCTREE_HPP

#include <core/containers/Array.hpp>
#include <core/containers/FixedArray.hpp>
#include <core/containers/HashMap.hpp>
#include <core/utilities/Pair.hpp>
#include <scene/Entity.hpp>
#include <scene/VisibilityState.hpp>
#include <scene/ecs/EntityTag.hpp>

#include <math/Vector3.hpp>
#include <math/BoundingBox.hpp>
#include <math/Ray.hpp>

#include <Types.hpp>

// #define HYP_OCTREE_DEBUG 1

namespace hyperion {

class Entity;
class EntityManager;
class Camera;

/*! \brief Represents an octant in an octree
 *  \details The bits are ordered as follows:
 *  - 0-2: index of topmost parent octant
 *  - 3-5: index of second parent octant
 *  - 6-8: index of third parent octant
 * ... and so on until the index of the octant itself.
 * 
 * The maximum depth of an octree using this ID scheme is 64 bits / 3 bits for index = 21 octants.
*/
struct OctantID
{
    static constexpr uint64 invalid_bits = 1ull << 63;
    static constexpr SizeType max_depth = 64 / 3;

    uint64  index_bits { 0 };
    uint8   depth { 0 };

    OctantID() = default;

    explicit OctantID(uint64 index_bits, uint8 depth)
        : index_bits(index_bits), depth(depth) {}

    explicit OctantID(uint8 child_index, OctantID parent_id)
        : index_bits(!parent_id.IsInvalid()
            ? parent_id.index_bits | (uint64(child_index) << (uint64(parent_id.GetDepth() + uint8(1)) * 3ull))
            : child_index),
          depth(parent_id.GetDepth() + uint8(1)) {}

    OctantID(const OctantID &other)                 = default;
    OctantID &operator=(const OctantID &other)      = default;
    OctantID(OctantID &&other) noexcept             = default;
    OctantID &operator=(OctantID &&other) noexcept  = default;
    ~OctantID()                                     = default;
    
    HYP_NODISCARD HYP_FORCE_INLINE
    bool IsInvalid() const // This bit is reserved for invalid octants -- We use 3 bits for each index, leaving 1 bit left on a 64-bit integer
        { return index_bits & invalid_bits; }
    
    HYP_NODISCARD HYP_FORCE_INLINE
    bool operator==(const OctantID &other) const
        { return index_bits == other.index_bits && depth == other.depth; }
    
    HYP_NODISCARD HYP_FORCE_INLINE
    bool operator!=(const OctantID &other) const
        { return !(*this == other); }
    
    HYP_NODISCARD HYP_FORCE_INLINE
    uint8 GetIndex(uint8 depth) const
        { return (index_bits >> (uint64(depth) * 3ull)) & 0x7; }
    
    HYP_NODISCARD HYP_FORCE_INLINE
    uint8 GetIndex() const
        { return GetIndex(depth); }
    
    HYP_NODISCARD HYP_FORCE_INLINE
    uint8 GetDepth() const
        { return depth; }
    
    HYP_NODISCARD HYP_FORCE_INLINE
    bool IsSiblingOf(OctantID other) const
        { return depth == other.depth && (index_bits & ~(~0ull << (uint64(depth) * 3ull))) == (other.index_bits & ~(~0ull << (uint64(depth) * 3ull))); }
    
    HYP_NODISCARD HYP_FORCE_INLINE
    bool IsChildOf(OctantID other) const
        { return depth > other.depth && (index_bits & ~(~0ull << (uint64(other.depth) * 3ull))) == other.index_bits; }
    
    HYP_NODISCARD HYP_FORCE_INLINE
    bool IsParentOf(OctantID other) const
        { return depth < other.depth && index_bits == (other.index_bits & ~(~0ull << (uint64(depth) * 3ull))); }

    HYP_NODISCARD HYP_FORCE_INLINE
    OctantID GetParent() const
    {
        if (depth == 0) {
            return OctantID::Invalid();
        }

        return OctantID(index_bits & ~(~0ull << (uint64(depth) * 3ull)), depth - 1);
    }
    
    HYP_NODISCARD HYP_FORCE_INLINE
    HashCode GetHashCode() const
    {
        HashCode hc;
        hc.Add(index_bits);
        hc.Add(depth);

        return hc;
    }

    /*! \brief Get the special invalid OctantID. */
    HYP_API static OctantID Invalid();
};

class Octree;

struct OctreeState
{
    HashMap<ID<Entity>, Octree *>   node_to_octree;
    uint8                           visibility_cursor { 0u };

    // If any octants need to be rebuilt, their topmost parent that needs to be rebuilt will be stored here
    OctantID                        rebuild_state = OctantID::Invalid();

    /*! \brief Mark the octant as dirty, meaning it needs to be rebuilt */
    void MarkOctantDirty(OctantID octant_id);
};

class HYP_API Octree
{
    friend class Entity;

    enum
    {
        DEPTH_SEARCH_INF = -1,
        DEPTH_SEARCH_ONLY_THIS = 0
    };

    static constexpr float growth_factor = 1.5f;
    // the length value at which to stop recusively dividing
    // for a small enough object
    static constexpr float min_aabb_size = 1.0f; 
    static const BoundingBox default_bounds;

    Octree(RC<EntityManager> entity_manager, const BoundingBox &aabb, Octree *parent, uint8 index);

public:
    struct Result
    {
        enum
        {
            OCTREE_OK  = 0,
            OCTREE_ERR = 1
        } result;

        const char  *message;
        int         error_code = 0;

        Result()
            : Result(OCTREE_OK) {}
        
        Result(decltype(result) result, const char *message = "", int error_code = 0)
            : result(result), message(message), error_code(error_code) {}
        Result(const Result &other)
            : result(other.result), message(other.message), error_code(other.error_code) {}

        HYP_FORCE_INLINE
        operator bool() const { return result == OCTREE_OK; }
    };

    using InsertResult = Pair<Result, OctantID>;

    struct Octant
    {
        UniquePtr<Octree>   octree;
        BoundingBox         aabb;
    };

    struct Node
    {
        ID<Entity>  id;
        BoundingBox aabb;

        Node() = default;

        Node(ID<Entity> id, const BoundingBox &aabb)
            : id(id),
              aabb(aabb)
        {
        }

        Node(const Node &other)
            : id(other.id),
              aabb(other.aabb)
        {
        }

        Node &operator=(const Node &other)
        {
            id = other.id;
            aabb = other.aabb;

            return *this;
        }

        Node(Node &&other) noexcept
            : id(other.id),
              aabb(other.aabb)
        {
            other.id = ID<Entity>::invalid;
            other.aabb = BoundingBox::Empty();
        }

        Node &operator=(Node &&other) noexcept
        {
            id = other.id;
            aabb = other.aabb;

            other.id = ID<Entity>::invalid;
            other.aabb = BoundingBox::Empty();

            return *this;
        }

        ~Node() = default;

        HYP_FORCE_INLINE
        bool operator==(const Node &other) const
        {
            return id == other.id
                && aabb == other.aabb;
        }

        HYP_FORCE_INLINE
        bool operator!=(const Node &other) const
            { return !(*this == other); }

        HashCode GetHashCode() const
        {
            HashCode hc;

            hc.Add(id.GetHashCode());
            hc.Add(aabb.GetHashCode());

            return hc;
        }
    };

    Octree(RC<EntityManager> entity_manager);
    Octree(RC<EntityManager> entity_manager, const BoundingBox &aabb);
    Octree(const Octree &other)                 = delete;
    Octree &operator=(const Octree &other)      = delete;
    Octree(Octree &&other) noexcept             = delete;
    Octree &operator=(Octree &&other) noexcept  = delete;
    ~Octree();

    const RC<VisibilityState> &GetVisibilityState() const
        { return m_visibility_state; }

    /*! \brief Get the EntityManager the Octree is using to manage entities.
     *  \returns The EntityManager the Octree is set to use */
    const RC<EntityManager> &GetEntityManager() const
        { return m_entity_manager; }

    /*! \brief Set the EntityManager for the Octree to use. For internal use from \ref{Scene} only
     *  \internal */
    void SetEntityManager(RC<EntityManager> entity_manager);

    BoundingBox &GetAABB()
        { return m_aabb; }

    const BoundingBox &GetAABB() const
        { return m_aabb; }

    const Array<Node> &GetNodes() const
        { return m_nodes; }

    OctantID GetOctantID() const
        { return m_octant_id; }

    const FixedArray<Octant, 8> &GetOctants() const
        { return m_octants; }

    /*! \brief Get the child (nested) octant with the specified index
     *  \param octant_id The OctantID to use to find the octant (see OctantID struct)
     *  \return The octant with the specified index, or nullptr if it doesn't exist
    */
    Octree *GetChildOctant(OctantID octant_id);

    bool IsDivided() const
        { return m_is_divided; }

    /*! \brief Get a hashcode of all entities currently in this Octant that have the given tags (child octants affect this too)
    */
    template <EntityTag... tags>
    HashCode GetEntryListHash() const
    {
        const uint32 mask = ((tags == EntityTag::NONE ? 0 : (1u << (uint32(tags) - 1))) | ...);

        return HashCode(m_entry_hashes[mask])
            .Add(m_invalidation_marker);
    }

    /*! \brief Get a hashcode of all entities currently in this Octant that match the mask tag (child octants affect this too)
    */
    HashCode GetEntryListHash(uint32 entity_tag_mask) const
    {
        AssertThrow(entity_tag_mask < m_entry_hashes.Size());

        return HashCode(m_entry_hashes[entity_tag_mask])
            .Add(m_invalidation_marker);
    }
        
    void Clear();
    InsertResult Insert(ID<Entity> id, const BoundingBox &aabb, bool allow_rebuild = false);
    Result Remove(ID<Entity> id, bool allow_rebuild = false);
    
    /*! \brief Update the entry in the octree. 
     * \param id The ID of the entity to update
     * \param aabb The new AABB of the entity
     * \param allow_rebuild If true, the octree will be rebuilt if the entity doesn't fit in the new octant. Otherwise, the octree will be marked as dirty and rebuilt on the next call to PerformUpdates()
     * \param force_invalidation If true, the entry will have its invalidation marker incremented, causing the octant's hash to be updated
     */
    InsertResult Update(ID<Entity> id, const BoundingBox &aabb, bool force_invalidation = false, bool allow_rebuild = false);
    InsertResult Rebuild();
    InsertResult Rebuild(const BoundingBox &new_aabb);
    
    void CollectEntities(Array<ID<Entity>> &out) const;
    void CollectEntitiesInRange(const Vector3 &position, float radius, Array<ID<Entity>> &out) const;
    bool GetNearestOctants(const Vector3 &position, FixedArray<Octree *, 8> &out) const;
    bool GetNearestOctant(const Vector3 &position, Octree const *&out) const;
    bool GetFittingOctant(const BoundingBox &aabb, Octree const *&out) const;

    void NextVisibilityState();
    void CalculateVisibility(const Handle<Camera> &camera);

    void PerformUpdates();

    OctreeState *GetState() const
        { return m_state; }

    bool TestRay(const Ray &ray, RayTestResults &out_results) const;

private:
    void ResetNodesHash();
    void RebuildNodesHash(uint level = 0);

    void ClearInternal(Array<Node> &out_nodes);
    void Clear(Array<Node> &out_nodes);

    /*! \brief Move the entity to a new octant. If allow_rebuild is true, the octree will be rebuilt if the entity doesn't fit in the new octant,
        and subdivided octants will be collapsed if they are empty + new octants will be created if they are needed.
     */
    InsertResult Move(ID<Entity> id, const BoundingBox &aabb, bool allow_rebuild, const Array<Node>::Iterator *it = nullptr);

    auto FindNode(ID<Entity> id)
    {
        return m_nodes.FindIf([id](const Node &item)
        {
            return item.id == id;
        });
    }

    bool IsRoot() const { return m_parent == nullptr; }
    bool Empty() const { return m_nodes.Empty(); }
    
    void SetParent(Octree *parent);
    bool EmptyDeep(int depth = DEPTH_SEARCH_INF, uint8 octant_mask = 0xff) const;

    void InitOctants();
    void Divide();
    void Undivide();

    void Invalidate();

    /*! \brief If \ref{allow_rebuild} is true, removes any potentially empty octants above the node.
        If \ref{allow_rebuild} is false, marks them as dirty so they get removed on the next call to PerformUpdates()
    */
    void CollapseParents(bool allow_rebuild);
    InsertResult InsertInternal(ID<Entity> id, const BoundingBox &aabb);
    InsertResult UpdateInternal(ID<Entity> id, const BoundingBox &aabb, bool force_invalidation, bool allow_rebuild);
    Result RemoveInternal(ID<Entity> id, bool allow_rebuild);
    InsertResult RebuildExtendInternal(const BoundingBox &extend_include_aabb);
    void UpdateVisibilityState(const Handle<Camera> &camera, uint16 validity_marker);

    /* Called from entity - remove the pointer */
    // void OnEntityRemoved(Entity *entity);

    RC<EntityManager>                                   m_entity_manager;
    
    Array<Node>                                         m_nodes;
    FixedArray<HashCode, 1u << uint(EntityTag::MAX)>    m_entry_hashes;
    Octree                                              *m_parent;
    BoundingBox                                         m_aabb;
    FixedArray<Octant, 8>                               m_octants;
    bool                                                m_is_divided;
    OctreeState                                         *m_state;
    RC<VisibilityState>                                 m_visibility_state;
    OctantID                                            m_octant_id;
    uint                                                m_invalidation_marker;
};

} // namespace hyperion

#endif

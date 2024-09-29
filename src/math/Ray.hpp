/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_RAY_HPP
#define HYPERION_RAY_HPP

#include <math/Vector3.hpp>
#include <math/Vertex.hpp>
#include <math/Transform.hpp>

#include <core/containers/FlatSet.hpp>
#include <core/utilities/Optional.hpp>
#include <core/utilities/Tuple.hpp>

#include <HashCode.hpp>
#include <Types.hpp>

namespace hyperion {

class BoundingBox;
struct Triangle;
class RayTestResults;
struct RayHit;

using RayHitID = uint;

HYP_STRUCT()
struct HYP_API Ray
{
    HYP_FIELD(SerializeAs=Position)
    Vec3f   position;

    HYP_FIELD(SerializeAs=Direction)
    Vec3f   direction;

    HYP_FORCE_INLINE bool operator==(const Ray &other) const
    {
        return position == other.position
            && direction == other.direction;
    }

    HYP_FORCE_INLINE bool operator!=(const Ray &other) const
    {
        return position != other.position
            || direction != other.direction;
    }

    bool TestAABB(const BoundingBox &aabb) const;
    bool TestAABB(const BoundingBox &aabb, RayTestResults &out_results) const;
    bool TestAABB(const BoundingBox &aabb, RayHitID hit_id, RayTestResults &out_results) const;
    bool TestAABB(const BoundingBox &aabb, RayHitID hit_id, const void *user_data, RayTestResults &out_results) const;

    bool TestTriangle(const Triangle &triangle) const;
    bool TestTriangle(const Triangle &triangle, RayTestResults &out_results) const;
    bool TestTriangle(const Triangle &triangle, RayHitID hit_id, RayTestResults &out_results) const;
    bool TestTriangle(const Triangle &triangle, RayHitID hit_id, const void *user_data, RayTestResults &out_results) const;
    
    Optional<RayHit> TestTriangleList(
        const Array<Vertex> &vertices,
        const Array<uint32> &indices,
        const Transform &transform
    ) const;

    Optional<RayHit> TestTriangleList(
        const Array<Triangle> &triangles,
        const Transform &transform
    ) const;

    bool TestTriangleList(
        const Array<Vertex> &vertices,
        const Array<uint32> &indices,
        const Transform &transform,
        RayTestResults &out_results
    ) const;

    bool TestTriangleList(
        const Array<Triangle> &triangles,
        const Transform &transform,
        RayTestResults &out_results
    ) const;

    bool TestTriangleList(
        const Array<Vertex> &vertices,
        const Array<uint32> &indices,
        const Transform &transform,
        RayHitID hit_id,
        RayTestResults &out_results
    ) const;

    bool TestTriangleList(
        const Array<Triangle> &triangles,
        const Transform &transform,
        RayHitID hit_id,
        RayTestResults &out_results
    ) const;

    bool TestTriangleList(
        const Array<Vertex> &vertices,
        const Array<uint32> &indices,
        const Transform &transform,
        RayHitID hit_id,
        const void *user_data,
        RayTestResults &out_results
    ) const;

    bool TestTriangleList(
        const Array<Triangle> &triangles,
        const Transform &transform,
        RayHitID hit_id,
        const void *user_data,
        RayTestResults &out_results
    ) const;

    HYP_FORCE_INLINE HashCode GetHashCode() const
    {
        HashCode hc;
        hc.Add(position.GetHashCode());
        hc.Add(direction.GetHashCode());

        return hc;
    }
};

static_assert(sizeof(Ray) == 32, "sizeof(Ray) must be 32 bytes to match C# struct size");

struct RayHit
{
    static constexpr bool no_hit = false;
    
    Vec3f       hitpoint;
    Vec3f       normal;
    Vec3f       barycentric_coords;
    float       distance = 0.0f;
    RayHitID    id = ~0u;
    const void *user_data = nullptr;

    bool operator<(const RayHit &other) const
    {
        return Tie(
            distance,
            hitpoint,
            normal,
            barycentric_coords,
            id,
            user_data
        ) < Tie(
            other.distance,
            other.hitpoint,
            other.normal,
            other.barycentric_coords,
            other.id,
            other.user_data
        );
    }

    bool operator==(const RayHit &other) const
    {
        return distance == other.distance
            && hitpoint == other.hitpoint
            && normal == other.normal
            && barycentric_coords == other.barycentric_coords
            && id == other.id
            && user_data == other.user_data;
    }

    HashCode GetHashCode() const
    {
        HashCode hc;

        hc.Add(distance);
        hc.Add(hitpoint.GetHashCode());
        hc.Add(normal.GetHashCode());
        hc.Add(barycentric_coords.GetHashCode());
        hc.Add(id);
        hc.Add(uintptr_t(user_data));

        return hc;
    }
};

class RayTestResults : public FlatSet<RayHit>
{
public:
    bool AddHit(const RayHit &hit)
        { return Insert(hit).second; }
};

} // namespace hyperion

#endif

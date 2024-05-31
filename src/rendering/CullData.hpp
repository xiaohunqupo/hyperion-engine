/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_CULL_DATA_HPP
#define HYPERION_CULL_DATA_HPP

#include <math/Extent.hpp>

#include <rendering/backend/RenderObject.hpp>

namespace hyperion {

struct CullData
{
    ImageViewRef    depth_pyramid_image_view;
    Extent3D        depth_pyramid_dimensions;

    CullData()
        : depth_pyramid_dimensions { 1, 1, 1 }
    {
    }

    bool operator==(const CullData &other) const
    {
        return depth_pyramid_image_view == other.depth_pyramid_image_view
            && depth_pyramid_dimensions == other.depth_pyramid_dimensions;
    }

    bool operator!=(const CullData &other) const
    {
        return !operator==(other);
    }
};

} // namespace hyperion

#endif
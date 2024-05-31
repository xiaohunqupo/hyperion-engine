/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_DEPTH_PYRAMID_RENDERER_HPP
#define HYPERION_DEPTH_PYRAMID_RENDERER_HPP

#include <core/containers/Array.hpp>

#include <rendering/backend/RenderObject.hpp>

#include <math/Extent.hpp>

namespace hyperion {

class DepthPyramidRenderer
{
public:
    DepthPyramidRenderer();
    ~DepthPyramidRenderer();

    const ImageViewRef &GetResultImageView() const
        { return m_depth_pyramid_view; }

    const Array<ImageViewRef> &GetMipImageVIews() const
        { return m_depth_pyramid_mips; }

    bool IsRendered() const
        { return m_is_rendered; }

    Extent3D GetExtent() const;

    void Create(const AttachmentRef &depth_attachment);

    void Render(Frame *frame);

private:
    AttachmentRef               m_depth_attachment;

    ImageRef                    m_depth_pyramid;
    ImageViewRef                m_depth_pyramid_view;
    Array<ImageViewRef>         m_depth_pyramid_mips;
    Array<DescriptorTableRef>   m_mip_descriptor_tables;
    SamplerRef                  m_depth_pyramid_sampler;

    ComputePipelineRef          m_generate_depth_pyramid;

    bool                        m_is_rendered;
};

} // namespace hyperion

#endif
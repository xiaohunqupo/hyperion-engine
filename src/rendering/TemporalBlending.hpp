/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_TEMPORAL_BLENDING_HPP
#define HYPERION_TEMPORAL_BLENDING_HPP

#include <Constants.hpp>

#include <core/containers/FixedArray.hpp>

#include <core/functional/Delegate.hpp>

#include <rendering/Shader.hpp>

#include <rendering/backend/RenderObject.hpp>

namespace hyperion {

enum class TemporalBlendTechnique
{
    TECHNIQUE_0,
    TECHNIQUE_1,
    TECHNIQUE_2,
    TECHNIQUE_3,

    TECHNIQUE_4 // Progressive blending for path tracing
};

enum class TemporalBlendFeedback
{
    LOW,
    MEDIUM,
    HIGH
};

class TemporalBlending
{
public:
    friend struct RenderCommand_RecreateTemporalBlendingFramebuffer;

    TemporalBlending(
        const Vec2u &extent,
        TemporalBlendTechnique technique,
        TemporalBlendFeedback feedback,
        const FixedArray<ImageViewRef, max_frames_in_flight> &input_image_views
    );

    TemporalBlending(
        const Vec2u &extent,
        InternalFormat image_format,
        TemporalBlendTechnique technique,
        TemporalBlendFeedback feedback,
        const FramebufferRef &input_framebuffer
    );

    TemporalBlending(
        const Vec2u &extent,
        InternalFormat image_format,
        TemporalBlendTechnique technique,
        TemporalBlendFeedback feedback,
        const FixedArray<ImageViewRef, max_frames_in_flight> &input_image_views
    );

    TemporalBlending(const TemporalBlending &other) = delete;
    TemporalBlending &operator=(const TemporalBlending &other) = delete;
    ~TemporalBlending();

    HYP_FORCE_INLINE TemporalBlendTechnique GetTechnique() const
        { return m_technique; }

    HYP_FORCE_INLINE TemporalBlendFeedback GetFeedback() const
        { return m_feedback; }

    HYP_FORCE_INLINE const Handle<Texture> &GetResultTexture() const
        { return m_result_texture; }

    HYP_FORCE_INLINE const Handle<Texture> &GetHistoryTexture() const
        { return m_history_texture; }

    void ResetProgressiveBlending();

    void Create();
    void Render(Frame *frame);

    void Resize(Vec2u new_size);

private:
    void Resize_Internal(Vec2u new_size);

    ShaderProperties GetShaderProperties() const;

    void CreateImageOutputs();
    void CreateDescriptorSets();
    void CreateComputePipelines();

    Vec2u                                           m_extent;
    InternalFormat                                  m_image_format;
    TemporalBlendTechnique                          m_technique;
    TemporalBlendFeedback                           m_feedback;

    uint16                                          m_blending_frame_counter;

    ComputePipelineRef                              m_perform_blending;
    DescriptorTableRef                              m_descriptor_table;

    FixedArray<ImageViewRef, max_frames_in_flight>  m_input_image_views;
    FramebufferRef                                  m_input_framebuffer;

    Handle<Texture>                                 m_result_texture;
    Handle<Texture>                                 m_history_texture;

    DelegateHandler                                 m_after_swapchain_recreated_delegate;

    bool                                            m_is_initialized;
};

} // namespace hyperion

#endif
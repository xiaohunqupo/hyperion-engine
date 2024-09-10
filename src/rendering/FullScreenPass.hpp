/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_FULL_SCREEN_PASS_HPP
#define HYPERION_FULL_SCREEN_PASS_HPP

#include <rendering/Shader.hpp>
#include <rendering/Mesh.hpp>

#include <rendering/backend/RenderObject.hpp>

#include <core/functional/Delegate.hpp>

#include <Constants.hpp>

#include <Types.hpp>

namespace hyperion {

using renderer::PushConstantData;

class Engine;

class HYP_API FullScreenPass
{
public:
    friend struct RenderCommand_RecreateFullScreenPassFramebuffer;

    FullScreenPass();

    FullScreenPass(
        InternalFormat image_format,
        Vec2u extent = Vec2u { 0, 0 }
    );

    FullScreenPass(
        const ShaderRef &shader,
        InternalFormat image_format = InternalFormat::RGB8_SRGB,
        Vec2u extent = Vec2u { 0, 0 }
    );

    FullScreenPass(
        const ShaderRef &shader,
        const DescriptorTableRef &descriptor_table,
        InternalFormat image_format = InternalFormat::RGB8_SRGB,
        Vec2u extent = Vec2u { 0, 0 }
    );

    FullScreenPass(const FullScreenPass &)              = delete;
    FullScreenPass &operator=(const FullScreenPass &)   = delete;
    virtual ~FullScreenPass();

    HYP_FORCE_INLINE const Vec2u &GetExtent() const
        { return m_extent; }

    HYP_FORCE_INLINE InternalFormat GetFormat() const
        { return m_image_format; }

    const AttachmentRef &GetAttachment(uint attachment_index) const;

    HYP_FORCE_INLINE const CommandBufferRef &GetCommandBuffer(uint index) const
        { return m_command_buffers[index]; }

    HYP_FORCE_INLINE const FramebufferRef &GetFramebuffer() const
        { return m_framebuffer; }

    HYP_FORCE_INLINE const ShaderRef &GetShader() const
        { return m_shader; }

    void SetShader(const ShaderRef &shader);

    HYP_FORCE_INLINE const Handle<Mesh> &GetQuadMesh() const
        { return m_full_screen_quad; }

    HYP_FORCE_INLINE const Handle<RenderGroup> &GetRenderGroup() const
        { return m_render_group; }

    HYP_FORCE_INLINE void SetPushConstants(const PushConstantData &pc)
        { m_push_constant_data = pc; }

    HYP_FORCE_INLINE void SetPushConstants(const void *ptr, SizeType size)
        { SetPushConstants(PushConstantData(ptr, size)); }

    HYP_FORCE_INLINE const BlendFunction &GetBlendFunction() const
        { return m_blend_function; }

    /*! \brief Sets the blend function of the render pass.
        Must be set before Create() is called. */
    void SetBlendFunction(const BlendFunction &blend_function);

    HYP_FORCE_INLINE const Optional<DescriptorTableRef> &GetDescriptorTable() const
        { return m_descriptor_table; }

    /*! \brief Resizes the full screen pass to the new size.
     *  Callable on any thread, as it enqueues a render command. */
    void Resize(Vec2u new_size);

    virtual void CreateCommandBuffers();
    virtual void CreateFramebuffer();
    virtual void CreatePipeline(const RenderableAttributeSet &renderable_attributes);
    virtual void CreatePipeline();
    virtual void CreateDescriptors();

    /*! \brief Create the full screen pass */
    virtual void Create();

    virtual void Render(Frame *frame);
    virtual void Record(uint frame_index);

    void Begin(Frame *frame);
    void End(Frame *frame);

protected:
    void CreateQuad();

    virtual void Resize_Internal(Vec2u new_size);

    FixedArray<CommandBufferRef, max_frames_in_flight>  m_command_buffers;
    FramebufferRef                                      m_framebuffer;
    ShaderRef                                           m_shader;
    Handle<RenderGroup>                                 m_render_group;
    Handle<Mesh>                                        m_full_screen_quad;
    Vec2u                                               m_extent;

    PushConstantData                                    m_push_constant_data;

    InternalFormat                                      m_image_format;

    BlendFunction                                       m_blend_function;

    Optional<DescriptorTableRef>                        m_descriptor_table;

private:
    bool                                                m_is_initialized;
};
} // namespace hyperion

#endif

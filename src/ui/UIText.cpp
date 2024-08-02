/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <ui/UIText.hpp>
#include <ui/UIStage.hpp>

#include <rendering/ShaderGlobals.hpp>

#include <rendering/backend/RenderCommand.hpp>
#include <rendering/backend/RenderConfig.hpp>
#include <rendering/backend/RendererFrameHandler.hpp>
#include <rendering/backend/RendererFrame.hpp>
#include <rendering/backend/RendererGraphicsPipeline.hpp>

#include <scene/camera/OrthoCamera.hpp>

#include <scene/ecs/components/MeshComponent.hpp>
#include <scene/ecs/components/BoundingBoxComponent.hpp>

#include <math/Vector3.hpp>
#include <math/Quaternion.hpp>

#include <core/logging/Logger.hpp>

#include <util/MeshBuilder.hpp>

#include <util/profiling/ProfileScope.hpp>

#include <Engine.hpp>

namespace hyperion {

HYP_DECLARE_LOG_CHANNEL(UI);

struct UICharMesh
{
    RC<StreamedMeshData>    mesh_data;
    BoundingBox             aabb;
    Transform               transform;
};

class CharMeshBuilder
{
public:
    CharMeshBuilder(const UITextOptions &options)
        : m_options(options)
    {
        m_quad_mesh = MeshBuilder::Quad();
        InitObject(m_quad_mesh);
    }

    Array<UICharMesh> BuildCharMeshes(const FontAtlas &font_atlas, const String &text) const;
    Handle<Mesh> OptimizeCharMeshes(Vec2i screen_size, Array<UICharMesh> &&char_meshes) const;

private:
    Handle<Mesh>    m_quad_mesh;

    UITextOptions   m_options;
};

Array<UICharMesh> CharMeshBuilder::BuildCharMeshes(const FontAtlas &font_atlas, const String &text) const
{
    Array<UICharMesh> char_meshes;
    
    Vec2f placement;

    const SizeType length = text.Length();
    
    const Vec2f cell_dimensions = Vec2f(font_atlas.GetCellDimensions()) / 64.0f;
    AssertThrowMsg(cell_dimensions.x * cell_dimensions.y != 0.0f, "Cell dimensions are invalid");

    const float cell_dimensions_ratio = cell_dimensions.x / cell_dimensions.y;

    AssertThrowMsg(font_atlas.GetAtlases() != nullptr, "Font atlas invalid");

    const Handle<Texture> &main_texture_atlas = font_atlas.GetAtlases()->GetMainAtlas();
    AssertThrowMsg(main_texture_atlas.IsValid(), "Main texture atlas is invalid");

    const Vec2f atlas_pixel_size = Vec2f::One() / Vec2f(Extent2D(main_texture_atlas->GetExtent()));

    for (SizeType i = 0; i < length; i++) {
        const utf::u32char ch = text.GetChar(i);

        if (ch == utf::u32char(' ')) {
            // add room for space
            placement.x += cell_dimensions.x * 0.5f;

            continue;
        }

        if (ch == utf::u32char('\n')) {
            // reset placement, add room for newline
            placement.x = 0.0f;
            placement.y += cell_dimensions.y;

            continue;
        }

        Optional<Glyph::Metrics> glyph_metrics = font_atlas.GetGlyphMetrics(ch);

        if (!glyph_metrics.HasValue()) {
            // @TODO Add a placeholder character for missing glyphs
            continue;
        }

        if (glyph_metrics->metrics.width == 0 || glyph_metrics->metrics.height == 0) {
            // empty width or height will cause issues
            continue;
        }

        const Vec2i char_offset = glyph_metrics->image_position;

        const Vec2f glyph_dimensions = Vec2f { float(glyph_metrics->metrics.width), float(glyph_metrics->metrics.height) } / 64.0f;
        const Vec2f glyph_scaling = Vec2f(glyph_dimensions) / (Vec2f(cell_dimensions));

        UICharMesh char_mesh;
        char_mesh.mesh_data = m_quad_mesh->GetStreamedMeshData();

        auto ref = char_mesh.mesh_data->AcquireRef();

        Array<Vertex> vertices = ref->GetMeshData().vertices;
        const Array<uint32> &indices = ref->GetMeshData().indices;

        const float bearing_y = float(glyph_metrics->metrics.height - glyph_metrics->metrics.bearing_y) / 64.0f;
        const float char_width = float(glyph_metrics->metrics.advance / 64) / 64.0f;

        for (Vertex &vert : vertices) {
            const Vec3f current_position = vert.GetPosition();

            Vec2f position = (Vec2f { current_position.x, current_position.y } + Vec2f { 1.0f, 1.0f }) * 0.5f;
            position *= glyph_dimensions;
            position.y += cell_dimensions.y - glyph_dimensions.y;
            position.y += bearing_y;
            position += placement;

            vert.SetPosition(Vec3f { position.x, position.y, current_position.z });
            vert.SetTexCoord0((Vec2f(1.0f / float(length) * float(i)) + vert.GetTexCoord0() * Vec2f(1.0f / float(length))));
            // vert.SetTexCoord0((Vec2f(char_offset) + (vert.GetTexCoord0() * glyph_dimensions * 64.0f)) * atlas_pixel_size);
        }

        char_mesh.aabb.Extend(Vec3f(placement.x, placement.y, 0.0f));
        char_mesh.aabb.Extend(Vec3f(placement.x + glyph_dimensions.x, placement.y + cell_dimensions.y, 0.0f));

        char_mesh.mesh_data = StreamedMeshData::FromMeshData({
            std::move(vertices),
            indices
        });

        placement.x += char_width;

        char_meshes.PushBack(std::move(char_mesh));
    }

    return char_meshes;
}

Handle<Mesh> CharMeshBuilder::OptimizeCharMeshes(Vec2i screen_size, Array<UICharMesh> &&char_meshes) const
{
    if (char_meshes.Empty()) {
        return { };
    }

    Transform base_transform;

    BoundingBox aabb = char_meshes[0].aabb;
    HYP_LOG(UI, LogLevel::INFO, "Char mesh AABB: {}", aabb);

    Handle<Mesh> char_mesh = CreateObject<Mesh>(char_meshes[0].mesh_data);
    Handle<Mesh> transformed_mesh = MeshBuilder::ApplyTransform(char_mesh.Get(), base_transform * char_meshes[0].transform);

    for (SizeType i = 1; i < char_meshes.Size(); i++) {
        aabb.Extend(char_meshes[i].aabb);

        char_mesh = CreateObject<Mesh>(char_meshes[i].mesh_data);

        transformed_mesh = MeshBuilder::Merge(
            transformed_mesh.Get(),
            char_mesh.Get(),
            Transform::identity,
            base_transform * char_meshes[i].transform
        );
    }

    InitObject(transformed_mesh);

    // Override mesh AABB with custom calculated AABB
    transformed_mesh->SetAABB(aabb);

    return transformed_mesh;
}

#pragma region UITextRenderer

struct alignas(16) UITextCharacterShaderData
{
    Matrix4 transform;
    Vec2f   texcoord_start;
    Vec2f   texcoord_end;
};

struct alignas(16) UITextUniforms
{
    Matrix4 model_matrix;
    Matrix4 view_matrix;
    Matrix4 projection_matrix;
};

class UITextRenderer
{
public:
    UITextRenderer(Extent2D extent)
    {
        static constexpr SizeType initial_character_instance_buffer_size = sizeof(UITextCharacterShaderData) * 1024;

        { // instance buffer
            m_character_instance_buffer = MakeRenderObject<GPUBuffer>(GPUBufferType::STORAGE_BUFFER);
            DeferCreate(m_character_instance_buffer, g_engine->GetGPUDevice(), initial_character_instance_buffer_size);
        }

        { // uniform buffer
            m_uniform_buffer = MakeRenderObject<GPUBuffer>(GPUBufferType::CONSTANT_BUFFER);
            DeferCreate(m_uniform_buffer, g_engine->GetGPUDevice(), sizeof(UITextUniforms));
        }

        { // shader
            m_shader = g_shader_manager->GetOrCreate(NAME("UIText"), ShaderProperties(static_mesh_vertex_attributes));
            AssertThrow(m_shader.IsValid());
        }

        { // mesh
            m_quad_mesh = MeshBuilder::Quad();
            InitObject(m_quad_mesh);
        }

        { // framebuffer
            m_framebuffer = MakeRenderObject<Framebuffer>(
                extent,
                renderer::RenderPassStage::PRESENT,
                renderer::RenderPassMode::RENDER_PASS_INLINE
            );
            
            m_framebuffer->AddAttachment(
                0,
                InternalFormat::RGBA8,
                ImageType::TEXTURE_TYPE_2D,
                renderer::RenderPassStage::SHADER,
                renderer::LoadOperation::CLEAR,
                renderer::StoreOperation::STORE
            );

            DeferCreate(m_framebuffer, g_engine->GetGPUDevice());
        }

        { // camera
            m_camera = CreateObject<Camera>(extent.width, extent.height);

            m_camera->SetViewMatrix(Matrix4::LookAt(
                Vec3f::Zero(),
                Vec3f::UnitZ(),
                Vec3f::UnitY()
            ));

            m_camera->SetToOrthographicProjection(
                0.0f, float(extent.width),
                0.0f, float(extent.height),
                -1.0f, 1.0f
            );

            m_camera->SetFramebuffer(m_framebuffer);

            InitObject(m_camera);
            std::cout << "Ortho Camera View: " << m_camera->GetViewMatrix() << "\n";
            std::cout << "Ortho Camera Projection: " << m_camera->GetProjectionMatrix() << "\n";
        }
    }

    UITextRenderer(const UITextRenderer &other)             = delete;
    UITextRenderer &operator=(const UITextRenderer &other)  = delete;

    UITextRenderer(UITextRenderer &&other) noexcept
        : m_framebuffer(std::move(other.m_framebuffer)),
          m_shader(std::move(other.m_shader)),
          m_quad_mesh(std::move(other.m_quad_mesh)),
          m_camera(std::move(other.m_camera)),
          m_character_instance_buffer(std::move(other.m_character_instance_buffer)),
          m_uniform_buffer(std::move(other.m_uniform_buffer)),
          m_render_group(std::move(other.m_render_group))
    {
    }

    UITextRenderer &operator=(UITextRenderer &&other) noexcept
    {
        if (this != &other) {
            SafeRelease(std::move(m_shader));
            SafeRelease(std::move(m_framebuffer));
            SafeRelease(std::move(m_character_instance_buffer));
            SafeRelease(std::move(m_uniform_buffer));

            m_shader = std::move(other.m_shader);
            m_framebuffer = std::move(other.m_framebuffer);
            m_quad_mesh = std::move(other.m_quad_mesh);
            m_camera = std::move(other.m_camera);
            m_character_instance_buffer = std::move(other.m_character_instance_buffer);
            m_uniform_buffer = std::move(other.m_uniform_buffer);
            m_render_group = std::move(other.m_render_group);
        }

        return *this;
    }

    ~UITextRenderer()
    {
        SafeRelease(std::move(m_shader));
        SafeRelease(std::move(m_framebuffer));
        SafeRelease(std::move(m_character_instance_buffer));
        SafeRelease(std::move(m_uniform_buffer));
    }

    HYP_FORCE_INLINE const FramebufferRef &GetFramebuffer() const
        { return m_framebuffer; }

    void RenderText(Frame *frame, const UITextRenderData &render_data)
    {
        HYP_SCOPE;
        Threads::AssertOnThread(ThreadName::THREAD_RENDER);

        const uint frame_index = frame->GetFrameIndex();

        g_engine->GetRenderState().BindCamera(m_camera.Get());

        m_framebuffer->BeginCapture(frame->GetCommandBuffer(), frame_index);
        
        const SizeType characters_byte_size = render_data.characters.Size() * sizeof(UITextCharacterShaderData);

        bool was_character_instance_buffer_rebuilt = false;

        if (characters_byte_size > m_character_instance_buffer->Size()) {
            ByteBuffer previous_data;
            previous_data.SetSize(m_character_instance_buffer->Size());

            m_character_instance_buffer->Read(
                g_engine->GetGPUDevice(),
                m_character_instance_buffer->Size(),
                previous_data.Data()
            );

            HYPERION_ASSERT_RESULT(m_character_instance_buffer->EnsureCapacity(
                g_engine->GetGPUDevice(),
                characters_byte_size,
                &was_character_instance_buffer_rebuilt
            ));

            m_character_instance_buffer->Copy(
                g_engine->GetGPUDevice(),
                previous_data.Size(),
                previous_data.Data()
            );
        }

        { // copy characters shader data
            Array<UITextCharacterShaderData> characters;
            characters.Resize(render_data.characters.Size());

            for (SizeType index = 0; index < render_data.characters.Size(); index++) {
                characters[index] = UITextCharacterShaderData {
                    render_data.characters[index].transform,
                    render_data.characters[index].texcoord_start,
                    render_data.characters[index].texcoord_end
                };
            }

            m_character_instance_buffer->Copy(
                g_engine->GetGPUDevice(),
                0,
                characters_byte_size,
                characters.Data()
            );
        }

        { // copy uniform
            // // testing
            // g_engine->GetRenderData()->cameras.UpdateBuffer(g_engine->GetGPUDevice(), frame_index);

            UITextUniforms uniforms;
            uniforms.model_matrix = render_data.transform;
            uniforms.view_matrix = m_camera->GetViewMatrix();
            uniforms.projection_matrix = m_camera->GetProjectionMatrix();

            m_uniform_buffer->Copy(
                g_engine->GetGPUDevice(),
                0,
                sizeof(UITextUniforms),
                &uniforms
            );
        }

        CreateRenderGroup(render_data.font_atlas_texture);

        const uint descriptor_set_index = m_render_group->GetPipeline()->GetDescriptorTable()->GetDescriptorSetIndex(NAME("UITextDescriptorSet"));
        AssertThrow(descriptor_set_index != ~0u);

        if (was_character_instance_buffer_rebuilt) {
            const DescriptorSetRef &descriptor_set = m_render_group->GetPipeline()->GetDescriptorTable()->GetDescriptorSet(descriptor_set_index, frame_index);
            descriptor_set->SetElement(NAME("CharacterInstanceBuffer"), m_character_instance_buffer);
            descriptor_set->Update(g_engine->GetGPUDevice());
        }

        m_render_group->GetPipeline()->Bind(frame->GetCommandBuffer());

        if (renderer::RenderConfig::ShouldCollectUniqueDrawCallPerMaterial()) {
            m_render_group->GetPipeline()->GetDescriptorTable()->Bind<GraphicsPipelineRef>(
                frame->GetCommandBuffer(),
                frame_index,
                m_render_group->GetPipeline(),
                {
                    {
                        NAME("Scene"),
                        {
                            { NAME("ScenesBuffer"), HYP_RENDER_OBJECT_OFFSET(Scene, g_engine->GetRenderState().GetScene().id.ToIndex()) },
                            { NAME("CamerasBuffer"), HYP_RENDER_OBJECT_OFFSET(Camera, g_engine->GetRenderState().GetCamera().id.ToIndex()) },
                            { NAME("LightsBuffer"), HYP_RENDER_OBJECT_OFFSET(Light, 0) },
                            { NAME("EnvGridsBuffer"), HYP_RENDER_OBJECT_OFFSET(EnvGrid, g_engine->GetRenderState().bound_env_grid.ToIndex()) },
                            { NAME("CurrentEnvProbe"), HYP_RENDER_OBJECT_OFFSET(EnvProbe, g_engine->GetRenderState().GetActiveEnvProbe().ToIndex()) }
                        }
                    },
                    {
                        NAME("Object"),
                        {
                            { NAME("MaterialsBuffer"), 0 },
                            { NAME("SkeletonsBuffer"), 0 },
                            { NAME("EntityInstanceBatchesBuffer"), 0 }
                        }
                    }
                }
            );
        } else {
            m_render_group->GetPipeline()->GetDescriptorTable()->Bind<GraphicsPipelineRef>(
                frame->GetCommandBuffer(),
                frame_index,
                m_render_group->GetPipeline(),
                {
                    {
                        NAME("Scene"),
                        {
                            { NAME("ScenesBuffer"), HYP_RENDER_OBJECT_OFFSET(Scene, g_engine->GetRenderState().GetScene().id.ToIndex()) },
                            { NAME("CamerasBuffer"), HYP_RENDER_OBJECT_OFFSET(Camera, g_engine->GetRenderState().GetCamera().id.ToIndex()) },
                            { NAME("LightsBuffer"), HYP_RENDER_OBJECT_OFFSET(Light, 0) },
                            { NAME("EnvGridsBuffer"), HYP_RENDER_OBJECT_OFFSET(EnvGrid, g_engine->GetRenderState().bound_env_grid.ToIndex()) },
                            { NAME("CurrentEnvProbe"), HYP_RENDER_OBJECT_OFFSET(EnvProbe, g_engine->GetRenderState().GetActiveEnvProbe().ToIndex()) }
                        }
                    },
                    {
                        NAME("Object"),
                        {
                            { NAME("SkeletonsBuffer"), 0 },
                            { NAME("EntityInstanceBatchesBuffer"), 0 }
                        }
                    }
                }
            );
        }

        m_quad_mesh->Render(frame->GetCommandBuffer(), render_data.characters.Size());

        m_framebuffer->EndCapture(frame->GetCommandBuffer(), frame_index);

        g_engine->GetRenderState().UnbindCamera();
    }

private:
    DescriptorTableRef CreateDescriptorTable(const Handle<Texture> &font_atlas_texture)
    {
        renderer::DescriptorTableDeclaration descriptor_table_decl = m_shader->GetCompiledShader()->GetDescriptorUsages().BuildDescriptorTable();

        DescriptorTableRef descriptor_table = MakeRenderObject<DescriptorTable>(descriptor_table_decl);
        AssertThrow(descriptor_table != nullptr);

        for (uint frame_index = 0; frame_index < max_frames_in_flight; frame_index++) {
            const DescriptorSetRef &descriptor_set = descriptor_table->GetDescriptorSet(NAME("UITextDescriptorSet"), frame_index);
            AssertThrow(descriptor_set != nullptr);

            descriptor_set->SetElement(NAME("CharacterInstanceBuffer"), m_character_instance_buffer);
            descriptor_set->SetElement(NAME("UITextUniforms"), m_uniform_buffer);
            descriptor_set->SetElement(NAME("FontAtlasTexture"), font_atlas_texture->GetImageView());
        }

        DeferCreate(descriptor_table, g_engine->GetGPUDevice());

        return descriptor_table;
    }

    void CreateRenderGroup(const Handle<Texture> &font_atlas_texture)
    {
        Handle<RenderGroup> render_group = CreateObject<RenderGroup>(
            m_shader,
            RenderableAttributeSet(
                MeshAttributes {
                    .vertex_attributes = static_mesh_vertex_attributes
                },
                MaterialAttributes {
                    .bucket         = Bucket::BUCKET_TRANSLUCENT,
                    .fill_mode      = FillMode::FILL,
                    .blend_function = BlendFunction::None(),
                    .cull_faces     = FaceCullMode::NONE
                }
            ),
            CreateDescriptorTable(font_atlas_texture),
            RenderGroupFlags::NONE
        );

        render_group->AddFramebuffer(m_framebuffer);

        InitObject(render_group);

        m_render_group = std::move(render_group);
    }

    FramebufferRef          m_framebuffer;
    ShaderRef               m_shader;
    Handle<Mesh>            m_quad_mesh;
    Handle<Camera>          m_camera;
    GPUBufferRef            m_character_instance_buffer;
    GPUBufferRef            m_uniform_buffer;
    Handle<RenderGroup>     m_render_group;
};

#pragma endregion UITextRenderer

struct FontAtlasCharacterIterator
{
    Vec2f           placement;
    Vec2f           atlas_pixel_size;
    Vec2f           cell_dimensions;

    utf::u32char    char_value;

    Vec2i           char_offset;

    Vec2f           glyph_dimensions;
    Vec2f           glyph_scaling;

    float           bearing_y;
    float           char_width;
};

static void ForEachCharacter(const FontAtlas &font_atlas, const String &text, const Proc<void, const FontAtlasCharacterIterator &> &proc)
{
    Vec2f placement;

    const SizeType length = text.Length();
    
    const Vec2f cell_dimensions = Vec2f(font_atlas.GetCellDimensions()) / 64.0f;
    AssertThrowMsg(cell_dimensions.x * cell_dimensions.y != 0.0f, "Cell dimensions are invalid");

    const float cell_dimensions_ratio = cell_dimensions.x / cell_dimensions.y;

    AssertThrowMsg(font_atlas.GetAtlases() != nullptr, "Font atlas invalid");

    const Handle<Texture> &main_texture_atlas = font_atlas.GetAtlases()->GetMainAtlas();
    AssertThrowMsg(main_texture_atlas.IsValid(), "Main texture atlas is invalid");

    const Vec2f atlas_pixel_size = Vec2f::One() / Vec2f(Extent2D(main_texture_atlas->GetExtent()));

    for (SizeType i = 0; i < length; i++) {
        UITextCharacter character { };

        const utf::u32char ch = text.GetChar(i);

        if (ch == utf::u32char(' ')) {
            // add room for space
            placement.x += cell_dimensions.x * 0.5f;

            continue;
        }

        if (ch == utf::u32char('\n')) {
            // reset placement, add room for newline
            placement.x = 0.0f;
            placement.y += cell_dimensions.y;

            continue;
        }

        Optional<Glyph::Metrics> glyph_metrics = font_atlas.GetGlyphMetrics(ch);

        if (!glyph_metrics.HasValue()) {
            // @TODO Add a placeholder character for missing glyphs
            continue;
        }

        if (glyph_metrics->metrics.width == 0 || glyph_metrics->metrics.height == 0) {
            // empty width or height will cause issues
            continue;
        }

        FontAtlasCharacterIterator iter;

        iter.char_value = ch;

        iter.placement = placement;
        iter.atlas_pixel_size = atlas_pixel_size;
        iter.cell_dimensions = cell_dimensions;

        iter.char_offset = glyph_metrics->image_position;

        iter.glyph_dimensions = Vec2f { float(glyph_metrics->metrics.width), float(glyph_metrics->metrics.height) } / 64.0f;
        iter.glyph_scaling = Vec2f(iter.glyph_dimensions) / (Vec2f(cell_dimensions));

        iter.bearing_y = float(glyph_metrics->metrics.height - glyph_metrics->metrics.bearing_y) / 64.0f;
        iter.char_width = float(glyph_metrics->metrics.advance / 64) / 64.0f;

        proc(iter);

        placement.x += iter.char_width;
    }
}

static BoundingBox CalculateTextAABB(const FontAtlas &font_atlas, const String &text)
{
    BoundingBox aabb;

    ForEachCharacter(font_atlas, text, [&aabb](const FontAtlasCharacterIterator &iter)
    {
        BoundingBox character_aabb;
        character_aabb.Extend(Vec3f(iter.placement.x, iter.placement.y, 0.0f));
        character_aabb.Extend(Vec3f(iter.placement.x + iter.glyph_dimensions.x, iter.placement.y + iter.cell_dimensions.y, 0.0f));

        aabb.Extend(character_aabb);
    });

    return aabb;
}

#pragma region Render commands

struct RENDER_COMMAND(UpdateUITextRenderData) : renderer::RenderCommand
{
    String                  text;
    RC<UITextRenderData>    render_data;
    Vec2i                   size;
    Matrix4                 text_transform;
    RC<FontAtlas>           font_atlas;
    Handle<Texture>         font_atlas_texture;

    RENDER_COMMAND(UpdateUITextRenderData)(const String &text, const RC<UITextRenderData> &render_data, Vec2i size, const Matrix4 &text_transform, const RC<FontAtlas> &font_atlas, const Handle<Texture> &font_atlas_texture)
        : text(text),
          render_data(render_data),
          size(size),
          text_transform(text_transform),
          font_atlas(font_atlas),
          font_atlas_texture(font_atlas_texture)
    {
    }

    virtual ~RENDER_COMMAND(UpdateUITextRenderData)() override = default;

    virtual renderer::Result operator()() override
    {
        if (!render_data) {
            return { renderer::Result::RENDERER_ERR, "Cannot update UI text render data, invalid render data pointer set" };
        }

        if (!font_atlas) {
            return { renderer::Result::RENDERER_ERR, "Cannot update UI text render data, invalid font atlas texture set" };
        }

        render_data->characters.Clear();

        ForEachCharacter(*font_atlas, text, [&](const FontAtlasCharacterIterator &iter)
        {
            Transform character_transform;
            character_transform.SetScale(Vec3f(iter.glyph_dimensions.x, iter.glyph_dimensions.y, 1.0f));
            character_transform.GetTranslation().y += iter.cell_dimensions.y - iter.glyph_dimensions.y;
            character_transform.GetTranslation().y += iter.bearing_y;
            character_transform.GetTranslation() += Vec3f(iter.placement.x, iter.placement.y, 0.0f);
            character_transform.UpdateMatrix();

            UITextCharacter character { };
            character.transform = character_transform.GetMatrix();
            character.texcoord_start = Vec2f(iter.char_offset) * iter.atlas_pixel_size;
            character.texcoord_end = (Vec2f(iter.char_offset) + (iter.glyph_dimensions * 64.0f)) * iter.atlas_pixel_size;

            render_data->characters.PushBack(character);
        });

        render_data->size = size;
        render_data->transform = text_transform;
        render_data->font_atlas = std::move(font_atlas);
        render_data->font_atlas_texture = std::move(font_atlas_texture);

        HYPERION_RETURN_OK;
    }
};

struct RENDER_COMMAND(RepaintUIText) : renderer::RenderCommand
{
    RC<UITextRenderData>    render_data;
    Handle<Texture>         texture;
    UITextRenderer          text_renderer;

    RENDER_COMMAND(RepaintUIText)(const RC<UITextRenderData> &render_data, const Handle<Texture> &texture, UITextRenderer &&text_renderer)
        : render_data(render_data),
          texture(texture),
          text_renderer(std::move(text_renderer))
    {
    }

    virtual ~RENDER_COMMAND(RepaintUIText)() override
    {
        g_safe_deleter->SafeRelease(std::move(texture));
    }

    virtual renderer::Result operator()() override
    {
        if (!render_data) {
            return { renderer::Result::RENDERER_ERR, "Cannot repaint UI text, invalid render data pointer set" };
        }

        if (!texture.IsValid()) {
            return { renderer::Result::RENDERER_ERR, "Cannot repaint UI text, invalid texture set" };
        }

        if (render_data->size.x * render_data->size.y <= 0) {
            HYP_LOG(UI, LogLevel::WARNING, "Cannot repaint UI text, size is zero");

            HYPERION_RETURN_OK;
        }

        Frame *frame = g_engine->GetGPUInstance()->GetFrameHandler()->GetCurrentFrame();

        text_renderer.RenderText(frame, *render_data);

        const ImageRef &src_image = text_renderer.GetFramebuffer()->GetAttachment(0)->GetImage();

        // testing --- 

        // ByteBuffer byte_buffer;
        // byte_buffer.SetSize(render_data->size.x * render_data->size.y);

        // for (SizeType i = 0; i < render_data->size.x * render_data->size.y; i++) {
        //     byte_buffer.Data()[i] = 255;
        // }

        // RC<StreamedTextureData> streamed_texture_data(new StreamedTextureData(TextureData {
        //     TextureDesc {
        //         ImageType::TEXTURE_TYPE_2D,
        //         InternalFormat::R8,
        //         Extent3D { uint32(render_data->size.x), uint32(render_data->size.y), 1 }
        //     },
        //     byte_buffer
        // }));

        // Handle<Texture> test_texture = CreateObject<Texture>(std::move(streamed_texture_data));
        // InitObject(test_texture);

        // const ImageRef &src_image = test_texture->GetImage();



        const ImageRef &dst_image = texture->GetImage();

        src_image->InsertBarrier(frame->GetCommandBuffer(), renderer::ResourceState::COPY_SRC);
        dst_image->InsertBarrier(frame->GetCommandBuffer(), renderer::ResourceState::COPY_DST);

        HYP_LOG(UI, LogLevel::DEBUG, "Blit image ({} x {}) to image ({} x {})", src_image->GetExtent().width, src_image->GetExtent().height, dst_image->GetExtent().width, dst_image->GetExtent().height);

        dst_image->Blit(
            frame->GetCommandBuffer(),
            src_image
        );

        src_image->InsertBarrier(frame->GetCommandBuffer(), renderer::ResourceState::SHADER_RESOURCE);
        dst_image->InsertBarrier(frame->GetCommandBuffer(), renderer::ResourceState::SHADER_RESOURCE);

        HYPERION_RETURN_OK;
    };
};

#pragma endregion Render commands

#pragma region UIText

UIText::UIText(UIStage *parent, NodeProxy node_proxy)
    : UIObject(parent, std::move(node_proxy), UIObjectType::TEXT),
      m_render_data(new UITextRenderData)
{
    m_text_color = Color(Vec4f::One());
}

const RC<UITextRenderData> &UIText::GetRenderData() const
{
    return m_render_data;
}

void UIText::Init()
{
    UIObject::Init();

    UpdateMesh();
}

void UIText::SetText(const String &text)
{
    UIObject::SetText(text);

    UpdateSize();
    UpdateMesh();
}

RC<FontAtlas> UIText::GetFontAtlasOrDefault() const
{
    return m_font_atlas != nullptr
        ? m_font_atlas
        : (GetStage() != nullptr ? GetStage()->GetDefaultFontAtlas() : nullptr);
}

void UIText::SetFontAtlas(RC<FontAtlas> font_atlas)
{
    m_font_atlas = std::move(font_atlas);

    UpdateSize();
    UpdateMesh();
}

void UIText::UpdateMesh()
{
    const Scene *scene = GetScene();

    if (!scene) {
        return;
    }

    MeshComponent &mesh_component = scene->GetEntityManager()->GetComponent<MeshComponent>(GetEntity());

    Handle<Mesh> mesh;

    if (RC<FontAtlas> font_atlas = GetFontAtlasOrDefault()) {
        Handle<Texture> font_atlas_texture = font_atlas->GetAtlases()->GetAtlasForPixelSize(GetActualSize().y);
        UpdateRenderData(font_atlas, font_atlas_texture);

        // CharMeshBuilder char_mesh_builder(m_options);

        // mesh = char_mesh_builder.OptimizeCharMeshes(GetStage()->GetActualSize(), char_mesh_builder.BuildCharMeshes(*font_atlas, m_text));


        m_text_aabb = CalculateTextAABB(*font_atlas, m_text);

        HYP_LOG(UI, LogLevel::DEBUG, "Text aabb for {} = {}", m_text, m_text_aabb);
    } else {
        HYP_LOG(UI, LogLevel::WARNING, "No font atlas for UIText {}", GetName());

        UpdateRenderData(nullptr, Handle<Texture>::empty);
    }

    // mesh = GetQuadMesh();

    // // AssertThrow(mesh.IsValid());
    // // m_text_aabb = mesh->GetAABB();

    // // const Vec3f text_extent = mesh->GetAABB().GetExtent();
    // // const float text_aabb_ratio = text_extent.y / text_extent.x;

    // // SetAspectRatio(text_aabb_ratio);

    // // if (!mesh.IsValid()) {
    //     // mesh = GetQuadMesh();
    // // }

    // g_safe_deleter->SafeRelease(std::move(mesh_component.mesh));

    // mesh_component.mesh = mesh;
    // mesh_component.flags |= MESH_COMPONENT_FLAG_DIRTY;
}

void UIText::UpdateRenderData(const RC<FontAtlas> &font_atlas, const Handle<Texture> &font_atlas_texture)
{
    HYP_SCOPE;

    const NodeProxy &node = GetNode();

    const Vec2i size = MathUtil::Max(GetActualSize(), Vec2i::One());
    
    // Transform transform = node.IsValid() ? node->GetWorldTransform() : Transform::identity;
    // transform.SetTranslation(Vec3f::Zero());


    // // test
    // transform.SetScale(Vec3f(1.0f));
    Transform transform;
    transform.SetScale(Vec3f(size.x, size.y, 1.0f));

    if (!m_texture.IsValid() || Extent2D(m_texture->GetExtent()) != Extent2D(size)) {
        g_safe_deleter->SafeRelease(std::move(m_texture));

        m_texture = CreateObject<Texture>(Texture2D(
            Extent2D(size),
            InternalFormat::RGBA8,
            FilterMode::TEXTURE_FILTER_LINEAR,
            WrapMode::TEXTURE_WRAP_CLAMP_TO_EDGE
        ));
        
        InitObject(m_texture);
    }

    if (font_atlas != nullptr && font_atlas_texture != nullptr) {
        PUSH_RENDER_COMMAND(UpdateUITextRenderData, m_text, m_render_data, size, transform.GetMatrix(), font_atlas, font_atlas_texture);
    }

    SetNeedsRepaintFlag(true);
}

bool UIText::Repaint_Internal()
{
    HYP_SCOPE;

    const Vec2i size = GetActualSize();

    if (size.x * size.y <= 0) {
        HYP_LOG(UI, LogLevel::WARNING, "Cannot repaint UI text for element: {} - size is zero", GetName());

        return false;
    }

    UITextRenderer text_renderer { Extent2D(size) };

    PUSH_RENDER_COMMAND(RepaintUIText, m_render_data, m_texture, std::move(text_renderer));

    return true;
}

MaterialAttributes UIText::GetMaterialAttributes() const
{
    return MaterialAttributes {
        .shader_definition  = ShaderDefinition { NAME("UIObject"), ShaderProperties(static_mesh_vertex_attributes, { "TYPE_TEXT" }) },
        .bucket             = Bucket::BUCKET_UI,
        .blend_function     = BlendFunction(BlendModeFactor::SRC_ALPHA, BlendModeFactor::ONE_MINUS_SRC_ALPHA,
                                            BlendModeFactor::ONE, BlendModeFactor::ONE_MINUS_SRC_ALPHA),
        .cull_faces         = FaceCullMode::BACK,
        .flags              = MaterialAttributeFlags::NONE
    };
}

Material::ParameterTable UIText::GetMaterialParameters() const
{
    return {
        { Material::MATERIAL_KEY_ALBEDO, Vec4f(GetTextColor()) }
    };
}

Material::TextureSet UIText::GetMaterialTextures() const
{
    return {
        { Material::MATERIAL_TEXTURE_ALBEDO_MAP, m_texture }
    };
}

void UIText::UpdateSize(bool update_children)
{
    UIObject::UpdateSize(update_children);

    // Update material to get new font size if necessary
    UpdateMaterial();
    UpdateMeshData();
}

BoundingBox UIText::CalculateInnerAABB_Internal() const
{
    return m_text_aabb;
}

void UIText::OnFontAtlasUpdate_Internal()
{
    UIObject::OnFontAtlasUpdate_Internal();

    UpdateMesh();
    UpdateMaterial(false);
}

#pragma endregion UIText

} // namespace hyperion
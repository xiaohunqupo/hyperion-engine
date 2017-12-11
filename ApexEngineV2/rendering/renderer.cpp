#include "renderer.h"

namespace apex {
Renderer::Renderer()
{
    sky_bucket.reserve(5);
    opaque_bucket.reserve(30);
    transparent_bucket.reserve(20);
    particle_bucket.reserve(5);
}

void Renderer::ClearRenderables()
{
    sky_bucket.clear();
    opaque_bucket.clear();
    transparent_bucket.clear();
    particle_bucket.clear();
}

void Renderer::FindRenderables(Entity *top)
{
    if (top->GetRenderable() != nullptr) {
        BucketItem bucket_item;
        bucket_item.renderable = top->GetRenderable().get();
        bucket_item.material = &top->GetMaterial();
        bucket_item.transform = top->GetGlobalTransform();

        switch (top->GetRenderable()->GetRenderBucket()) {
            case Renderable::RB_SKY:
                sky_bucket.push_back(bucket_item);
                break;
            case Renderable::RB_OPAQUE:
                opaque_bucket.push_back(bucket_item);
                break;
            case Renderable::RB_TRANSPARENT:
                transparent_bucket.push_back(bucket_item);
                break;
            case Renderable::RB_PARTICLE:
                particle_bucket.push_back(bucket_item);
                break;
        }
    }

    for (size_t i = 0; i < top->NumChildren(); i++) {
        Entity *child = top->GetChild(i).get();
        FindRenderables(child);
    }
}

void Renderer::RenderBucket(Camera *cam, Bucket_t &bucket)
{
    for (BucketItem &it : bucket) {
        if (it.renderable->m_shader != nullptr) {
            it.renderable->m_shader->ApplyMaterial(*it.material);
            it.renderable->m_shader->ApplyTransforms(it.transform.GetMatrix(), cam);
            it.renderable->m_shader->Use();
            it.renderable->Render();
            it.renderable->m_shader->End();
        }
    }
}

void Renderer::RenderAll(Camera *cam)
{
    CoreEngine::GetInstance()->Viewport(0, 0, cam->GetWidth(), cam->GetHeight());
    RenderBucket(cam, sky_bucket);
    RenderBucket(cam, opaque_bucket);
    RenderBucket(cam, transparent_bucket);
    RenderBucket(cam, particle_bucket);
}
} // namespace apex
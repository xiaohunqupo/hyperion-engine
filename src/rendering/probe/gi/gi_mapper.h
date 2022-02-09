#ifndef GI_MAPPER_H
#define GI_MAPPER_H

#include "../probe.h"
#include "gi_mapper_camera.h"
#include "../../../math/bounding_box.h"
#include "../../renderable.h"

#include <memory>
#include <array>
#include <utility>

namespace hyperion {
class Shader;
class ComputeShader;

class GIMapper : public Probe {
public:
    GIMapper(const Vector3 &origin, const BoundingBox &bounds);
    ~GIMapper();

    virtual void Bind(Shader *shader) override;
    virtual void Update(double dt) override;
    virtual void Render(Renderer *renderer, Camera *cam) override;

private:
    virtual std::shared_ptr<Renderable> CloneImpl() override;

    std::shared_ptr<Texture3D> m_texture;
    std::shared_ptr<ComputeShader> m_clear_shader;
    std::shared_ptr<ComputeShader> m_mipmap_shader;

    Vector3 m_previous_origin;
    double m_render_tick;
    int m_render_index;
    bool m_is_first_run;
};
} // namespace apex

#endif
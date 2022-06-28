#include "tonemap.h"
#include <rendering/shader.h>
#include <rendering/post_fx.h>
#include <engine.h>

namespace hyperion::v2 {

TonemapEffect::TonemapEffect()
    : PostProcessingEffect(stage, index)
{
}

TonemapEffect::~TonemapEffect() = default;

Ref<Shader> TonemapEffect::CreateShader(Engine *engine)
{
    return engine->resources.shaders.Add(std::make_unique<Shader>(
        std::vector<SubShader> {
            SubShader{ShaderModule::Type::VERTEX, {
                Reader(FileSystem::Join(engine->assets.GetBasePath(), "/vkshaders/filter_pass_vert.spv")).ReadBytes()
            }},
            SubShader{ShaderModule::Type::FRAGMENT, {
                Reader(FileSystem::Join(engine->assets.GetBasePath(), "/vkshaders/tonemap.frag.spv")).ReadBytes()
            }}
        }
    ));
}

} // namespace hyperion::v2
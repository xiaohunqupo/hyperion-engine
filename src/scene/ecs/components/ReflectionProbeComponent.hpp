/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_ECS_REFLECTION_PROBE_COMPONENT_HPP
#define HYPERION_ECS_REFLECTION_PROBE_COMPONENT_HPP

#include <core/Name.hpp>

#include <core/memory/RefCountedPtr.hpp>

#include <scene/camera/Camera.hpp>

#include <math/Matrix4.hpp>
#include <math/Extent.hpp>
#include <math/Vector3.hpp>

#include <HashCode.hpp>

namespace hyperion {

class EnvProbe;
class ReflectionProbeRenderer;

HYP_STRUCT(Component, Label="Reflection Probe Component", Description="Handles cubemap reflection calculations for a single EnvProbe source", Editor=true)
struct ReflectionProbeComponent
{
    HYP_FIELD(Property="Dimensions", Serialize=true, Editor=true, Label="Dimensions")
    Vec2u                       dimensions = Vec2u { 256, 256 };

    HYP_FIELD(Property="ReflectionProbeRenderer", Serialize=false, Editor=false)
    RC<ReflectionProbeRenderer> reflection_probe_renderer;

    HYP_FIELD()
    HashCode                    transform_hash_code;
};

} // namespace hyperion

#endif
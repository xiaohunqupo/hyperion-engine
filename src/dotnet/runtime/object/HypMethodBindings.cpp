/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <core/object/HypClass.hpp>
#include <core/object/HypMethod.hpp>
#include <core/object/HypClassRegistry.hpp>
#include <core/object/HypObject.hpp>
#include <core/object/HypData.hpp>

#include <core/Name.hpp>

#include <dotnet/Object.hpp>

#include <dotnet/runtime/ManagedHandle.hpp>

#include <asset/serialization/fbom/FBOM.hpp>

#include <Types.hpp>

using namespace hyperion;

extern "C" {

HYP_EXPORT void HypMethod_GetName(const HypMethod *method, Name *out_name)
{
    if (!method || !out_name) {
        return;
    }

    *out_name = method->name;
}

HYP_EXPORT void HypMethod_GetReturnTypeID(const HypMethod *method, TypeID *out_return_type_id)
{
    if (!method || !out_return_type_id) {
        return;
    }

    *out_return_type_id = method->return_type_id;
}

HYP_EXPORT uint32 HypMethod_GetParameters(const HypMethod *method, const HypMethodParameter **out_params)
{
    if (!method || !out_params) {
        return 0;
    }

    if (method->params.Empty()) {
        return 0;
    }

    *out_params = method->params.Begin();

    return (uint32)method->params.Size();
}

HYP_EXPORT uint32 HypMethod_GetFlags(const HypMethod *method)
{
    if (!method) {
        return uint32(HypMethodFlags::NONE);
    }

    return uint32(method->flags);
}

HYP_EXPORT bool HypMethod_Invoke(const HypMethod *method, HypData *args, uint32 num_args, HypData *out_result)
{
    if (!method || !out_result) {
        return false;
    }

    if (num_args != 0 && !args) {
        return false;
    }

    Span<HypData> args_view(args, num_args);

    *out_result = method->Invoke(args_view);

    return true;
}

} // extern "C"
/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <core/object/HypData.hpp>
#include <core/object/HypClass.hpp>

#include <core/Name.hpp>

#include <dotnet/Object.hpp>
#include <dotnet/Class.hpp>

#include <dotnet/runtime/ManagedHandle.hpp>

#include <asset/serialization/fbom/FBOM.hpp>

#include <Engine.hpp>
#include <Types.hpp>

using namespace hyperion;

extern "C" {

HYP_EXPORT void HypData_Construct(ValueStorage<HypData> *hyp_data_storage)
{
    if (hyp_data_storage) {
        hyp_data_storage->Construct();
    }
}

HYP_EXPORT void HypData_Destruct(ValueStorage<HypData> *hyp_data_storage)
{
    if (hyp_data_storage) {
        hyp_data_storage->Destruct();
    }
}

HYP_EXPORT void HypData_GetTypeID(const HypData *hyp_data, TypeID *out_type_id)
{
    if (!hyp_data || !out_type_id) {
        return;
    }

    *out_type_id = hyp_data->GetTypeID();
}

HYP_EXPORT bool HypData_IsValid(const HypData *hyp_data)
{
    if (!hyp_data) {
        return false;
    }

    return hyp_data->IsValid();
}

#define HYP_DEFINE_HYPDATA_GET(type, name) \
    HYP_EXPORT bool HypData_Get##name(const HypData *hyp_data, type *out_value) \
    { \
        if (!hyp_data || !out_value) { \
            return false; \
        } \
        \
        if (hyp_data->Is<type>()) { \
            *out_value = hyp_data->Get<type>(); \
            \
            return true; \
        } \
        \
        return false; \
    }

#define HYP_DEFINE_HYPDATA_IS(type, name) \
    HYP_EXPORT bool HypData_Is##name(const HypData *hyp_data) \
    { \
        if (!hyp_data) { \
            return false; \
        } \
        \
        return hyp_data->Is<type>(); \
    }

#define HYP_DEFINE_HYPDATA_SET(type, name) \
    HYP_EXPORT bool HypData_Set##name(HypData *hyp_data, type value) \
    { \
        if (!hyp_data) { \
            return false; \
        } \
        \
        *hyp_data = HypData(value); \
        return true; \
    }

HYP_DEFINE_HYPDATA_GET(int8, Int8)
HYP_DEFINE_HYPDATA_GET(int16, Int16)
HYP_DEFINE_HYPDATA_GET(int32, Int32)
HYP_DEFINE_HYPDATA_GET(int64, Int64)
HYP_DEFINE_HYPDATA_GET(uint8, UInt8)
HYP_DEFINE_HYPDATA_GET(uint16, UInt16)
HYP_DEFINE_HYPDATA_GET(uint32, UInt32)
HYP_DEFINE_HYPDATA_GET(uint64, UInt64)
HYP_DEFINE_HYPDATA_GET(float, Float)
HYP_DEFINE_HYPDATA_GET(double, Double)
HYP_DEFINE_HYPDATA_GET(bool, Bool)

HYP_DEFINE_HYPDATA_SET(int8, Int8)
HYP_DEFINE_HYPDATA_SET(int16, Int16)
HYP_DEFINE_HYPDATA_SET(int32, Int32)
HYP_DEFINE_HYPDATA_SET(int64, Int64)
HYP_DEFINE_HYPDATA_SET(uint8, UInt8)
HYP_DEFINE_HYPDATA_SET(uint16, UInt16)
HYP_DEFINE_HYPDATA_SET(uint32, UInt32)
HYP_DEFINE_HYPDATA_SET(uint64, UInt64)
HYP_DEFINE_HYPDATA_SET(float, Float)
HYP_DEFINE_HYPDATA_SET(double, Double)
HYP_DEFINE_HYPDATA_SET(bool, Bool)

HYP_DEFINE_HYPDATA_IS(int8, Int8)
HYP_DEFINE_HYPDATA_IS(int16, Int16)
HYP_DEFINE_HYPDATA_IS(int32, Int32)
HYP_DEFINE_HYPDATA_IS(int64, Int64)
HYP_DEFINE_HYPDATA_IS(uint8, UInt8)
HYP_DEFINE_HYPDATA_IS(uint16, UInt16)
HYP_DEFINE_HYPDATA_IS(uint32, UInt32)
HYP_DEFINE_HYPDATA_IS(uint64, UInt64)
HYP_DEFINE_HYPDATA_IS(float, Float)
HYP_DEFINE_HYPDATA_IS(double, Double)
HYP_DEFINE_HYPDATA_IS(bool, Bool)

#undef HYP_DEFINE_HYPDATA_GET
#undef HYP_DEFINE_HYPDATA_IS
#undef HYP_DEFINE_HYPDATA_SET

HYP_EXPORT bool HypData_IsID(const HypData *hyp_data)
{
    if (!hyp_data) {
        return false;
    }

    return hyp_data->Is<IDBase>();
}

HYP_EXPORT bool HypData_GetID(const HypData *hyp_data, IDBase *out_id)
{
    if (!hyp_data || !out_id) {
        return false;
    }

    if (hyp_data->Is<IDBase>()) {
        *out_id = hyp_data->Get<IDBase>();

        return true;
    }

    return false;
}

HYP_EXPORT bool HypData_SetID(HypData *hyp_data, uint32 id_value)
{
    if (!hyp_data) {
        return false;
    }

    *hyp_data = HypData(IDBase { id_value });

    return true;
}

HYP_EXPORT bool HypData_IsHypObject(const HypData *hyp_data)
{
    if (!hyp_data) {
        return false;
    }

    TypeID type_id;
    AnyRef any_ref;

    if (hyp_data->Is<AnyHandle>()) {
        const AnyHandle &handle = hyp_data->Get<AnyHandle>();

        return handle.IsValid() && GetClass(handle.GetTypeID()) != nullptr;
    } else if (hyp_data->Is<RC<void>>()) {
        const RC<void> &rc = hyp_data->Get<RC<void>>();

        return rc != nullptr && GetClass(rc.GetTypeID()) != nullptr;
    } else if (hyp_data->Is<AnyRef>()) {
        const AnyRef &any_ref = hyp_data->Get<AnyRef>();

        return any_ref.HasValue() && GetClass(any_ref.GetTypeID()) != nullptr;
    } else {
        return false;
    }
}

HYP_EXPORT bool HypData_GetHypObject(const HypData *hyp_data, void **out_object)
{
    if (!hyp_data || !out_object) {
        return false;
    }

    TypeID type_id;
    AnyRef any_ref;

    DebugLog(LogType::Debug, "GetHypObject, current type = %u  anyref = %u\n", hyp_data->value.GetTypeID().Value(), TypeID::ForType<AnyRef>().Value());
    if (hyp_data->value.GetTypeID() == TypeID::ForType<AnyRef>()) {
        AssertThrow(hyp_data->Is<AnyRef>());
    }

    if (hyp_data->Is<AnyHandle>()) {
        const AnyHandle &handle = hyp_data->Get<AnyHandle>();

        type_id = handle.GetTypeID();
        any_ref = handle.ToAnyRef();
    } else if (hyp_data->Is<RC<void>>()) {
        const RC<void> &rc = hyp_data->Get<RC<void>>();

        type_id = rc.GetTypeID();
        any_ref = AnyRef(type_id, rc.Get());
    } else if (hyp_data->Is<AnyRef>()) {
        any_ref = hyp_data->Get<AnyRef>();
        type_id = any_ref.GetTypeID();
    } else {
        return false;
    }

    *out_object = nullptr;

    if (type_id && any_ref.HasValue()) {
        const HypClass *hyp_class = GetClass(type_id);

        if (!hyp_class) {
            return false;
        }

        const IHypObjectInitializer *object_initializer = hyp_class->GetObjectInitializer(any_ref.GetPointer());
        AssertThrow(object_initializer != nullptr);

        dotnet::Object *managed_object = object_initializer->GetManagedObject();
        AssertThrow(managed_object != nullptr);

        *out_object = managed_object->GetUnderlyingObject().ptr;

        return true;
    }

    return false;
}

HYP_EXPORT bool HypData_SetHypObject(HypData *hyp_data, const HypClass *hyp_class, void *native_address)
{
    if (!hyp_data || !hyp_class || !native_address) {
        return false;
    }

    const TypeID type_id = hyp_class->GetTypeID();

    if (hyp_class->UseHandles()) {
        ObjectContainerBase &container = ObjectPool::GetContainer(type_id);
        
        const uint32 index = container.GetObjectIndex(native_address);
        if (index == ~0u) {
            HYP_FAIL("Address %p is not valid for object container for TypeID %u", native_address, type_id.Value());
        }

        *hyp_data = HypData(AnyHandle(type_id, IDBase { index + 1 }));

        return true;
    } else if (hyp_class->UseRefCountedPtr()) {
        RC<void> rc;
        rc.SetRefCountData_Internal(static_cast<typename RC<void>::RefCountedPtrBase::RefCountDataType *>(native_address), /* inc_ref */ true);

        *hyp_data = HypData(std::move(rc));

        return true;
    } else {
        HYP_FAIL("Unhandled HypClass allocation method");
    }

    return false;
}

} // extern "C"
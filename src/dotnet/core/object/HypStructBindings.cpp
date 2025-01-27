/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <core/object/HypStruct.hpp>
#include <core/object/HypClass.hpp>

#include <core/Defines.hpp>

#include <core/logging/Logger.hpp>

namespace hyperion {

HYP_DECLARE_LOG_CHANNEL(Object);

using DynamicHypStructInstance_DestructFunction = void(*)(void *);

class DynamicHypStructInstance final : public HypStruct
{
public:
    DynamicHypStructInstance(TypeID type_id, Name name, uint32 size, Span<const HypClassAttribute> attributes, EnumFlags<HypClassFlags> flags, Span<HypMember> members, DynamicHypStructInstance_DestructFunction destruct_function)
        : HypStruct(type_id, name, Name::Invalid(), attributes, flags, members),
          m_size(size),
          m_destruct_function(destruct_function)
    {
    }

    virtual ~DynamicHypStructInstance() override = default;

    virtual SizeType GetSize() const override
    {
        return m_size;
    }
    
    virtual bool GetManagedObject(const void *object_ptr, dotnet::ObjectReference &out_object_reference) const override
    {
        AssertThrow(object_ptr != nullptr);

        // Construct a new instance of the struct and return an ObjectReference pointing to it.
        if (!CreateStructInstance(out_object_reference, object_ptr, m_size)) {
            return false;
        }

        return true;
    }

    virtual bool CanCreateInstance() const override
    {
        return true;
    }

    virtual void ConstructFromBytes(ConstByteView view, HypData &out) const override
    {
        void *data = Memory::Allocate(m_size);
        Memory::MemCpy(data, view.Data(), m_size);

        out = HypData(Any::FromVoidPointer(m_type_id, data, m_destruct_function));
    }

    virtual fbom::FBOMResult SerializeStruct(ConstAnyRef in, fbom::FBOMObject &out) const override
    {
        HYP_NOT_IMPLEMENTED();
    }

    virtual fbom::FBOMResult DeserializeStruct(const fbom::FBOMObject &in, HypData &out) const override
    {
        HYP_NOT_IMPLEMENTED();
    }

protected:
    virtual void CreateInstance_Internal(HypData &out) const override
    {
        HYP_NOT_IMPLEMENTED_VOID();
    }

    virtual void CreateInstance_Internal(HypData &out, UniquePtr<dotnet::Object> &&managed_object) const override
    {
        HYP_NOT_IMPLEMENTED_VOID();
    }

    virtual HashCode GetInstanceHashCode_Internal(ConstAnyRef ref) const override
    {
        HYP_NOT_IMPLEMENTED();
    }
    
    uint32                                      m_size;
    DynamicHypStructInstance_DestructFunction   m_destruct_function;
};

extern "C" {

HYP_EXPORT HypStruct *HypStruct_CreateDynamicHypStruct(const TypeID *type_id, const char *type_name, uint32 size, DynamicHypStructInstance_DestructFunction destruct_function)
{
    AssertThrow(type_id != nullptr);
    AssertThrow(type_name != nullptr);

    if (size == 0) {
        HYP_LOG(Object, Error, "Cannot create HypStruct with size 0");

        return nullptr;
    }

    return new DynamicHypStructInstance(*type_id, CreateNameFromDynamicString(type_name), size, Span<const HypClassAttribute>(), HypClassFlags::STRUCT_TYPE | HypClassFlags::DYNAMIC, Span<HypMember>(), destruct_function);
}

HYP_EXPORT void HypStruct_DestroyDynamicHypStruct(HypStruct *hyp_struct)
{
    AssertThrow(hyp_struct != nullptr);

    delete hyp_struct;
}

} // extern "C"

} // namespace hyperion
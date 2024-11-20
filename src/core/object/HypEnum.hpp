/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_CORE_HYP_ENUM_HPP
#define HYPERION_CORE_HYP_ENUM_HPP

#include <core/object/HypClass.hpp>
#include <core/object/HypData.hpp>

#include <core/utilities/TypeAttributes.hpp>

namespace hyperion {

class HypEnum : public HypClass
{
public:
    HypEnum(TypeID type_id, Name name, Name parent_name, Span<const HypClassAttribute> attributes, EnumFlags<HypClassFlags> flags, Span<HypMember> members)
        : HypClass(type_id, name, parent_name, attributes, flags, members)
    {
    }

    virtual ~HypEnum() override = default;

    virtual bool IsValid() const override
    {
        return true;
    }

    virtual HypClassAllocationMethod GetAllocationMethod() const override
    {
        return HypClassAllocationMethod::NONE;
    }

    virtual SizeType GetSize() const override = 0;

    virtual bool GetManagedObject(const void *object_ptr, dotnet::ObjectReference &out_object_reference) const override = 0;

    virtual bool CanCreateInstance() const override = 0;

    virtual TypeID GetUnderlyingTypeID() const = 0;

protected:
    virtual IHypObjectInitializer *GetObjectInitializer_Internal(void *object_ptr) const override
    {
        return nullptr;
    }

    virtual void CreateInstance_Internal(HypData &out) const override = 0;

    virtual HashCode GetInstanceHashCode_Internal(ConstAnyRef ref) const override = 0;
};

template <class T>
class HypEnumInstance final : public HypEnum
{
public:
    static HypEnumInstance &GetInstance(Name name, Name parent_name, Span<const HypClassAttribute> attributes, EnumFlags<HypClassFlags> flags, Span<HypMember> members)
    {
        static HypEnumInstance instance { name, parent_name, attributes, flags, members };

        return instance;
    }

    HypEnumInstance(Name name, Name parent_name, Span<const HypClassAttribute> attributes, EnumFlags<HypClassFlags> flags, Span<HypMember> members)
        : HypEnum(TypeID::ForType<T>(), name, parent_name, attributes, flags, members)
    {
    }

    virtual ~HypEnumInstance() override = default;

    virtual SizeType GetSize() const override
    {
        return sizeof(T);
    }
    
    virtual bool GetManagedObject(const void *object_ptr, dotnet::ObjectReference &out_object_reference) const override
    {
        HYP_NOT_IMPLEMENTED();
    }

    virtual bool CanCreateInstance() const override
    {
        return true;
    }

    virtual TypeID GetUnderlyingTypeID() const override
    {
        static const TypeID type_id = TypeID::ForType<std::underlying_type_t<T>>();

        return type_id;
    }

protected:
    virtual void CreateInstance_Internal(HypData &out) const override
    {
        out = T { };
    }

    virtual void CreateInstance_Internal(HypData &out, UniquePtr<dotnet::Object> &&managed_object) const override
    {
        HYP_NOT_IMPLEMENTED_VOID();
    }

    virtual HashCode GetInstanceHashCode_Internal(ConstAnyRef ref) const override
    {
        return HashCode::GetHashCode(ref.Get<T>());
    }
};

} // namespace hyperion

#endif
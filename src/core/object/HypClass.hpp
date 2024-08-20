/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_CORE_HYP_CLASS_HPP
#define HYPERION_CORE_HYP_CLASS_HPP

#include <core/object/HypClassRegistry.hpp>
#include <core/object/HypObjectEnums.hpp>
#include <core/object/HypData.hpp>

#include <core/containers/LinkedList.hpp>
#include <core/containers/HashMap.hpp>
#include <core/containers/Array.hpp>

#include <core/utilities/ValueStorage.hpp>
#include <core/utilities/Span.hpp>

#include <core/memory/UniquePtr.hpp>
#include <core/memory/Any.hpp>
#include <core/memory/AnyRef.hpp>

#include <asset/serialization/fbom/FBOMResult.hpp>

namespace hyperion {

namespace dotnet {
class Class;
class Object;
struct ObjectReference;
} // namespace dotnet

namespace fbom {
class FBOMData;
} // namespace fbom

struct HypMember;
struct HypProperty;
struct HypMethod;
struct HypField;

class IHypObjectInitializer;

class HYP_API HypClass
{
public:
    friend struct detail::HypClassRegistrationBase;

    HypClass(TypeID type_id, Name name, Name parent_name, Span<HypClassAttribute> attributes, EnumFlags<HypClassFlags> flags, Span<HypMember> members);
    HypClass(const HypClass &other)                 = delete;
    HypClass &operator=(const HypClass &other)      = delete;
    HypClass(HypClass &&other) noexcept             = delete;
    HypClass &operator=(HypClass &&other) noexcept  = delete;
    virtual ~HypClass();

    virtual void Initialize();

    virtual bool IsValid() const
        { return false; }

    virtual HypClassAllocationMethod GetAllocationMethod() const = 0;

    HYP_FORCE_INLINE bool UseHandles() const
        { return GetAllocationMethod() == HypClassAllocationMethod::OBJECT_POOL_HANDLE; }

    HYP_FORCE_INLINE bool UseRefCountedPtr() const
        { return GetAllocationMethod() == HypClassAllocationMethod::REF_COUNTED_PTR; }

    HYP_FORCE_INLINE Name GetName() const
        { return m_name; }

    const HypClass *GetParent() const;

    virtual SizeType GetSize() const = 0;

    virtual const IHypObjectInitializer *GetObjectInitializer(const void *object_ptr) const = 0;

    HYP_FORCE_INLINE TypeID GetTypeID() const
        { return m_type_id; }

    HYP_FORCE_INLINE EnumFlags<HypClassFlags> GetFlags() const
        { return m_flags; }

    HYP_FORCE_INLINE bool IsClassType() const
        { return m_flags & HypClassFlags::CLASS_TYPE; }

    HYP_FORCE_INLINE bool IsStructType() const
        { return m_flags & HypClassFlags::STRUCT_TYPE; }

    HYP_FORCE_INLINE bool IsAbstract() const
        { return m_attributes.Contains("abstract"); }

    HYP_FORCE_INLINE const String *GetAttribute(UTF8StringView key) const
    {
        auto it = m_attributes.FindAs(key);

        if (it == m_attributes.End()) {
            return nullptr;
        }

        return &it->second;
    }

    HypProperty *GetProperty(WeakName name) const;

    HYP_FORCE_INLINE const Array<HypProperty *> &GetProperties() const
        { return m_properties; }

    HypMethod *GetMethod(WeakName name) const;

    HYP_FORCE_INLINE const Array<HypMethod *> &GetMethods() const
        { return m_methods; }

    HypField *GetField(WeakName name) const;

    HYP_FORCE_INLINE const Array<HypField *> &GetFields() const
        { return m_fields; }

    dotnet::Class *GetManagedClass() const;

    virtual bool GetManagedObject(const void *object_ptr, dotnet::ObjectReference &out_object_reference) const = 0;

    virtual bool CanCreateInstance() const = 0;

    HYP_FORCE_INLINE void CreateInstance(HypData &out) const
    {
        AssertThrowMsg(CanCreateInstance() && !IsAbstract(), "Cannot create a new instance for HypClass %s",
            GetName().LookupString());

        CreateInstance_Internal(out);
    }

    HYP_FORCE_INLINE HashCode GetInstanceHashCode(ConstAnyRef ref) const
    {
        AssertThrowMsg(ref.GetTypeID() == GetTypeID(), "Expected HypClass instance to have type ID %u but got type ID %u",
            ref.GetTypeID().Value(), GetTypeID().Value());

        return GetInstanceHashCode_Internal(ref);
    }

protected:
    virtual void CreateInstance_Internal(HypData &out) const = 0;

    virtual HashCode GetInstanceHashCode_Internal(ConstAnyRef ref) const = 0;

    static bool GetManagedObjectFromObjectInitializer(const IHypObjectInitializer *object_initializer, dotnet::ObjectReference &out_object_reference);

    TypeID                          m_type_id;
    Name                            m_name;
    Name                            m_parent_name;
    HashMap<String, String>         m_attributes;
    EnumFlags<HypClassFlags>        m_flags;
    Array<HypProperty *>            m_properties;
    HashMap<Name, HypProperty *>    m_properties_by_name;
    Array<HypMethod *>              m_methods;
    HashMap<Name, HypMethod *>      m_methods_by_name;
    Array<HypField *>               m_fields;
    HashMap<Name, HypField *>       m_fields_by_name;
};

template <class T>
class HypClassInstance : public HypClass
{
public:
    static HypClassInstance &GetInstance(Name name, Name parent_name, Span<HypClassAttribute> attributes, EnumFlags<HypClassFlags> flags, Span<HypMember> members)
    {
        static HypClassInstance instance { name, parent_name, attributes, flags, members };

        return instance;
    }

    HypClassInstance(Name name, Name parent_name, Span<HypClassAttribute> attributes, EnumFlags<HypClassFlags> flags, Span<HypMember> members)
        : HypClass(TypeID::ForType<T>(), name, parent_name, attributes, flags, members)
    {
    }

    virtual ~HypClassInstance() = default;

    virtual bool IsValid() const override
    {
        return true;
    }

    virtual HypClassAllocationMethod GetAllocationMethod() const override
    {
        if constexpr (has_opaque_handle_defined<T>) {
            return HypClassAllocationMethod::OBJECT_POOL_HANDLE;
        } else {
            static_assert(std::is_base_of_v<EnableRefCountedPtrFromThisBase<>, T>, "HypObject must inherit EnableRefCountedPtrFromThis<T> if it does not use ObjectPool (Handle<T>)");
            
            return HypClassAllocationMethod::REF_COUNTED_PTR;
        }
    }

    virtual SizeType GetSize() const override
    {
        return sizeof(T);
    }

    virtual const IHypObjectInitializer *GetObjectInitializer(const void *object_ptr) const override
    {
        if (!object_ptr) {
            return nullptr;
        }

        return &static_cast<const T *>(object_ptr)->GetObjectInitializer();
    }

    virtual bool GetManagedObject(const void *object_ptr, dotnet::ObjectReference &out_object_reference) const override
    {
        if (!object_ptr) {
            return false;
        }

        return GetManagedObjectFromObjectInitializer(GetObjectInitializer(object_ptr), out_object_reference);
    }

    virtual bool CanCreateInstance() const override
    {
        if constexpr (std::is_default_constructible_v<T>) {
            return true;
        } else {
            return false;
        }
    }
    
protected:
    virtual void CreateInstance_Internal(HypData &out) const override
    {
        if constexpr (std::is_default_constructible_v<T>) {
            if constexpr (has_opaque_handle_defined<T>) {
                out = CreateObject<T>();
            } else {
                out = RC<T>::Construct();
            }
        } else {
            HYP_NOT_IMPLEMENTED_VOID();
        }
    }

    virtual HashCode GetInstanceHashCode_Internal(ConstAnyRef ref) const override
    {
        if constexpr (HasGetHashCode<T, HashCode>::value) {
            return HashCode::GetHashCode(ref.Get<T>());
        } else {
            HYP_NOT_IMPLEMENTED();
        }
    }
};

} // namespace hyperion

#endif
/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <core/Handle.hpp>

namespace hyperion {

#define DEF_HANDLE(T) \
    IObjectContainer *g_container_ptr_##T = &ObjectPool::GetObjectContainerHolder().Add(TypeID::ForType< T >()); \
    IObjectContainer *HandleDefinition< T >::GetAllottedContainerPointer() \
    { \
        return g_container_ptr_##T; \
    }

#define DEF_HANDLE_NS(ns, T) \
    IObjectContainer *g_container_ptr_##T = &ObjectPool::GetObjectContainerHolder().Add(TypeID::ForType< ns::T >()); \
    IObjectContainer *HandleDefinition< ns::T >::GetAllottedContainerPointer() \
    { \
        return g_container_ptr_##T; \
    }

#include <core/inl/HandleDefinitions.inl>

#undef DEF_HANDLE
#undef DEF_HANDLE_NS

#pragma region AnyHandle

AnyHandle::AnyHandle(HypObjectBase *hyp_object_ptr)
    : type_id(hyp_object_ptr ? hyp_object_ptr->GetObjectHeader_Internal()->container->GetObjectTypeID() : TypeID::Void()),
      ptr(hyp_object_ptr ? hyp_object_ptr->GetObjectHeader_Internal() : nullptr)
{
    if (IsValid()) {
        ptr->IncRefStrong();
    }
}

AnyHandle::AnyHandle(const AnyHandle &other)
    : type_id(other.type_id),
      ptr(other.ptr)
{
    if (IsValid()) {
        ptr->IncRefStrong();
    }
}

AnyHandle &AnyHandle::operator=(const AnyHandle &other)
{
    if (this == &other) {
        return *this;
    }

    if (IsValid()) {
        ptr->DecRefStrong();
    }

    ptr = other.ptr;
    type_id = other.type_id;

    if (IsValid()) {
        ptr->IncRefStrong();
    }

    return *this;
}

AnyHandle::AnyHandle(AnyHandle &&other) noexcept
    : type_id(other.type_id),
      ptr(other.ptr)
{
    other.ptr = nullptr;
}

AnyHandle &AnyHandle::operator=(AnyHandle &&other) noexcept
{
    if (this == &other) {
        return *this;
    }

    if (IsValid()) {
        ptr->DecRefStrong();
    }

    ptr = other.ptr;
    type_id = other.type_id;

    other.ptr = nullptr;

    return *this;
}

AnyHandle::~AnyHandle()
{
    if (IsValid()) {
        ptr->DecRefStrong();
    }
}

AnyHandle::IDType AnyHandle::GetID() const
{
    if (!IsValid()) {
        return IDType();
    }

    return IDType { ptr->index + 1 };
}

AnyRef AnyHandle::ToRef() const
{
    if (!IsValid()) {
        return AnyRef(type_id, nullptr);
    }

    return AnyRef(type_id, ptr->container->GetObject(ptr));
}

void AnyHandle::Reset()
{
    if (IsValid()) {
        ptr->DecRefStrong();
    }

    ptr = nullptr;
}

HypObjectBase *AnyHandle::Release()
{
    if (!IsValid()) {
        return nullptr;
    }

    HypObjectBase *hyp_object_ptr = ptr->Release();
    AssertThrow(hyp_object_ptr != nullptr);

    ptr = nullptr;

    return hyp_object_ptr;
}

#pragma endregion AnyHandle

} // namespace hyperion
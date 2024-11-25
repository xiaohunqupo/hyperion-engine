/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_CORE_HANDLE_HPP
#define HYPERION_CORE_HANDLE_HPP

#include <core/Core.hpp>
#include <core/ID.hpp>
#include <core/ObjectPool.hpp>
#include <core/object/HypObjectFwd.hpp>
#include <core/memory/UniquePtr.hpp>

namespace hyperion {

template <class T>
class ObjectContainer;

template <class T>
struct Handle;

template <class T>
struct WeakHandle;

template <class T>
static inline ObjectContainer<T> &GetContainer(UniquePtr<ObjectContainerBase> &allotted_container)
{
    return ObjectPool::template GetContainer<T>(allotted_container);
}

template <class T>
static inline UniquePtr<ObjectContainerBase> *AllotContainer()
{
    return ObjectPool::template AllotContainer<T>();
}

struct HandleBase
{
    uint index;

    HandleBase() : index(0) { }
    HandleBase(uint index) : index(index) { }
    HandleBase(const HandleBase &other)                 = default;
    HandleBase &operator=(const HandleBase &other)      = default;
    HandleBase(HandleBase &other) noexcept              = default;
    HandleBase &operator=(HandleBase &&other) noexcept  = default;
};

template <class T>
HYP_FORCE_INLINE static auto GetContainer() -> std::conditional_t< implementation_exists<T>, ObjectContainer<T> *, ObjectContainerBase * >
{
    // If T is a defined type, we can use ObjectContainer<T> methods (ObjectContainer<T> is final, virtual calls can be optimized)
    if constexpr (implementation_exists<T>) {
        return reinterpret_cast<ObjectContainer<T> *>(Handle<T>::GetContainer_Internal());
    } else {
        return Handle<T>::GetContainer_Internal();
    }
}

/*! \brief Definition of a handle for a type.
 *  \details In Hyperion, a handle is a reference to an instance of a specific core engine object type.
 *  These objects are managed by the ObjectPool and are reference counted.
 *  \note Objects that are managed by the ObjectPool must have a HandleDefinition defined for them.
 *  Check the HandleDefinitions.inl file for list of defined handles.
 *  \tparam T The type of object that the handle is referencing. 
*/
template <class T>
struct Handle : HandleBase
{
    using IDType = ID<T>;

    static_assert(has_opaque_handle_defined<T>, "Type does not support handles");
    
    static ObjectContainerBase *s_container;

    static ObjectContainerBase *GetContainer_Internal()
    {
        static struct Initializer
        {
            Initializer()
            {
                //s_container = &(ObjectPool::template GetContainer<T>(*HandleDefinition<T>::GetAllottedContainerPointer()));
                s_container = HandleDefinition<T>::GetAllottedContainerPointer()->Get();
                AssertThrow(s_container != nullptr);
            }
        } initializer;

        return s_container;
    }

    static const Handle empty;

    Handle() : HandleBase() { }

    /*! \brief Construct a handle from the given ID.
     *  \param id The ID of the object to reference. */
    explicit Handle(IDType id)
        : HandleBase(id.Value())
    {
        if (index != 0) {
            GetContainer<T>()->IncRefStrong(index - 1);
        }
    }

    template <class TPointerType, typename = std::enable_if_t<IsHypObject<TPointerType>::value && std::is_convertible_v<TPointerType *, T *>>>
    explicit Handle(TPointerType *ptr)
        : HandleBase()
    {
        if (ptr != nullptr) {
            *this = ptr->HandleFromThis();
        }
    }

    Handle(const Handle &other)
        : HandleBase(static_cast<const HandleBase &>(other))
    {
        if (index != 0) {
            GetContainer<T>()->IncRefStrong(index - 1);
        }
    }

    Handle &operator=(const Handle &other)
    {
        if (other.index == index) {
            return *this;
        }

        if (index != 0) {
            GetContainer<T>()->DecRefStrong(index - 1);
        }

        index = other.index;

        if (index != 0) {
            GetContainer<T>()->IncRefStrong(index - 1);
        }

        return *this;
    }

    Handle(Handle &&other) noexcept
        : HandleBase(static_cast<HandleBase &&>(other))
    {
        other.index = 0;
    }

    Handle &operator=(Handle &&other) noexcept
    {
        if (other.index == index) {
            return *this;
        }

        if (index != 0) {
            GetContainer<T>()->DecRefStrong(index - 1);
        }

        index = other.index;
        other.index = 0;

        return *this;
    }

    ~Handle()
    {
        if (index != 0) {
            GetContainer<T>()->DecRefStrong(index - 1);
        }
    }
    
    HYP_FORCE_INLINE T *operator->() const
        { return Get(); }
    
    HYP_FORCE_INLINE T &operator*()
        { return *Get(); }
    
    HYP_FORCE_INLINE const T &operator*() const
        { return *Get(); }
    
    HYP_FORCE_INLINE bool operator!() const
        { return !IsValid(); }
    
    HYP_FORCE_INLINE explicit operator bool() const
        { return IsValid(); }

    HYP_FORCE_INLINE operator IDType() const
        { return IDType(index); }
    
    HYP_FORCE_INLINE bool operator==(std::nullptr_t) const
        { return !IsValid(); }
    
    HYP_FORCE_INLINE bool operator!=(std::nullptr_t) const
        { return IsValid(); }
    
    HYP_FORCE_INLINE bool operator==(const Handle &other) const
        { return index == other.index; }
    
    HYP_FORCE_INLINE bool operator!=(const Handle &other) const
        { return index != other.index; }
    
    /*! \brief Compare two handles by their index.
     *  \param other The handle to compare to.
     *  \return True if the handle is less than the other handle. */
    HYP_FORCE_INLINE bool operator<(const Handle &other) const
        { return index < other.index; }
    
    HYP_FORCE_INLINE bool operator==(const IDType &id) const
        { return index == id.Value(); }
    
    HYP_FORCE_INLINE bool operator!=(const IDType &id) const
        { return index != id.Value(); }

    HYP_FORCE_INLINE bool operator<(const IDType &id) const
        { return index < id.Value(); }
    
    /*! \brief Check if the handle is valid. A handle is valid if its index is greater than 0.
     *  \return True if the handle is valid. */
    HYP_FORCE_INLINE bool IsValid() const
        { return index != 0; }
    
    /*! \brief Get a referenceable ID for the object that the handle is referencing.
     *  \return The ID of the object. */
    HYP_FORCE_INLINE IDType GetID() const
        { return { uint(index) }; }
    
    /*! \brief Get the TypeID for this handle type
     *  \return The TypeID for the handle */
    HYP_FORCE_INLINE constexpr TypeID GetTypeID() const
        { return TypeID::ForType<T>(); }
    
    /*! \brief Get a pointer to the object that the handle is referencing.
     *  \return A pointer to the object. */
    HYP_FORCE_INLINE T *Get() const
    {
        if (index == 0) {
            return nullptr;
        }

        return static_cast<T *>(GetContainer<T>()->GetPointer(index - 1));
    }
    
    /*! \brief Reset the handle to an empty state.
     *  \details This will decrement the strong reference count of the object that the handle is referencing.
     *  The index is set to 0. */
    HYP_FORCE_INLINE void Reset()
    {
        if (index != 0) {
            GetContainer<T>()->DecRefStrong(index - 1);
        }

        index = 0;
    }

    HYP_FORCE_INLINE WeakHandle<T> ToWeak() const
    {
        return WeakHandle<T>(*this);
    }
    
    static Name GetTypeName()
    {
        static const Name type_name = CreateNameFromDynamicString(TypeNameWithoutNamespace<NormalizedType<T>>().Data());

        return type_name;
    }
    
    HYP_FORCE_INLINE HashCode GetHashCode() const
    {
        HashCode hc;
        hc.Add(GetTypeName().GetHashCode());
        hc.Add(index);

        return hc;
    }
};

template <class T>
struct WeakHandle
{
    using IDType = ID<T>;

    static_assert(has_opaque_handle_defined<T>, "Type does not support handles");

    static const WeakHandle empty;

    /*! \brief The index of the object in the object pool's container for \ref{T} */
    uint index;

    WeakHandle()
        : index(0)
    {
    }

    /*! \brief Construct a WeakHandle from the given ID.
     *  \param id The ID of the object to reference. */
    explicit WeakHandle(IDType id)
        : index(id.Value())
    {
        if (index != 0) {
            GetContainer<T>()->IncRefWeak(index - 1);
        }
    }

    template <class TPointerType, typename = std::enable_if_t<IsHypObject<TPointerType>::value && std::is_convertible_v<TPointerType *, T *>>>
    explicit WeakHandle(TPointerType *ptr)
        : index(0)
    {
        if (ptr != nullptr) {
            *this = ptr->WeakHandleFromThis();
        }
    }

    WeakHandle(const Handle<T> &other)
        : index(other.index)
    {
        if (index != 0) {
            GetContainer<T>()->IncRefWeak(index - 1);
        }
    }

    WeakHandle &operator=(const Handle<T> &other)
    {
        if (index != 0) {
            GetContainer<T>()->DecRefWeak(index - 1);
        }

        index = other.index;

        if (index != 0) {
            GetContainer<T>()->IncRefWeak(index - 1);
        }

        return *this;
    }

    WeakHandle(const WeakHandle &other)
        : index(other.index)
    {
        if (index != 0) {
            GetContainer<T>()->IncRefWeak(index - 1);
        }
    }

    WeakHandle &operator=(const WeakHandle &other)
    {
        if (index != 0) {
            GetContainer<T>()->DecRefWeak(index - 1);
        }

        index = other.index;

        if (index != 0) {
            GetContainer<T>()->IncRefWeak(index - 1);
        }

        return *this;
    }

    WeakHandle(WeakHandle &&other) noexcept
        : index(other.index)
    {
        other.index = 0;
    }

    WeakHandle &operator=(WeakHandle &&other) noexcept
    {
        if (index != 0) {
            GetContainer<T>()->DecRefWeak(index - 1);
        }

        index = other.index;
        other.index = 0;

        return *this;
    }

    ~WeakHandle()
    {
        if (index != 0) {
            GetContainer<T>()->DecRefWeak(index - 1);
        }
    }

    /*! \brief Lock the weak handle to get a strong reference to the object.
     *  \details If the object is still alive, a strong reference is returned. Otherwise, an empty handle is returned.
     *  \return A strong reference to the object. */
    HYP_NODISCARD HYP_FORCE_INLINE Handle<T> Lock() const
    {
        if (index == 0) {
            return Handle<T>();
        }

        return GetContainer<T>()->GetObjectBytes(index - 1).GetRefCountStrong() != 0
            ? Handle<T>(IDType { index })
            : Handle<T>();
    }

    HYP_FORCE_INLINE T *GetUnsafe() const
    {
        if (index == 0) {
            return nullptr;
        }

        return &GetContainer<T>()->Get(index - 1);
    }
    
    HYP_FORCE_INLINE bool operator!() const
        { return !IsValid(); }
    
    HYP_FORCE_INLINE explicit operator bool() const
        { return IsValid(); }

    HYP_FORCE_INLINE operator IDType() const
        { return IDType(index); }
    
    HYP_FORCE_INLINE bool operator==(std::nullptr_t) const
        { return !IsValid(); }
    
    HYP_FORCE_INLINE bool operator!=(std::nullptr_t) const
        { return IsValid(); }
    
    HYP_FORCE_INLINE bool operator==(const WeakHandle &other) const
        { return index == other.index; }
    
    HYP_FORCE_INLINE bool operator!=(const WeakHandle &other) const
        { return index != other.index; }

    HYP_FORCE_INLINE bool operator<(const WeakHandle &other) const
        { return index < other.index; }
    
    HYP_FORCE_INLINE bool operator==(const Handle<T> &other) const
        { return index == other.index; }
    
    HYP_FORCE_INLINE bool operator!=(const Handle<T> &other) const
        { return index != other.index; }
    
    HYP_FORCE_INLINE bool operator<(const Handle<T> &other) const
        { return index < other.index; }
    
    HYP_FORCE_INLINE bool operator==(const IDType &id) const
        { return index == id.Value(); }
    
    HYP_FORCE_INLINE bool operator!=(const IDType &id) const
        { return index != id.Value(); }
    
    HYP_FORCE_INLINE bool operator<(const IDType &id) const
        { return index < id.Value(); }
    
    HYP_FORCE_INLINE bool IsValid() const
        { return index != 0; }
    
    HYP_FORCE_INLINE IDType GetID() const
        { return { uint(index) }; }
    
    /*! \brief Get the TypeID for this handle type
     *  \return The TypeID for the handle */
    HYP_FORCE_INLINE constexpr TypeID GetTypeID() const
        { return TypeID::ForType<T>(); }

    void Reset()
    {
        if (index != 0) {
            GetContainer<T>()->DecRefWeak(index - 1);
        }

        index = 0;
    }
    
    static Name GetTypeName()
    {
        static const Name type_name = CreateNameFromDynamicString(TypeNameWithoutNamespace<NormalizedType<T>>().Data());

        return type_name;
    }
    
    HYP_FORCE_INLINE HashCode GetHashCode() const
    {
        HashCode hc;
        hc.Add(GetTypeName().GetHashCode());
        hc.Add(index);

        return hc;
    }
};

struct AnyHandle
{
    using IDType = IDBase;

    TypeID  type_id = TypeID::Void();
    uint32  index = 0;

    HYP_API AnyHandle(TypeID type_id, IDBase id);

    template <class T>
    explicit AnyHandle(ID<T> id)
        : AnyHandle(TypeID::ForType<T>(), IDBase { id.Value() })
    {
    }

    template <class T>
    AnyHandle(const Handle<T> &handle)
        : AnyHandle(TypeID::ForType<T>(), IDBase { handle.index })
    {
    }

    template <class T>
    AnyHandle(Handle<T> &&handle)
        : type_id(TypeID::ForType<T>()),
          index(handle.index)
    {
        handle.index = 0;
    }

    AnyHandle(const AnyHandle &other)
        : AnyHandle(other.type_id, IDBase { other.index })
    {
    }

    HYP_API AnyHandle &operator=(const AnyHandle &other);

    AnyHandle(AnyHandle &&other) noexcept
        : type_id(other.type_id),
          index(other.index)
    {
        other.type_id = TypeID::Void();
        other.index = 0;
    }

    HYP_API AnyHandle &operator=(AnyHandle &&other) noexcept;

    HYP_API ~AnyHandle();
    
    HYP_FORCE_INLINE bool operator!() const
        { return !IsValid(); }
    
    HYP_FORCE_INLINE explicit operator bool() const
        { return IsValid(); }
    
    HYP_FORCE_INLINE bool operator==(std::nullptr_t) const
        { return !IsValid(); }
    
    HYP_FORCE_INLINE bool operator!=(std::nullptr_t) const
        { return IsValid(); }
    
    HYP_FORCE_INLINE bool operator==(const AnyHandle &other) const
        { return type_id == other.type_id && index == other.index; }
    
    HYP_FORCE_INLINE bool operator!=(const AnyHandle &other) const
        { return type_id != other.type_id || index != other.index; }
    
    template <class T>
    HYP_FORCE_INLINE bool operator==(const Handle<T> &other) const
        { return type_id == other.GetTypeID() && index == other.index; }
    
    template <class T>
    HYP_FORCE_INLINE bool operator!=(const Handle<T> &other) const
        { return type_id != other.GetTypeID() || index != other.index; }
    
    /*! \brief Compare two handles by their index.
     *  \param other The handle to compare to.
     *  \return True if the handle is less than the other handle. */
    HYP_FORCE_INLINE bool operator<(const AnyHandle &other) const
        { return index < other.index; }
    
    template <class T>
    HYP_FORCE_INLINE bool operator==(const ID<T> &id) const
        { return TypeID::ForType<T>() == type_id && index == id.Value(); }
    
    template <class T>
    HYP_FORCE_INLINE bool operator!=(const IDType &id) const
        { return TypeID::ForType<T>() != type_id || index != id.Value(); }
    
    /*! \brief Check if the handle is valid. A handle is valid if its index is greater than 0.
     *  \return True if the handle is valid. */
    HYP_FORCE_INLINE bool IsValid() const
        { return index != 0; }
    
    /*! \brief Get a referenceable ID for the object that the handle is referencing.
     *  \return The ID of the object. */
    HYP_FORCE_INLINE IDType GetID() const
        { return { uint(index) }; }
    
    /*! \brief Get the TypeID for this handle type
     *  \return The TypeID for the handle */
    HYP_FORCE_INLINE TypeID GetTypeID() const
        { return type_id; }

    template <class T>
    HYP_FORCE_INLINE bool Is() const
        { return type_id == TypeID::ForType<T>(); }

    template <class T>
    HYP_NODISCARD Handle<T> Cast() const
    {
        if (!type_id || type_id != TypeID::ForType<T>()) {
            return { };
        }

        return Handle<T>(typename Handle<T>::IDType { index });
    }

    HYP_API AnyRef ToRef() const;

    template <class T>
    HYP_FORCE_INLINE T *TryGet() const
        { return ToRef().TryGet<T>(); }
};

template <class T>
const Handle<T> Handle<T>::empty = { };

template <class T>
const WeakHandle<T> WeakHandle<T>::empty = { };

template <class T>
ObjectContainerBase *Handle<T>::s_container = nullptr;

//template <class T>
//ObjectContainerBase *WeakHandle<T>::s_container = nullptr;

template <class T>
HYP_NODISCARD HYP_FORCE_INLINE inline Handle<T> CreateObject()
{
    //auto *container = GetContainer<T>();
    
    ObjectContainer<T> &container = ObjectPool::GetObjectContainerHolder().GetObjectContainer<T>(HandleDefinition<T>::GetAllottedContainerPointer());

    const uint32 index = container.NextIndex();
    container.ConstructAtIndex(index);

    return Handle<T>(ID<T>::FromIndex(index));
}

template <class T, class... Args>
HYP_NODISCARD HYP_FORCE_INLINE inline Handle<T> CreateObject(Args &&... args)
{
    //auto *container = GetContainer<T>();
    
    ObjectContainer<T> &container = ObjectPool::GetObjectContainerHolder().GetObjectContainer<T>(HandleDefinition<T>::GetAllottedContainerPointer());

    const uint32 index = container.NextIndex();

    container.ConstructAtIndex(
        index,
        std::forward<Args>(args)...
    );

    return Handle<T>(ID<T>::FromIndex(index));
}

template <class T>
HYP_FORCE_INLINE inline bool InitObject(const Handle<T> &handle)
{
    if (!handle) {
        return false;
    }

    if (!handle->GetID()) {
        return false;
    }

    handle->Init();

    return true;
}

#define DEF_HANDLE(T, _max_size) \
    class T; \
    \
    extern UniquePtr<ObjectContainerBase> *g_container_ptr_##T; \
    \
    template <> \
    struct HandleDefinition< T > \
    { \
        static constexpr const char *class_name = HYP_STR(T); \
        static constexpr SizeType max_size = (_max_size); \
        \
        HYP_API static UniquePtr<ObjectContainerBase> *GetAllottedContainerPointer(); \
    };

#define DEF_HANDLE_NS(ns, T, _max_size) \
    namespace ns { \
    class T; \
    } \
    \
    extern UniquePtr<ObjectContainerBase> *g_container_ptr_##T; \
    \
    template <> \
    struct HandleDefinition< ns::T > \
    { \
        static constexpr const char *class_name = HYP_STR(ns) "::" HYP_STR(T); \
        static constexpr SizeType max_size = (_max_size); \
        \
        HYP_API static UniquePtr<ObjectContainerBase> *GetAllottedContainerPointer(); \
    };

#include <core/inl/HandleDefinitions.inl>

#undef DEF_HANDLE
#undef DEF_HANDLE_NS

} // namespace hyperion

#endif
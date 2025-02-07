/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_CORE_HYP_OBJECT_HPP
#define HYPERION_CORE_HYP_OBJECT_HPP

#include <core/ID.hpp>
#include <core/Name.hpp>
#include <core/Defines.hpp>

#include <core/object/HypObjectFwd.hpp>
#include <core/object/HypObjectEnums.hpp>

#include <core/utilities/TypeID.hpp>

#include <core/threading/AtomicVar.hpp>

#include <core/functional/Delegate.hpp>

#include <dotnet/Object.hpp>

#include <HashCode.hpp>
#include <Types.hpp>

#include <type_traits>

namespace hyperion {

class HypClass;
struct HypData;

template <class T>
struct Handle;

template <class T>
struct WeakHandle;

namespace dotnet {
class Class;
} // namespace dotnet

extern HYP_API void InitHypObjectInitializer(IHypObjectInitializer *initializer, void *parent, TypeID type_id, const HypClass *hyp_class, dotnet::Object &&managed_object);
extern HYP_API const HypClass *GetClass(TypeID type_id);
extern HYP_API HypClassAllocationMethod GetHypClassAllocationMethod(const HypClass *hyp_class);
extern HYP_API dotnet::Class *GetHypClassManagedClass(const HypClass *hyp_class);

// Ensure the current HypObjectInitializer being constructed is the same as the one passed
extern HYP_API void CheckHypObjectInitializer(const IHypObjectInitializer *initializer, TypeID type_id, const HypClass *hyp_class, const void *address);
extern HYP_API void CleanupHypObjectInitializer(const HypClass *hyp_class, dotnet::Object *managed_object_ptr);

extern HYP_API bool IsInstanceOfHypClass(const HypClass *hyp_class, const void *ptr, TypeID type_id);
extern HYP_API bool IsInstanceOfHypClass(const HypClass *hyp_class, const HypClass *instance_hyp_class);

template <class T>
class HypObjectInitializer final : public IHypObjectInitializer
{
    // static const void (*s_fixup_fptr)(void *_this, IHypObjectInitializer *ptr);

public:
    HypObjectInitializer(T *_this)
    {
        CheckHypObjectInitializer(this, GetTypeID_Static(), GetClass_Static(), _this);
    }

    virtual ~HypObjectInitializer() override
    {
#ifdef HYP_DEBUG_MODE
        HYP_MT_CHECK_RW(m_data_race_detector);
#endif

        CleanupHypObjectInitializer(GetClass_Static(), &m_managed_object);
    }

    virtual TypeID GetTypeID() const override
    {
        return GetTypeID_Static();
    }

    virtual const HypClass *GetClass() const override
    {
        return GetClass_Static();
    }

    virtual dotnet::Class *GetManagedClass() const override
    {
        return GetHypClassManagedClass(GetClass_Static());
    }

    virtual void SetManagedObject(dotnet::Object &&managed_object) override
    {
#ifdef HYP_DEBUG_MODE
        HYP_MT_CHECK_RW(m_data_race_detector);
#endif

        m_managed_object = std::move(managed_object);
    }

    virtual dotnet::Object *GetManagedObject() const override
    {
#ifdef HYP_DEBUG_MODE
        HYP_MT_CHECK_READ(m_data_race_detector);
#endif

        return m_managed_object.IsValid() ? const_cast<dotnet::Object *>(&m_managed_object) : nullptr;
    }

    virtual void FixupPointer(void *_this, IHypObjectInitializer *ptr) override
    {
        static_cast<T *>(_this)->m_hyp_object_initializer_ptr = ptr;
    }

    HYP_FORCE_INLINE static TypeID GetTypeID_Static()
    {
        static constexpr TypeID type_id = TypeID::ForType<T>();

        return type_id;
    }

    HYP_FORCE_INLINE static const HypClass *GetClass_Static()
    {
        static const HypClass *hyp_class = ::hyperion::GetClass(TypeID::ForType<T>());

        return hyp_class;
    }

private:
    dotnet::Object      m_managed_object;

#ifdef HYP_DEBUG_MODE
    DataRaceDetector    m_data_race_detector;
#endif
};

// template <class T>
// const void (*HypObjectInitializer<T>::s_fixup_fptr)(void *, IHypObjectInitializer *) = [](void *_this, IHypObjectInitializer *_ptr)
// {
// };

// template <class T>
// struct HypObjectInitializerRef
// {
//     Variant<HypObjectInitializer<T>, IHypObjectInitializer *>   value;
//     IHypObjectInitializer                                       *ptr;

//     HypObjectInitializerRef()
//         : ptr(nullptr)
//     {
//     }

//     HypObjectInitializerRef
// };

#define HYP_OBJECT_BODY(T, ...) \
    private: \
        friend class HypObjectInitializer<T>; \
        friend struct HypClassInitializer_##T; \
        \
        HypObjectInitializer<T> m_hyp_object_initializer { this }; \
        IHypObjectInitializer   *m_hyp_object_initializer_ptr = &m_hyp_object_initializer; \
        \
    public: \
        struct HypObjectData \
        { \
            using Type = T; \
            \
            static constexpr bool is_hyp_object = true; \
        }; \
        \
        HYP_FORCE_INLINE IHypObjectInitializer *GetObjectInitializer() const \
            { return m_hyp_object_initializer_ptr; } \
        \
        HYP_FORCE_INLINE dotnet::Object *GetManagedObject() const \
            { return m_hyp_object_initializer_ptr->GetManagedObject(); } \
        \
        HYP_FORCE_INLINE const HypClass *InstanceClass() const \
            { return m_hyp_object_initializer_ptr->GetClass(); } \
        \
        HYP_FORCE_INLINE static const HypClass *Class() \
            { return HypObjectInitializer<T>::GetClass_Static(); } \
        \
        template <class TOther> \
        HYP_FORCE_INLINE bool IsInstanceOf() const \
        { \
            if constexpr (std::is_same_v<T, TOther> || std::is_base_of_v<TOther, T>) { \
                return true; \
            } else { \
                const HypClass *other_hyp_class = GetClass(TypeID::ForType<TOther>()); \
                if (!other_hyp_class) { \
                    return false; \
                } \
                return IsInstanceOfHypClass(other_hyp_class, InstanceClass()); \
            } \
        } \
        \
        HYP_FORCE_INLINE bool IsInstanceOf(const HypClass *other_hyp_class) const \
        { \
            if (!other_hyp_class) { \
                return false; \
            } \
            return IsInstanceOfHypClass(other_hyp_class, InstanceClass()); \
        } \
    private:

template <class T>
class HypObject : public HypObjectBase
{
    using InnerType = T;

public:
    enum InitState : uint16
    {
        INIT_STATE_UNINITIALIZED    = 0x0,
        INIT_STATE_INIT_CALLED      = 0x1,
        INIT_STATE_READY            = 0x2
    };

    HypObject()
        : m_init_state(INIT_STATE_UNINITIALIZED)
    {
    }

    HypObject(const HypObject &other)               = delete;
    HypObject &operator=(const HypObject &other)    = delete;

    HypObject(HypObject &&other) noexcept
        : m_init_state(other.m_init_state.Exchange(INIT_STATE_UNINITIALIZED, MemoryOrder::ACQUIRE_RELEASE))
    {
    }

    HypObject &operator=(HypObject &&other) noexcept = delete;

    virtual ~HypObject() override = default;

    HYP_FORCE_INLINE ID<InnerType> GetID() const
        { return ID<InnerType>(HypObjectBase::GetID().Value()); }

    HYP_FORCE_INLINE bool IsInitCalled() const
        { return m_init_state.Get(MemoryOrder::RELAXED) & INIT_STATE_INIT_CALLED; }

    HYP_FORCE_INLINE bool IsReady() const
        { return m_init_state.Get(MemoryOrder::RELAXED) & INIT_STATE_READY; }

    // HYP_FORCE_INLINE static const HypClass *GetClass()
    //     { return s_class_info.GetClass(); }

    void Init()
    {
        m_init_state.BitOr(INIT_STATE_INIT_CALLED, MemoryOrder::RELAXED);
    }

    HYP_FORCE_INLINE Handle<T> HandleFromThis() const
        { return Handle<T>(GetObjectHeader_Internal()); }

    HYP_FORCE_INLINE WeakHandle<T> WeakHandleFromThis() const
        { return WeakHandle<T>(GetObjectHeader_Internal());  }

protected:
    void SetReady(bool is_ready)
    {
        if (is_ready) {
            m_init_state.BitOr(INIT_STATE_READY, MemoryOrder::RELAXED);
        } else {
            m_init_state.BitAnd(~INIT_STATE_READY, MemoryOrder::RELAXED);
        }
    }

    HYP_FORCE_INLINE void AssertReady() const
    {
        AssertThrowMsg(
            IsReady(),
            "Object is not in ready state; maybe Init() has not been called on it, "
            "or the component requires an event to be sent from the Engine instance to determine that "
            "it is ready to be constructed, and this event has not yet been sent.\n"
        );
    }

    HYP_FORCE_INLINE void AssertIsInitCalled() const
    {
        AssertThrowMsg(
            IsInitCalled(),
            "Object has not had Init() called on it!\n"
        );
    }

    void AddDelegateHandler(Name name, DelegateHandler &&delegate_handler)
    {
        m_delegate_handlers.Add(name, std::move(delegate_handler));
    }

    void AddDelegateHandler(DelegateHandler &&delegate_handler)
    {
        m_delegate_handlers.Add(std::move(delegate_handler));
    }

    bool RemoveDelegateHandler(WeakName name)
    {
        return m_delegate_handlers.Remove(name);
    }
    
    AtomicVar<uint16>   m_init_state;
    DelegateHandlerSet  m_delegate_handlers;
};

} // namespace hyperion

#endif
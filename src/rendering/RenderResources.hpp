/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_RENDER_RESOURCES_HPP
#define HYPERION_RENDER_RESOURCES_HPP

#include <core/threading/Threads.hpp>
#include <core/threading/AtomicVar.hpp>
#include <core/threading/Semaphore.hpp>
#include <core/threading/DataRaceDetector.hpp>

#include <core/functional/Proc.hpp>

#include <core/memory/RefCountedPtr.hpp>
#include <core/memory/UniquePtr.hpp>

#include <core/memory/MemoryPool.hpp>

#include <Types.hpp>

namespace hyperion {

template <class T>
class RenderResourcesMemoryPool;

class IRenderResourcesMemoryPool;

class GPUBufferHolderBase;

struct RenderResourcesMemoryPoolHandle
{
    uint32  index = ~0u;

    HYP_FORCE_INLINE explicit operator bool() const
        { return index != ~0u; }

    HYP_FORCE_INLINE bool operator!() const
        { return index == ~0u; }
};

// Represents the objects an engine object (e.g Material) uses while it is currently being rendered.
// The resources are reference counted internally, so as long as the object is being used for rendering somewhere,
// the resources will remain in memory.
class RenderResourcesBase
{
    using PreInitSemaphore = Semaphore<int32, SemaphoreDirection::WAIT_FOR_ZERO_OR_NEGATIVE, threading::detail::AtomicSemaphoreImpl<int32, SemaphoreDirection::WAIT_FOR_ZERO_OR_NEGATIVE>>;
    using InitSemaphore = Semaphore<int32, SemaphoreDirection::WAIT_FOR_POSITIVE, threading::detail::AtomicSemaphoreImpl<int32, SemaphoreDirection::WAIT_FOR_POSITIVE>>;
    using CompletionSemaphore = Semaphore<int32, SemaphoreDirection::WAIT_FOR_ZERO_OR_NEGATIVE, threading::detail::AtomicSemaphoreImpl<int32, SemaphoreDirection::WAIT_FOR_ZERO_OR_NEGATIVE>>;

public:
    template <class T>
    friend class RenderResourcesMemoryPool;

    RenderResourcesBase();

    RenderResourcesBase(const RenderResourcesBase &other)                   = delete;
    RenderResourcesBase &operator=(const RenderResourcesBase &other)        = delete;

    RenderResourcesBase(RenderResourcesBase &&other) noexcept;
    RenderResourcesBase &operator=(RenderResourcesBase &&other) noexcept    = delete;

    virtual ~RenderResourcesBase();

    virtual bool IsNull() const
        { return false; }

    void Claim();
    void Unclaim();

    /*! \brief Waits (blocking) until all operations on this RenderResources are complete and the RenderResources is no longer being used. */
    void WaitForCompletion();

    /*! \note Only call from render thread or from task on a task thread that is initiated by the render thread. */
    HYP_FORCE_INLINE uint32 GetBufferIndex() const
        { return m_buffer_index; }

#ifdef HYP_DEBUG_MODE
    HYP_FORCE_INLINE uint32 GetUseCount() const
    {
        return m_ref_count.Get(MemoryOrder::SEQUENTIAL);
    }
#endif

protected:
    HYP_FORCE_INLINE bool IsInitialized() const
        { return m_init_semaphore.IsInSignalState(); }

    virtual void Initialize() = 0;
    virtual void Destroy() = 0;
    virtual void Update() = 0;

    virtual GPUBufferHolderBase *GetGPUBufferHolder() const { return nullptr; }

    virtual Name GetTypeName() const = 0;

    /*! \brief Performs an operation on the render thread if the resources are initialized,
     *  otherwise executes it immediately on the calling thread. Initialization on the render thread will not begin until at least the end of the given proc,
     *  so it is safe to use this method on any thread.
     *  \param proc The operation to perform.
     *  \param force_render_thread If true, the operation will be performed on the render thread regardless of initialization state. */
    void Execute(Proc<void> &&proc, bool force_render_thread = false);

    void SetNeedsUpdate();

    bool                            m_is_initialized;
    uint32                          m_buffer_index;
    void                            *m_buffer_address;

private:
    void AcquireBufferIndex();
    void ReleaseBufferIndex();

    RenderResourcesMemoryPoolHandle m_pool_handle;

    AtomicVar<int16>                m_ref_count;
    AtomicVar<int16>                m_update_counter;

    PreInitSemaphore                m_pre_init_semaphore;
    InitSemaphore                   m_init_semaphore;
    CompletionSemaphore             m_completion_semaphore;

#ifdef HYP_ENABLE_MT_CHECK
    DataRaceDetector                m_data_race_detector;
#endif
};

class IRenderResourcesMemoryPool
{
public:
    virtual ~IRenderResourcesMemoryPool() = default;
};

extern HYP_API IRenderResourcesMemoryPool *GetOrCreateRenderResourcesMemoryPool(TypeID type_id, UniquePtr<IRenderResourcesMemoryPool>(*create_fn)(void));

template <class T>
class RenderResourcesMemoryPool final : private MemoryPool<ValueStorage<T>>, public IRenderResourcesMemoryPool
{
public:
    static_assert(std::is_base_of_v<RenderResourcesBase, T>, "T must be a subclass of RenderResourcesBase");

    using Base = MemoryPool<ValueStorage<T>>;

    static RenderResourcesMemoryPool<T> *GetInstance()
    {
        static IRenderResourcesMemoryPool *pool = GetOrCreateRenderResourcesMemoryPool(TypeID::ForType<T>(), []() -> UniquePtr<IRenderResourcesMemoryPool>
        {
            return MakeUnique<RenderResourcesMemoryPool<T>>();
        });
        
        return static_cast<RenderResourcesMemoryPool<T> *>(pool);
    }

    RenderResourcesMemoryPool()
        : Base()
    {
    }

    virtual ~RenderResourcesMemoryPool() override = default;

    template <class... Args>
    T *Allocate(Args &&... args)
    {
        ValueStorage<T> *element;
        const uint32 index = Base::AcquireIndex(&element);

        T *ptr = element->Construct(std::forward<Args>(args)...);
        ptr->m_pool_handle = RenderResourcesMemoryPoolHandle { index };

        return ptr;
    }

    void Free(T *ptr)
    {
        AssertThrow(ptr != nullptr);

        // Wait for it to finish any tasks before destroying
        ptr->WaitForCompletion();

        const RenderResourcesMemoryPoolHandle pool_handle = ptr->m_pool_handle;
        AssertThrow(pool_handle);

        // Invoke the destructor
        ValueStorage<T> &element = Base::GetElement(pool_handle.index);
        element.Destruct();

        Base::ReleaseIndex(pool_handle.index);
    }
};

template <class T, class... Args>
HYP_FORCE_INLINE static T *AllocateRenderResources(Args &&... args)
{
    return RenderResourcesMemoryPool<T>::GetInstance()->Allocate(std::forward<Args>(args)...);
}

template <class T>
HYP_FORCE_INLINE static void FreeRenderResources(T *ptr)
{
    if (!ptr) {
        return;
    }

    RenderResourcesMemoryPool<T>::GetInstance()->Free(ptr);
}

// Returns a render resources object that will return true for IsNull().
// To be used as a placeholder.
HYP_API RenderResourcesBase &GetNullRenderResources();

class RenderResourcesHandle
{
public:
    RenderResourcesHandle()
        : render_resources(&GetNullRenderResources())
    {
    }

    RenderResourcesHandle(RenderResourcesBase &render_resources)
        : render_resources(&render_resources)
    {
        render_resources.Claim();
    }

    RenderResourcesHandle(const RenderResourcesHandle &other)
        : render_resources(other.render_resources)
    {
        if (!render_resources->IsNull()) {
            render_resources->Claim();
        }
    }

    RenderResourcesHandle &operator=(const RenderResourcesHandle &other)
    {
        if (this == &other || render_resources == other.render_resources) {
            return *this;
        }

        if (!render_resources->IsNull()) {
            render_resources->Unclaim();
        }

        render_resources = other.render_resources;

        if (!render_resources->IsNull()) {
            render_resources->Claim();
        }

        return *this;
    }

    RenderResourcesHandle(RenderResourcesHandle &&other) noexcept
        : render_resources(other.render_resources)
    {
        other.render_resources = &GetNullRenderResources();
    }

    RenderResourcesHandle &operator=(RenderResourcesHandle &&other) noexcept
    {
        if (this == &other) {
            return *this;
        }

        if (!render_resources->IsNull()) {
            render_resources->Unclaim();
        }

        render_resources = other.render_resources;
        other.render_resources = &GetNullRenderResources();

        return *this;
    }

    ~RenderResourcesHandle()
    {
        if (!render_resources->IsNull()) {
            render_resources->Unclaim();
        }
    }

    void Reset()
    {
        if (!render_resources->IsNull()) {
            render_resources->Unclaim();

            render_resources = &GetNullRenderResources();
        }
    }

    HYP_FORCE_INLINE explicit operator bool() const
        { return !render_resources->IsNull(); }

    HYP_FORCE_INLINE bool operator!() const
        { return render_resources->IsNull(); }

    HYP_FORCE_INLINE bool operator==(const RenderResourcesHandle &other) const
        { return render_resources == other.render_resources; }

    HYP_FORCE_INLINE bool operator!=(const RenderResourcesHandle &other) const
        { return render_resources != other.render_resources; }

    HYP_FORCE_INLINE RenderResourcesBase *operator->() const
        { return render_resources; }

    HYP_FORCE_INLINE RenderResourcesBase &operator*() const
        { return *render_resources; }

private:
    RenderResourcesBase *render_resources;
};

template <class T>
class TRenderResourcesHandle
{
public:
    TRenderResourcesHandle()    = default;

    TRenderResourcesHandle(T &render_resources)
        : handle(render_resources)
    {
        static_assert(std::is_base_of_v<RenderResourcesBase, T>, "T must be a subclass of RenderResourcesBase");
    }

    TRenderResourcesHandle(const TRenderResourcesHandle &other)
        : handle(other.handle)
    {
    }

    TRenderResourcesHandle &operator=(const TRenderResourcesHandle &other)
    {
        if (this == &other) {
            return *this;
        }

        handle = other.handle;

        return *this;
    }

    TRenderResourcesHandle(TRenderResourcesHandle &&other) noexcept
        : handle(std::move(other.handle))
    {
    }

    TRenderResourcesHandle &operator=(TRenderResourcesHandle &&other) noexcept
    {
        if (this == &other) {
            return *this;
        }

        handle = std::move(other.handle);

        return *this;
    }

    ~TRenderResourcesHandle()   = default;

    HYP_FORCE_INLINE void Reset()
    {
        handle.Reset();
    }

    HYP_FORCE_INLINE explicit operator bool() const
    {
        // Check if the handle is not null and not the null render resources.
        return bool(handle);
    }

    HYP_FORCE_INLINE operator RenderResourcesHandle &() &
        { return handle; }

    HYP_FORCE_INLINE operator const RenderResourcesHandle &() const &
        { return handle; }

    HYP_FORCE_INLINE operator RenderResourcesHandle &&() &&
        { return std::move(handle); }

    HYP_FORCE_INLINE bool operator!() const
        { return !handle; }

    HYP_FORCE_INLINE bool operator==(const TRenderResourcesHandle &other) const
        { return handle == other.handle; }

    HYP_FORCE_INLINE bool operator!=(const TRenderResourcesHandle &other) const
        { return handle != other.handle; }

    HYP_FORCE_INLINE T *operator->() const
    {
        static_assert(std::is_base_of_v<RenderResourcesBase, T>, "T must be a subclass of RenderResourcesBase");

        RenderResourcesBase &ptr = *handle;

        if (ptr.IsNull()) {
            return nullptr;
        }

        // can safely cast to T since we know it's not null
        return static_cast<T *>(&ptr);
    }

    HYP_FORCE_INLINE T &operator*() const
    {
        static_assert(std::is_base_of_v<RenderResourcesBase, T>, "T must be a subclass of RenderResourcesBase");

        RenderResourcesBase &ptr = *handle;

        if (ptr.IsNull()) {
            HYP_FAIL("Dereferenced null render resources handle");
        }

        // can safely cast to T since we know it's not null
        return *static_cast<T *>(&ptr);
    }

private:
    RenderResourcesHandle   handle;
};

} // namespace hyperion

#endif
/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */
#ifndef HYPERION_CORE_ID_CREATOR_HPP
#define HYPERION_CORE_ID_CREATOR_HPP

#include <core/containers/Queue.hpp>
#include <core/threading/AtomicVar.hpp>
#include <core/threading/Mutex.hpp>
#include <core/containers/TypeMap.hpp>
#include <core/ID.hpp>
#include <Constants.hpp>
#include <Types.hpp>
#include <core/Defines.hpp>

#include <mutex>
#include <atomic>

namespace hyperion {

struct IDGenerator
{
    TypeID              type_id;
    AtomicVar<uint32>   id_counter { 0u };
    AtomicVar<uint32>   num_free_indices { 0u };
    Mutex               free_id_mutex;
    Queue<uint>         free_indices;

    uint NextID()
    {
        if (num_free_indices.Get(MemoryOrder::ACQUIRE) == 0) {
            return id_counter.Increment(1, MemoryOrder::ACQUIRE_RELEASE) + 1;
        }

        Mutex::Guard guard(free_id_mutex);
        const uint index = free_indices.Pop();

        num_free_indices.Decrement(1, MemoryOrder::RELEASE);

        return index;
    }

    void FreeID(uint index)
    {
        Mutex::Guard guard(free_id_mutex);

        free_indices.Push(index);

        num_free_indices.Increment(1, MemoryOrder::RELEASE);
    }
};

} // namespace hyperion

#endif